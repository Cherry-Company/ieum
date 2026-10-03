/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerTests.h"

#include "../shared/FakePlatformScreen.h"
#include "base/EventQueue.h"
#include "common/FullscreenGeometry.h"
#include "deskflow/AppUtil.h"
#include "deskflow/Screen.h"
#include "io/IStream.h"
#include "server/ClientProxy1_11.h"
#include "server/CursorTransform.h"
#include "server/PrimaryClient.h"
#include "server/Server.h"

namespace {

class TestAppUtil : public AppUtil
{
public:
  int run() override
  {
    return 0;
  }
  void startNode() override
  {
  }
  std::vector<std::string> getKeyboardLayoutList() override
  {
    return {"en"};
  }
  std::string getCurrentLanguageCode() override
  {
    return "en";
  }
};

class SinkStream : public deskflow::IStream
{
public:
  void close() override
  {
  }
  uint32_t read(void *, uint32_t) override
  {
    return 0;
  }
  void write(const void *, uint32_t) override
  {
  }
  void flush() override
  {
  }
  void shutdownInput() override
  {
  }
  void shutdownOutput() override
  {
  }
  void *getEventTarget() const override
  {
    return const_cast<SinkStream *>(this);
  }
  bool isReady() const override
  {
    return false;
  }
  uint32_t getSize() const override
  {
    return 0;
  }
};

class CursorClient : public ClientProxy1_11
{
public:
  CursorClient(IEventQueue *events, Server *server) : ClientProxy1_11("secondary", new SinkStream, server, events)
  {
  }
  deskflow::DisplayGeometry shape{-1920, -1080, 3840, 2160};
  deskflow::DisplayLayout displays{{-1920, -1080, 1920, 1080}, {0, 0, 1920, 1080}};
  QPoint position{100, -500};
  int entries = 0;

  void getShape(int32_t &x, int32_t &y, int32_t &width, int32_t &height) const override
  {
    x = shape.x;
    y = shape.y;
    width = shape.width;
    height = shape.height;
  }
  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = position.x();
    y = position.y();
  }
  deskflow::DisplayLayout getDisplayLayout() const override
  {
    return displays;
  }
  void enter(int32_t x, int32_t y, uint32_t, KeyModifierMask, bool) override
  {
    position = {x, y};
    ++entries;
  }
  void mouseMove(int32_t x, int32_t y) override
  {
    position = {x, y};
  }
};

void dispatchMotion(EventQueue &events, EventTypes type, void *target, int32_t x, int32_t y)
{
  Event event(type, target, IPrimaryScreen::MotionInfo::alloc(x, y));
  events.dispatchEvent(event);
  Event::deleteData(event);
}

} // namespace

void ServerTests::initTestCase()
{
  m_arch.init();
  static TestAppUtil appUtil;
}

void ServerTests::screenSwitch_tracksPhysicalPositionAfterGapEntry()
{
  EventQueue events;
  deskflow::server::Config config(&events);
  QVERIFY(config.addScreen("primary"));
  QVERIFY(config.addScreen("secondary"));
  QVERIFY(config.addOption("", kOptionClipboardSharing, 0));
  QVERIFY(config.addOption("", kOptionDisableLockToScreen, 1));
  auto *platform = new FakePlatformScreen(&events, true);
  deskflow::Screen screen(platform, &events);
  PrimaryClient primary("primary", &screen);
  Server server(config, &primary, &screen, &events);
  auto *client = new CursorClient(&events, &server);
  server.adoptClient(client);

  Server::SwitchToScreenInfo jumpInfo("secondary");
  Event jump(EventTypes::ServerSwitchToScreen, config.getInputFilter(), &jumpInfo, Event::EventFlags::DontFreeData);
  QVERIFY(events.dispatchEvent(jump));
  Event::deleteData(jump);
  QCOMPARE(client->entries, 1);
  QCOMPARE(client->position, QPoint(-1, -500));

  for (int i = 0; i < 100; ++i) {
    dispatchMotion(events, EventTypes::PrimaryScreenMotionOnSecondary, platform, 10, 0);
    QCOMPARE(client->position, QPoint(-1, -500));
  }
  dispatchMotion(events, EventTypes::PrimaryScreenMotionOnSecondary, platform, -10, 0);
  QCOMPARE(client->position, QPoint(-11, -500));
}

void ServerTests::screenSwitch_fullscreenCancelsPendingSwitch()
{
  EventQueue events;
  deskflow::server::Config config(&events);
  QVERIFY(config.addScreen("primary"));
  QVERIFY(config.addScreen("secondary"));
  QVERIFY(config.connect("primary", Direction::Right, 0.0f, 1.0f, "secondary", 0.0f, 1.0f));
  QVERIFY(config.addOption("", kOptionClipboardSharing, 0));
  QVERIFY(config.addOption("", kOptionDisableLockToScreen, 1));
  QVERIFY(config.addOption("", kOptionScreenSwitchDelay, 200));
  auto *platform = new FakePlatformScreen(&events, true);
  deskflow::Screen screen(platform, &events);
  PrimaryClient primary("primary", &screen);
  Server server(config, &primary, &screen, &events);
  auto *client = new CursorClient(&events, &server);
  server.adoptClient(client);

  dispatchMotion(events, EventTypes::PrimaryScreenMotionOnPrimary, platform, 1919, 500);
  QCOMPARE(client->entries, 0);
  platform->fullscreen = true;
  dispatchMotion(events, EventTypes::PrimaryScreenMotionOnPrimary, platform, 1000, 500);
  dispatchMotion(events, EventTypes::PrimaryScreenMotionOnPrimary, platform, 1919, 500);
  QCOMPARE(client->entries, 0);

  platform->fullscreen = false;
  dispatchMotion(events, EventTypes::PrimaryScreenMotionOnPrimary, platform, 1919, 500);
  QCOMPARE(client->entries, 0);
  QTest::qWait(1550);
  // A queued event from the cancelled timer must not switch or dereference
  // the cleared destination once the automatic capture grace period expires.
  QVERIFY(events.dispatchEvent(Event(EventTypes::Timer, &server)));
  QCOMPARE(client->entries, 0);

  dispatchMotion(events, EventTypes::PrimaryScreenMotionOnPrimary, platform, 1919, 500);
  QVERIFY(events.dispatchEvent(Event(EventTypes::Timer, &server)));
  QCOMPARE(client->entries, 1);
}

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

void ServerTests::cursorTransform_mapsPhysicalEdgeDisplays()
{
  using deskflow::DisplayGeometry;
  using deskflow::DisplayLayout;
  using deskflow::server::cursor::mapAcrossDisplayEdges;

  const DisplayGeometry windowsDesktop{0, 0, 4000, 2560};
  const DisplayLayout windowsDisplays{{0, 0, 1440, 2560}, {1440, 0, 2560, 1440}};
  const DisplayGeometry macDesktop{0, 0, 2560, 1440};
  const DisplayLayout macDisplays{macDesktop};

  // The old aggregate mapping produced 405. The two touching 1440 px edges
  // should preserve y=720 in both directions.
  const auto toMac =
      mapAcrossDisplayEdges(windowsDisplays, windowsDesktop, macDisplays, macDesktop, Direction::Right, 720, 405);
  QVERIFY(toMac.has_value());
  QCOMPARE(*toMac, 720);

  const auto toWindows =
      mapAcrossDisplayEdges(macDisplays, macDesktop, windowsDisplays, windowsDesktop, Direction::Left, 720, 1280);
  QVERIFY(toWindows.has_value());
  QCOMPARE(*toWindows, 720);

  const DisplayLayout invalidLayout{{0, 0, 1440, 2560}};
  QVERIFY(!mapAcrossDisplayEdges(invalidLayout, windowsDesktop, macDisplays, macDesktop, Direction::Right, 720, 405)
               .has_value());
}

void ServerTests::cursorTransform_mapsStackedEdgeDisplaysContinuously()
{
  using deskflow::DisplayGeometry;
  using deskflow::DisplayLayout;
  using deskflow::server::cursor::mapAcrossDisplayEdges;

  const DisplayGeometry sourceDesktop{-2560, -720, 5120, 2160};
  const DisplayLayout sourceDisplays{{-2560, 0, 2560, 1440}, {0, -720, 2560, 2160}};
  const DisplayGeometry destinationDesktop{0, -1200, 1920, 2400};
  const DisplayLayout destinationDisplays{{0, -1200, 1920, 1200}, {0, 0, 1920, 1200}};

  QCOMPARE(
      *mapAcrossDisplayEdges(
          sourceDisplays, sourceDesktop, destinationDisplays, destinationDesktop, Direction::Right, -720, -1200
      ),
      -1200
  );
  QCOMPARE(
      *mapAcrossDisplayEdges(
          sourceDisplays, sourceDesktop, destinationDisplays, destinationDesktop, Direction::Right, 359, -1
      ),
      -1
  );
  QCOMPARE(
      *mapAcrossDisplayEdges(
          sourceDisplays, sourceDesktop, destinationDisplays, destinationDesktop, Direction::Right, 360, 0
      ),
      0
  );
  QCOMPARE(
      *mapAcrossDisplayEdges(
          sourceDisplays, sourceDesktop, destinationDisplays, destinationDesktop, Direction::Right, 1439, 1199
      ),
      1199
  );

  int previous = std::numeric_limits<int>::min();
  for (int y = -720; y < 1440; ++y) {
    const auto mapped = mapAcrossDisplayEdges(
        sourceDisplays, sourceDesktop, destinationDisplays, destinationDesktop, Direction::Right, y, 0
    );
    QVERIFY(mapped.has_value());
    QVERIFY(*mapped >= previous);
    previous = *mapped;
  }
}

void ServerTests::cursorTransform_collapsesPhysicalEdgeGaps()
{
  using deskflow::DisplayGeometry;
  using deskflow::DisplayLayout;
  using deskflow::server::cursor::mapAcrossDisplayEdges;

  const DisplayGeometry sourceDesktop{0, -1200, 3840, 3000};
  const DisplayLayout sourceDisplays{{1920, -1200, 1920, 1000}, {1920, 200, 1920, 1600}, {0, 0, 1920, 1080}};
  const DisplayGeometry destinationDesktop{-1920, 0, 1920, 2000};
  const DisplayLayout destinationDisplays{destinationDesktop};

  const auto beforeGap = mapAcrossDisplayEdges(
      sourceDisplays, sourceDesktop, destinationDisplays, destinationDesktop, Direction::Right, -201, 0
  );
  const auto afterGap = mapAcrossDisplayEdges(
      sourceDisplays, sourceDesktop, destinationDisplays, destinationDesktop, Direction::Right, 200, 0
  );
  QVERIFY(beforeGap.has_value());
  QVERIFY(afterGap.has_value());
  QVERIFY(*afterGap - *beforeGap <= 1);

  const auto inGap = mapAcrossDisplayEdges(
      sourceDisplays, sourceDesktop, destinationDisplays, destinationDesktop, Direction::Right, 0, 0
  );
  QVERIFY(inGap.has_value());
  QVERIFY(std::abs(*inGap - *beforeGap) <= 1 || std::abs(*inGap - *afterGap) <= 1);
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

void ServerTests::cursorTransform_projectsDesktopGapsOntoPhysicalDisplays()
{
  using deskflow::server::cursor::clampToDisplayLayout;
  const deskflow::DisplayGeometry desktop{-1920, -1080, 3840, 2160};
  const deskflow::DisplayLayout displays{{-1920, -1080, 1920, 1080}, {0, 0, 1920, 1080}};

  int32_t x = 100;
  int32_t y = -500;
  clampToDisplayLayout(displays, desktop, x, y);
  QCOMPARE(x, -1);
  QCOMPARE(y, -500);

  x = -500;
  y = 100;
  clampToDisplayLayout(displays, desktop, x, y);
  QCOMPARE(x, -500);
  QCOMPARE(y, -1);

  // Repeated motion into a desktop gap must stay at the actual display edge
  // instead of accumulating an invisible offset before the return crossing.
  for (int i = 0; i < 100; ++i) {
    y += 10;
    clampToDisplayLayout(displays, desktop, x, y);
    QCOMPARE(x, -500);
    QCOMPARE(y, -1);
  }
}

void ServerTests::cursorTransform_preservesValidDisplayPositions()
{
  using deskflow::server::cursor::clampToDisplayLayout;
  const deskflow::DisplayGeometry desktop{-1920, -1080, 3840, 2160};
  const deskflow::DisplayLayout displays{{-1920, -1080, 1920, 1080}, {0, 0, 1920, 1080}};

  int32_t x = -400;
  int32_t y = -300;
  clampToDisplayLayout(displays, desktop, x, y);
  QCOMPARE(x, -400);
  QCOMPARE(y, -300);

  x = 3000;
  y = 500;
  clampToDisplayLayout(displays, desktop, x, y);
  QCOMPARE(x, 1919);
  QCOMPARE(y, 500);

  // Legacy peers have only their aggregate desktop; keep that fallback.
  x = 100;
  y = -500;
  clampToDisplayLayout({}, desktop, x, y);
  QCOMPARE(x, 100);
  QCOMPARE(y, -500);
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

void ServerTests::fullscreenGeometry_detectsPointerCapture()
{
  using deskflow::fullscreen::Bounds;
  using deskflow::fullscreen::pointerIsConfinedToDisplay;

  const Bounds desktop{0.0, 0.0, 3840.0, 1080.0};
  const Bounds gameDisplay{1920.0, 0.0, 3840.0, 1080.0};
  QVERIFY(pointerIsConfinedToDisplay(gameDisplay, gameDisplay, desktop));
  QVERIFY(pointerIsConfinedToDisplay({2100.0, 100.0, 3700.0, 1000.0}, gameDisplay, desktop));
  QVERIFY(!pointerIsConfinedToDisplay(desktop, gameDisplay, desktop));
  QVERIFY(pointerIsConfinedToDisplay({2000.0, 100.0, 2030.0, 130.0}, gameDisplay, desktop));
  // FPS games may pin the pointer to a single pixel while consuming raw input.
  QVERIFY(pointerIsConfinedToDisplay({2880.0, 540.0, 2881.0, 541.0}, gameDisplay, desktop));
  QVERIFY(!pointerIsConfinedToDisplay({2880.0, 540.0, 2880.0, 540.0}, gameDisplay, desktop));
}

QTEST_MAIN(ServerTests)
