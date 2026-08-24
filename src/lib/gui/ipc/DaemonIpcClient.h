/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IpcClient.h"

#include <QObject>

namespace deskflow::gui::ipc {

class DaemonIpcClient : public IpcClient
{
  Q_OBJECT

public:
  explicit DaemonIpcClient(QObject *parent = nullptr);
  void sendLogLevel(const QString &logLevel);
  QString sendConfigFile(const QString &configFile);
  QString sendStartProcess();
  QString sendStopProcess();
  void sendClearSettings();
  void requestLogPath();

Q_SIGNALS:
  void logPathReceived(const QString &logPath);
  void commandResult(const QString &requestId, const QString &command, bool success, const QString &detail);

protected:
  DaemonIpcClient(QObject *parent, const QString &socketName, int retryLimit, int retryDelayMs);
  void processCommand(const QString &command, const QStringList &parts) override;

private:
  QString sendCorrelatedCommand(const QString &command);
  QString sendCorrelatedCommand(const QString &command, const QString &argument);
};

} // namespace deskflow::gui::ipc
