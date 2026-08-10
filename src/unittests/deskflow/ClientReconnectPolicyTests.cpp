/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ClientReconnectPolicy.h"

#include <QTest>

class ClientReconnectPolicyTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void keepsStartupRecoveryFast()
  {
    QCOMPARE(deskflow::reconnect::retryDelay(0, true), 0.25);
    QCOMPARE(deskflow::reconnect::retryDelay(19, true), 0.25);
    QCOMPARE(deskflow::reconnect::retryDelay(20, true), 1.0);
  }

  void capsLongRunningReconnectDelay()
  {
    QCOMPARE(deskflow::reconnect::retryDelay(79, true), 1.0);
    QCOMPARE(deskflow::reconnect::retryDelay(80, true), 2.0);
    QCOMPARE(deskflow::reconnect::retryDelay(100000, true), 2.0);
    QCOMPARE(deskflow::reconnect::retryDelay(100000, false), 1.0);
  }
};

QTEST_GUILESS_MAIN(ClientReconnectPolicyTests)

#include "ClientReconnectPolicyTests.moc"
