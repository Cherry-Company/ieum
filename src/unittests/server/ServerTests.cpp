/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerTests.h"

#include "common/FullscreenGeometry.h"
#include "server/CursorTransform.h"
#include "server/Server.h"

void ServerTests::SwitchToScreenInfo_alloc_screen()
{
  auto actual = new Server::SwitchToScreenInfo("test");
  QCOMPARE(actual->m_screen, "test");
  delete actual;
}

void ServerTests::KeyboardBroadcastInfo_alloc_stateAndSceens()
{
  auto info = new Server::KeyboardBroadcastInfo(Server::KeyboardBroadcastInfo::State::kOn, "test");
  QCOMPARE(info->m_state, Server::KeyboardBroadcastInfo::State::kOn);
  QCOMPARE(info->m_screens, "test");
  delete info;
}

void ServerTests::cursorTransform_clampsOutOfBounds()
{
  using namespace deskflow::server::cursor;
  QCOMPARE(clampCoordinate(-500, -100, 200), -100);
  QCOMPARE(clampCoordinate(500, -100, 200), 99);
  QCOMPARE(fromFraction(-1.0f, -100, 200), -100);
  QCOMPARE(fromFraction(2.0f, -100, 200), 99);
}

void ServerTests::cursorTransform_mapsBetweenDifferentResolutions()
{
  using namespace deskflow::server::cursor;
  const auto sourceFraction = toFraction(539, 0, 1080);
  QCOMPARE(fromFraction(sourceFraction, 0, 2160), 1079);
}

void ServerTests::cursorTransform_roundTripsMixedOrigins()
{
  using namespace deskflow::server::cursor;
  for (int coordinate = -1080; coordinate < 840; coordinate += 31) {
    const auto mapped = fromFraction(toFraction(coordinate, -1080, 1920), 0, 2160);
    const auto roundTrip = fromFraction(toFraction(mapped, 0, 2160), -1080, 1920);
    QVERIFY(std::abs(roundTrip - coordinate) <= 1);
  }
  QCOMPARE(fromFraction(std::numeric_limits<float>::quiet_NaN(), -100, 200), 0);
}

void ServerTests::cursorTransform_preservesSubpixelMotion()
{
  using namespace deskflow::server::cursor;
  double remainder = 0.0;
  int total = 0;
  for (int i = 0; i < 4; ++i) {
    total += scaleDelta(1, 25, remainder);
  }
  QCOMPARE(total, 1);

  remainder = 0.0;
  total = 0;
  for (int i = 0; i < 4; ++i) {
    total += scaleDelta(-1, 25, remainder);
  }
  QCOMPARE(total, -1);

  QCOMPARE(scaleDelta(std::numeric_limits<int32_t>::max(), 400, remainder), std::numeric_limits<int32_t>::max());
  QCOMPARE(remainder, 0.0);
}

void ServerTests::fullscreenGeometry_distinguishesFullscreenFromMaximized()
{
  using deskflow::fullscreen::Bounds;
  using deskflow::fullscreen::coversDisplay;
  const Bounds display{-1920.0, 0.0, 0.0, 1080.0};

  QVERIFY(coversDisplay({-1924.0, -4.0, 4.0, 1084.0}, display));
  QVERIFY(coversDisplay({-1916.0, 4.0, -4.0, 1076.0}, display));
  QVERIFY(!coversDisplay({-1920.0, 0.0, 0.0, 1040.0}, display));
  QVERIFY(!coversDisplay({-1800.0, 0.0, 0.0, 1080.0}, display));
}

QTEST_MAIN(ServerTests)
