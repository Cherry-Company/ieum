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
  return static_cast<uint32_t>((offset * 65535 + maximum / 2) / maximum);
}

} // namespace deskflow::win32
