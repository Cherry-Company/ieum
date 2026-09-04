/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/win32/WindowsFileTransferDropBroker.h"

#include <QSignalSpy>
#include <QTest>

#include <filesystem>
#include <memory>
#include <vector>

using namespace deskflow::filetransfer;
using deskflow::gui::WindowsFileTransferDropBroker;
using deskflow::gui::WindowsFileTransferDropBrokerDependencies;
using deskflow::gui::WindowsFileTransferDropHost;

namespace {

class FakeHost final : public WindowsFileTransferDropHost
{
public:
  bool configure(const MSWindowsEdgeDropBounds &bounds, std::uint32_t activeSides) override
  {
    configurations.emplace_back(bounds, activeSides);
    return true;
  }

  void clear() noexcept override
  {
    ++clearCount;
  }

  std::vector<std::pair<MSWindowsEdgeDropBounds, std::uint32_t>> configurations;
  int clearCount = 0;
};

struct BrokerHarness
{
  WindowsFileTransferDropBrokerDependencies dependencies()
  {
    WindowsFileTransferDropBrokerDependencies result;
    result.oleInitialize = [this] {
      ++oleInitializeCount;
      return oleResult;
    };
    result.oleUninitialize = [this] { ++oleUninitializeCount; };
    result.createHost = [this](MSWindowsEdgeDropHostCallbacks callbacks) {
      hostCallbacks = std::move(callbacks);
      auto instance = std::make_unique<FakeHost>();
      host = instance.get();
      return instance;
    };
    result.virtualDesktopBounds = [this] { return bounds; };
    return result;
  }

  HRESULT oleResult = S_OK;
  int oleInitializeCount = 0;
  int oleUninitializeCount = 0;
  MSWindowsEdgeDropBounds bounds{-1920, -200, 3840, 1280};
  FakeHost *host = nullptr;
  MSWindowsEdgeDropHostCallbacks hostCallbacks;
};

constexpr auto kLeftMask = static_cast<std::uint32_t>(DirectionMask::LeftMask);
constexpr auto kRightMask = static_cast<std::uint32_t>(DirectionMask::RightMask);

} // namespace

class WindowsFileTransferDropBrokerTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void failsClosedWhenOleIsUnavailable();
  void balancesSuccessfulOleInitialization();
  void configuresValidSidesAndClearsZeroSynchronously();
  void coalescesGeometryRefreshes();
  void deliversExactlyOneEncodedUnicodeDrop();
};

void WindowsFileTransferDropBrokerTests::failsClosedWhenOleIsUnavailable()
{
  BrokerHarness harness;
  harness.oleResult = RPC_E_CHANGED_MODE;
  {
    WindowsFileTransferDropBroker broker({}, harness.dependencies());
    QVERIFY(!broker.isAvailable());
    QVERIFY(!broker.setActiveSides(kLeftMask));
    QVERIFY(harness.host == nullptr);
  }
  QCOMPARE(harness.oleInitializeCount, 1);
  QCOMPARE(harness.oleUninitializeCount, 0);
}

void WindowsFileTransferDropBrokerTests::balancesSuccessfulOleInitialization()
{
  BrokerHarness harness;
  harness.oleResult = S_FALSE;
  {
    WindowsFileTransferDropBroker broker({}, harness.dependencies());
    QVERIFY(broker.isAvailable());
    QCOMPARE(harness.oleUninitializeCount, 0);
  }
  QCOMPARE(harness.oleInitializeCount, 1);
  QCOMPARE(harness.oleUninitializeCount, 1);
}

void WindowsFileTransferDropBrokerTests::configuresValidSidesAndClearsZeroSynchronously()
{
  BrokerHarness harness;
  WindowsFileTransferDropBroker broker({}, harness.dependencies());
  QVERIFY(broker.setActiveSides(kLeftMask | kRightMask));
  QCOMPARE(harness.host->configurations.size(), std::size_t{1});
  QCOMPARE(harness.host->configurations.front().first, harness.bounds);
  QCOMPARE(harness.host->configurations.front().second, kLeftMask | kRightMask);

  QVERIFY(broker.setActiveSides(0));
  QCOMPARE(harness.host->clearCount, 1);
  QCOMPARE(broker.activeSides(), 0U);
}

void WindowsFileTransferDropBrokerTests::coalescesGeometryRefreshes()
{
  BrokerHarness harness;
  WindowsFileTransferDropBroker broker({}, harness.dependencies());
  QVERIFY(broker.setActiveSides(kLeftMask));
  QCOMPARE(harness.host->configurations.size(), std::size_t{1});

  harness.bounds = {-2560, -400, 5120, 1800};
  QVERIFY(QMetaObject::invokeMethod(&broker, "scheduleGeometryRefresh", Qt::DirectConnection));
  QVERIFY(QMetaObject::invokeMethod(&broker, "scheduleGeometryRefresh", Qt::DirectConnection));
  QVERIFY(QMetaObject::invokeMethod(&broker, "scheduleGeometryRefresh", Qt::DirectConnection));
  QTRY_COMPARE_WITH_TIMEOUT(harness.host->configurations.size(), std::size_t{2}, 1000);
  QCOMPARE(harness.host->configurations.back().first, harness.bounds);
}

void WindowsFileTransferDropBrokerTests::deliversExactlyOneEncodedUnicodeDrop()
{
  BrokerHarness harness;
  QStringList delivered;
  WindowsFileTransferDropBroker broker(
      [&delivered](const QString &encoded) { delivered.append(encoded); }, harness.dependencies()
  );
  QVERIFY(harness.hostCallbacks.handoff);

  harness.hostCallbacks.handoff(
      {.direction = Direction::Left,
       .x = -12,
       .y = 34,
       .paths =
           {std::filesystem::path(L"C:\\Users\\테스트 사용자\\edge one.txt"),
            std::filesystem::path(L"D:\\space path\\파일 둘.bin")}}
  );

  QCOMPARE(delivered.size(), 1);
  QCOMPARE(
      delivered.constFirst(), QStringLiteral(
                                  "eyJ2IjoxLCJkIjoxLCJ4IjotMTIsInkiOjM0LCJwIjpbIkM6XFxVc2Vyc1xc7YWM7Iqk7Yq4IOyCrOyaqeye"
                                  "kFxcZWRnZSBvbmUudHh0IiwiRDpcXHNwYWNlIHBhdGhcXO2MjOydvCDrkZguYmluIl19"
                              )
  );
}

QTEST_MAIN(WindowsFileTransferDropBrokerTests)

#include "WindowsFileTransferDropBrokerTests.moc"
