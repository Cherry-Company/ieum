/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpcClient.h"

#include "common/Constants.h"
#include "common/FileTransferEdgeDropIpc.h"

#include <QDebug>
#include <QLocalSocket>
#include <QObject>
#include <QString>

#include <limits>

namespace deskflow::gui::ipc {

CoreIpcClient::CoreIpcClient(QObject *parent) : CoreIpcClient(parent, kCoreIpcName, 20, 250)
{
}

CoreIpcClient::CoreIpcClient(
    QObject *parent, const QString &socketName, const int retryLimit, const int retryDelayMs
)
    : IpcClient(parent, socketName, QStringLiteral("core"), retryLimit, retryDelayMs)
{
  connect(this, &IpcClient::connected, this, [this] { m_exactVersionConnected = true; });
  connect(this, &IpcClient::versionMismatch, this, [this] {
    m_exactVersionConnected = false;
    setFileTransferActiveSides(0);
  });
  const auto disconnected = [this] {
    m_exactVersionConnected = false;
    setFileTransferActiveSides(0);
  };
  connect(this, &IpcClient::connectionFailed, this, disconnected);
  connect(this, &IpcClient::serverShutdown, this, disconnected);
}

void CoreIpcClient::sendStop()
{
  sendMessage(QStringLiteral("stop"));
}

void CoreIpcClient::sendReloadConfig()
{
  sendMessage(QStringLiteral("reloadConfig"));
}

void CoreIpcClient::requestFileTransferActiveSides()
{
  if (m_exactVersionConnected) {
    sendMessage(QStringLiteral("fileTransferActiveSides"));
  }
}

bool CoreIpcClient::sendFileTransferEdgeDrop(const QString &encodedValue)
{
  if (!m_exactVersionConnected || !isConnected() || !::deskflow::ipc::decodeFileTransferEdgeDropIpc(encodedValue).ok()) {
    return false;
  }
  sendMessage(QStringLiteral("fileTransferEdgeDrop=%1").arg(encodedValue));
  return true;
}

void CoreIpcClient::processCommand(const QString &command, const QStringList &parts)
{
  if (command == QStringLiteral("fileTransferActiveSides")) {
    if (!m_exactVersionConnected || parts.size() != 2) {
      return;
    }

    const auto &text = parts.at(1);
    if (text.isEmpty() || (text.size() > 1 && text.startsWith(QLatin1Char('0')))) {
      return;
    }

    std::uint64_t value = 0;
    for (const auto character : text) {
      if (character < QLatin1Char('0') || character > QLatin1Char('9')) {
        return;
      }
      value = value * 10 + static_cast<std::uint64_t>(character.unicode() - QLatin1Char('0').unicode());
      if (value > (std::numeric_limits<std::uint32_t>::max)()) {
        return;
      }
    }

    constexpr auto allowed = static_cast<std::uint32_t>(DirectionMask::LeftMask) |
                             static_cast<std::uint32_t>(DirectionMask::RightMask) |
                             static_cast<std::uint32_t>(DirectionMask::TopMask) |
                             static_cast<std::uint32_t>(DirectionMask::BottomMask);
    const auto activeSides = static_cast<std::uint32_t>(value);
    if ((activeSides & ~allowed) != 0) {
      return;
    }
    setFileTransferActiveSides(activeSides);
    return;
  }

  const auto args = parts.size() >= 2 ? parts.at(1) : QString();
  Q_EMIT commandReceived(command, args);
}

void CoreIpcClient::setFileTransferActiveSides(const std::uint32_t activeSides)
{
  if (m_fileTransferActiveSides == activeSides) {
    return;
  }
  m_fileTransferActiveSides = activeSides;
  Q_EMIT fileTransferActiveSidesChanged(activeSides);
}

} // namespace deskflow::gui::ipc
