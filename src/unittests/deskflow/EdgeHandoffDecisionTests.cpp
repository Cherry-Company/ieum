/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/EdgeHandoffDecision.h"

#include <QTest>

#include <chrono>
#include <optional>

using namespace std::chrono_literals;
using namespace deskflow::filetransfer;

namespace {

const EdgeTarget kRightTarget{Direction::Right, "target"};
const EdgeTarget kLeftTarget{Direction::Left, "other"};

EdgeHandoffDecision::TimePoint at(const std::chrono::milliseconds offset)
{
  return EdgeHandoffDecision::TimePoint{} + offset;
}

} // namespace

class EdgeHandoffDecisionTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void startsAsIdle()
  {
    const EdgeHandoffDecision decision;

    QCOMPARE(decision.state(), EdgeHandoffState::Idle);
    QVERIFY(!decision.target().has_value());
  }

  void armsOnlyAtTheDwellBoundary()
  {
    EdgeHandoffDecision decision;

    QCOMPARE(decision.observe(kRightTarget, at(0ms)), EdgeHandoffState::Candidate);
    QCOMPARE(decision.observe(kRightTarget, at(249ms)), EdgeHandoffState::Candidate);
    QCOMPARE(decision.observe(kRightTarget, at(250ms)), EdgeHandoffState::Armed);
    QCOMPARE(decision.target(), std::optional<EdgeTarget>{kRightTarget});
  }

  void movingAwayDisarms()
  {
    EdgeHandoffDecision decision;
    QCOMPARE(decision.observe(kRightTarget, at(0ms)), EdgeHandoffState::Candidate);
    QCOMPARE(decision.observe(kRightTarget, at(250ms)), EdgeHandoffState::Armed);

    QCOMPARE(decision.observe(std::nullopt, at(251ms)), EdgeHandoffState::Idle);
    QCOMPARE(decision.state(), EdgeHandoffState::Idle);
    QVERIFY(!decision.target().has_value());
  }

  void changingTargetRestartsDwell()
  {
    EdgeHandoffDecision decision;
    QCOMPARE(decision.observe(kRightTarget, at(0ms)), EdgeHandoffState::Candidate);

    QCOMPARE(decision.observe(kLeftTarget, at(200ms)), EdgeHandoffState::Candidate);
    QCOMPARE(decision.observe(kLeftTarget, at(449ms)), EdgeHandoffState::Candidate);
    QCOMPARE(decision.observe(kLeftTarget, at(450ms)), EdgeHandoffState::Armed);
    QCOMPARE(decision.target(), std::optional<EdgeTarget>{kLeftTarget});
  }

  void invalidTargetsNeverArm_data()
  {
    QTest::addColumn<int>("direction");
    QTest::addColumn<QString>("screen");

    QTest::newRow("no-direction") << static_cast<int>(Direction::NoDirection) << QStringLiteral("target");
    QTest::newRow("above-bottom") << 5 << QStringLiteral("target");
    QTest::newRow("byte-maximum") << 255 << QStringLiteral("target");
    QTest::newRow("empty-screen") << static_cast<int>(Direction::Left) << QString{};
  }

  void invalidTargetsNeverArm()
  {
    QFETCH(int, direction);
    QFETCH(QString, screen);

    EdgeHandoffDecision decision;
    const EdgeTarget invalid{static_cast<Direction>(direction), screen.toStdString()};

    QCOMPARE(decision.observe(invalid, at(0ms)), EdgeHandoffState::Idle);
    QCOMPARE(decision.observe(invalid, at(1000ms)), EdgeHandoffState::Idle);
    QVERIFY(!decision.target().has_value());
  }

  void releaseBeforeArmedReturnsNothingAndResets()
  {
    EdgeHandoffDecision decision;
    QCOMPARE(decision.observe(kRightTarget, at(0ms)), EdgeHandoffState::Candidate);
    QCOMPARE(decision.observe(kRightTarget, at(249ms)), EdgeHandoffState::Candidate);

    const auto released = decision.release();

    QVERIFY(!released.has_value());
    QCOMPARE(decision.state(), EdgeHandoffState::Idle);
    QVERIFY(!decision.target().has_value());
  }

  void releaseWhenArmedReturnsTargetAndResets()
  {
    EdgeHandoffDecision decision;
    QCOMPARE(decision.observe(kRightTarget, at(0ms)), EdgeHandoffState::Candidate);
    QCOMPARE(decision.observe(kRightTarget, at(250ms)), EdgeHandoffState::Armed);

    const auto released = decision.release();

    QVERIFY(released.has_value());
    QCOMPARE(*released, kRightTarget);
    QCOMPARE(decision.state(), EdgeHandoffState::Idle);
    QVERIFY(!decision.target().has_value());
  }

  void backwardsClockRestartsDwell()
  {
    EdgeHandoffDecision decision;
    QCOMPARE(decision.observe(kRightTarget, at(100ms)), EdgeHandoffState::Candidate);

    QCOMPARE(decision.observe(kRightTarget, at(50ms)), EdgeHandoffState::Candidate);
    QCOMPARE(decision.observe(kRightTarget, at(299ms)), EdgeHandoffState::Candidate);
    QCOMPARE(decision.observe(kRightTarget, at(300ms)), EdgeHandoffState::Armed);
  }

  void customDwellIsHonored()
  {
    EdgeHandoffDecision tenMilliseconds{10ms};
    QCOMPARE(tenMilliseconds.observe(kRightTarget, at(0ms)), EdgeHandoffState::Candidate);
    QCOMPARE(tenMilliseconds.observe(kRightTarget, at(9ms)), EdgeHandoffState::Candidate);
    QCOMPARE(tenMilliseconds.observe(kRightTarget, at(10ms)), EdgeHandoffState::Armed);

    EdgeHandoffDecision zeroDwell{0ms};
    QCOMPARE(zeroDwell.observe(kRightTarget, at(0ms)), EdgeHandoffState::Armed);

    EdgeHandoffDecision negativeDwell{-1ms};
    QCOMPARE(negativeDwell.observe(kRightTarget, at(0ms)), EdgeHandoffState::Armed);
  }
};

QTEST_GUILESS_MAIN(EdgeHandoffDecisionTests)

#include "EdgeHandoffDecisionTests.moc"
