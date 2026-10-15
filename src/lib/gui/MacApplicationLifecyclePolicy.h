/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

namespace deskflow::gui {

[[nodiscard]] constexpr bool macShouldHideOnClose(bool spontaneous, bool explicitQuit, bool systemShutdown) noexcept
{
  return spontaneous && !explicitQuit && !systemShutdown;
}

[[nodiscard]] constexpr bool macCanUseNotifications(bool hasBundleIdentifier, bool hasStableCodeIdentity) noexcept
{
  return hasBundleIdentifier && hasStableCodeIdentity;
}

} // namespace deskflow::gui
