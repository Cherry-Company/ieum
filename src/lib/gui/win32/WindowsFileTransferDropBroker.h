/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/win32/MSWindowsEdgeDropHost.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace deskflow::gui {

class WindowsFileTransferDropHost
{
public:
  virtual ~WindowsFileTransferDropHost() = default;
  [[nodiscard]] virtual bool
  configure(const filetransfer::MSWindowsEdgeDropBounds &bounds, std::uint32_t activeSides) = 0;
  virtual void clear() noexcept = 0;
};

struct WindowsFileTransferDropBrokerDependencies
{
  std::function<HRESULT()> oleInitialize;
  std::function<void()> oleUninitialize;
  std::function<std::unique_ptr<WindowsFileTransferDropHost>(filetransfer::MSWindowsEdgeDropHostCallbacks)> createHost;
  std::function<filetransfer::MSWindowsEdgeDropBounds()> virtualDesktopBounds;
};

class WindowsFileTransferDropBroker final : public QObject
{
  Q_OBJECT

public:
  using HandoffHandler = std::function<void(const QString &encodedValue)>;

  explicit WindowsFileTransferDropBroker(
      HandoffHandler handoff, WindowsFileTransferDropBrokerDependencies dependencies = {}, QObject *parent = nullptr
  );
  ~WindowsFileTransferDropBroker() override;

  WindowsFileTransferDropBroker(const WindowsFileTransferDropBroker &) = delete;
  WindowsFileTransferDropBroker &operator=(const WindowsFileTransferDropBroker &) = delete;

  [[nodiscard]] bool isAvailable() const noexcept;
  [[nodiscard]] bool setActiveSides(std::uint32_t activeSides);
  [[nodiscard]] std::uint32_t activeSides() const noexcept;

private Q_SLOTS:
  void scheduleGeometryRefresh();

private:
  void reconnectScreenSignals();
  void refreshGeometry();
  void handleDrop(filetransfer::FileTransferEdgeDrop drop) const;

  HandoffHandler m_handoff;
  WindowsFileTransferDropBrokerDependencies m_dependencies;
  std::unique_ptr<WindowsFileTransferDropHost> m_host;
  std::vector<QMetaObject::Connection> m_screenConnections;
  std::uint32_t m_activeSides = 0;
  bool m_oleInitialized = false;
  bool m_refreshScheduled = false;
};

} // namespace deskflow::gui
