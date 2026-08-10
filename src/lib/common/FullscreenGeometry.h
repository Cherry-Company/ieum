/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace deskflow::fullscreen {

struct Bounds
{
  double left;
  double top;
  double right;
  double bottom;
};

inline bool coversDisplay(const Bounds &window, const Bounds &display, double edgeTolerance = 8.0)
{
  const auto displayWidth = display.right - display.left;
  const auto displayHeight = display.bottom - display.top;
  if (displayWidth <= 0.0 || displayHeight <= 0.0) {
    return false;
  }

  const auto intersectionWidth =
      (std::max)(0.0, (std::min)(window.right, display.right) - (std::max)(window.left, display.left));
  const auto intersectionHeight =
      (std::max)(0.0, (std::min)(window.bottom, display.bottom) - (std::max)(window.top, display.top));
  const auto coverage = intersectionWidth * intersectionHeight / (displayWidth * displayHeight);

  return std::isfinite(coverage) && coverage >= 0.98 && window.left <= display.left + edgeTolerance &&
         window.top <= display.top + edgeTolerance && window.right >= display.right - edgeTolerance &&
         window.bottom >= display.bottom - edgeTolerance;
}

inline bool
pointerIsConfinedToDisplay(const Bounds &clip, const Bounds &display, const Bounds &desktop, double edgeTolerance = 4.0)
{
  const auto clipWidth = clip.right - clip.left;
  const auto clipHeight = clip.bottom - clip.top;
  if (clipWidth < 64.0 || clipHeight < 64.0) {
    return false;
  }

  // The ordinary unclipped state covers the complete virtual desktop. A
  // smaller rectangle contained by one physical display is an explicit
  // pointer-capture request, which games commonly use in both exclusive and
  // borderless/windowed modes.
  if (coversDisplay(clip, desktop, edgeTolerance)) {
    return false;
  }

  return clip.left >= display.left - edgeTolerance && clip.top >= display.top - edgeTolerance &&
         clip.right <= display.right + edgeTolerance && clip.bottom <= display.bottom + edgeTolerance;
}

} // namespace deskflow::fullscreen
