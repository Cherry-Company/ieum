/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/ExternalUrlLauncher.h"

#include <QTest>

class ExternalUrlLauncherTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void reportsSuccessfulLaunch()
  {
    const QUrl url(QStringLiteral("https://github.com/sponsors/victoriousian"));
    QUrl openedUrl;
    const auto result = deskflow::gui::launchExternalUrl(url, [&openedUrl](const QUrl &value) {
      openedUrl = value;
      return true;
    });
    QVERIFY(result.opened);
    QVERIFY(result.fallbackUrl.isEmpty());
    QCOMPARE(openedUrl, url);
  }

  void preservesFullUrlForFailureFallback()
  {
    const QUrl url(QStringLiteral(
        "https://github.com/sponsors/victoriousian/"
        "sponsorships?metadata_campaign=ieum_pro_local_ea&metadata_claim=000102030405060708090a0b0c0d0e0f"
    ));
    const auto result = deskflow::gui::launchExternalUrl(url, [](const QUrl &) { return false; });
    QVERIFY(!result.opened);
    QCOMPARE(result.fallbackUrl, url.toString(QUrl::FullyEncoded));
    QVERIFY(result.fallbackUrl.contains(QStringLiteral("metadata_claim=000102030405060708090a0b0c0d0e0f")));
  }
};

QTEST_MAIN(ExternalUrlLauncherTests)

#include "ExternalUrlLauncherTests.moc"
