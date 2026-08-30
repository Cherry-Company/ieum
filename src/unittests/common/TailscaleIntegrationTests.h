/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class TailscaleIntegrationTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void parsesRunningStatus();
  void parsesLoginState();
  void parsesMachineAuthorizationState();
  void rejectsMalformedStatus();
  void queryKeepsEventLoopResponsive();
  void hungQueryTimesOut();
  void duplicateQueriesAreCoalesced();
  void lateCompletionDoesNotReplaceNewerQuery();
  void destructionIgnoresLateCompletion();
  void completedQueryParsesStatus();
};
