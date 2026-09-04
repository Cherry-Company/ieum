/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpcServer.h"

#include "base/Log.h"
#include "common/Constants.h"
#include "common/FileTransferEdgeDropIpc.h"

#ifdef Q_OS_WIN
#include "deskflow/win32/CoreIpcClientValidator.h"
#endif

#include <QLocalSocket>

#include <functional>
#include <utility>

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

} // namespace deskflow::core::ipc
