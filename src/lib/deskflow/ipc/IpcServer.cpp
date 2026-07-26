/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "IpcServer.h"

#include "base/Log.h"
#include "common/VersionInfo.h"

#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(Q_OS_UNIX)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace deskflow::core::ipc {

namespace {
constexpr auto kMaxIpcMessageBytes = 64 * 1024;
}

IpcServer::IpcServer(QObject *parent, const QString &serverName, const QString &typeName)
    : QObject(parent),
      m_server{new QLocalServer(this)}, // NOSONAR - Qt memory
      m_serverName(serverName),
      m_typeName(typeName.toUtf8())
{
  // do nothing
}

IpcServer::~IpcServer()
{
  m_server->close();
  releaseOwnership();
}

bool IpcServer::acquireOwnership()
{
#ifdef Q_OS_WIN
  if (m_ownershipMutex != nullptr) {
    return true;
  }

  const auto mutexName = QStringLiteral("Global\\%1-owner").arg(m_serverName);
  const auto mutex = CreateMutexW(nullptr, FALSE, reinterpret_cast<LPCWSTR>(mutexName.utf16()));
  if (mutex == nullptr) {
    const auto error = GetLastError();
    if (error == ERROR_ACCESS_DENIED) {
      LOG_ERR("%s ipc endpoint is owned by another Windows session", m_typeName.constData());
    } else {
      LOG_ERR("%s ipc ownership mutex failed with Windows error: %lu", m_typeName.constData(), error);
    }
    return false;
  }

  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(mutex);
    LOG_ERR("%s ipc endpoint is already owned by another process", m_typeName.constData());
    return false;
  }

  m_ownershipMutex = mutex;
#elif defined(Q_OS_UNIX)
  if (m_ownershipFd >= 0) {
    return true;
  }

  auto lockName = m_serverName;
  lockName.replace('/', '_');
  lockName.replace('\\', '_');
  const auto lockPath = QDir::temp().filePath(QStringLiteral(".%1.owner.lock").arg(lockName));
  const auto nativeLockPath = QFile::encodeName(lockPath);
  const auto fd = ::open(nativeLockPath.constData(), O_CREAT | O_RDWR | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    LOG_ERR("%s ipc ownership file failed to open: %s", m_typeName.constData(), std::strerror(errno));
    return false;
  }

  if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
    const auto error = errno;
    ::close(fd);
    if (error == EWOULDBLOCK || error == EAGAIN) {
      LOG_ERR("%s ipc endpoint is already owned by another process", m_typeName.constData());
    } else {
      LOG_ERR("%s ipc ownership lock failed: %s", m_typeName.constData(), std::strerror(error));
    }
    return false;
  }

  m_ownershipFd = fd;
#endif
  return true;
}

void IpcServer::releaseOwnership()
{
#ifdef Q_OS_WIN
  if (m_ownershipMutex != nullptr) {
    CloseHandle(static_cast<HANDLE>(m_ownershipMutex));
    m_ownershipMutex = nullptr;
  }
#elif defined(Q_OS_UNIX)
  if (m_ownershipFd >= 0) {
    ::flock(m_ownershipFd, LOCK_UN);
    ::close(m_ownershipFd);
    m_ownershipFd = -1;
  }
#endif
}

bool IpcServer::listen()
{
  if (!acquireOwnership()) {
    return false;
  }

  // IPC server normally runs as system, but GUI runs as regular user, so we need to allow world access.
  m_server->setSocketOptions(QLocalServer::WorldAccessOption);

  connect(m_server, &QLocalServer::newConnection, this, &IpcServer::handleNewConnection);
  if (m_server->listen(m_serverName)) {
    LOG_DEBUG("%s ipc server listening on: %s", m_typeName.constData(), m_serverName.toUtf8().constData());
    return true;
  }

  LOG_WARN(
      "%s ipc server initial listen failed on %s: %s", m_typeName.constData(), m_serverName.toUtf8().constData(),
      m_server->errorString().toUtf8().constData()
  );

  // Unix socket files can survive a crash. Remove one only after proving that
  // no live server accepts connections; never unlink an active peer.
  QLocalSocket probe;
  probe.connectToServer(m_serverName);
  if (probe.state() == QLocalSocket::ConnectedState || probe.waitForConnected(250)) {
    LOG_ERR(
        "%s ipc name is already owned by a running process: %s", m_typeName.constData(),
        m_serverName.toUtf8().constData()
    );
    releaseOwnership();
    return false;
  }

  const auto probeError = probe.error();
  if (probeError != QLocalSocket::ServerNotFoundError && probeError != QLocalSocket::ConnectionRefusedError) {
    LOG_ERR(
        "%s ipc server failed to listen on %s: %s", m_typeName.constData(), m_serverName.toUtf8().constData(),
        m_server->errorString().toUtf8().constData()
    );
    releaseOwnership();
    return false;
  }

  if (!QLocalServer::removeServer(m_serverName) || !m_server->listen(m_serverName)) {
    LOG_ERR(
        "%s ipc server failed to recover stale endpoint %s: %s", m_typeName.constData(),
        m_serverName.toUtf8().constData(), m_server->errorString().toUtf8().constData()
    );
    releaseOwnership();
    return false;
  }

  LOG_INFO("%s ipc server recovered stale endpoint: %s", m_typeName.constData(), m_serverName.toUtf8().constData());
  return true;
}

void IpcServer::handleNewConnection()
{
  QLocalSocket *clientSocket = m_server->nextPendingConnection();
  if (!clientSocket) {
    LOG_ERR("%s ipc server failed to get new connection", m_typeName.constData());
    return;
  }

  LOG_DEBUG("%s ipc server got new connection", m_typeName.constData());
  m_clients.insert(clientSocket);
  m_receiveBuffers.insert(clientSocket, {});

  connect(clientSocket, &QLocalSocket::readyRead, this, &IpcServer::handleReadyRead);
  connect(clientSocket, &QLocalSocket::disconnected, this, &IpcServer::handleDisconnected);
  connect(clientSocket, &QLocalSocket::errorOccurred, this, &IpcServer::handleErrorOccurred);
}

void IpcServer::handleReadyRead()
{
  const auto clientSocket = qobject_cast<QLocalSocket *>(sender());
  LOG_VERBOSE("%s ipc server ready to read data", m_typeName.constData());

  auto &buffer = m_receiveBuffers[clientSocket];
  buffer.append(clientSocket->readAll());
  if (buffer.isEmpty()) {
    LOG_WARN("%s ipc server got empty message", m_typeName.constData());
    return;
  }

  if (buffer.size() > kMaxIpcMessageBytes) {
    LOG_ERR("%s ipc server message exceeded %d bytes", m_typeName.constData(), kMaxIpcMessageBytes);
    m_receiveBuffers.remove(clientSocket);
    clientSocket->disconnectFromServer();
    return;
  }

  qsizetype delimiter = -1;
  while ((delimiter = buffer.indexOf('\n')) >= 0) {
    auto messageData = buffer.left(delimiter);
    buffer.remove(0, delimiter + 1);
    if (messageData.endsWith('\r')) {
      messageData.chop(1);
    }
    processMessage(clientSocket, QString::fromUtf8(messageData));
  }

  if (!buffer.isEmpty()) {
    LOG_VERBOSE("%s ipc server buffered %d incomplete bytes", m_typeName.constData(), buffer.size());
  }
}

void IpcServer::handleDisconnected()
{
  const auto clientSocket = qobject_cast<QLocalSocket *>(sender());
  LOG_DEBUG("%s ipc server client disconnected", m_typeName.constData());
  m_clients.remove(clientSocket);
  m_receiveBuffers.remove(clientSocket);
  clientSocket->deleteLater();
}

void IpcServer::handleErrorOccurred()
{
  const auto clientSocket = qobject_cast<QLocalSocket *>(sender());
  LOG_ERR("%s ipc server client error: %s", m_typeName.constData(), clientSocket->errorString().toUtf8().constData());
  m_clients.remove(clientSocket);
  m_receiveBuffers.remove(clientSocket);
  clientSocket->deleteLater();
}

void IpcServer::processMessage(QLocalSocket *clientSocket, const QString &message)
{
  LOG_VERBOSE("%s ipc server got message: %s", m_typeName.constData(), message.toUtf8().constData());
  const auto parts = message.split('=');
  if (parts.isEmpty()) {
    LOG_ERR("%s ipc server got invalid message: %s", m_typeName.constData(), message.toUtf8().constData());
    writeToClientSocket(clientSocket, QStringLiteral("error"));
    return;
  }

  if (const auto &command = parts.at(0); command == QStringLiteral("hello")) {
    if (parts.size() < 2) {
      LOG_ERR("%s ipc client hello missing version", m_typeName.constData());
      writeToClientSocket(clientSocket, "error=missing version");
      clientSocket->flush();
      clientSocket->disconnectFromServer();
      return;
    }

    const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
    const auto clientVersion = parts.at(1);
    LOG_DEBUG("%s ipc server got hello message (version: %s)", m_typeName.constData(), versionId.toUtf8().constData());

    if (clientVersion != versionId) {
      LOG_WARN(
          "%s ipc client version mismatch (client: %s, server: %s)", m_typeName.constData(),
          clientVersion.toUtf8().constData(), versionId.toUtf8().constData()
      );
      writeToClientSocket(clientSocket, QStringLiteral("versionMismatch=%1").arg(versionId));
      clientSocket->flush();
      return;
    }

    LOG_DEBUG("%s ipc server sending hello back", m_typeName.constData());
    writeToClientSocket(clientSocket, QStringLiteral("hello=%1").arg(versionId));

    // Replay messages that were queued before any clients connected.
    LOG_VERBOSE("ipc server replaying %d pending messages", m_pendingMessages.size());
    for (const auto &pending : std::as_const(m_pendingMessages)) {
      LOG_VERBOSE("%s ipc server replaying: %s", m_typeName.constData(), pending.toUtf8().constData());
      writeToClientSocket(clientSocket, pending);
    }
    m_pendingMessages.clear();
  } else if (command == QStringLiteral("noop")) {
    LOG_DEBUG("%s ipc server got noop message", m_typeName.constData());
    writeToClientSocket(clientSocket, QStringLiteral("ok"));
  } else {
    processCommand(clientSocket, command, parts);
  }

  clientSocket->flush();
}

void IpcServer::broadcastCommand(const QString &command, const QString &args)
{
  const auto message = args.isEmpty() ? command : QStringLiteral("%1=%2").arg(command, args);

  if (m_clients.isEmpty()) {
    LOG_VERBOSE(
        "%s ipc server has no clients, message queued: %s", m_typeName.constData(), message.toUtf8().constData()
    );
    m_pendingMessages.append(message);
    return;
  }

  LOG_VERBOSE(
      "%s ipc server broadcasting message to %d clients: %s", m_typeName.constData(), m_clients.size(),
      message.toUtf8().constData()
  );
  for (auto *client : std::as_const(m_clients)) {
    writeToClientSocket(client, message);
    client->flush();
  }
}

void IpcServer::writeToClientSocket(QLocalSocket *&clientSocket, const QString &message) const
{
  QByteArray messageData = message.toUtf8() + '\n';
  qint64 bytesWritten = clientSocket->write(messageData);
  if (bytesWritten != messageData.size()) {
    LOG_ERR("%s ipc server failed to write full message to client socket", m_typeName.constData());
  } else {
    LOG_VERBOSE(
        "%s ipc server wrote message to client socket: %s", m_typeName.constData(), message.toUtf8().constData()
    );
  }
}

} // namespace deskflow::core::ipc
