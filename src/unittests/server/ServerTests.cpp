/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerTests.h"

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

QTEST_MAIN(ServerTests)
