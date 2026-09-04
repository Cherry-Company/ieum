/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ClientApp.h"

#include "deskflow/ClientReconnectPolicy.h"
#include "deskflow/FileTransferService.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "client/Client.h"
#include "common/ExitCodes.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "deskflow/Screen.h"
#include "deskflow/ScreenException.h"
#include "deskflow/ipc/CoreIpc.h"
#include "net/NetworkAddress.h"
#include "net/SocketException.h"
#include "net/SocketMultiplexer.h"
#include "net/TCPSocketFactory.h"
#include "platform/FileTransferPlatformFactory.h"

#if defined(Q_OS_WIN)
#include "platform/MSWindowsScreen.h"
#endif

#include <QFileInfo> // Must include before XWindowsScreen to avoid conflicts with xlib.h

#if WINAPI_XWINDOWS
#include "platform/XWindowsScreen.h"
#endif

#if WINAPI_LIBEI
#include "platform/EiScreen.h"
#endif

#if defined(Q_OS_MAC)
#include "platform/OSXScreen.h"
#endif

#include <algorithm>
#include <memory>

ClientApp::ClientApp(IEventQueue *events, const QString &processName) : App(events, processName)
{
  // do nothing
}

ClientApp::~ClientApp() = default;

void ClientApp::parseArgs()
{
  // save server addresses (comma-separated list supported)
  if (const auto addressList = Settings::value(Settings::Client::RemoteHost).toString(); !addressList.isEmpty()) {
    const int port = Settings::value(Settings::Core::Port).toInt();
    const QStringList addresses = addressList.split(',', Qt::SkipEmptyParts);

    for (const QString &addr : addresses) {
      const QString trimmedAddr = addr.trimmed();
      if (trimmedAddr.isEmpty()) {
        continue;
      }

      try {
        NetworkAddress netAddr(trimmedAddr.toStdString(), port);
        netAddr.resolve();
        m_serverAddresses.append(netAddr);
        LOG_DEBUG("added server address: %s", qPrintable(trimmedAddr));
      } catch (SocketAddressException &e) {
        // allow an address that we can't look up if we're restartable.
        // we'll try to resolve the address each time we connect to the
        // server.  a bad port will never get better.
        if (e.getError() == SocketAddressException::SocketError::BadPort) {
          LOG_CRIT("%s: %s" BYE, qPrintable(processName()), e.what(), qPrintable(processName()));
          bye(s_exitFailed);
        } else {
          // Still add it - we'll try to resolve later
          NetworkAddress netAddr(trimmedAddr.toStdString(), port);
          m_serverAddresses.append(netAddr);
          LOG_WARN("could not resolve address '%s': %s (will retry later)", qPrintable(trimmedAddr), e.what());
        }
      }
    }

    if (m_serverAddresses.isEmpty()) {
      LOG_CRIT("%s: no valid server addresses specified" BYE, qPrintable(processName()), qPrintable(processName()));
      bye(s_exitFailed);
    }

    LOG_INFO("configured %zu server address(es)", static_cast<size_t>(m_serverAddresses.size()));
  }
}

const char *ClientApp::daemonName() const
{
  if (deskflow::platform::isWindows())
    return "Ieum Client";
  return "deskflow-client";
}

deskflow::Screen *ClientApp::createScreen()
{
#if defined(Q_OS_WIN)
  return new deskflow::Screen(
      new MSWindowsScreen(
          false, Settings::value(Settings::Core::UseHooks).toBool(), getEvents(),
          Settings::value(Settings::Client::LanguageSync).toBool()
      ),
      getEvents()
  );
#elif defined(Q_OS_MAC)
  return new deskflow::Screen(
      new OSXScreen(getEvents(), false, Settings::value(Settings::Client::LanguageSync).toBool()), getEvents()
  );
#else
  if (deskflow::platform::isWayland()) {
#if WINAPI_LIBEI
    LOG_INFO("using ei screen for wayland");
    return new deskflow::Screen(new deskflow::EiScreen(false, getEvents(), true), getEvents());
#else
    throw XNoEiSupport();
#endif
  }
#if WINAPI_XWINDOWS
  LOG_INFO("using legacy x windows screen");
  return new deskflow::Screen(
      new XWindowsScreen(qPrintable(Settings::value(Settings::Core::Display).toString()), false, getEvents()),
      getEvents()
  );
#endif
#endif // end os check
}

deskflow::Screen *ClientApp::openClientScreen()
{
  deskflow::Screen *screen = createScreen();
  getEvents()->addHandler(EventTypes::ScreenError, screen->getEventTarget(), [this](const auto &) {
    handleScreenError();
  });
  return screen;
}

void ClientApp::closeClientScreen(deskflow::Screen *screen)
{
  if (screen != nullptr) {
    getEvents()->removeHandler(EventTypes::ScreenError, screen->getEventTarget());
    delete screen;
  }
}

void ClientApp::handleClientRestart(const Event &, EventQueueTimer *timer)
{
  if (timer != m_restartTimer) {
    return;
  }

  getEvents()->removeHandler(EventTypes::Timer, timer);
  getEvents()->deleteTimer(timer);
  m_restartTimer = nullptr;

  if (!m_suspended) {
    startClient();
  }
}

void ClientApp::scheduleClientRestart(double retryTime)
{
  cancelClientRestart();
  if (m_suspended) {
    return;
  }

  LOG_DEBUG("retry in %.2f seconds", retryTime);
  ipcSendToClient("retryIn", QString::number(std::max(1.0, retryTime), 'f', 0));
  m_restartTimer = getEvents()->newOneShotTimer(retryTime, nullptr);
  getEvents()->addHandler(EventTypes::Timer, m_restartTimer, [this, timer = m_restartTimer](const auto &e) {
    handleClientRestart(e, timer);
  });
}

void ClientApp::cancelClientRestart()
{
  if (m_restartTimer == nullptr) {
    return;
  }
  getEvents()->removeHandler(EventTypes::Timer, m_restartTimer);
  getEvents()->deleteTimer(m_restartTimer);
  m_restartTimer = nullptr;
}

void ClientApp::handleClientConnected()
{
  cancelClientRestart();
  startFileTransferService();
  LOG_DEBUG("connected to server");
  ipcSendConnectionState(deskflow::core::ConnectionState::Connected);
  m_retryCount = 0;
  m_currentServerIndex = 0;
  m_lastServerAddressIndex = 0;
}

void ClientApp::handleClientFailed(const Event &e)
{
  if ((++m_lastServerAddressIndex) < m_client->getLastResolvedAddressesCount()) {
    // Try next resolved address for current hostname
    std::unique_ptr<Client::FailInfo> info(static_cast<Client::FailInfo *>(e.getData()));

    LOG_WARN("failed to connect to server=%s, trying next resolved address", qPrintable(info->m_what));
    if (!m_suspended) {
      scheduleClientRestart(retryTime());
    }
  } else {
    // All resolved addresses exhausted, try next server in list
    m_lastServerAddressIndex = 0;
    tryNextServer();

    if (m_currentServerIndex == 0) {
      // We've cycled through all servers, treat as refused
      handleClientRefused(e);
    } else {
      std::unique_ptr<Client::FailInfo> info(static_cast<Client::FailInfo *>(e.getData()));
      LOG_WARN("failed to connect to server=%s, trying next server in list", qPrintable(info->m_what));
      if (!m_suspended) {
        scheduleClientRestart(retryTime());
      }
    }
  }
}

void ClientApp::handleClientRefused(const Event &e)
{
  std::unique_ptr<Client::FailInfo> info(static_cast<Client::FailInfo *>(e.getData()));

  if (!info->m_retry) {
    LOG_ERR("failed to connect to server: %s", qPrintable(info->m_what));
    getEvents()->addEvent(Event(EventTypes::Quit));
  } else {
    LOG_WARN("failed to connect to server: %s", qPrintable(info->m_what));
    if (!m_suspended) {
      scheduleClientRestart(retryTime());
      ++m_retryCount;
    }
  }
}

void ClientApp::handleClientDisconnected()
{
  stopFileTransferService();
  m_retryCount = 0;
  LOG_DEBUG("disconnected from server");
  ipcSendConnectionState(deskflow::core::ConnectionState::Disconnected);
  if (!m_suspended) {
    scheduleClientRestart(retryTime());
  }
}

Client *ClientApp::openClient(const std::string &name, const NetworkAddress &address, deskflow::Screen *screen)
{
  auto *client = new Client(getEvents(), name, address, getSocketFactory(), screen);

  try {
    getEvents()->addHandler(EventTypes::ClientConnected, client->getEventTarget(), [this](const auto &) {
      handleClientConnected();
    });
    getEvents()->addHandler(EventTypes::ClientConnectionFailed, client->getEventTarget(), [this](const auto &e) {
      handleClientFailed(e);
    });
    getEvents()->addHandler(EventTypes::ClientConnectionRefused, client->getEventTarget(), [this](const auto &e) {
      handleClientRefused(e);
    });
    getEvents()->addHandler(EventTypes::ClientDisconnected, client->getEventTarget(), [this](const auto &) {
      handleClientDisconnected();
    });

  } catch (std::bad_alloc &ba) {
    delete client;
    throw ba;
  }

  return client;
}

void ClientApp::closeClient(Client *client)
{
  if (client == nullptr) {
    return;
  }
  using enum EventTypes;
  auto *target = client->getEventTarget();
  getEvents()->removeHandler(ClientConnected, target);
  getEvents()->removeHandler(ClientConnectionFailed, target);
  getEvents()->removeHandler(ClientConnectionRefused, target);
  getEvents()->removeHandler(ClientDisconnected, target);
  delete client;
}

bool ClientApp::startClient()
{
  deskflow::Screen *clientScreen = nullptr;
  try {
    if (m_clientScreen == nullptr) {
      clientScreen = openClientScreen();
      m_client = openClient(
          Settings::value(Settings::Core::ComputerName).toString().toStdString(), getCurrentServerAddress(),
          clientScreen
      );
      m_clientScreen = clientScreen;
      LOG_INFO("started client");
    }

    m_client->setServerAddress(getCurrentServerAddress());
    m_client->connect(m_lastServerAddressIndex);

    return true;
  } catch (ScreenUnavailableException &e) {
    LOG_WARN("secondary screen unavailable: %s", e.what());
    closeClientScreen(clientScreen);
  } catch (ScreenOpenFailureException &e) {
    LOG_CRIT("failed to start client: %s", e.what());
    closeClientScreen(clientScreen);
    m_retryCount = 0;
    return false;
  } catch (BaseException &e) {
    LOG_CRIT("failed to start client: %s", e.what());
    closeClientScreen(clientScreen);
    m_retryCount = 0;
    return false;
  }

  scheduleClientRestart(retryTime());
  ++m_retryCount;
  return true;
}

void ClientApp::stopClient()
{
  cancelClientRestart();
  stopFileTransferService();
  closeClient(m_client);
  closeClientScreen(m_clientScreen);
  m_client = nullptr;
  m_clientScreen = nullptr;
  m_retryCount = 0;
}

void ClientApp::startFileTransferService()
{
  if (m_fileTransferService != nullptr || m_client == nullptr || m_clientScreen == nullptr ||
      !deskflow::filetransfer::supportsFileTransferPlatform() ||
      !Settings::value(Settings::Security::TlsEnabled).toBool() || !Settings::hasProFileTransferEntitlement()) {
    return;
  }

  const auto sendEnabled = Settings::value(Settings::FileTransfer::Enabled).toBool();
  const auto receiveEnabled = Settings::value(Settings::FileTransfer::ReceiveEnabled).toBool();
  if (!sendEnabled && !receiveEnabled) {
    return;
  }

  auto platform = deskflow::filetransfer::createFileTransferPlatform();
  if (platform == nullptr) {
    return;
  }
  auto *platformScreen = m_clientScreen->getPlatformScreen();
  m_fileTransferService =
      std::make_unique<deskflow::filetransfer::FileTransferService>(deskflow::filetransfer::FileTransferServiceOptions{
          .localScreen = Settings::value(Settings::Core::ComputerName).toString().toStdString(),
          .destinationDirectory = deskflow::filetransfer::fileTransferPathFromQString(
              Settings::value(Settings::FileTransfer::DownloadDirectory).toString()
          ),
          .receiveEnabled = receiveEnabled,
          .authorizeFileTransfer = [] { return Settings::hasProFileTransferEntitlement(); },
          .sendControl = [this](
                             const deskflow::filetransfer::FileTransferControlMessage &message
                         ) { return m_client != nullptr && m_client->sendFileTransferControl(message); },
          .sendData = [this](
                          const deskflow::filetransfer::FileTransferDataMessage &message
                      ) { return m_client != nullptr && m_client->sendFileTransferData(message); },
          .sendEdge = [this](
                          const deskflow::filetransfer::FileTransferEdgeMessage &message
                      ) { return m_client != nullptr && m_client->sendFileTransferEdge(message); },
          .activeSidesChanged = [platformScreen, sendEnabled](std::uint32_t activeSides) {
#if defined(Q_OS_WIN)
            static_cast<void>(platformScreen);
            ipcSendFileTransferActiveSides(sendEnabled ? activeSides : 0);
#else
            (void)platformScreen->configureFileTransferEdgeDrop(sendEnabled ? activeSides : 0);
#endif
          },
          .platform = std::move(platform),
          .events = getEvents(),
          .eventTarget = m_client->getEventTarget(),
      });

#if defined(Q_OS_WIN)
  ipcSendFileTransferActiveSides(0);
#else
  if (sendEnabled && !platformScreen->installFileTransferEdgeDrop([this](auto drop) {
        if (m_fileTransferService == nullptr || !m_fileTransferService->beginEdgeDrop(std::move(drop))) {
          LOG_WARN("could not start local file transfer");
        }
      })) {
    LOG_WARN("could not install file-transfer edge drop host");
  }
#endif
}

void ClientApp::stopFileTransferService() noexcept
{
#if defined(Q_OS_WIN)
  ipcSendFileTransferActiveSides(0);
#else
  if (m_clientScreen != nullptr) {
    m_clientScreen->getPlatformScreen()->uninstallFileTransferEdgeDrop();
  }
#endif
  m_fileTransferService.reset();
}

bool ClientApp::beginFileTransferEdgeDrop(deskflow::filetransfer::FileTransferEdgeDrop drop)
{
  return m_fileTransferService != nullptr && Settings::value(Settings::FileTransfer::Enabled).toBool() &&
         m_fileTransferService->beginEdgeDrop(std::move(drop));
}

int ClientApp::mainLoop()
{
  // create socket multiplexer.  this must happen after daemonization
  // on unix because threads evaporate across a fork().
  setSocketMultiplexer(std::make_unique<SocketMultiplexer>());

  // start client, etc
  appUtil().startNode();

  // run event loop.  if startClient() failed we're supposed to retry
  // later.  the timer installed by startClient() will take care of
  // that.
  int exitCode = getEvents()->loop();

  // close down
  LOG_DEBUG("stopping client");
  stopClient();
  LOG_INFO("stopped client");

  return exitCode;
}

int ClientApp::start()
{
  initApp();
  return mainLoop();
}

int ClientApp::runInner(StartupFunc startup)
{
  int result;
  try {
    // run
    result = startup();
  } catch (...) {
    throw;
  }

  return result;
}

NetworkAddress &ClientApp::getCurrentServerAddress()
{
  if (m_serverAddresses.isEmpty()) {
    throw std::runtime_error("No server addresses configured");
  }
  return m_serverAddresses[m_currentServerIndex];
}

void ClientApp::tryNextServer()
{
  if (m_serverAddresses.size() > 1) {
    m_currentServerIndex = (m_currentServerIndex + 1) % m_serverAddresses.size();
    LOG_DEBUG("switching to server %zu of %zu", m_currentServerIndex + 1, m_serverAddresses.size());
  }
}

void ClientApp::startNode()
{
  // start the client.  if this return false then we've failed and
  // we shouldn't retry.
  LOG_VERBOSE("starting client");
  if (!startClient()) {
    bye(s_exitFailed);
  }
}

ISocketFactory *ClientApp::getSocketFactory() const
{
  return new TCPSocketFactory(getEvents(), getSocketMultiplexer());
}

double ClientApp::retryTime() const
{
  return deskflow::reconnect::retryDelay(
      m_retryCount, Settings::value(Settings::Client::DynamicConnectionRetry).toBool()
  );
}
