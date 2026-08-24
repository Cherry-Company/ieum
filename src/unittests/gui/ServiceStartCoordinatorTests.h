/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class ServiceStartCoordinatorTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void waitsForConfigStartAndCoreReadiness();
  void failsWhenConfigIsRejected();
  void failsWhenSpawnIsRejected();
  void failsWhenDaemonDisconnects();
  void failsOnTimeout();
  void stopDuringStartIgnoresStaleResults();
};
