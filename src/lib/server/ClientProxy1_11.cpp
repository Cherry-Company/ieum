/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Cherry Inc.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_11.h"

#include "base/Log.h"
#include "deskflow/ProtocolUtil.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <tuple>

namespace {

constexpr size_t kMaximumDisplayCount = 32;

} // namespace

ClientProxy1_11::ClientProxy1_11(
    const std::string &name, deskflow::IStream *stream, Server *server, IEventQueue *events
)
    : ClientProxy1_10(name, stream, server, events)
{
}

deskflow::DisplayLayout ClientProxy1_11::getDisplayLayout() const
{
  return m_displayLayout.empty() ? ClientProxy1_10::getDisplayLayout() : m_displayLayout;
}

bool ClientProxy1_11::isForegroundFullscreen() const
{
  return m_foregroundFullscreen;
}

bool ClientProxy1_11::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgCDisplayLayout, 4) == 0) {
    return recvDisplayLayout();
  }
  if (memcmp(code, kMsgCForegroundFullscreen, 4) == 0) {
    return recvForegroundFullscreen();
  }
  return ClientProxy1_10::parseMessage(code);
}

bool ClientProxy1_11::recvDisplayLayout()
{
  std::vector<uint32_t> packed;
  if (!ProtocolUtil::readf(getStream(), kMsgCDisplayLayout + 4, &packed)) {
    return false;
  }
  if (packed.empty() || packed.size() % 4 != 0 || packed.size() / 4 > kMaximumDisplayCount) {
    LOG_WARN("client \"%s\" sent an invalid display layout with %zu values", getName().c_str(), packed.size());
    return true;
  }

  int32_t desktopX = 0;
  int32_t desktopY = 0;
  int32_t desktopWidth = 0;
  int32_t desktopHeight = 0;
  getShape(desktopX, desktopY, desktopWidth, desktopHeight);
  const auto desktopRight = static_cast<int64_t>(desktopX) + desktopWidth;
  const auto desktopBottom = static_cast<int64_t>(desktopY) + desktopHeight;

  deskflow::DisplayLayout layout;
  layout.reserve(packed.size() / 4);
  for (size_t i = 0; i < packed.size(); i += 4) {
    const deskflow::DisplayGeometry display{
        std::bit_cast<int32_t>(packed[i]), std::bit_cast<int32_t>(packed[i + 1]), std::bit_cast<int32_t>(packed[i + 2]),
        std::bit_cast<int32_t>(packed[i + 3])
    };
    const auto right = static_cast<int64_t>(display.x) + display.width;
    const auto bottom = static_cast<int64_t>(display.y) + display.height;
    if (display.width <= 0 || display.height <= 0 || display.x < desktopX || display.y < desktopY ||
        right > desktopRight || bottom > desktopBottom) {
      LOG_WARN(
          "client \"%s\" sent an out-of-bounds display: %d,%d %dx%d inside %d,%d %dx%d", getName().c_str(), display.x,
          display.y, display.width, display.height, desktopX, desktopY, desktopWidth, desktopHeight
      );
      return true;
    }
    layout.push_back(display);
  }

  std::ranges::sort(layout, {}, [](const auto &display) {
    return std::tuple{display.x, display.y, display.width, display.height};
  });
  layout.erase(std::unique(layout.begin(), layout.end()), layout.end());
  m_displayLayout = std::move(layout);
  LOG_DEBUG("received %zu physical displays from client \"%s\"", m_displayLayout.size(), getName().c_str());
  return true;
}

bool ClientProxy1_11::recvForegroundFullscreen()
{
  int8_t fullscreen = 0;
  if (!ProtocolUtil::readf(getStream(), kMsgCForegroundFullscreen + 4, &fullscreen)) {
    return false;
  }
  if (fullscreen != 0 && fullscreen != 1) {
    LOG_WARN("client \"%s\" sent an invalid foreground fullscreen state: %d", getName().c_str(), fullscreen);
    return true;
  }

  const bool newState = fullscreen != 0;
  if (newState != m_foregroundFullscreen) {
    LOG_DEBUG("client \"%s\" foreground capture changed: %s", getName().c_str(), newState ? "active" : "inactive");
  }
  m_foregroundFullscreen = newState;
  return true;
}
