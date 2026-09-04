/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IpcClient.h"

#include <QObject>

#include <cstdint>

namespace deskflow::gui::ipc {

class CoreIpcClient : public IpcClient
{
  Q_OBJECT

public:
  explicit CoreIpcClient(QObject *parent = nullptr);

  void sendStop();
  void sendReloadConfig();
  void requestFileTransferActiveSides();
  [[nodiscard]] bool sendFileTransferEdgeDrop(const QString &encodedValue);

Q_SIGNALS:
  void commandReceived(const QString &command, const QString &args);
  void fileTransferActiveSidesChanged(std::uint32_t activeSides);

protected:
  CoreIpcClient(QObject *parent, const QString &socketName, int retryLimit, int retryDelayMs);
  void processCommand(const QString &command, const QStringList &parts) override;

private:
  void setFileTransferActiveSides(std::uint32_t activeSides);

  std::uint32_t m_fileTransferActiveSides = 0;
  bool m_exactVersionConnected = false;
};

} // namespace deskflow::gui::ipc
