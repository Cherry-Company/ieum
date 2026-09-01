/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateShutdown.h"

namespace deskflow::gui {

bool waitForPreviousInstanceExit(
    const std::function<bool()> &isRunning, const std::function<void()> &waitForNextCheck, int maxChecks
)
{
  for (int check = 0; check < maxChecks; ++check) {
    if (!isRunning()) {
      return true;
    }
    if (check + 1 < maxChecks) {
      waitForNextCheck();
    }
  }
  return false;
}

} // namespace deskflow::gui
