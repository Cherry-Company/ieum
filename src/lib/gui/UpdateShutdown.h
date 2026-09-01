/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <functional>

namespace deskflow::gui {

[[nodiscard]] bool waitForPreviousInstanceExit(
    const std::function<bool()> &isRunning, const std::function<void()> &waitForNextCheck, int maxChecks
);

} // namespace deskflow::gui
