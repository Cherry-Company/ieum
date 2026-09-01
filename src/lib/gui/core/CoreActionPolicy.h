/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Enums.h"

namespace deskflow::gui {

[[nodiscard]] constexpr bool coreCanStop(deskflow::core::ProcessState state) noexcept
{
  using enum deskflow::core::ProcessState;
  return state == Starting || state == Started || state == RetryPending;
}

[[nodiscard]] constexpr bool coreToggleEnabled(deskflow::core::ProcessState state, bool canStart) noexcept
{
  return canStart || coreCanStop(state);
}

} // namespace deskflow::gui
