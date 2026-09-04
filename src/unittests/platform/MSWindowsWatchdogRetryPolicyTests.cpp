/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsWatchdogRetryPolicy.h"

#include <QTest>

class MSWindowsWatchdogRetryPolicyTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void usesBoundedExponentialBackoff()
  {
    QCOMPARE(deskflow::platform::watchdogRetryDelaySeconds(0), 0.0);
    QCOMPARE(deskflow::platform::watchdogRetryDelaySeconds(1), 0.0);
    QCOMPARE(deskflow::platform::watchdogRetryDelaySeconds(2), 1.0);
    QCOMPARE(deskflow::platform::watchdogRetryDelaySeconds(3), 2.0);
    QCOMPARE(deskflow::platform::watchdogRetryDelaySeconds(4), 4.0);
    QCOMPARE(deskflow::platform::watchdogRetryDelaySeconds(5), 8.0);
    QCOMPARE(deskflow::platform::watchdogRetryDelaySeconds(6), 16.0);
    QCOMPARE(deskflow::platform::watchdogRetryDelaySeconds(7), 30.0);
    QCOMPARE(deskflow::platform::watchdogRetryDelaySeconds(1000), 30.0);
  }
};

QTEST_MAIN(MSWindowsWatchdogRetryPolicyTests)

#include "MSWindowsWatchdogRetryPolicyTests.moc"
