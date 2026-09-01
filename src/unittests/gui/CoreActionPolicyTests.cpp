/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreActionPolicyTests.h"

#include "gui/core/CoreActionPolicy.h"

using deskflow::core::ProcessState;
using deskflow::gui::coreCanStop;
using deskflow::gui::coreToggleEnabled;

void CoreActionPolicyTests::startInProgress_remainsStoppableWhenConfigurationBecomesInvalid()
{
  QVERIFY(coreCanStop(ProcessState::Starting));
  QVERIFY(coreToggleEnabled(ProcessState::Starting, false));
  QVERIFY(coreCanStop(ProcessState::RetryPending));
  QVERIFY(coreToggleEnabled(ProcessState::RetryPending, false));
}

void CoreActionPolicyTests::stoppedCore_requiresValidConfigurationToStart()
{
  QVERIFY(!coreCanStop(ProcessState::Stopped));
  QVERIFY(!coreToggleEnabled(ProcessState::Stopped, false));
  QVERIFY(coreToggleEnabled(ProcessState::Stopped, true));
}

QTEST_GUILESS_MAIN(CoreActionPolicyTests)
