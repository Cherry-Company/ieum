/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Heesang Kim
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace deskflow::server::cursor {

inline constexpr int kMinimumPointerSpeed = 25;
inline constexpr int kDefaultPointerSpeed = 100;
inline constexpr int kMaximumPointerSpeed = 400;

inline int32_t clampCoordinate(int32_t coordinate, int32_t origin, int32_t extent)
{
  if (extent <= 0) {
    return origin;
  }

  const auto lower = static_cast<int64_t>(origin);
  const auto upper =
      std::min(lower + static_cast<int64_t>(extent) - 1, static_cast<int64_t>(std::numeric_limits<int32_t>::max()));
  return static_cast<int32_t>(std::clamp(static_cast<int64_t>(coordinate), lower, upper));
}

inline float toFraction(int32_t coordinate, int32_t origin, int32_t extent)
{
  if (extent <= 0) {
    return 0.5f;
  }

  const auto clamped = clampCoordinate(coordinate, origin, extent);
  return static_cast<float>(
      (static_cast<double>(clamped) - static_cast<double>(origin) + 0.5) / static_cast<double>(extent)
  );
}

inline int32_t fromFraction(float fraction, int32_t origin, int32_t extent)
{
  if (extent <= 0) {
    return origin;
  }

  const auto finiteFraction = std::isfinite(fraction) ? fraction : 0.5f;
  const auto upper = std::nextafter(1.0f, 0.0f);
  const auto clampedFraction = std::clamp(finiteFraction, 0.0f, upper);
  const auto coordinate =
      static_cast<int64_t>(origin) + static_cast<int64_t>(clampedFraction * static_cast<float>(extent));
  return clampCoordinate(
      static_cast<int32_t>(std::clamp(
          coordinate, static_cast<int64_t>(std::numeric_limits<int32_t>::min()),
          static_cast<int64_t>(std::numeric_limits<int32_t>::max())
      )),
      origin, extent
  );
}

inline int32_t scaleDelta(int32_t delta, int speedPercent, double &remainder)
{
  const auto speed = std::clamp(speedPercent, kMinimumPointerSpeed, kMaximumPointerSpeed);
  const auto scaled = static_cast<double>(delta) * static_cast<double>(speed) / 100.0 + remainder;
  const auto integral = std::trunc(scaled);
  if (integral >= static_cast<double>(std::numeric_limits<int32_t>::max())) {
    remainder = 0.0;
    return std::numeric_limits<int32_t>::max();
  }
  if (integral <= static_cast<double>(std::numeric_limits<int32_t>::min())) {
    remainder = 0.0;
    return std::numeric_limits<int32_t>::min();
  }
  remainder = scaled - integral;
  return static_cast<int32_t>(integral);
}

} // namespace deskflow::server::cursor
