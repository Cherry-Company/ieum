/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/UrlConstants.h"
#include "licensing/ProEarlyAccessClaim.h"

#include <QByteArray>
#include <QTest>
#include <QUrlQuery>

using deskflow::licensing::createProEarlyAccessClaim;
using deskflow::licensing::isValidProEarlyAccessClaim;
using deskflow::licensing::proEarlyAccessMailtoUrl;
using deskflow::licensing::proEarlyAccessSponsorUrl;

class ProEarlyAccessClaimTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void encodesExactlySixteenEntropyBytesAsLowercaseHex()
  {
    QByteArray entropy;
    for (char value = 0; value < 16; ++value) {
      entropy.push_back(value);
    }

    const auto claim = createProEarlyAccessClaim(entropy);
    QCOMPARE(claim, QStringLiteral("000102030405060708090a0b0c0d0e0f"));
    QVERIFY(isValidProEarlyAccessClaim(claim));
  }

  void rejectsMalformedClaims_data()
  {
    QTest::addColumn<QString>("claim");
    QTest::newRow("empty") << QString{};
    QTest::newRow("short") << QStringLiteral("000102030405060708090a0b0c0d0e");
    QTest::newRow("long") << QStringLiteral("000102030405060708090a0b0c0d0e0f00");
    QTest::newRow("uppercase") << QStringLiteral("000102030405060708090A0B0C0D0E0F");
    QTest::newRow("non-hex") << QStringLiteral("000102030405060708090a0b0c0d0e0g");
    QTest::newRow("whitespace") << QStringLiteral(" 000102030405060708090a0b0c0d0e0f");
  }

  void rejectsMalformedClaims()
  {
    QFETCH(QString, claim);
    QVERIFY(!isValidProEarlyAccessClaim(claim));
    QVERIFY(proEarlyAccessSponsorUrl(claim).isEmpty());
    QVERIFY(proEarlyAccessMailtoUrl(claim).isEmpty());
  }

  void buildsCanonicalSponsorUrl()
  {
    const auto claim = QStringLiteral("000102030405060708090a0b0c0d0e0f");
    const auto url = proEarlyAccessSponsorUrl(claim);
    QCOMPARE(url.adjusted(QUrl::RemoveQuery), QUrl(kUrlProEarlyAccessSponsor));

    const QUrlQuery query(url);
    const auto items = query.queryItems(QUrl::FullyDecoded);
    QCOMPARE(items.size(), 3);
    QCOMPARE(query.queryItemValue(QStringLiteral("metadata_campaign")), QStringLiteral("ieum_pro_local_ea"));
    QCOMPARE(query.queryItemValue(QStringLiteral("metadata_source")), QStringLiteral("desktop_app"));
    QCOMPARE(query.queryItemValue(QStringLiteral("metadata_claim")), claim);
  }

  void buildsPreparedRequestEmailWithoutLicenseMaterial()
  {
    const auto claim = QStringLiteral("000102030405060708090a0b0c0d0e0f");
    const auto url = proEarlyAccessMailtoUrl(claim);
    QCOMPARE(url.scheme(), QStringLiteral("mailto"));
    QCOMPARE(url.path(), kProEarlyAccessEmail);

    const QUrlQuery query(url);
    const auto subject = query.queryItemValue(QStringLiteral("subject"), QUrl::FullyDecoded);
    const auto body = query.queryItemValue(QStringLiteral("body"), QUrl::FullyDecoded);
    QVERIFY(subject.contains(QStringLiteral("Ieum Pro Local Early Access")));
    QVERIFY(body.contains(claim));
    QVERIFY(body.contains(QStringLiteral("GitHub username")));
    QVERIFY(!body.contains(QStringLiteral("IEUM1")));
    QVERIFY(!body.contains(QStringLiteral("license key"), Qt::CaseInsensitive));
  }
};

QTEST_GUILESS_MAIN(ProEarlyAccessClaimTests)

#include "ProEarlyAccessClaimTests.moc"
