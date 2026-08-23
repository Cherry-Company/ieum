/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DaemonIpcClient.h"

#include "common/Constants.h"

#include <QDebug>
#include <QUuid>

namespace deskflow::gui::ipc {

DaemonIpcClient::DaemonIpcClient(QObject *parent) : DaemonIpcClient(parent, kDaemonIpcName, 8, 250)
{
}

DaemonIpcClient::DaemonIpcClient(
    QObject *parent, const QString &socketName, const int retryLimit, const int retryDelayMs
)
    : IpcClient(parent, socketName, QStringLiteral("daemon"), retryLimit, retryDelayMs)
{
}

void DaemonIpcClient::sendLogLevel(const QString &logLevel)
{
  sendMessage(QStringLiteral("logLevel=%1").arg(logLevel));
}

QString DaemonIpcClient::sendConfigFile(const QString &configFile)
{
  return sendCorrelatedCommand(QStringLiteral("configFile"), configFile);
}

QString DaemonIpcClient::sendStartProcess()
{
  return sendCorrelatedCommand(QStringLiteral("start"));
}

QString DaemonIpcClient::sendStopProcess()
{
  return sendCorrelatedCommand(QStringLiteral("stop"));
}

void DaemonIpcClient::sendClearSettings()
{
  sendMessage(QStringLiteral("clearSettings"));
}

void DaemonIpcClient::requestLogPath()
{
  sendMessage(QStringLiteral("logPath"));
}

void DaemonIpcClient::processCommand(const QString &command, const QStringList &parts)
{
  if (command == QStringLiteral("logPath") && parts.size() == 2) {
    Q_EMIT logPathReceived(parts.at(1));
  } else if (command == QStringLiteral("commandResult") && parts.size() == 2) {
    const auto fields = parts.at(1).split('\t', Qt::KeepEmptyParts);
    if (fields.size() < 3 || fields.at(0).isEmpty() || fields.at(1).isEmpty()) {
      qWarning() << "daemon ipc client got malformed command result";
      return;
    }

    const auto &status = fields.at(2);
    if (status != QStringLiteral("ok") && status != QStringLiteral("error")) {
      qWarning() << "daemon ipc client got invalid command result status:" << status;
      return;
    }

    const auto detail = fields.mid(3).join('\t');
    Q_EMIT commandResult(fields.at(0), fields.at(1), status == QStringLiteral("ok"), detail);
  }
}

QString DaemonIpcClient::sendCorrelatedCommand(const QString &command)
{
  const auto requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  sendMessage(QStringLiteral("%1=%2").arg(command, requestId));
  return requestId;
}

QString DaemonIpcClient::sendCorrelatedCommand(const QString &command, const QString &argument)
{
  const auto requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  sendMessage(QStringLiteral("%1=%2\t%3").arg(command, requestId, argument));
  return requestId;
}

} // namespace deskflow::gui::ipc
