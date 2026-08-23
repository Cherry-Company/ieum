/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IpcServer.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class QLocalSocket;

namespace deskflow::core::ipc {

class DaemonIpcServer : public IpcServer
{
  Q_OBJECT

public:
  explicit DaemonIpcServer(QObject *parent, const QString &logFilename);
  DaemonIpcServer(QObject *parent, const QString &logFilename, const QString &serverName);

  void completeCommand(const QString &requestId, bool success, const QString &detail = {});

Q_SIGNALS:
  void configFileRequested(const QString &requestId, const QString &configFile);
  void startCommandRequested(const QString &requestId);
  void stopCommandRequested(const QString &requestId);

private:
  struct PendingCommand
  {
    QPointer<QLocalSocket> socket;
    QString command;
    bool legacyResponse = false;
  };

  void processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts) override;
  void processLogLevel(QLocalSocket *&clientSocket, const QStringList &messageParts);
  void processConfigFile(QLocalSocket *&clientSocket, const QStringList &messageParts);
  bool registerCommand(
      QLocalSocket *&clientSocket, const QString &requestId, const QString &command, bool legacyResponse = false
  );
  static bool isValidRequestId(const QString &requestId);

private:
  const QString m_logFilename;
  QHash<QString, PendingCommand> m_pendingCommands;
};

} // namespace deskflow::core::ipc
