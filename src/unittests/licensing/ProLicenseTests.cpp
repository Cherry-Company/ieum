/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "licensing/ProLicense.h"

#include "fixtures/invalid-signed-licenses.h"
#include "fixtures/test-public-key.h"
#include "fixtures/valid-perpetual-license.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTest>
#include <QTime>
#include <QTimeZone>

namespace {

using deskflow::licensing::ProLicenseKeyRing;
using deskflow::licensing::ProLicenseStatus;
using deskflow::licensing::test_fixture::kKeyId;
using deskflow::licensing::test_fixture::kPublicKeyHex;

QDateTime utc(int year, int month, int day, int hour = 0, int minute = 0, int second = 0)
{
  return QDateTime(QDate(year, month, day), QTime(hour, minute, second), QTimeZone::UTC);
}

ProLicenseKeyRing testKeyRing()
{
  return {{QString::fromLatin1(kKeyId), QByteArray::fromHex(kPublicKeyHex)}};
}

QString latin1(const char *value)
{
  return QString::fromLatin1(value);
}

} // namespace

class ProLicenseTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void acceptsValidPerpetualLicense();
  void acceptsValidDatedLicense();
  void enforcesValidityWindow();
  void rejectsMalformedEnvelope_data();
  void rejectsMalformedEnvelope();
  void rejectsUnsupportedSignedClaims();
  void rejectsMalformedSignedClaims();
  void rejectsTampering();
};

void ProLicenseTests::acceptsValidPerpetualLicense()
{
  const auto result = deskflow::licensing::verifyProLicense(
      latin1(deskflow::licensing::test_fixture::kValidPerpetual), utc(2026, 8, 31), testKeyRing()
  );

  QCOMPARE(result.status, ProLicenseStatus::Valid);
  QCOMPARE(result.licenseId, QStringLiteral("11111111-2222-4333-8444-555555555555"));
  QCOMPARE(result.recipient, QStringLiteral("fixture-owner"));
  QCOMPARE(result.notBefore, utc(2026, 8, 1));
  QVERIFY(!result.expiresAt.has_value());
  QVERIFY(result.permits(QStringLiteral("file-transfer")));
  QVERIFY(!result.permits(QStringLiteral("hosted-relay")));
}

void ProLicenseTests::acceptsValidDatedLicense()
{
  const auto result = deskflow::licensing::verifyProLicense(
      latin1(deskflow::licensing::test_fixture::kValidDated), utc(2026, 8, 31), testKeyRing()
  );

  QCOMPARE(result.status, ProLicenseStatus::Valid);
  QCOMPARE(result.licenseId, QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"));
  QVERIFY(result.expiresAt.has_value());
  QCOMPARE(*result.expiresAt, utc(2026, 9, 30, 23, 59, 59));
}

void ProLicenseTests::enforcesValidityWindow()
{
  const auto future = deskflow::licensing::verifyProLicense(
      latin1(deskflow::licensing::test_fixture::kValidPerpetual), utc(2026, 7, 31, 23, 59, 59), testKeyRing()
  );
  QCOMPARE(future.status, ProLicenseStatus::NotYetValid);
  QVERIFY(!future.permits(QStringLiteral("file-transfer")));

  const auto expired = deskflow::licensing::verifyProLicense(
      latin1(deskflow::licensing::test_fixture::kValidDated), utc(2026, 10, 1), testKeyRing()
  );
  QCOMPARE(expired.status, ProLicenseStatus::Expired);
  QVERIFY(!expired.permits(QStringLiteral("file-transfer")));
}

void ProLicenseTests::rejectsMalformedEnvelope_data()
{
  QTest::addColumn<QString>("license");
  QTest::addColumn<ProLicenseStatus>("expected");

  const auto valid = latin1(deskflow::licensing::test_fixture::kValidPerpetual);
  const auto parts = valid.split('.');
  QCOMPARE(parts.size(), 3);

  QTest::newRow("empty") << QString() << ProLicenseStatus::Missing;
  QTest::newRow("whitespace") << QStringLiteral(" \r\n\t ") << ProLicenseStatus::Missing;
  QTest::newRow("whole input too large") << QString(4097, 'A') << ProLicenseStatus::TooLarge;
  QTest::newRow("wrong prefix") << QStringLiteral("IEUM2.%1.%2").arg(parts[1], parts[2]) << ProLicenseStatus::Malformed;
  QTest::newRow("two segments") << QStringLiteral("IEUM1.%1").arg(parts[1]) << ProLicenseStatus::Malformed;
  QTest::newRow("four segments") << valid + QStringLiteral(".extra") << ProLicenseStatus::Malformed;
  QTest::newRow("padded payload") << QStringLiteral("IEUM1.%1=.%2").arg(parts[1], parts[2])
                                  << ProLicenseStatus::Malformed;
  QTest::newRow("invalid payload alphabet")
      << QStringLiteral("IEUM1.bad+value.%1").arg(parts[2]) << ProLicenseStatus::Malformed;
  QTest::newRow("empty signature") << QStringLiteral("IEUM1.%1.").arg(parts[1]) << ProLicenseStatus::Malformed;
  QTest::newRow("short signature") << QStringLiteral("IEUM1.%1.YQ").arg(parts[1]) << ProLicenseStatus::Malformed;
  const auto oversizedPayload =
      QByteArray(2049, 'x').toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
  QTest::newRow("decoded payload too large")
      << QStringLiteral("IEUM1.%1.%2").arg(QString::fromLatin1(oversizedPayload), parts[2])
      << ProLicenseStatus::TooLarge;
}

void ProLicenseTests::rejectsMalformedEnvelope()
{
  QFETCH(QString, license);
  QFETCH(ProLicenseStatus, expected);

  const auto result = deskflow::licensing::verifyProLicense(license, utc(2026, 8, 31), testKeyRing());
  QCOMPARE(result.status, expected);
  QVERIFY(result.licenseId.isEmpty());
  QVERIFY(result.recipient.isEmpty());
  QVERIFY(result.features.isEmpty());
}

void ProLicenseTests::rejectsUnsupportedSignedClaims()
{
  for (const auto &fixture : deskflow::licensing::test_fixture::kUnsupportedFixtures) {
    const auto result = deskflow::licensing::verifyProLicense(latin1(fixture.license), utc(2026, 8, 31), testKeyRing());
    QVERIFY2(result.status == ProLicenseStatus::Unsupported, fixture.name);
    QVERIFY(result.recipient.isEmpty());
    QVERIFY(result.features.isEmpty());
  }
}

void ProLicenseTests::rejectsMalformedSignedClaims()
{
  for (const auto &fixture : deskflow::licensing::test_fixture::kMalformedSignedFixtures) {
    const auto result = deskflow::licensing::verifyProLicense(latin1(fixture.license), utc(2026, 8, 31), testKeyRing());
    QVERIFY2(result.status == ProLicenseStatus::Malformed, fixture.name);
    QVERIFY(result.recipient.isEmpty());
    QVERIFY(result.features.isEmpty());
  }
}

void ProLicenseTests::rejectsTampering()
{
  const auto valid = latin1(deskflow::licensing::test_fixture::kValidPerpetual);
  auto parts = valid.split('.');
  QCOMPARE(parts.size(), 3);
  auto payload = QByteArray::fromBase64(parts[1].toLatin1(), QByteArray::Base64UrlEncoding);
  QVERIFY(payload.contains("fixture-owner"));
  payload.replace("fixture-owner", "fixture-other");
  parts[1] = QString::fromLatin1(payload.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
  const auto payloadTampered = parts.join('.');
  QCOMPARE(
      deskflow::licensing::verifyProLicense(payloadTampered, utc(2026, 8, 31), testKeyRing()).status,
      ProLicenseStatus::InvalidSignature
  );

  auto signatureTampered = valid;
  const auto signatureIndex = signatureTampered.lastIndexOf('.') + 1;
  QVERIFY(signatureIndex > 0 && signatureIndex < signatureTampered.size());
  signatureTampered[signatureIndex] =
      signatureTampered[signatureIndex] == QLatin1Char('A') ? QLatin1Char('B') : QLatin1Char('A');
  QCOMPARE(
      deskflow::licensing::verifyProLicense(signatureTampered, utc(2026, 8, 31), testKeyRing()).status,
      ProLicenseStatus::InvalidSignature
  );
}

QTEST_MAIN(ProLicenseTests)

#include "ProLicenseTests.moc"
