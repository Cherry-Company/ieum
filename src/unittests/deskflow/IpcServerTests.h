/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QTest>

class IpcServerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void refusesDuplicateWithoutDisruptingFirstServer();
  void preservesCommandArgumentBoundaryFromClient();
  void redactsFileTransferEdgeDropInServerLogs();
  void returnsVersionMismatchForDifferentClientVersion();
  void correlatesDelayedDaemonCommandResults();
  void acknowledgesLegacyDaemonCommandsAfterCompletion();

private:
  Log m_log;
};
