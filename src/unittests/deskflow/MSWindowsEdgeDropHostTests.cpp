/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "deskflow/win32/MSWindowsEdgeDropHost.h"

#include "base/DirectionTypes.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

using deskflow::filetransfer::makeMSWindowsEdgeDropLayout;
using deskflow::filetransfer::MSWindowsEdgeDropBounds;
using deskflow::filetransfer::MSWindowsEdgeDropWindow;

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

const MSWindowsEdgeDropWindow &windowFor(const std::vector<MSWindowsEdgeDropWindow> &windows, Direction direction)
{
  const auto found = std::ranges::find(windows, direction, &MSWindowsEdgeDropWindow::direction);
  require(found != windows.end(), "expected edge window is missing");
  return *found;
}

std::uint32_t allSides()
{
  using enum DirectionMask;
  return static_cast<std::uint32_t>(LeftMask) | static_cast<std::uint32_t>(RightMask) |
         static_cast<std::uint32_t>(TopMask) | static_cast<std::uint32_t>(BottomMask);
}

void laysOutAllSidesWithoutCornerOverlap()
{
  const MSWindowsEdgeDropBounds desktop{-1920, -120, 3840, 2280};
  const auto windows = makeMSWindowsEdgeDropLayout(desktop, allSides(), 3);

  require(windows.size() == 4, "all configured sides should produce four windows");
  require(
      windowFor(windows, Direction::Left) == MSWindowsEdgeDropWindow{Direction::Left, -1920, -120, 3, 2280},
      "left strip should preserve negative virtual coordinates"
  );
  require(
      windowFor(windows, Direction::Right) == MSWindowsEdgeDropWindow{Direction::Right, 1917, -120, 3, 2280},
      "right strip should sit inside the virtual desktop"
  );
  require(
      windowFor(windows, Direction::Top) == MSWindowsEdgeDropWindow{Direction::Top, -1917, -120, 3834, 3},
      "top strip should exclude left and right corners"
  );
  require(
      windowFor(windows, Direction::Bottom) == MSWindowsEdgeDropWindow{Direction::Bottom, -1917, 2157, 3834, 3},
      "bottom strip should exclude left and right corners"
  );
}

void includesOnlyConfiguredSides()
{
  using enum DirectionMask;
  const auto mask = static_cast<std::uint32_t>(RightMask) | static_cast<std::uint32_t>(BottomMask);
  const auto windows = makeMSWindowsEdgeDropLayout({10, 20, 100, 80}, mask, 2);

  require(windows.size() == 2, "partial mask should produce only two windows");
  require(windowFor(windows, Direction::Right).x == 108, "right strip should use requested thickness");
  require(
      windowFor(windows, Direction::Bottom) == MSWindowsEdgeDropWindow{Direction::Bottom, 12, 98, 96, 2},
      "bottom strip should remain disjoint from side corners"
  );
}

void rejectsInvalidOrUnusableGeometry()
{
  using enum DirectionMask;
  const auto top = static_cast<std::uint32_t>(TopMask);

  require(makeMSWindowsEdgeDropLayout({0, 0, 0, 100}, allSides(), 3).empty(), "zero width should fail closed");
  require(makeMSWindowsEdgeDropLayout({0, 0, 100, -1}, allSides(), 3).empty(), "negative height should fail closed");
  require(makeMSWindowsEdgeDropLayout({0, 0, 100, 100}, allSides(), 0).empty(), "zero thickness should fail closed");
  require(makeMSWindowsEdgeDropLayout({0, 0, 100, 100}, 0, 3).empty(), "empty side mask should create nothing");
  require(
      makeMSWindowsEdgeDropLayout({0, 0, 4, 100}, top, 3).empty(),
      "horizontal strip with no corner-safe width should be omitted"
  );
}

} // namespace

int main()
{
  laysOutAllSidesWithoutCornerOverlap();
  includesOnlyConfiguredSides();
  rejectsInvalidOrUnusableGeometry();
  std::cout << "PASS: MSWindowsEdgeDropHostTests\n";
  return 0;
}
