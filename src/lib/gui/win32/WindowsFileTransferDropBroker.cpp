/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/win32/WindowsFileTransferDropBroker.h"

#include "common/FileTransferEdgeDropIpc.h"

#include <QGuiApplication>
#include <QScreen>
#include <QThread>
#include <QTimer>

#include <utility>

namespace deskflow::gui {
namespace {

constexpr auto kAllowedSides =
    static_cast<std::uint32_t>(DirectionMask::LeftMask) | static_cast<std::uint32_t>(DirectionMask::RightMask) |
    static_cast<std::uint32_t>(DirectionMask::TopMask) | static_cast<std::uint32_t>(DirectionMask::BottomMask);

class NativeDropHost final : public WindowsFileTransferDropHost
{
public:
  explicit NativeDropHost(filetransfer::MSWindowsEdgeDropHostCallbacks callbacks)
      : m_host(GetModuleHandleW(nullptr), std::move(callbacks))
  {
  }

  bool configure(const filetransfer::MSWindowsEdgeDropBounds &bounds, std::uint32_t activeSides) override
  {
    return m_host.configure(bounds, activeSides);
  }

  void clear() noexcept override
  {
    m_host.clear();
  }

private:
  filetransfer::MSWindowsEdgeDropHost m_host;
};

WindowsFileTransferDropBrokerDependencies withProductionDefaults(WindowsFileTransferDropBrokerDependencies dependencies)
{
  if (!dependencies.oleInitialize) {
    dependencies.oleInitialize = [] { return OleInitialize(nullptr); };
  }
  if (!dependencies.oleUninitialize) {
    dependencies.oleUninitialize = [] { OleUninitialize(); };
  }
  if (!dependencies.createHost) {
    dependencies.createHost = [](filetransfer::MSWindowsEdgeDropHostCallbacks callbacks) {
      return std::make_unique<NativeDropHost>(std::move(callbacks));
    };
  }
  if (!dependencies.virtualDesktopBounds) {
    dependencies.virtualDesktopBounds = [] {
      return filetransfer::MSWindowsEdgeDropBounds{
          GetSystemMetrics(SM_XVIRTUALSCREEN),
          GetSystemMetrics(SM_YVIRTUALSCREEN),
          GetSystemMetrics(SM_CXVIRTUALSCREEN),
          GetSystemMetrics(SM_CYVIRTUALSCREEN),
      };
    };
  }
  return dependencies;
}

} // namespace

WindowsFileTransferDropBroker::WindowsFileTransferDropBroker(
    HandoffHandler handoff, WindowsFileTransferDropBrokerDependencies dependencies, QObject *parent
)
    : QObject(parent),
      m_handoff(std::move(handoff)),
      m_dependencies(withProductionDefaults(std::move(dependencies)))
{
  if (qGuiApp == nullptr || QThread::currentThread() != qGuiApp->thread()) {
    return;
  }

  const auto oleResult = m_dependencies.oleInitialize();
  if (oleResult != S_OK && oleResult != S_FALSE) {
    return;
  }
  m_oleInitialized = true;

  filetransfer::MSWindowsEdgeDropHostCallbacks callbacks;
  callbacks.handoff = [this](filetransfer::FileTransferEdgeDrop drop) { handleDrop(std::move(drop)); };
  m_host = m_dependencies.createHost(std::move(callbacks));
  if (!m_host) {
    return;
  }

  connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *) {
    reconnectScreenSignals();
    scheduleGeometryRefresh();
  });
  connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
    reconnectScreenSignals();
    scheduleGeometryRefresh();
  });
  reconnectScreenSignals();
}

WindowsFileTransferDropBroker::~WindowsFileTransferDropBroker()
{
  for (const auto &connection : m_screenConnections) {
    disconnect(connection);
  }
  m_screenConnections.clear();
  if (m_host) {
    m_host->clear();
    m_host.reset();
  }
  if (m_oleInitialized) {
    m_dependencies.oleUninitialize();
  }
}

bool WindowsFileTransferDropBroker::isAvailable() const noexcept
{
  return m_oleInitialized && m_host != nullptr;
}

bool WindowsFileTransferDropBroker::setActiveSides(const std::uint32_t activeSides)
{
  if ((activeSides & ~kAllowedSides) != 0) {
    return false;
  }

  if (activeSides == 0) {
    m_activeSides = 0;
    if (m_host) {
      m_host->clear();
    }
    return true;
  }

  if (!isAvailable()) {
    return false;
  }

  if (!m_host->configure(m_dependencies.virtualDesktopBounds(), activeSides)) {
    m_activeSides = 0;
    m_host->clear();
    return false;
  }
  m_activeSides = activeSides;
  return true;
}

std::uint32_t WindowsFileTransferDropBroker::activeSides() const noexcept
{
  return m_activeSides;
}

void WindowsFileTransferDropBroker::scheduleGeometryRefresh()
{
  if (m_refreshScheduled || m_activeSides == 0) {
    return;
  }
  m_refreshScheduled = true;
  QTimer::singleShot(0, this, [this] {
    m_refreshScheduled = false;
    refreshGeometry();
  });
}

void WindowsFileTransferDropBroker::reconnectScreenSignals()
{
  for (const auto &connection : m_screenConnections) {
    disconnect(connection);
  }
  m_screenConnections.clear();
  for (auto *screen : QGuiApplication::screens()) {
    m_screenConnections.push_back(connect(screen, &QScreen::geometryChanged, this, [this](const QRect &) {
      scheduleGeometryRefresh();
    }));
  }
}

void WindowsFileTransferDropBroker::refreshGeometry()
{
  if (m_activeSides == 0 || !m_host) {
    return;
  }
  const auto sides = m_activeSides;
  if (!m_host->configure(m_dependencies.virtualDesktopBounds(), sides)) {
    m_activeSides = 0;
    m_host->clear();
  }
}

void WindowsFileTransferDropBroker::handleDrop(filetransfer::FileTransferEdgeDrop drop) const
{
  QStringList paths;
  paths.reserve(static_cast<qsizetype>(drop.paths.size()));
  for (const auto &path : drop.paths) {
    const auto &native = path.native();
    paths.append(QString::fromWCharArray(native.data(), static_cast<qsizetype>(native.size())));
  }

  const auto encoded = ipc::encodeFileTransferEdgeDropIpc(
      {.direction = drop.direction, .x = drop.x, .y = drop.y, .paths = std::move(paths)}
  );
  if (encoded.ok() && m_handoff) {
    m_handoff(encoded.encodedValue);
  }
}

} // namespace deskflow::gui
