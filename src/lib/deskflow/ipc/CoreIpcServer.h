/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IpcServer.h"
#include "base/DirectionTypes.h"

#include <QObject>
#include <QSet>
#include <QStringList>

#include <atomic>
#include <cstdint>
#include <functional>

class QLocalSocket;

namespace deskflow::core::ipc {

class CoreIpcServer : public IpcServer
{
  Q_OBJECT

public:
  explicit CoreIpcServer(QObject *parent);
  ~CoreIpcServer() override;

  static CoreIpcServer &instance();
  void publishFileTransferActiveSides(std::uint32_t activeSides);

Q_SIGNALS:
  void reloadConfigRequested();
  void fileTransferEdgeDropRequested(Direction direction, qint32 x, qint32 y, const QStringList &paths);

protected:
  using ClientValidator = std::function<bool(QLocalSocket *)>;

  CoreIpcServer(QObject *parent, const QString &serverName, ClientValidator clientValidator);

private:
  void processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts) override;

  ClientValidator m_clientValidator;
  std::atomic<std::uint32_t> m_fileTransferActiveSides{0};
};

} // namespace deskflow::core::ipc
