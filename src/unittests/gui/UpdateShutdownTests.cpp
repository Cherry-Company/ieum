/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateShutdownTests.h"

#include "gui/UpdateShutdown.h"

#include <QTest>

using deskflow::gui::waitForPreviousInstanceExit;

void UpdateShutdownTests::waitsUntilPreviousInstanceHasExited()
{
  int checks = 0;
  int waits = 0;
  const auto exited = waitForPreviousInstanceExit(
      [&checks] { return ++checks < 3; }, [&waits] { ++waits; }, 5
  );

  QVERIFY(exited);
  QCOMPARE(checks, 3);
  QCOMPARE(waits, 2);
}

void UpdateShutdownTests::reportsTimeoutWhenPreviousInstanceRemains()
{
  int checks = 0;
  int waits = 0;
  const auto exited = waitForPreviousInstanceExit(
      [&checks] {
        ++checks;
        return true;
      },
      [&waits] { ++waits; }, 3
  );

  QVERIFY(!exited);
  QCOMPARE(checks, 3);
  QCOMPARE(waits, 2);
}

QTEST_GUILESS_MAIN(UpdateShutdownTests)
