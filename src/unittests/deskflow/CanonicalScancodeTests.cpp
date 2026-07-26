/*
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CanonicalScancodeTests.h"

#include "deskflow/CanonicalScancode.h"
#include "deskflow/IKeyState.h"

#include <set>

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

void CanonicalScancodeTests::tableIsUnambiguous()
{
  // A repeated code on either side would make the reverse lookup pick the wrong
  // key, which is silent: the wrong character just gets typed.
  std::set<KeyButton> set1Codes;
  std::set<uint16_t> virtualKeys;
  for (uint16_t virtualKey = 0; virtualKey <= 0x7f; ++virtualKey) {
    const auto scancode = deskflow::scancode::set1FromMacVirtualKey(virtualKey);
    if (!scancode.has_value()) {
      continue;
    }
    QVERIFY2(
        set1Codes.insert(*scancode).second, qPrintable(QStringLiteral("duplicate Set-1 0x%1").arg(*scancode, 0, 16))
    );
    QVERIFY(virtualKeys.insert(virtualKey).second);

    // every mapping must survive a round trip in both directions
    QCOMPARE(deskflow::scancode::macVirtualKeyFromSet1(*scancode).value(), virtualKey);
    QVERIFY(deskflow::scancode::isSet1Scancode(*scancode));
  }
  QVERIFY(!set1Codes.empty());
}

void CanonicalScancodeTests::canonicalFlagSurvivesTheWire()
{
  using namespace deskflow::scancode;
  for (uint16_t virtualKey = 0; virtualKey <= 0x7f; ++virtualKey) {
    const auto scancode = set1FromMacVirtualKey(virtualKey);
    if (!scancode.has_value()) {
      continue;
    }
    const KeyButton marked = markCanonical(*scancode);
    QVERIFY(isCanonical(marked));
    QCOMPARE(stripCanonical(marked), *scancode);
  }

  // an unmarked platform button must never be mistaken for a scancode
  QVERIFY(!isCanonical(0x001));
  QVERIFY(!isCanonical(0x15c));
  QCOMPARE(stripCanonical(0x01e), KeyButton{0x01e});
}

void CanonicalScancodeTests::canonicalFlagMustBeStrippedBeforeKeyState()
{
  // The flag intentionally sits outside KeyState's button table. Receivers
  // must strip it before forwarding a key through the translated path.
  const auto buttonMask = static_cast<KeyButton>(IKeyState::s_numButtons - 1);
  QCOMPARE(deskflow::scancode::kCanonicalFlag & buttonMask, 0);

  for (uint16_t virtualKey = 0; virtualKey <= 0x7f; ++virtualKey) {
    const auto scancode = deskflow::scancode::set1FromMacVirtualKey(virtualKey);
    if (!scancode.has_value()) {
      continue;
    }
    QVERIFY(deskflow::scancode::markCanonical(*scancode) >= IKeyState::s_numButtons);
    QCOMPARE(deskflow::scancode::markCanonical(*scancode) & buttonMask, *scancode);
    QCOMPARE(deskflow::scancode::stripCanonical(deskflow::scancode::markCanonical(*scancode)), *scancode);
  }
}

void CanonicalScancodeTests::rejectsNonSet1Scancodes()
{
  using deskflow::scancode::isSet1Scancode;
  QVERIFY(isSet1Scancode(0x01e));
  QVERIFY(isSet1Scancode(0x15c));
  QVERIFY(!isSet1Scancode(0x000));
  QVERIFY(!isSet1Scancode(0x100)); // extended prefix with no key byte
  QVERIFY(!isSet1Scancode(0x200)); // wider than Set-1 ever goes
  QVERIFY(!isSet1Scancode(deskflow::scancode::markCanonical(0x01e)));
}

QTEST_MAIN(CanonicalScancodeTests)
