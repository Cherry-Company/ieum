/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2024 - 2025 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreProcess.h"

#include "common/ExitCodes.h"
#include "gui/core/ServiceStartCoordinator.h"
#include "gui/ipc/CoreIpcClient.h"
#include "gui/ipc/DaemonIpcClient.h"

#if defined(Q_OS_MACOS)
#include "OSXHelpers.h"
#endif

#ifdef Q_OS_LINUX
#include <signal.h>
#include <sys/prctl.h>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMetaEnum>
#include <QMutexLocker>
#include <QRegularExpression>

namespace deskflow::gui {

const int kRetryDelay = 1000;
const int kServiceStartTimeout = 15000;
const auto kLineSplitRegex = QRegularExpression("\r|\n|\r\n");

QString CoreProcess::processModeToString(const Settings::ProcessMode mode)
{
  return QVariant::fromValue(mode).toString().toLower();
}

QString CoreProcess::processStateToString(const CoreProcess::ProcessState state)
{
  return QVariant::fromValue(state).toString().toLower();
}

/**
 * @brief Wraps options that contain spaces in quotes
 *
 * Useful to handle things like paths with spaces (e.g. "C:\Program Files").
 *
 * Can also be used to create a representation of a command that can be pasted
 * into a terminal.
 */
QString CoreProcess::makeQuotedArgs(const QString &app, const QStringList &args)
{
  QStringList command = {app};
  command.append(args);

  static const auto quote = QStringLiteral("\"");
  static const auto space = QStringLiteral(" ");
  QStringList quoted;
  for (const auto &item : std::as_const(command)) {
    auto temp = item.simplified();
    if (const auto wrapped = (temp.startsWith(quote) && temp.endsWith(quote)); temp.contains(space) && !wrapped) {
      temp = QStringLiteral("%1%2%1").arg(quote, temp);
    }
    quoted.append(temp);
  }

  return quoted.join(space);
}

/**
 * @brief If IPv6, ensures the IP is surround in square brackets.
 */
QString CoreProcess::wrapIpv6(const QString &address)
{
  static const auto colon = QStringLiteral(":");
  static const auto openBracket = QStringLiteral("[");
  static const auto closeBracket = QStringLiteral("]");

  if (!address.contains(colon) || address.isEmpty()) {
    return address;
  }

  QString wrapped = address;

  if (!address.startsWith(openBracket)) {
    wrapped.prepend(openBracket);
  }

  if (!address.endsWith(closeBracket)) {
    wrapped.append(closeBracket);
  }

  return wrapped;
}

//
// CoreProcess
//

CoreProcess::CoreProcess(const ServerConfig &serverConfig)
    : m_serverConfig(serverConfig),
      m_serviceStartCoordinator{new ServiceStartCoordinator(this, kServiceStartTimeout)},
      m_daemonIpcClient{new ipc::DaemonIpcClient(this)}
{
  m_appPath = QStringLiteral("%1/%2").arg(QCoreApplication::applicationDirPath(), kCoreBinName);
  if (!QFile::exists(m_appPath)) {
    qFatal("core server binary does not exist");
    return;
  }

  connect(m_daemonIpcClient, &ipc::DaemonIpcClient::connected, this, &CoreProcess::daemonIpcClientConnected);
  connect(m_daemonIpcClient, &ipc::DaemonIpcClient::connectionFailed, this, [this] {
    Q_EMIT daemonIpcClientConnectionFailed();
    m_serviceStartCoordinator->transportFailed(m_coreStartGeneration);
  });
  connect(m_daemonIpcClient, &ipc::DaemonIpcClient::logPathReceived, this, &CoreProcess::setupDaemonLogTail);
  connect(m_daemonIpcClient, &ipc::DaemonIpcClient::commandResult, this, &CoreProcess::handleDaemonCommandResult);

  connect(m_serviceStartCoordinator, &ServiceStartCoordinator::configRequested, this, [this](quint64 generation) {
    if (generation != m_coreStartGeneration || m_processState != ProcessState::Starting) {
      return;
    }
    m_daemonConfigRequestId = m_daemonIpcClient->sendConfigFile(m_serviceConfigFile);
  });
  connect(m_serviceStartCoordinator, &ServiceStartCoordinator::startRequested, this, [this](quint64 generation) {
    if (generation != m_coreStartGeneration || m_processState != ProcessState::Starting) {
      return;
    }
    m_daemonStartRequestId = m_daemonIpcClient->sendStartProcess();
  });
  connect(
      m_serviceStartCoordinator, &ServiceStartCoordinator::coreConnectionRequested, this,
      [this](quint64 generation) { connectCoreIpc(generation); }
  );
  connect(m_serviceStartCoordinator, &ServiceStartCoordinator::succeeded, this, [this](quint64 generation) {
    if (generation != m_coreStartGeneration || m_processState != ProcessState::Starting) {
      return;
    }
    clearServiceStartRequests();
    setProcessState(ProcessState::Started);
  });
  connect(m_serviceStartCoordinator, &ServiceStartCoordinator::failed, this, &CoreProcess::handleServiceStartFailure);

  connect(&m_retryTimer, &QTimer::timeout, this, [this] {
    if (m_processState == ProcessState::RetryPending) {
      start();
    } else {
      qDebug("retry cancelled, process state is not retry pending");
    }
  });
}

void CoreProcess::onProcessReadyReadStandardOutput()
{
  if (m_process) {
    handleLogLines(m_process->readAllStandardOutput());
  }
}

void CoreProcess::onProcessReadyReadStandardError()
{
  if (m_process) {
    handleLogLines(m_process->readAllStandardError());
  }
}

void CoreProcess::daemonIpcClientConnected()
{
  applyLogLevel();
  m_daemonIpcClient->requestLogPath();
  m_serviceStartCoordinator->daemonConnected(m_coreStartGeneration);
}

void CoreProcess::checkExistingProcess()
{
  qInfo("checking existing core");

#if defined(Q_OS_WIN)
  if (Settings::value(Settings::Core::ProcessMode).value<ProcessMode>() == ProcessMode::Desktop) {
    const auto commandGeneration = ++m_daemonCommandGeneration;
    auto stopServiceProcess = [this, commandGeneration] {
      if (commandGeneration != m_daemonCommandGeneration) {
        return;
      }
      qInfo("clearing any service-managed core before desktop recovery");
      m_daemonIpcClient->sendStopProcess();
    };

    if (m_daemonIpcClient->isConnected()) {
      stopServiceProcess();
    } else {
      connect(
          m_daemonIpcClient, &ipc::DaemonIpcClient::connected, this, stopServiceProcess,
          static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::QueuedConnection)
      );
      m_daemonIpcClient->connectToServer();
    }
  }
#endif

  auto *client = new ipc::CoreIpcClient(this);
  connect(client, &ipc::CoreIpcClient::connected, this, [client] {
    qInfo("existing core has matching version, asking it to stop");
    client->sendStop();
  });
  connect(client, &ipc::CoreIpcClient::versionMismatch, this, [client] {
    qInfo("existing core has mismatched version, asking it to stop");
    client->sendStop();
  });
  connect(client, &ipc::CoreIpcClient::serverShutdown, this, [this, client] {
    qInfo("existing core stopped successfully");
    m_duplicateRecoveryAttempts = 0;
    client->deleteLater();
    scheduleRestart(kRetryDelay);
  });
  connect(client, &ipc::CoreIpcClient::connectionFailed, this, [this, client] {
    client->deleteLater();
    if (++m_duplicateRecoveryAttempts <= 5) {
      qWarning(
          "could not contact existing core, retrying recovery (%d/5)", //
          m_duplicateRecoveryAttempts
      );
      scheduleRestart(kRetryDelay * 2);
    } else {
      qCritical("could not contact existing core after 5 recovery attempts");
      setProcessState(ProcessState::Stopped);
    }
  });
  client->connectToServer();
}

void CoreProcess::onProcessFinished(int exitCode, QProcess::ExitStatus)
{
  using enum ProcessState;
  const auto wasStarted = m_processState == Started;
  setConnectionState(ConnectionState::Disconnected);

  if (m_retryTimer.isActive()) {
    m_retryTimer.stop();
  }

  if (auto *finishedProcess = qobject_cast<QProcess *>(sender())) {
    if (finishedProcess == m_process) {
      m_process = nullptr;
    }
    finishedProcess->deleteLater();
  }

  if (m_restartRequested) {
    qDebug("desktop process stopped for restart");
    m_restartRequested = false;
    scheduleRestart(kRetryDelay);
    return;
  }

  if (exitCode != s_exitSuccess) {
    setProcessState(Stopped);
    if (exitCode == s_exitDuplicate) {
      checkExistingProcess();
      return;
    }
    qWarning("desktop process exited with code: %d", exitCode);
    return;
  }

  qDebug("desktop process exited normally");

  if (wasStarted) {
    qDebug("desktop process was running, retrying in %d ms", kRetryDelay);
    scheduleRestart(kRetryDelay);
  } else {
    setProcessState(Stopped);
  }
}

void CoreProcess::scheduleRestart(int delayMs)
{
  if (m_retryTimer.isActive()) {
    m_retryTimer.stop();
  }
  setProcessState(ProcessState::RetryPending);
  m_retryTimer.setSingleShot(true);
  m_retryTimer.start(delayMs);
}

void CoreProcess::connectCoreIpc(quint64 startGeneration)
{
  if (startGeneration != m_coreStartGeneration ||
      (m_processState != ProcessState::Starting && m_processState != ProcessState::Started)) {
    return;
  }

  if (m_coreIpcClient) {
    m_coreIpcClient->disconnectFromServer();
    m_coreIpcClient->deleteLater();
  }

  auto *client = new ipc::CoreIpcClient(this);
  m_coreIpcClient = client;
  connect(client, &ipc::CoreIpcClient::commandReceived, this, &CoreProcess::onCoreIpcMessageReceived);
  connect(client, &ipc::CoreIpcClient::connected, this, [this, client, startGeneration] {
    if (m_coreIpcClient != client || startGeneration != m_coreStartGeneration) {
      return;
    }
    m_duplicateRecoveryAttempts = 0;
    qDebug("connected to core ipc server");
    if (m_processState == ProcessState::Starting) {
      m_serviceStartCoordinator->coreConnected(startGeneration);
    }
  });

  auto reconnect = [this, client, startGeneration] {
    if (m_coreIpcClient != client) {
      return;
    }
    const auto wasStarting = m_processState == ProcessState::Starting;
    m_coreIpcClient = nullptr;
    client->deleteLater();
    if (startGeneration != m_coreStartGeneration) {
      return;
    }
    if (wasStarting) {
      m_serviceStartCoordinator->coreConnectionFailed(startGeneration);
    } else if (m_processState == ProcessState::Started) {
      QTimer::singleShot(kRetryDelay, this, [this, startGeneration] { connectCoreIpc(startGeneration); });
    }
  };
  connect(client, &ipc::CoreIpcClient::connectionFailed, this, [reconnect] {
    qWarning("lost core ipc connection, retrying");
    reconnect();
  });
  connect(client, &ipc::CoreIpcClient::serverShutdown, this, [reconnect] {
    qDebug("core ipc server shut down, waiting for recovery");
    reconnect();
  });

  client->connectToServer();
}

void CoreProcess::applyLogLevel()
{
  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  if (processMode == ProcessMode::Service) {
    qDebug() << "setting daemon log level:" << Settings::logLevelText();
    m_daemonIpcClient->sendLogLevel(Settings::logLevelText());
  }
}

void CoreProcess::startForegroundProcess(const QStringList &args)
{
  using enum ProcessState;

  if (m_processState != Starting) {
    qFatal("core process must be in starting state");
  }

  // only make quoted args for printing the command for convenience; so that the
  // core command can be easily copy/pasted to the terminal for testing.
  const auto quoted = makeQuotedArgs(m_appPath, args);
  qInfo("running command: %s", qPrintable(quoted));

#ifdef Q_OS_LINUX
  m_process->setChildProcessModifier([] {
    // the core process becomes orphaned when the gui process exits abruptly (e.g. with kill -9),
    // so ensure the os also kills the core when that happens to the gui.
    prctl(PR_SET_PDEATHSIG, SIGTERM);
  });
#endif

  m_process->start(m_appPath, args);

  if (m_process->waitForStarted()) {
    setProcessState(Started);
  } else {
    m_process->deleteLater();
    m_process = nullptr;
    setProcessState(Stopped);
    Q_EMIT error(Error::StartFailed);
  }
}

void CoreProcess::startProcessFromDaemon()
{
  if (m_processState != ProcessState::Starting) {
    qFatal("core process must be in starting state");
  }

  ++m_daemonCommandGeneration;
  clearServiceStartRequests();
  m_serviceConfigFile = Settings::settingsFile();
  qInfo("sending start to daemon (config file: %s)", qPrintable(m_serviceConfigFile));
  m_serviceStartCoordinator->begin(m_coreStartGeneration);

  if (m_daemonIpcClient->isConnected()) {
    m_serviceStartCoordinator->daemonConnected(m_coreStartGeneration);
  } else {
    m_daemonIpcClient->connectToServer();
  }
}

void CoreProcess::handleDaemonCommandResult(
    const QString &requestId, const QString &command, const bool success, const QString &detail
)
{
  if (!success && !detail.isEmpty()) {
    qWarning().noquote() << QStringLiteral("daemon rejected %1 command: %2").arg(command, detail);
  }

  if (requestId == m_daemonConfigRequestId && command == QStringLiteral("configFile")) {
    m_daemonConfigRequestId.clear();
    m_serviceStartCoordinator->configResult(m_coreStartGeneration, success);
  } else if (requestId == m_daemonStartRequestId && command == QStringLiteral("start")) {
    m_daemonStartRequestId.clear();
    m_serviceStartCoordinator->startResult(m_coreStartGeneration, success);
  }
}

void CoreProcess::handleServiceStartFailure(const quint64 startGeneration)
{
  if (startGeneration != m_coreStartGeneration || m_processState != ProcessState::Starting) {
    return;
  }

  qWarning("service-managed core failed to become ready");
  clearServiceStartRequests();
  if (m_coreIpcClient) {
    m_coreIpcClient->disconnectFromServer();
    m_coreIpcClient->deleteLater();
    m_coreIpcClient = nullptr;
  }
  requestDaemonStop();
  setProcessState(ProcessState::Stopped);
  setConnectionState(ConnectionState::Disconnected);
  Q_EMIT error(Error::StartFailed);
}

void CoreProcess::requestDaemonStop()
{
  const auto commandGeneration = ++m_daemonCommandGeneration;
  auto sendStop = [this, commandGeneration] {
    if (commandGeneration != m_daemonCommandGeneration) {
      qDebug("discarding stale daemon stop command");
      return;
    }
    m_daemonIpcClient->sendStopProcess();
  };

  if (m_daemonIpcClient->isConnected()) {
    sendStop();
  } else {
    connect(
        m_daemonIpcClient, &ipc::DaemonIpcClient::connected, this, sendStop,
        static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::QueuedConnection)
    );
    m_daemonIpcClient->connectToServer();
  }
}

void CoreProcess::clearServiceStartRequests()
{
  m_daemonConfigRequestId.clear();
  m_daemonStartRequestId.clear();
  m_serviceConfigFile.clear();
}

void CoreProcess::stopForegroundProcess() const
{
  if (m_processState != ProcessState::Stopping) {
    qFatal("core process must be in stopping state");
  }

  if (!m_process) {
    qFatal("process not set, cannot stop");
  }

  qInfo("stopping core desktop process");

  if (m_process->state() == QProcess::ProcessState::Running) {
    qDebug("process is running, closing");
    m_process->close();
  } else {
    qDebug("process is not running, skipping terminate");
  }
}

void CoreProcess::stopProcessFromDaemon()
{
  if (m_processState != ProcessState::Stopping) {
    qFatal("core process must be in stopping state");
  }

  const auto commandGeneration = ++m_daemonCommandGeneration;
  auto sendStop = [this, commandGeneration] {
    if (commandGeneration != m_daemonCommandGeneration) {
      qDebug("discarding stale daemon stop command");
      return;
    }
    m_daemonIpcClient->sendStopProcess();
    if (m_restartRequested) {
      m_restartRequested = false;
      scheduleRestart(kRetryDelay);
    } else {
      setProcessState(ProcessState::Stopped);
    }
  };

  if (m_daemonIpcClient->isConnected()) {
    sendStop();
  } else {
    connect(
        m_daemonIpcClient, &ipc::DaemonIpcClient::connected, this, sendStop,
        static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::QueuedConnection)
    );
    m_daemonIpcClient->connectToServer();
  }
}

void CoreProcess::handleLogLines(const QString &text)
{
  const auto lines = text.split(kLineSplitRegex);
  for (const auto &line : lines) {
    if (line.isEmpty()) {
      continue;
    }

#if defined(Q_OS_MACOS)
    // HACK: macOS 10.13.4+ spamming error lines in logs making them
    // impossible to read and debug; giving users a red herring.
    if (line.contains("calling TIS/TSM in non-main thread environment")) {
      continue;
    }

    // the core process is not allowed to show the permission prompt
    // (called "notification permission") and the notification log line is emitted from
    // deep inside cocoa code in the core binary to stdout, so it can't be sent over
    // ipc from the core to the gui and instead the gui has to parse the core output.
    static const QString needle = "OSX Notification: ";
    if (line.contains(needle) && line.contains('|')) {
      const int delimiterPosition = line.indexOf('|');
      const int start = line.indexOf(needle);
      const QString title = line.mid(start + needle.length(), delimiterPosition - start - needle.length());
      const QString body = line.mid(delimiterPosition + 1, line.length() - delimiterPosition);
      if (!showOSXNotification(title, body)) {
        qDebug("osx notification was not shown");
      }
    }
#endif

    Q_EMIT logLine(line);
  }
}

void CoreProcess::start(std::optional<ProcessMode> processModeOption)
{
  if (m_processState == ProcessState::Stopping) {
    qDebug("core start requested while stopping, queueing restart");
    m_restartRequested = true;
    return;
  }

  if (m_processState == ProcessState::Started || m_processState == ProcessState::Starting) {
    qCritical("core process cannot start while state is %s", qPrintable(processStateToString(m_processState)));
    return;
  }

  if (m_mode == Settings::CoreMode::None) {
    qFatal("set core mode before starting");
    return;
  }

  QMutexLocker locker(&m_processMutex);

  if (m_processState == ProcessState::Stopped) {
    // A deliberate user start begins a fresh duplicate-core recovery cycle.
    // Timer-driven retries enter through RetryPending and retain their count.
    m_duplicateRecoveryAttempts = 0;
  }
  m_restartRequested = false;

  const auto currentMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();
  const auto processMode = processModeOption.value_or(currentMode);
  const auto coreMode = QVariant::fromValue(m_mode).toString().toLower();

  qInfo().noquote() << QString("starting %1 process (%2 mode)").arg(coreMode, processModeToString(processMode));

  setProcessState(ProcessState::Starting);

#ifdef Q_OS_MACOS
  requestOSXNotificationPermission();
#endif

  setConnectionState(ConnectionState::Connecting);

  if (processMode == ProcessMode::Desktop) {
    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &CoreProcess::onProcessFinished, Qt::UniqueConnection);
    connect(
        m_process, &QProcess::readyReadStandardOutput, this, &CoreProcess::onProcessReadyReadStandardOutput,
        Qt::UniqueConnection
    );
    connect(
        m_process, &QProcess::readyReadStandardError, this, &CoreProcess::onProcessReadyReadStandardError,
        Qt::UniqueConnection
    );
  }

  QStringList args = {coreMode};

  if (m_mode == Settings::CoreMode::Server) {
    const auto [hasNeededPermissions, configFilename] = persistServerConfig();
    if (configFilename.isEmpty()) {
      qFatal("config file name empty for server args");
      return;
    }
    if (!hasNeededPermissions) {
      setProcessState(ProcessState::Stopped);
      setConnectionState(ConnectionState::Disconnected);
      Q_EMIT error(Error::StartFailed);
      return;
    }
    qInfo("core config file: %s", qPrintable(configFilename));
  }

  qDebug().noquote() << "log level:" << Settings::logLevelText();

  if (Settings::value(Settings::Log::ToFile).toBool()) {
    const auto logFile = Settings::value(Settings::Log::File).toString();
    QDir(QFileInfo(logFile).absolutePath()).mkpath(".");
    qInfo().noquote() << "log file:" << logFile;
  }

  // Desktop mode reports process start synchronously. Service mode connects core IPC before reporting Started.
  if (m_coreStartConnection) {
    disconnect(m_coreStartConnection);
  }
  const auto startGeneration = ++m_coreStartGeneration;
  m_coreStartConnection = connect(
      this, &CoreProcess::processStateChanged, this,
      [this, startGeneration, processMode](ProcessState state) {
        if (state != ProcessState::Started || processMode != ProcessMode::Desktop) {
          return;
        }

        // Delay briefly to give the core process time to start its IPC server.
        QTimer::singleShot(kRetryDelay, this, [this, startGeneration] { connectCoreIpc(startGeneration); });
      },
      static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::QueuedConnection)
  );

  if (processMode == ProcessMode::Desktop) {
    startForegroundProcess(args);
  } else if (processMode == ProcessMode::Service) {
    startProcessFromDaemon();
  }

  m_lastProcessMode = processMode;
}

void CoreProcess::stop(std::optional<ProcessMode> processModeOption)
{
  stop(processModeOption, false);
}

void CoreProcess::stop(std::optional<ProcessMode> processModeOption, bool restartRequested)
{
  QMutexLocker locker(&m_processMutex);

  m_restartRequested = restartRequested;
  const auto cancelledStartGeneration = m_coreStartGeneration;
  ++m_coreStartGeneration;
  if (m_retryTimer.isActive()) {
    m_retryTimer.stop();
  }

  const auto currentMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();
  const auto processMode = processModeOption.value_or(currentMode);

  qInfo("stopping core process (%s mode)", qPrintable(processModeToString(processMode)));

  if (m_coreIpcClient) {
    m_coreIpcClient->disconnectFromServer();
    m_coreIpcClient->deleteLater();
    m_coreIpcClient = nullptr;
  }
  if (m_coreStartConnection) {
    disconnect(m_coreStartConnection);
    m_coreStartConnection = {};
  }

  if (m_processState == ProcessState::RetryPending) {
    qDebug("core process retry was pending, cancelling");
    setProcessState(ProcessState::Stopped);
    if (m_restartRequested) {
      m_restartRequested = false;
      scheduleRestart(kRetryDelay);
    }
  } else if (m_processState == ProcessState::Stopping) {
    qDebug("core process is already stopping");
  } else if (m_processState == ProcessState::Starting) {
    qDebug("core process is starting, cancelling");
    m_serviceStartCoordinator->cancel(cancelledStartGeneration);
    clearServiceStartRequests();
    if (processMode == ProcessMode::Service) {
      requestDaemonStop();
    } else {
      ++m_daemonCommandGeneration;
    }
    setProcessState(ProcessState::Stopped);
    if (m_restartRequested) {
      m_restartRequested = false;
      scheduleRestart(kRetryDelay);
    }
  } else if (m_processState != ProcessState::Stopped) {
    setProcessState(ProcessState::Stopping);

    if (processMode == ProcessMode::Service) {
      stopProcessFromDaemon();
    } else if (processMode == ProcessMode::Desktop) {
      stopForegroundProcess();
    }

  } else {
    qWarning("core process already stopped");
  }

  setConnectionState(ConnectionState::Disconnected);
}

void CoreProcess::restart()
{
  qDebug("restarting core process");

  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();
  const auto previousMode = m_lastProcessMode.value_or(processMode);

  if (m_processState == ProcessState::Stopped) {
    start();
    return;
  }

  if (previousMode != processMode) {
    const auto debugMessage = QStringLiteral("process mode changed to %1, stopping %2 process")
                                  .arg(processModeToString(processMode), processModeToString(previousMode));
    qDebug().noquote() << debugMessage;
  }

  if (previousMode == ProcessMode::Desktop) {
    // QProcess shutdown is asynchronous. Starting before its finished signal
    // overwrites m_process and leaves the old IPC endpoint alive.
    stop(previousMode, true);
    return;
  }

  // The daemon connection can also be asynchronous. Queue the new start only
  // after the stop command has been sent; the watchdog serializes replacement.
  stop(previousMode, true);
}

bool CoreProcess::reloadServerConfig()
{
  if (m_mode != Settings::CoreMode::Server || m_processState != ProcessState::Started || m_coreIpcClient == nullptr ||
      !m_coreIpcClient->isConnected()) {
    return false;
  }

  const auto [configReady, configFile] = persistServerConfig();
  if (!configReady) {
    qWarning() << "cannot reload unreadable server config:" << configFile;
    return false;
  }

  qDebug() << "reloading server config without restarting the core:" << configFile;
  m_coreIpcClient->sendReloadConfig();
  return true;
}

void CoreProcess::cleanup()
{
  qInfo("cleaning up core process");

  const auto isDesktop = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>() == ProcessMode::Desktop;
  const auto isRunning = m_processState == ProcessState::Started;
  if (isDesktop && isRunning) {
    stop();
  }
}

QPair<bool, QString> CoreProcess::persistServerConfig() const
{
  if (Settings::value(Settings::Server::ExternalConfig).toBool()) {
    return {Settings::isServerConfigFileReadable(), Settings::value(Settings::Server::ExternalConfigFile).toString()};
  }

  const auto configFilePath = Settings::defaultValue(Settings::Server::ExternalConfigFile).toString();
  QFile configFile(configFilePath);
  if (!configFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qWarning() << "failed to open core config file for write:" << configFilePath;
    return {false, configFile.fileName()};
  }

  m_serverConfig.save(configFile);
  configFile.close();
  return {Settings::isServerConfigFileReadable(), configFile.fileName()};
}

void CoreProcess::setConnectionState(ConnectionState state)
{
  if (m_connectionState == state) {
    return;
  }

  m_connectionState = state;
  Q_EMIT connectionStateChanged(state);
}

void CoreProcess::setProcessState(ProcessState state)
{
  if (m_processState == state) {
    return;
  }

  qDebug(
      "core process state changed: %s -> %s", //
      qPrintable(processStateToString(m_processState)), qPrintable(processStateToString(state))
  );
  m_processState = state;
  Q_EMIT processStateChanged(state);
}

void CoreProcess::onCoreIpcMessageReceived(const QString &command, const QString &args)
{
  if (command == "connectionState") {
    const auto metaEnum = QMetaEnum::fromType<ConnectionState>();
    bool ok = false;
    const auto state = static_cast<ConnectionState>(metaEnum.keyToValue(args.toUtf8().constData(), &ok));
    if (!ok) {
      qWarning("core ipc got unknown connection state: %s", args.toUtf8().constData());
      return;
    }
    setConnectionState(state);
  } else if (command == "connectedClients") {
    const auto clients = args.isEmpty() ? QStringList() : args.split(",");
    Q_EMIT connectedClientsChanged(clients);
  } else if (command == "secureSocket") {
    Q_EMIT secureSocket(true);
    if (args != m_secureSocketVersion) {
      m_secureSocketVersion = args;
      Q_EMIT securityLevelChanged(args);
    }
  } else if (command == "unrecognisedClient") {
    Q_EMIT unrecognisedClient(args);
  } else if (command == "connectionRefused") {
    const auto metaEnum = QMetaEnum::fromType<deskflow::core::ConnectionRefusal>();
    bool ok = false;
    const auto reason =
        static_cast<deskflow::core::ConnectionRefusal>(metaEnum.keyToValue(args.toUtf8().constData(), &ok));
    if (ok) {
      Q_EMIT connectionRefused(reason);
    } else {
      qWarning("core ipc got unknown connection refusal: %s", args.toUtf8().constData());
    }
  } else if (command == "retryIn") {
    Q_EMIT retryIn(args.toInt());
  } else if (command == "peerFingerprint") {
    Q_EMIT peerFingerprint(args);
  } else if (command == "missingKeyboardLayouts") {
    Q_EMIT missingKeyboardLayouts(args);
  } else if (command == "inputLanguageStatus") {
    const auto fields = args.split("|");
    if (fields.size() != 4) {
      qWarning("core ipc got malformed input language status: %s", qUtf8Printable(args));
      return;
    }

    bool categoryOk = false;
    bool composingOk = false;
    const auto category = fields.at(2).toInt(&categoryOk);
    const auto composing = fields.at(3).toInt(&composingOk);
    if (!categoryOk || !composingOk || category < 0 || category > 2 || composing < 0 || composing > 1) {
      qWarning("core ipc got invalid input language status: %s", qUtf8Printable(args));
      return;
    }
    Q_EMIT inputLanguageStatusChanged(fields.at(0), fields.at(1), category, composing != 0);
  }
}

bool CoreProcess::checkSecureSocket(const QString &line)
{
  static const QString tlsCheckString = "network encryption protocol: ";
  const auto index = line.indexOf(tlsCheckString, 0, Qt::CaseInsensitive);
  if (index == -1) {
    return false;
  }

  Q_EMIT secureSocket(true);
  if (const auto ssv = line.mid(index + tlsCheckString.size()); ssv != m_secureSocketVersion) {
    m_secureSocketVersion = ssv;
    Q_EMIT securityLevelChanged(ssv);
  }

  return true;
}

QString CoreProcess::correctedAddress(const QString &address) const
{
  return wrapIpv6(address.simplified());
}

void CoreProcess::setupDaemonLogTail(const QString &logPath)
{
  qDebug() << "daemon log path:" << logPath;

  if (QFileInfo logFile(logPath); !logFile.isFile()) {
    auto file = QFile(logPath);
    if (!file.open(QFile::ReadWrite)) {
      qCritical() << "daemon log path file can not be written:" << logPath;
      return;
    }
    file.write(""); // Create an empty file
  }

  if (m_daemonFileTail) {
    m_daemonFileTail->setWatchedFile(logPath);
  } else {
    m_daemonFileTail = new FileTail(logPath, this);
    connect(m_daemonFileTail, &FileTail::newLine, this, &CoreProcess::handleLogLines);
  }
}

void CoreProcess::clearSettings()
{
  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();
  if (processMode == ProcessMode::Desktop) {
    qDebug("no core settings to clear in desktop mode");
    return;
  }

  if (processMode != ProcessMode::Service) {
    qFatal("invalid process mode");
  }

  qInfo("clearing core settings through daemon");
  m_daemonIpcClient->sendClearSettings();
}

void CoreProcess::retryDaemon()
{
  m_daemonIpcClient->connectToServer();
}

} // namespace deskflow::gui
