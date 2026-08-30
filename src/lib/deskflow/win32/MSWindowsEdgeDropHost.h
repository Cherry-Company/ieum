/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "deskflow/win32/MSWindowsEdgeDropTarget.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace deskflow::filetransfer {

struct MSWindowsEdgeDropBounds
{
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;

  bool operator==(const MSWindowsEdgeDropBounds &) const = default;
};

struct MSWindowsEdgeDropWindow
{
  Direction direction = Direction::NoDirection;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;

  bool operator==(const MSWindowsEdgeDropWindow &) const = default;
};

[[nodiscard]] inline std::vector<MSWindowsEdgeDropWindow> makeMSWindowsEdgeDropLayout(
    const MSWindowsEdgeDropBounds &desktop, std::uint32_t activeSides, std::int32_t thickness = 3
)
{
  if (desktop.width <= 0 || desktop.height <= 0 || thickness <= 0 || activeSides == 0) {
    return {};
  }

  const auto right64 = static_cast<std::int64_t>(desktop.x) + desktop.width;
  const auto bottom64 = static_cast<std::int64_t>(desktop.y) + desktop.height;
  if (right64 > (std::numeric_limits<std::int32_t>::max)() || right64 < (std::numeric_limits<std::int32_t>::min)() ||
      bottom64 > (std::numeric_limits<std::int32_t>::max)() || bottom64 < (std::numeric_limits<std::int32_t>::min)()) {
    return {};
  }

  const auto edge = (std::min)({thickness, desktop.width, desktop.height});
  const auto right = static_cast<std::int32_t>(right64);
  const auto bottom = static_cast<std::int32_t>(bottom64);
  std::vector<MSWindowsEdgeDropWindow> result;
  result.reserve(4);

  const auto hasSide = [activeSides](DirectionMask side) {
    return (activeSides & static_cast<std::uint32_t>(side)) != 0;
  };
  using enum DirectionMask;
  if (hasSide(LeftMask)) {
    result.push_back({Direction::Left, desktop.x, desktop.y, edge, desktop.height});
  }
  if (hasSide(RightMask)) {
    result.push_back({Direction::Right, right - edge, desktop.y, edge, desktop.height});
  }

  const auto horizontalWidth64 = static_cast<std::int64_t>(desktop.width) - (2LL * edge);
  if (horizontalWidth64 > 0 && hasSide(TopMask)) {
    result.push_back({Direction::Top, desktop.x + edge, desktop.y, static_cast<std::int32_t>(horizontalWidth64), edge});
  }
  if (horizontalWidth64 > 0 && hasSide(BottomMask)) {
    result.push_back(
        {Direction::Bottom, desktop.x + edge, bottom - edge, static_cast<std::int32_t>(horizontalWidth64), edge}
    );
  }

  return result;
}

struct MSWindowsEdgeDropHostCallbacks
{
  using TargetResolver = std::function<std::optional<EdgeTarget>(Direction, POINTL)>;
  using HandoffHandler = std::function<void(const EdgeTarget &, std::vector<FileTransferSourceCandidate>)>;
  using StateHandler =
      std::function<void(Direction, EdgeHandoffState, const std::optional<EdgeTarget> &, std::size_t, std::uint64_t)>;

  TargetResolver resolveTarget;
  HandoffHandler handoff;
  StateHandler stateChanged;
};

struct MSWindowsEdgeDropHostOptions
{
  std::int32_t thickness = 3;
  MSWindowsEdgeDropOptions drop;
};

class MSWindowsEdgeDropHost final
{
public:
  MSWindowsEdgeDropHost(
      HINSTANCE instance, MSWindowsEdgeDropHostCallbacks callbacks, MSWindowsEdgeDropHostOptions options = {}
  );
  ~MSWindowsEdgeDropHost();

  MSWindowsEdgeDropHost(const MSWindowsEdgeDropHost &) = delete;
  MSWindowsEdgeDropHost &operator=(const MSWindowsEdgeDropHost &) = delete;

  [[nodiscard]] bool configure(const MSWindowsEdgeDropBounds &desktop, std::uint32_t activeSides);
  void clear() noexcept;

  [[nodiscard]] std::size_t windowCount() const noexcept;

private:
  struct HostedWindow
  {
    MSWindowsEdgeDropWindow layout;
    HWND handle = nullptr;
    MSWindowsEdgeDropTarget *target = nullptr;
    bool registered = false;
  };

  [[nodiscard]] bool registerWindowClass();
  [[nodiscard]] bool createHostedWindow(const MSWindowsEdgeDropWindow &layout);
  [[nodiscard]] MSWindowsEdgeDropCallbacks callbacksFor(Direction direction) const;
  void destroyHostedWindow(HostedWindow &hosted) noexcept;

  static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

  HINSTANCE m_instance = nullptr;
  MSWindowsEdgeDropHostCallbacks m_callbacks;
  MSWindowsEdgeDropHostOptions m_options;
  std::wstring m_className;
  ATOM m_windowClass = 0;
  std::vector<HostedWindow> m_windows;
};

} // namespace deskflow::filetransfer
