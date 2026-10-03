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

  void targetsEveryPixelExactly()
  {
    // SendInput decodes absolute coordinates using 65536 buckets over the
    // desktop extent, rather than interpolating between the endpoint pixels.
    for (const int32_t extent : {1080, 1440, 2160, 3840, 7680}) {
      for (const int32_t origin : {-3840, 0, 1920}) {
        for (int32_t pixel = 0; pixel < extent; ++pixel) {
          const auto absolute = deskflow::win32::normalizeAbsoluteMouseCoordinate(origin + pixel, origin, extent);
          const auto delivered = origin + static_cast<int32_t>(static_cast<int64_t>(absolute) * extent / 65536);
          QCOMPARE(delivered, origin + pixel);
        }
      }
    }
  }
};

QTEST_MAIN(MSWindowsMouseInputTests)

#include "MSWindowsMouseInputTests.moc"
