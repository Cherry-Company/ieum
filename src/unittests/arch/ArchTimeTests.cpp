/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "arch/Arch.h"

#include <QElapsedTimer>
#include <QTest>
#include <cmath>

class ArchTimeTests final : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void measuresSubsecondIntervalsWithoutIntegerTruncation()
  {
    const auto start = Arch::time();
    QElapsedTimer reference;
    reference.start();
    QTest::qSleep(60);
    const auto elapsed = Arch::time() - start;
    const auto expected = reference.nsecsElapsed() / 1.0e9;
    QVERIFY2(elapsed > 0.0, "The monotonic clock must advance within a second for input and connection timers.");
    QVERIFY2(
        std::abs(elapsed - expected) < 0.025,
        qPrintable(QStringLiteral("Clock measured %1 s, reference %2 s").arg(elapsed).arg(expected))
    );
  }
};

QTEST_GUILESS_MAIN(ArchTimeTests)
#include "ArchTimeTests.moc"
