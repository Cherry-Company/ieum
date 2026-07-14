/*
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CanonicalScancodeTests.h"

#include "deskflow/CanonicalScancode.h"

void CanonicalScancodeTests::mapsKoreanTypingKeys()
{
  QCOMPARE(deskflow::scancode::macVirtualKeyFromSet1(0x10).value(), 0x0c); // Q position
  QCOMPARE(deskflow::scancode::macVirtualKeyFromSet1(0x1e).value(), 0x00); // A position
  QCOMPARE(deskflow::scancode::macVirtualKeyFromSet1(0x39).value(), 0x31); // Space
}

void CanonicalScancodeTests::roundTripsExtendedKeys()
{
  for (const KeyButton scancode : {KeyButton{0x11d}, KeyButton{0x138}, KeyButton{0x14b}, KeyButton{0x153}}) {
    const auto virtualKey = deskflow::scancode::macVirtualKeyFromSet1(scancode);
    QVERIFY(virtualKey.has_value());
    QCOMPARE(deskflow::scancode::set1FromMacVirtualKey(*virtualKey).value(), scancode);
  }
}

void CanonicalScancodeTests::rejectsImeCommandKeys()
{
  QVERIFY(!deskflow::scancode::macVirtualKeyFromSet1(0x190).has_value()); // LANG1 uses DILC
}

QTEST_MAIN(CanonicalScancodeTests)
