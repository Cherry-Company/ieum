/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsMouseInput.h"

#include <QTest>

class MSWindowsMouseInputTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void mapsVirtualDesktopEdges()
  {
    QCOMPARE(deskflow::win32::normalizeAbsoluteMouseCoordinate(-1920, -1920, 3840), uint32_t{0});
    QCOMPARE(deskflow::win32::normalizeAbsoluteMouseCoordinate(1919, -1920, 3840), uint32_t{65535});
  }

  void mapsAndClampsVirtualDesktopCoordinates()
  {
    QCOMPARE(deskflow::win32::normalizeAbsoluteMouseCoordinate(0, -1920, 3840), uint32_t{32776});
    QCOMPARE(deskflow::win32::normalizeAbsoluteMouseCoordinate(-4000, -1920, 3840), uint32_t{0});
    QCOMPARE(deskflow::win32::normalizeAbsoluteMouseCoordinate(4000, -1920, 3840), uint32_t{65535});
  }

  void handlesDegenerateExtent()
  {
    QCOMPARE(deskflow::win32::normalizeAbsoluteMouseCoordinate(42, 0, 1), uint32_t{0});
    QCOMPARE(deskflow::win32::normalizeAbsoluteMouseCoordinate(42, 0, 0), uint32_t{0});
  }
};

QTEST_MAIN(MSWindowsMouseInputTests)

#include "MSWindowsMouseInputTests.moc"
