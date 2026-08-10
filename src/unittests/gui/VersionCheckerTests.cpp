/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "VersionCheckerTests.h"

#include "common/UrlConstants.h"
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

void VersionCheckerTests::packageFileName_selectsPlatformAndArchitecture()
{
  QCOMPARE(
      VersionChecker::packageFileName(
          QStringLiteral("0.1.0-alpha.19"), QStringLiteral("windows"), QStringLiteral("x86_64"), true
      ),
      QStringLiteral("Ieum-0.1.0-alpha.19-win-x64-ko-KR.msi")
  );
  QCOMPARE(
      VersionChecker::packageFileName(
          QStringLiteral("0.1.0-alpha.19"), QStringLiteral("windows"), QStringLiteral("arm64"), false
      ),
      QStringLiteral("Ieum-0.1.0-alpha.19-win-arm64.msi")
  );
  QCOMPARE(
      VersionChecker::packageFileName(
          QStringLiteral("0.1.0-alpha.19"), QStringLiteral("macos"), QStringLiteral("arm64"), false
      ),
      QStringLiteral("Ieum-0.1.0-alpha.19-macos-arm64.dmg")
  );
  QCOMPARE(
      VersionChecker::packageFileName(
          QStringLiteral("0.1.0-alpha.19"), QStringLiteral("macos"), QStringLiteral("x86_64"), false
      ),
      QStringLiteral("Ieum-0.1.0-alpha.19-macos-x86_64.dmg")
  );
}

void VersionCheckerTests::packageFileName_rejectsUntrustedVersionText()
{
  QVERIFY(
      VersionChecker::packageFileName(
          QStringLiteral("../../payload"), QStringLiteral("windows"), QStringLiteral("x64"), false
      )
          .isEmpty()
  );
  QVERIFY(VersionChecker::releasePageUrl(QStringLiteral("not-a-version")) == QUrl(kUrlDownload));
}

QTEST_MAIN(VersionCheckerTests)
