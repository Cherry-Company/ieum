/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/FullscreenCursorConfinement.h"

#include <QTest>

#include <vector>

using deskflow::fullscreen::Bounds;
using deskflow::fullscreen::CursorConfinement;

namespace {

constexpr Bounds desktop{-1920, 0, 1920, 1080};
constexpr Bounds game{0, 0, 1920, 1080};

struct ClipBackend
{
  Bounds current = desktop;
  std::vector<std::optional<Bounds>> requests;
  bool succeed = true;

  bool apply(const std::optional<Bounds> &bounds)
  {
    requests.push_back(bounds);
    if (succeed) {
      current = bounds.value_or(desktop);
    }
    return succeed;
  }

  bool update(CursorConfinement &confinement, std::optional<Bounds> target)
  {
    return confinement.update(target, current, desktop, [this](const auto &bounds) { return apply(bounds); });
  }
};

} // namespace

class FullscreenCursorConfinementTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void confinesAndReleasesOwnRestriction()
  {
    CursorConfinement confinement;
    ClipBackend backend;
    QVERIFY(backend.update(confinement, game));
    QVERIFY(backend.current == game);
    QVERIFY(confinement.owns(backend.current));
    QVERIFY(backend.update(confinement, game));
    QCOMPARE(backend.requests.size(), size_t{1});

    QVERIFY(backend.update(confinement, std::nullopt));
    QVERIFY(backend.current == desktop);
    QVERIFY(!confinement.owns(backend.current));
    QCOMPARE(backend.requests.size(), size_t{2});
    QVERIFY(!backend.requests.back());
  }

  void preservesGameCapture()
  {
    for (const Bounds clip : {game, Bounds{960, 540, 961, 541}, Bounds{100, 100, 1800, 1000}}) {
      CursorConfinement confinement;
      ClipBackend backend;
      backend.current = clip;
      QVERIFY(backend.update(confinement, game));
      QVERIFY(!confinement.owns(backend.current));
      QVERIFY(backend.update(confinement, std::nullopt));
      QVERIFY(backend.current == clip);
      QVERIFY(backend.requests.empty());
    }
  }

  void leavesNewCaptureAloneAfterFocusChange()
  {
    CursorConfinement confinement;
    ClipBackend backend;
    QVERIFY(backend.update(confinement, game));
    const Bounds otherCapture{-1000, 500, -999, 501};
    backend.current = otherCapture;

    QVERIFY(backend.update(confinement, std::nullopt));
    QVERIFY(backend.current == otherCapture);
    QCOMPARE(backend.requests.size(), size_t{1});
    QVERIFY(!confinement.owns(game));
  }

  void restoresPreexistingRestriction()
  {
    CursorConfinement confinement;
    ClipBackend backend;
    const Bounds previous{-1500, 0, 1900, 1080};
    backend.current = previous;
    QVERIFY(backend.update(confinement, game));
    QVERIFY(backend.update(confinement, std::nullopt));
    QVERIFY(backend.current == previous);
  }

  void movesRestrictionToAnotherDisplay()
  {
    CursorConfinement confinement;
    ClipBackend backend;
    const Bounds secondGame{-1920, 0, 0, 1080};
    QVERIFY(backend.update(confinement, game));
    QVERIFY(backend.update(confinement, secondGame));
    QVERIFY(backend.current == secondGame);
    QVERIFY(confinement.owns(secondGame));
    QVERIFY(backend.update(confinement, std::nullopt));
    QVERIFY(backend.current == desktop);
  }

  void retriesFailedApplyAndRelease()
  {
    CursorConfinement confinement;
    ClipBackend backend;
    backend.succeed = false;
    QVERIFY(!backend.update(confinement, game));
    QVERIFY(!confinement.owns(game));

    backend.succeed = true;
    QVERIFY(backend.update(confinement, game));
    backend.succeed = false;
    QVERIFY(!backend.update(confinement, std::nullopt));
    QVERIFY(confinement.owns(game));
    backend.succeed = true;
    QVERIFY(backend.update(confinement, std::nullopt));
    QVERIFY(!confinement.owns(game));
    QVERIFY(backend.current == desktop);
  }

  void releasesOwnClipAfterResolutionChange()
  {
    CursorConfinement confinement;
    ClipBackend backend;
    QVERIFY(backend.update(confinement, game));
    const Bounds smallerDesktop{-1920, 0, 1280, 720};
    const Bounds adjustedClip{0, 0, 1280, 720};
    QVERIFY(confinement.owns(adjustedClip, smallerDesktop));
    std::optional<Bounds> restored = game;
    QVERIFY(confinement.update(std::nullopt, adjustedClip, smallerDesktop, [&restored](const auto &bounds) {
      restored = bounds;
      return true;
    }));
    QVERIFY(!restored);
    QVERIFY(!confinement.owns(game));
  }

  void singleDisplayNeedsNoRestriction()
  {
    CursorConfinement confinement;
    size_t calls = 0;
    QVERIFY(confinement.update(game, game, game, [&calls](const auto &) {
      ++calls;
      return true;
    }));
    QCOMPARE(calls, size_t{0});
    QVERIFY(!confinement.owns(game));
  }
};

QTEST_MAIN(FullscreenCursorConfinementTests)

#include "FullscreenCursorConfinementTests.moc"
