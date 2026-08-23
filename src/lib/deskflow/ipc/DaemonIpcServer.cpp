/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DaemonIpcServer.h"

#include "base/Log.h"
#include "common/Constants.h"

#include <QLocalSocket>
#include <QRegularExpression>
#include <QUuid>

namespace deskflow::core::ipc {

const auto kAckMessage = "ok";
const auto kErrorMessage = "error";

QString createLegacyRequestId()
{
  return QStringLiteral("legacy-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

DaemonIpcServer::DaemonIpcServer(QObject *parent, const QString &logFilename)
    : DaemonIpcServer(parent, logFilename, kDaemonIpcName)
{
}

DaemonIpcServer::DaemonIpcServer(QObject *parent, const QString &logFilename, const QString &serverName)
    : IpcServer(parent, serverName, QStringLiteral("daemon")),
      m_logFilename(logFilename)
{
}

void DaemonIpcServer::processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts)
{
  if (command == QStringLiteral("logLevel")) {
    processLogLevel(clientSocket, parts);
  } else if (command == QStringLiteral("configFile")) {
    processConfigFile(clientSocket, parts);
  } else if (command == QStringLiteral("start")) {
    LOG_DEBUG("daemon ipc server got start message");
    if (parts.size() != 1 && parts.size() != 2) {
      LOG_ERR("daemon ipc server got invalid start message");
      writeToClientSocket(clientSocket, kErrorMessage);
      return;
    }
    const auto legacyResponse = parts.size() == 1;
    const auto requestId = legacyResponse ? createLegacyRequestId() : parts.at(1);
    if (!registerCommand(clientSocket, requestId, command, legacyResponse)) {
      return;
    }
    Q_EMIT startCommandRequested(requestId);
  } else if (command == QStringLiteral("stop")) {
    LOG_DEBUG("daemon ipc server got stop message");
    if (parts.size() != 1 && parts.size() != 2) {
      LOG_ERR("daemon ipc server got invalid stop message");
      writeToClientSocket(clientSocket, kErrorMessage);
      return;
    }
    const auto legacyResponse = parts.size() == 1;
    const auto requestId = legacyResponse ? createLegacyRequestId() : parts.at(1);
    if (!registerCommand(clientSocket, requestId, command, legacyResponse)) {
      return;
    }
    Q_EMIT stopCommandRequested(requestId);
  } else if (command == QStringLiteral("logPath")) {
    LOG_DEBUG("daemon ipc server got log path request");
    writeToClientSocket(clientSocket, QStringLiteral("logPath=%1").arg(m_logFilename.toUtf8()));
  } else if (command == QStringLiteral("clearSettings")) {
    LOG_DEBUG("daemon ipc server got clear settings message");
    Q_EMIT clearSettingsRequested();
    writeToClientSocket(clientSocket, kAckMessage);
  } else {
    LOG_WARN("daemon ipc server got unknown command: %s", command.toUtf8().constData());
  }
}

void DaemonIpcServer::processLogLevel(QLocalSocket *&clientSocket, const QStringList &messageParts)
{
  if (messageParts.size() < 2) {
    LOG_ERR("daemon ipc server got invalid log level message");
    writeToClientSocket(clientSocket, kErrorMessage);
    return;
  }

  const auto &logLevel = messageParts.at(1);
  if (logLevel.isEmpty()) {
    LOG_ERR("daemon ipc server got empty log level");
    writeToClientSocket(clientSocket, kErrorMessage);
    return;
  }

  LOG_DEBUG("daemon ipc server got new log level: %s", logLevel.toUtf8().constData());
  Q_EMIT logLevelChanged(logLevel);
  writeToClientSocket(clientSocket, kAckMessage);
}

void DaemonIpcServer::processConfigFile(QLocalSocket *&clientSocket, const QStringList &messageParts)
{
  if (messageParts.size() != 2) {
    LOG_ERR("daemon ipc server got invalid config file message");
    writeToClientSocket(clientSocket, kErrorMessage);
    return;
  }

  const auto &argument = messageParts.at(1);
  const auto separator = argument.indexOf('\t');
  if (separator == 0) {
    LOG_ERR("daemon ipc server got config file message without request id");
    writeToClientSocket(clientSocket, kErrorMessage);
    return;
  }

  const auto legacyResponse = separator < 0;
  const auto requestId = legacyResponse ? createLegacyRequestId() : argument.left(separator);
  const auto configFile = legacyResponse ? argument : argument.mid(separator + 1);
  if (!registerCommand(clientSocket, requestId, QStringLiteral("configFile"), legacyResponse)) {
    return;
  }

  if (configFile.isEmpty()) {
    LOG_ERR("daemon ipc server got empty config file path");
    completeCommand(requestId, false, QStringLiteral("config file path is empty"));
    return;
  }

  LOG_DEBUG("daemon ipc server got config file: %s", configFile.toUtf8().constData());
  Q_EMIT configFileRequested(requestId, configFile);
}

bool DaemonIpcServer::registerCommand(
    QLocalSocket *&clientSocket, const QString &requestId, const QString &command, const bool legacyResponse
)
{
  if (!isValidRequestId(requestId) || m_pendingCommands.contains(requestId)) {
    LOG_ERR("daemon ipc server got invalid or duplicate request id");
    writeToClientSocket(clientSocket, kErrorMessage);
    return false;
  }

  m_pendingCommands.insert(requestId, PendingCommand{clientSocket, command, legacyResponse});
  return true;
}

bool DaemonIpcServer::isValidRequestId(const QString &requestId)
{
  static const QRegularExpression validRequestId(QStringLiteral("^[A-Za-z0-9_-]{1,64}$"));
  return validRequestId.match(requestId).hasMatch();
}

void DaemonIpcServer::completeCommand(const QString &requestId, const bool success, const QString &detail)
{
  const auto pending = m_pendingCommands.take(requestId);
  auto *clientSocket = pending.socket.data();
  if (pending.command.isEmpty() || clientSocket == nullptr) {
    LOG_WARN("daemon ipc server cannot complete unknown or disconnected request");
    return;
  }

  QString response;
  if (pending.legacyResponse) {
    response = success ? kAckMessage : kErrorMessage;
  } else {
    auto safeDetail = detail;
    safeDetail.replace('\r', ' ');
    safeDetail.replace('\n', ' ');
    safeDetail.replace('\t', ' ');
    const auto status = success ? QStringLiteral("ok") : QStringLiteral("error");
    response = QStringLiteral("commandResult=%1\t%2\t%3\t%4").arg(requestId, pending.command, status, safeDetail);
  }
  writeToClientSocket(clientSocket, response);
  clientSocket->flush();
}

} // namespace deskflow::core::ipc
