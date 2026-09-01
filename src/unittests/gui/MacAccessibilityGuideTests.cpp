/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacAccessibilityGuideTests.h"

#include "gui/dialogs/MacAccessibilityGuide.h"

using deskflow::gui::macAccessibilityGuideSteps;
using deskflow::gui::MacAccessibilityStep;

void MacAccessibilityGuideTests::steps_followManualRecoveryOrder()
{
  QCOMPARE(
      macAccessibilityGuideSteps(), QList<MacAccessibilityStep>({
                                        MacAccessibilityStep::ShowApplication,
                                        MacAccessibilityStep::ResetPreviousApproval,
                                        MacAccessibilityStep::RegisterCurrentApplication,
                                        MacAccessibilityStep::OpenAccessibilitySettings,
                                        MacAccessibilityStep::CheckPermission,
                                    })
  );
}

QTEST_GUILESS_MAIN(MacAccessibilityGuideTests)
