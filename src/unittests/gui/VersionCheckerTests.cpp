/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "VersionCheckerTests.h"

#include "gui/VersionChecker.h"

void VersionCheckerTests::compareVersions_ordersPrereleaseStages()
{
  QCOMPARE(VersionChecker::compareVersions(QStringLiteral("0.1.0-alpha.2"), QStringLiteral("0.1.0-beta.1")), 1);
  QCOMPARE(VersionChecker::compareVersions(QStringLiteral("0.1.0-beta.1"), QStringLiteral("0.1.0-rc.1")), 1);
  QCOMPARE(VersionChecker::compareVersions(QStringLiteral("0.1.0-rc.1"), QStringLiteral("0.1.0")), 1);
}

void VersionCheckerTests::compareVersions_ordersPrereleaseSequence()
{
  QCOMPARE(VersionChecker::compareVersions(QStringLiteral("0.1.0-alpha.1"), QStringLiteral("0.1.0-alpha.2")), 1);
  QCOMPARE(VersionChecker::compareVersions(QStringLiteral("0.1.0-alpha.2"), QStringLiteral("0.1.0-alpha.1")), -1);
  QCOMPARE(VersionChecker::compareVersions(QStringLiteral("0.1.0-alpha.2"), QStringLiteral("0.1.0-alpha.2")), 0);
}

QTEST_MAIN(VersionCheckerTests)
