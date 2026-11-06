/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpcServer.h"

#include "base/Log.h"
#include "common/Constants.h"
#include "common/FileTransferEdgeDropIpc.h"
#include "common/ProfessionalMessage.h"

#ifdef Q_OS_WIN
#include "deskflow/win32/CoreIpcClientValidator.h"
#endif

#include <QLocalSocket>

#include <functional>
#include <utility>
#ifdef Q_OS_UNIX
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace deskflow::core::ipc {

namespace {

std::function<bool(QLocalSocket *)> productionClientValidator()
{
#ifdef Q_OS_WIN
  return [](QLocalSocket *clientSocket) {
    return clientSocket != nullptr && CoreIpcClientValidator{}.isAuthorized(*clientSocket);
  };
#else
  return [](QLocalSocket *) { return false; };
#endif
}

} // namespace

static CoreIpcServer *s_instance = nullptr;

CoreIpcServer::CoreIpcServer(QObject *parent) : CoreIpcServer(parent, kCoreIpcName, productionClientValidator())
{
}

CoreIpcServer::CoreIpcServer(QObject *parent, const QString &serverName, ClientValidator clientValidator)
    : IpcServer(parent, serverName, QStringLiteral("core")),
      m_clientValidator(std::move(clientValidator))
{
  assert(s_instance == nullptr);
  s_instance = this;
}

CoreIpcServer::~CoreIpcServer()
{
  if (s_instance == this) {
    s_instance = nullptr;
  }
}

CoreIpcServer &CoreIpcServer::instance()
{
  assert(s_instance != nullptr);
  return *s_instance;
}

void CoreIpcServer::publishFileTransferActiveSides(const std::uint32_t activeSides)
{
  if (m_fileTransferActiveSides.exchange(activeSides) == activeSides) {
    return;
  }
  QMetaObject::invokeMethod(
      this,
      [this] {
        broadcastCommand(
            QStringLiteral("fileTransferActiveSides"), QString::number(m_fileTransferActiveSides.load()), false
        );
      },
      Qt::QueuedConnection
  );
}

void CoreIpcServer::processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts)
{
  if (command == QStringLiteral("professionalRegister")) {
    if (parts.size() == 1 && hasCurrentVersionHello(clientSocket) && authorizeProfessional(clientSocket)) {
      if (!m_professionalClients.contains(clientSocket)) {
        m_professionalClients.insert(clientSocket);
        connect(clientSocket, &QLocalSocket::disconnected, this, [this, clientSocket] {
          m_professionalClients.remove(clientSocket);
          if (m_professionalClients.isEmpty())
            Q_EMIT professionalInputMode(false);
        });
        connect(clientSocket, &QObject::destroyed, this, [this, clientSocket] {
          if (m_professionalClients.remove(clientSocket) && m_professionalClients.isEmpty())
            Q_EMIT professionalInputMode(false);
        });
      }
    }
    return;
  }
  if (command == QStringLiteral("professional") || command == QStringLiteral("professionalMode")) {
    if (parts.size() != 2 || !hasCurrentVersionHello(clientSocket) || !m_professionalClients.contains(clientSocket))
      return;
    if (command == QStringLiteral("professionalMode")) {
      if (parts.at(1) == "0" || parts.at(1) == "1")
        Q_EMIT professionalInputMode(parts.at(1) == "1");
      return;
    }
    const auto encoded = parts.at(1).toLatin1();
    if (encoded.size() > professional::kMaxMessageBytes * 4 / 3 + 4)
      return;
    const auto bytes = QByteArray::fromBase64(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!professional::decode(bytes) || bytes.toBase64() != encoded)
      return;
    if (m_professionalRequests.fetch_add(1) >= 64) {
      m_professionalRequests.fetch_sub(1);
      return;
    }
    Q_EMIT professionalReceived(bytes);
    return;
  }
  if (command == QStringLiteral("fileTransferActiveSides")) {
    if (parts.size() != 1 || !hasCurrentVersionHello(clientSocket)) {
      writeToClientSocket(clientSocket, QStringLiteral("error"));
      return;
    }
    writeToClientSocket(
        clientSocket,
        QStringLiteral("fileTransferActiveSides=%1").arg(QString::number(m_fileTransferActiveSides.load()))
    );
    return;
  }
  if (command == QStringLiteral("stop")) {
    LOG_DEBUG("core ipc server got stop message");
    writeToClientSocket(clientSocket, QStringLiteral("ok"));
    broadcastCommand(QStringLiteral("bye"));
    Q_EMIT stopProcessRequested();
    return;
  }
  if (command == QStringLiteral("reloadConfig")) {
    LOG_DEBUG("core ipc server got reload config message");
    writeToClientSocket(clientSocket, QStringLiteral("ok"));
    Q_EMIT reloadConfigRequested();
    return;
  }
  if (command == QStringLiteral("fileTransferEdgeDrop")) {
    const auto reject = [this, &clientSocket] {
      LOG_WARN("core ipc server rejected file transfer edge drop");
      writeToClientSocket(clientSocket, QStringLiteral("error"));
    };

    if (parts.size() != 2 || parts.at(1).isEmpty() || !hasCurrentVersionHello(clientSocket)) {
      reject();
      return;
    }

    bool authorized = false;
    try {
      authorized = m_clientValidator && m_clientValidator(clientSocket);
    } catch (...) {
      authorized = false;
    }
    if (!authorized) {
      reject();
      return;
    }

    const auto decoded = ::deskflow::ipc::decodeFileTransferEdgeDropIpc(parts.at(1));
    if (!decoded.ok() || !decoded.value.has_value()) {
      reject();
      return;
    }

    const auto &value = *decoded.value;
    writeToClientSocket(clientSocket, QStringLiteral("ok"));
    Q_EMIT fileTransferEdgeDropRequested(value.direction, value.x, value.y, value.paths);
    return;
  }
  LOG_WARN("core ipc server got unknown command: %s", command.toUtf8().constData());
}

bool CoreIpcServer::authorizeProfessional(QLocalSocket *socket) const
{
#ifdef Q_OS_WIN
  return m_clientValidator && m_clientValidator(socket);
#elif defined(Q_OS_MACOS)
  uid_t uid = 0;
  gid_t gid = 0;
  return socket && getpeereid(static_cast<int>(socket->socketDescriptor()), &uid, &gid) == 0 && uid == geteuid();
#elif defined(Q_OS_LINUX)
  ucred credentials{};
  socklen_t size = sizeof(credentials);
  return socket &&
         getsockopt(static_cast<int>(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &credentials, &size) == 0 &&
         credentials.uid == geteuid();
#else
  return false;
#endif
}

void CoreIpcServer::publishProfessional(const QByteArray &bytes)
{
  if (bytes.isEmpty() || bytes.size() > professional::kMaxMessageBytes)
    return;
  const auto pending = m_professionalBytes.fetch_add(bytes.size());
  if (pending + bytes.size() > 512 * 1024) {
    m_professionalBytes.fetch_sub(bytes.size());
    return;
  }
  QMetaObject::invokeMethod(
      this,
      [this, bytes] {
        m_professionalBytes.fetch_sub(bytes.size());
        const auto message = QStringLiteral("professional=%1").arg(QString::fromLatin1(bytes.toBase64()));
        for (auto *client : std::as_const(m_professionalClients))
          if (client->state() == QLocalSocket::ConnectedState && hasCurrentVersionHello(client) &&
              client->bytesToWrite() < 256 * 1024)
            writeToClientSocket(client, message);
      },
      Qt::QueuedConnection
  );
}

void CoreIpcServer::publishProfessionalInputReady(bool active)
{
  QMetaObject::invokeMethod(
      this,
      [this, active] {
        for (auto *client : std::as_const(m_professionalClients))
          if (client->state() == QLocalSocket::ConnectedState && hasCurrentVersionHello(client))
            writeToClientSocket(client, QStringLiteral("professionalModeReady=%1").arg(active ? 1 : 0));
      },
      Qt::QueuedConnection
  );
}

} // namespace deskflow::core::ipc
