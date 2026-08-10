/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Cherry Inc.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"
#include "deskflow/DisplayGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

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

inline Direction oppositeDirection(Direction direction)
{
  using enum Direction;
  switch (direction) {
  case Left:
    return Right;
  case Right:
    return Left;
  case Top:
    return Bottom;
  case Bottom:
    return Top;
  case NoDirection:
    return NoDirection;
  }
  return NoDirection;
}

inline bool
touchesEdge(const deskflow::DisplayGeometry &display, const deskflow::DisplayGeometry &desktop, Direction edge)
{
  const auto displayRight = static_cast<int64_t>(display.x) + display.width;
  const auto displayBottom = static_cast<int64_t>(display.y) + display.height;
  const auto desktopRight = static_cast<int64_t>(desktop.x) + desktop.width;
  const auto desktopBottom = static_cast<int64_t>(desktop.y) + desktop.height;

  using enum Direction;
  switch (edge) {
  case Left:
    return display.x == desktop.x;
  case Right:
    return displayRight == desktopRight;
  case Top:
    return display.y == desktop.y;
  case Bottom:
    return displayBottom == desktopBottom;
  case NoDirection:
    return false;
  }
  return false;
}

inline int32_t orthogonalOrigin(const deskflow::DisplayGeometry &display, Direction edge)
{
  return edge == Direction::Left || edge == Direction::Right ? display.y : display.x;
}

inline int32_t orthogonalExtent(const deskflow::DisplayGeometry &display, Direction edge)
{
  return edge == Direction::Left || edge == Direction::Right ? display.height : display.width;
}

struct EdgeSpan
{
  int64_t begin;
  int64_t end;
};

inline std::vector<EdgeSpan>
edgeSpans(const deskflow::DisplayLayout &layout, const deskflow::DisplayGeometry &desktop, Direction edge)
{
  std::vector<EdgeSpan> spans;
  spans.reserve(layout.size());
  for (const auto &display : layout) {
    if (display.width <= 0 || display.height <= 0 || !touchesEdge(display, desktop, edge)) {
      continue;
    }

    const auto begin = static_cast<int64_t>(orthogonalOrigin(display, edge));
    spans.push_back({begin, begin + static_cast<int64_t>(orthogonalExtent(display, edge))});
  }

  std::ranges::sort(spans, {}, &EdgeSpan::begin);
  std::vector<EdgeSpan> merged;
  merged.reserve(spans.size());
  for (const auto &span : spans) {
    if (merged.empty() || span.begin > merged.back().end) {
      merged.push_back(span);
    } else {
      merged.back().end = std::max(merged.back().end, span.end);
    }
  }
  return merged;
}

inline int64_t edgeLength(const std::vector<EdgeSpan> &spans)
{
  int64_t length = 0;
  for (const auto &span : spans) {
    length += span.end - span.begin;
  }
  return length;
}

inline std::optional<double> fractionAlongEdge(const std::vector<EdgeSpan> &spans, int32_t coordinate)
{
  const auto total = edgeLength(spans);
  if (total <= 0) {
    return std::nullopt;
  }

  const auto position = static_cast<int64_t>(coordinate);
  int64_t traversed = 0;
  for (size_t index = 0; index < spans.size(); ++index) {
    const auto &span = spans[index];
    const auto length = span.end - span.begin;
    if (position >= span.begin && position < span.end) {
      return (static_cast<double>(traversed + position - span.begin) + 0.5) / static_cast<double>(total);
    }

    if (position < span.begin) {
      if (index == 0) {
        return 0.5 / static_cast<double>(total);
      }

      const auto leftDistance = position - spans[index - 1].end + 1;
      const auto rightDistance = span.begin - position;
      const auto nearest =
          leftDistance <= rightDistance ? static_cast<double>(traversed) - 0.5 : static_cast<double>(traversed) + 0.5;
      return std::clamp(nearest / static_cast<double>(total), 0.0, std::nextafter(1.0, 0.0));
    }
    traversed += length;
  }

  return (static_cast<double>(total) - 0.5) / static_cast<double>(total);
}

inline std::optional<int32_t> coordinateAlongEdge(const std::vector<EdgeSpan> &spans, double fraction)
{
  const auto total = edgeLength(spans);
  if (total <= 0) {
    return std::nullopt;
  }

  const auto finiteFraction = std::isfinite(fraction) ? fraction : 0.5;
  const auto normalized = std::clamp(finiteFraction, 0.0, std::nextafter(1.0, 0.0));
  const auto offset = std::min(static_cast<int64_t>(normalized * static_cast<double>(total)), total - 1);

  int64_t traversed = 0;
  for (const auto &span : spans) {
    const auto length = span.end - span.begin;
    if (offset < traversed + length) {
      const auto coordinate = span.begin + offset - traversed;
      return static_cast<int32_t>(std::clamp(
          coordinate, static_cast<int64_t>(std::numeric_limits<int32_t>::min()),
          static_cast<int64_t>(std::numeric_limits<int32_t>::max())
      ));
    }
    traversed += length;
  }
  return std::nullopt;
}

inline std::optional<int32_t> mapAcrossDisplayEdges(
    const deskflow::DisplayLayout &sourceLayout, const deskflow::DisplayGeometry &sourceDesktop,
    const deskflow::DisplayLayout &destinationLayout, const deskflow::DisplayGeometry &destinationDesktop,
    Direction exitDirection, int32_t sourceCoordinate, int32_t fallbackDestinationCoordinate
)
{
  static_cast<void>(fallbackDestinationCoordinate);
  if (exitDirection == Direction::NoDirection || sourceDesktop.width <= 0 || sourceDesktop.height <= 0 ||
      destinationDesktop.width <= 0 || destinationDesktop.height <= 0) {
    return std::nullopt;
  }

  const auto destinationEdge = oppositeDirection(exitDirection);
  const auto sourceSpans = edgeSpans(sourceLayout, sourceDesktop, exitDirection);
  const auto destinationSpans = edgeSpans(destinationLayout, destinationDesktop, destinationEdge);
  const auto fraction = fractionAlongEdge(sourceSpans, sourceCoordinate);
  if (!fraction) {
    return std::nullopt;
  }
  return coordinateAlongEdge(destinationSpans, *fraction);
}

} // namespace deskflow::server::cursor
