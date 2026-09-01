/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/win32/MSWindowsEdgeDropHost.h"

#include "base/Log.h"

#include <utility>

namespace deskflow::filetransfer {

MSWindowsEdgeDropHost::MSWindowsEdgeDropHost(
    HINSTANCE instance, MSWindowsEdgeDropHostCallbacks callbacks, MSWindowsEdgeDropHostOptions options
)
    : m_instance(instance),
      m_callbacks(std::move(callbacks)),
      m_options(std::move(options)),
      m_className(L"IeumFileTransferEdgeDrop-" + std::to_wstring(reinterpret_cast<std::uintptr_t>(this)))
{
}

MSWindowsEdgeDropHost::~MSWindowsEdgeDropHost()
{
  clear();
}

bool MSWindowsEdgeDropHost::configure(const MSWindowsEdgeDropBounds &desktop, std::uint32_t activeSides)
{
  clear();
  const auto layout = makeMSWindowsEdgeDropLayout(desktop, activeSides, m_options.thickness);
  if (layout.empty()) {
    return activeSides == 0;
  }
  if (!registerWindowClass()) {
    return false;
  }

  for (const auto &window : layout) {
    if (!createHostedWindow(window)) {
      clear();
      return false;
    }
  }
  return true;
}

void MSWindowsEdgeDropHost::clear() noexcept
{
  for (auto &window : m_windows) {
    destroyHostedWindow(window);
  }
  m_windows.clear();

  if (m_windowClass != 0) {
    if (!UnregisterClassW(m_className.c_str(), m_instance)) {
      LOG_WARN("failed to unregister file-transfer edge window class: %lu", GetLastError());
    }
    m_windowClass = 0;
  }
}

std::size_t MSWindowsEdgeDropHost::windowCount() const noexcept
{
  return m_windows.size();
}

bool MSWindowsEdgeDropHost::registerWindowClass()
{
  WNDCLASSEXW classInfo{};
  classInfo.cbSize = sizeof(classInfo);
  classInfo.style = CS_NOCLOSE;
  classInfo.lpfnWndProc = &MSWindowsEdgeDropHost::windowProc;
  classInfo.hInstance = m_instance;
  classInfo.lpszClassName = m_className.c_str();
  m_windowClass = RegisterClassExW(&classInfo);
  if (m_windowClass == 0) {
    LOG_WARN("failed to register file-transfer edge window class: %lu", GetLastError());
    return false;
  }
  return true;
}

bool MSWindowsEdgeDropHost::createHostedWindow(const MSWindowsEdgeDropWindow &layout)
{
  HostedWindow hosted;
  hosted.layout = layout;
  hosted.handle = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, m_className.c_str(), L"", WS_POPUP, layout.x,
      layout.y, layout.width, layout.height, nullptr, nullptr, m_instance, nullptr
  );
  if (hosted.handle == nullptr) {
    LOG_WARN("failed to create file-transfer edge window: %lu", GetLastError());
    return false;
  }

  if (!SetLayeredWindowAttributes(hosted.handle, 0, 1, LWA_ALPHA)) {
    LOG_WARN("failed to make file-transfer edge window transparent: %lu", GetLastError());
    destroyHostedWindow(hosted);
    return false;
  }

  hosted.target = new MSWindowsEdgeDropTarget(callbacksFor(layout.direction), m_options.drop);
  const auto registerResult = RegisterDragDrop(hosted.handle, hosted.target);
  if (FAILED(registerResult)) {
    LOG_WARN("failed to register file-transfer OLE drop target: 0x%08lx", static_cast<unsigned long>(registerResult));
    destroyHostedWindow(hosted);
    return false;
  }
  hosted.registered = true;

  if (!SetWindowPos(
          hosted.handle, HWND_TOPMOST, layout.x, layout.y, layout.width, layout.height, SWP_NOACTIVATE | SWP_SHOWWINDOW
      )) {
    LOG_WARN("failed to position file-transfer edge window: %lu", GetLastError());
    destroyHostedWindow(hosted);
    return false;
  }

  m_windows.push_back(hosted);
  return true;
}

MSWindowsEdgeDropCallbacks MSWindowsEdgeDropHost::callbacksFor(Direction direction) const
{
  MSWindowsEdgeDropCallbacks callbacks;
  callbacks.direction = direction;
  callbacks.handoff = [this, direction](POINTL point, std::vector<std::filesystem::path> paths) {
    if (m_callbacks.handoff) {
      m_callbacks.handoff(
          FileTransferEdgeDrop{
              .direction = direction,
              .x = point.x,
              .y = point.y,
              .paths = std::move(paths),
          }
      );
    }
  };
  callbacks.stateChanged = [this, direction](EdgeHandoffState state, std::size_t count) {
    if (m_callbacks.stateChanged) {
      m_callbacks.stateChanged(direction, state, count);
    }
  };
  return callbacks;
}

void MSWindowsEdgeDropHost::destroyHostedWindow(HostedWindow &hosted) noexcept
{
  if (hosted.registered && hosted.handle != nullptr) {
    const auto revokeResult = RevokeDragDrop(hosted.handle);
    if (FAILED(revokeResult)) {
      LOG_WARN("failed to revoke file-transfer OLE drop target: 0x%08lx", static_cast<unsigned long>(revokeResult));
    }
    hosted.registered = false;
  }
  if (hosted.target != nullptr) {
    hosted.target->Release();
    hosted.target = nullptr;
  }
  if (hosted.handle != nullptr) {
    DestroyWindow(hosted.handle);
    hosted.handle = nullptr;
  }
}

LRESULT CALLBACK MSWindowsEdgeDropHost::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {
  case WM_MOUSEACTIVATE:
    return MA_NOACTIVATE;
  case WM_ERASEBKGND:
    return 1;
  default:
    return DefWindowProcW(window, message, wParam, lParam);
  }
}

} // namespace deskflow::filetransfer
