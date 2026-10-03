/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <algorithm>
#include <cstdint>

namespace deskflow::win32 {

inline uint32_t normalizeAbsoluteMouseCoordinate(int32_t value, int32_t origin, int32_t extent)
{
  if (extent <= 1) {
    return 0;
  }

  const int64_t maximum = static_cast<int64_t>(extent) - 1;
  const int64_t offset = std::clamp(static_cast<int64_t>(value) - static_cast<int64_t>(origin), int64_t{0}, maximum);
  if (offset == 0) {
    return 0;
  }
  if (offset == maximum) {
    return 65535;
  }

  // SendInput maps 65536 buckets to extent pixels. Aim at the pixel's
  // center so decoding cannot land on the previous pixel through rounding.
  return static_cast<uint32_t>((offset * 65536 + 32768) / extent);
}

} // namespace deskflow::win32
