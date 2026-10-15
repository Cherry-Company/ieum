/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacApplicationLifecyclePolicyTests.h"

#include "gui/MacApplicationLifecyclePolicy.h"

using deskflow::gui::macShouldHideOnClose;

void MacApplicationLifecyclePolicyTests::redWindowClose_hidesButQuitAndShutdownExit()
{
  QVERIFY(macShouldHideOnClose(true, false, false));
  QVERIFY(!macShouldHideOnClose(true, true, false));
  QVERIFY(!macShouldHideOnClose(true, false, true));
  QVERIFY(!macShouldHideOnClose(false, false, false));
}

void MacApplicationLifecyclePolicyTests::notificationsRequirePackagedSignedApplication()
{
  using deskflow::gui::macCanUseNotifications;
  QVERIFY(!macCanUseNotifications(false, false));
  QVERIFY(!macCanUseNotifications(true, false));
  QVERIFY(!macCanUseNotifications(false, true));
  QVERIFY(macCanUseNotifications(true, true));
}

QTEST_GUILESS_MAIN(MacApplicationLifecyclePolicyTests)
