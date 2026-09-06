/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsKeyInput.h"

#include <QTest>

class MSWindowsKeyInputTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void modifierTransitions_data()
  {
    QTest::addColumn<int>("left");
    QTest::addColumn<int>("right");
    QTest::addColumn<int>("mask");
    QTest::newRow("shift") << VK_LSHIFT << VK_RSHIFT << int(KeyModifierShift);
    QTest::newRow("control") << VK_LCONTROL << VK_RCONTROL << int(KeyModifierControl);
    QTest::newRow("alt") << VK_LMENU << VK_RMENU << int(KeyModifierAlt);
    QTest::newRow("super") << VK_LWIN << VK_RWIN << int(KeyModifierSuper);
  }

  void modifierTransitions()
  {
    using deskflow::win32::isModifierRepeat;
    QFETCH(int, left);
    QFETCH(int, right);
    QFETCH(int, mask);
    QVERIFY(!isModifierRepeat(0, mask, left, true, false));
    // Holding the other side keeps the aggregate mask unchanged. These are
    // still distinct physical transitions which the raw client must receive.
    QVERIFY(!isModifierRepeat(mask, mask, right, true, false));
    QVERIFY(!isModifierRepeat(mask, mask, left, false, true));
    QVERIFY(!isModifierRepeat(mask, 0, right, false, true));
    QVERIFY(isModifierRepeat(mask, mask, right, true, true));
    QVERIFY(!isModifierRepeat(mask, mask, 'A', true, true));
  }
};

QTEST_MAIN(MSWindowsKeyInputTests)

#include "MSWindowsKeyInputTests.moc"
