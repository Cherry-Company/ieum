/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/MacLoginItems.h"

#include <QTest>

using namespace deskflow::gui;

class MacLoginItemsTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void migratesBeforeUnregisteringTheRunningAgent();
  void cleansUpLegacyEvenWhenMainAppAlreadyRegistered_data();
  void cleansUpLegacyEvenWhenMainAppAlreadyRegistered();
  void disabledPreferenceRemovesBothRegistrations();
  void failedRegistrationStillRetiresCrashingAgent();
  void cleanupFailureIsReportedAndRetried();
  void freshDisabledInstallationDoesNothing();
};

namespace {

struct Service
{
  MacLoginItemStatus state = MacLoginItemStatus::NotRegistered;
  bool succeeds = true;
  QString name;
  QStringList &calls;

  MacLoginItemOperations operations()
  {
    return {
        [this] { return state; },
        [this](bool enabled, QString *error) {
          calls.append(name + (enabled ? QStringLiteral(" register") : QStringLiteral(" unregister")));
          if (!succeeds) {
            *error = name + QStringLiteral(" failed");
            return false;
          }
          state = enabled ? MacLoginItemStatus::Enabled : MacLoginItemStatus::NotRegistered;
          return true;
        }
    };
  }
};

} // namespace

void MacLoginItemsTests::migratesBeforeUnregisteringTheRunningAgent()
{
  QStringList calls;
  Service main{MacLoginItemStatus::NotRegistered, true, "main", calls};
  Service legacy{MacLoginItemStatus::Enabled, true, "legacy", calls};
  QString error;
  QVERIFY(reconcileMacLoginItems(true, main.operations(), legacy.operations(), &error));
  QCOMPARE(calls, (QStringList{"main register", "legacy unregister"}));
  QVERIFY(error.isEmpty());
  calls.clear();
  QVERIFY(reconcileMacLoginItems(true, main.operations(), legacy.operations(), &error));
  QVERIFY(calls.isEmpty());
}

void MacLoginItemsTests::cleansUpLegacyEvenWhenMainAppAlreadyRegistered_data()
{
  QTest::addColumn<MacLoginItemStatus>("status");
  QTest::newRow("enabled") << MacLoginItemStatus::Enabled;
  QTest::newRow("approval required") << MacLoginItemStatus::RequiresApproval;
}

void MacLoginItemsTests::cleansUpLegacyEvenWhenMainAppAlreadyRegistered()
{
  QFETCH(MacLoginItemStatus, status);
  QStringList calls;
  Service main{status, true, "main", calls};
  Service legacy{MacLoginItemStatus::RequiresApproval, true, "legacy", calls};
  QVERIFY(reconcileMacLoginItems(true, main.operations(), legacy.operations(), nullptr));
  QCOMPARE(calls, QStringList{"legacy unregister"});
  QCOMPARE(main.state, status);
}

void MacLoginItemsTests::disabledPreferenceRemovesBothRegistrations()
{
  QStringList calls;
  Service main{MacLoginItemStatus::Enabled, true, "main", calls};
  Service legacy{MacLoginItemStatus::Enabled, true, "legacy", calls};
  QVERIFY(reconcileMacLoginItems(false, main.operations(), legacy.operations(), nullptr));
  QCOMPARE(calls, (QStringList{"main unregister", "legacy unregister"}));
}

void MacLoginItemsTests::failedRegistrationStillRetiresCrashingAgent()
{
  QStringList calls;
  Service main{MacLoginItemStatus::NotFound, false, "main", calls};
  Service legacy{MacLoginItemStatus::Enabled, true, "legacy", calls};
  QString error;
  QVERIFY(!reconcileMacLoginItems(true, main.operations(), legacy.operations(), &error));
  QCOMPARE(calls, (QStringList{"main register", "legacy unregister"}));
  QCOMPARE(error, QStringLiteral("main failed"));
}

void MacLoginItemsTests::cleanupFailureIsReportedAndRetried()
{
  QStringList calls;
  Service main{MacLoginItemStatus::NotRegistered, true, "main", calls};
  Service legacy{MacLoginItemStatus::Enabled, false, "legacy", calls};
  QString error;
  QVERIFY(!reconcileMacLoginItems(true, main.operations(), legacy.operations(), &error));
  QVERIFY(error.contains(QStringLiteral("legacy failed")));
  legacy.succeeds = true;
  calls.clear();
  QVERIFY(reconcileMacLoginItems(true, main.operations(), legacy.operations(), &error));
  QCOMPARE(calls, QStringList{"legacy unregister"});
  QVERIFY(error.isEmpty());
}

void MacLoginItemsTests::freshDisabledInstallationDoesNothing()
{
  QStringList calls;
  Service main{MacLoginItemStatus::NotRegistered, true, "main", calls};
  Service legacy{MacLoginItemStatus::NotRegistered, true, "legacy", calls};
  QVERIFY(reconcileMacLoginItems(false, main.operations(), legacy.operations(), nullptr));
  QVERIFY(calls.isEmpty());
}

QTEST_GUILESS_MAIN(MacLoginItemsTests)
#include "MacLoginItemsTests.moc"
