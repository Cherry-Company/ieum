/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <array>
#include <cstddef>

namespace deskflow::platform {

[[nodiscard]] constexpr double watchdogRetryDelaySeconds(const std::size_t consecutiveFailures) noexcept
{
  constexpr std::array delays{0.0, 1.0, 2.0, 4.0, 8.0, 16.0, 30.0};
  if (consecutiveFailures == 0) {
    return 0.0;
  }
  const auto index = consecutiveFailures - 1 < delays.size() ? consecutiveFailures - 1 : delays.size() - 1;
  return delays[index];
}

} // namespace deskflow::platform
