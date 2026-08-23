/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServiceStartCoordinatorTests.h"

#include "gui/core/ServiceStartCoordinator.h"

#include <QSignalSpy>

using deskflow::gui::ServiceStartCoordinator;

void ServiceStartCoordinatorTests::waitsForConfigStartAndCoreReadiness()
{
  ServiceStartCoordinator coordinator(nullptr, 1000);
  QSignalSpy configSpy(&coordinator, &ServiceStartCoordinator::configRequested);
  QSignalSpy startSpy(&coordinator, &ServiceStartCoordinator::startRequested);
  QSignalSpy coreSpy(&coordinator, &ServiceStartCoordinator::coreConnectionRequested);
  QSignalSpy successSpy(&coordinator, &ServiceStartCoordinator::succeeded);
  QSignalSpy failureSpy(&coordinator, &ServiceStartCoordinator::failed);
  constexpr quint64 generation = 41;

  coordinator.begin(generation);
  QVERIFY(coordinator.isActive());
  QVERIFY(configSpy.isEmpty());

  coordinator.daemonConnected(generation);
  QCOMPARE(configSpy.count(), 1);
  QVERIFY(startSpy.isEmpty());

  coordinator.configResult(generation, true);
  QCOMPARE(startSpy.count(), 1);
  QVERIFY(coreSpy.isEmpty());

  coordinator.startResult(generation, true);
  QCOMPARE(coreSpy.count(), 1);
  QVERIFY(successSpy.isEmpty());

  coordinator.coreConnected(generation);
  QCOMPARE(successSpy.count(), 1);
  QVERIFY(failureSpy.isEmpty());
  QVERIFY(!coordinator.isActive());
}

void ServiceStartCoordinatorTests::failsWhenConfigIsRejected()
{
  ServiceStartCoordinator coordinator(nullptr, 1000);
  QSignalSpy startSpy(&coordinator, &ServiceStartCoordinator::startRequested);
  QSignalSpy failureSpy(&coordinator, &ServiceStartCoordinator::failed);
  constexpr quint64 generation = 42;

  coordinator.begin(generation);
  coordinator.daemonConnected(generation);
  coordinator.configResult(generation, false);

  QCOMPARE(failureSpy.count(), 1);
  QVERIFY(startSpy.isEmpty());
  QVERIFY(!coordinator.isActive());
}

void ServiceStartCoordinatorTests::failsWhenSpawnIsRejected()
{
  ServiceStartCoordinator coordinator(nullptr, 1000);
  QSignalSpy coreSpy(&coordinator, &ServiceStartCoordinator::coreConnectionRequested);
  QSignalSpy failureSpy(&coordinator, &ServiceStartCoordinator::failed);
  constexpr quint64 generation = 43;

  coordinator.begin(generation);
  coordinator.daemonConnected(generation);
  coordinator.configResult(generation, true);
  coordinator.startResult(generation, false);

  QCOMPARE(failureSpy.count(), 1);
  QVERIFY(coreSpy.isEmpty());
  QVERIFY(!coordinator.isActive());
}

void ServiceStartCoordinatorTests::failsWhenDaemonDisconnects()
{
  ServiceStartCoordinator coordinator(nullptr, 1000);
  QSignalSpy failureSpy(&coordinator, &ServiceStartCoordinator::failed);
  constexpr quint64 generation = 46;

  coordinator.begin(generation);
  coordinator.transportFailed(generation);

  QCOMPARE(failureSpy.count(), 1);
  QVERIFY(!coordinator.isActive());
}

void ServiceStartCoordinatorTests::failsOnTimeout()
{
  ServiceStartCoordinator coordinator(nullptr, 20);
  QSignalSpy failureSpy(&coordinator, &ServiceStartCoordinator::failed);
  constexpr quint64 generation = 44;

  coordinator.begin(generation);

  QTRY_COMPARE_WITH_TIMEOUT(failureSpy.count(), 1, 500);
  QVERIFY(!coordinator.isActive());
}

void ServiceStartCoordinatorTests::stopDuringStartIgnoresStaleResults()
{
  ServiceStartCoordinator coordinator(nullptr, 1000);
  QSignalSpy startSpy(&coordinator, &ServiceStartCoordinator::startRequested);
  QSignalSpy coreSpy(&coordinator, &ServiceStartCoordinator::coreConnectionRequested);
  QSignalSpy successSpy(&coordinator, &ServiceStartCoordinator::succeeded);
  QSignalSpy failureSpy(&coordinator, &ServiceStartCoordinator::failed);
  constexpr quint64 generation = 45;

  coordinator.begin(generation);
  coordinator.daemonConnected(generation);
  coordinator.cancel(generation);
  coordinator.configResult(generation, true);
  coordinator.startResult(generation, true);
  coordinator.coreConnected(generation);

  QVERIFY(startSpy.isEmpty());
  QVERIFY(coreSpy.isEmpty());
  QVERIFY(successSpy.isEmpty());
  QVERIFY(failureSpy.isEmpty());
  QVERIFY(!coordinator.isActive());
}

QTEST_GUILESS_MAIN(ServiceStartCoordinatorTests)
