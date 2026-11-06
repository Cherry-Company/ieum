/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "licensing/PlanCatalog.h"
#include "licensing/ProLicense.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <memory>
#include <openssl/evp.h>

using namespace deskflow::licensing;

namespace {
const auto now = QDateTime::fromString(QStringLiteral("2026-11-06T12:00:00Z"), Qt::ISODate);

class TestAuthority
{
public:
  TestAuthority() : key(EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519"), EVP_PKEY_free)
  {
    if (!key)
      qFatal("Cannot create ephemeral test authority");
    QByteArray publicKey(32, Qt::Uninitialized);
    size_t size = static_cast<size_t>(publicKey.size());
    if (EVP_PKEY_get_raw_public_key(key.get(), reinterpret_cast<unsigned char *>(publicKey.data()), &size) != 1)
      qFatal("Cannot read test public key");
    ring.insert(QStringLiteral("ephemeral-test"), publicKey);
  }

  QString sign(const QJsonObject &claims)
  {
    const auto payload = QJsonDocument(claims).toJson(QJsonDocument::Compact);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    QByteArray signature(64, Qt::Uninitialized);
    size_t size = static_cast<size_t>(signature.size());
    if (!context || EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1 ||
        EVP_DigestSign(
            context.get(), reinterpret_cast<unsigned char *>(signature.data()), &size,
            reinterpret_cast<const unsigned char *>(payload.data()), static_cast<size_t>(payload.size())
        ) != 1)
      qFatal("Cannot sign test license");
    const auto flags = QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals;
    return QStringLiteral("IEUM1.%1.%2")
        .arg(QString::fromLatin1(payload.toBase64(flags)), QString::fromLatin1(signature.toBase64(flags)));
  }

  ProLicenseKeyRing ring;

private:
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key;
};

QJsonObject claims(Plan plan, const QString &kind = QStringLiteral("event"))
{
  return {
      {"schema", "ieum.pro-license.v2"},
      {"key_id", "ephemeral-test"},
      {"license_id", "e2309812-d46c-43ab-b445-bdc1c33d8167"},
      {"product", "ieum-desktop"},
      {"plan", planId(plan)},
      {"kind", kind},
      {"recipient", "test"},
      {"features", QJsonArray::fromStringList(planFeatures(plan))},
      {"issued_at", now.toString(Qt::ISODate)},
      {"not_before", now.toString(Qt::ISODate)},
      {"expires_at", QJsonValue::Null}
  };
}
} // namespace

class PlanLicenseTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void tiersAreCumulative();
  void rejectsEscalationAndUnknownFeatures();
  void rejectsUnboundedSubscription();
  void respectsExpiryAndInvalidTime();
  void cannotPromoteLegacyLicense();
  void preservesLegacyValidityWindow();
};

void PlanLicenseTests::tiersAreCumulative()
{
  TestAuthority authority;
  for (const auto plan : {Plan::ProfessionalLocal, Plan::Professional, Plan::ProfessionalMax}) {
    const auto license = verifyProLicense(authority.sign(claims(plan)), now, authority.ring);
    QCOMPARE(license.status, ProLicenseStatus::Valid);
    QCOMPARE(license.plan, plan);
    QVERIFY(license.permits(QStringLiteral("file-transfer")));
    QCOMPARE(license.permits(QStringLiteral("audio-sharing")), plan != Plan::ProfessionalLocal);
    QCOMPARE(license.permits(QStringLiteral("image-clipboard")), plan == Plan::ProfessionalMax);
    QCOMPARE(license.permits(QStringLiteral("simultaneous-input")), plan == Plan::ProfessionalMax);
    QVERIFY(!license.permits(QStringLiteral("made-up-feature")));
  }
}

void PlanLicenseTests::rejectsEscalationAndUnknownFeatures()
{
  TestAuthority authority;
  auto payload = claims(Plan::ProfessionalLocal);
  payload["features"] = QJsonArray::fromStringList(planFeatures(Plan::ProfessionalMax));
  QCOMPARE(verifyProLicense(authority.sign(payload), now, authority.ring).status, ProLicenseStatus::Unsupported);
  payload = claims(Plan::ProfessionalMax);
  payload["plan"] = "unknown";
  QCOMPARE(verifyProLicense(authority.sign(payload), now, authority.ring).status, ProLicenseStatus::Unsupported);
  payload = claims(Plan::ProfessionalMax);
  auto features = payload["features"].toArray();
  features.append("file-transfer");
  payload["features"] = features;
  QCOMPARE(verifyProLicense(authority.sign(payload), now, authority.ring).status, ProLicenseStatus::Unsupported);
}

void PlanLicenseTests::rejectsUnboundedSubscription()
{
  TestAuthority authority;
  auto payload = claims(Plan::Professional, QStringLiteral("subscription"));
  QCOMPARE(verifyProLicense(authority.sign(payload), now, authority.ring).status, ProLicenseStatus::Malformed);
  payload["expires_at"] = now.addDays(8).toString(Qt::ISODate);
  QCOMPARE(verifyProLicense(authority.sign(payload), now, authority.ring).status, ProLicenseStatus::Malformed);
  payload["expires_at"] = now.addDays(7).toString(Qt::ISODate);
  QCOMPARE(verifyProLicense(authority.sign(payload), now, authority.ring).status, ProLicenseStatus::Valid);
}

void PlanLicenseTests::respectsExpiryAndInvalidTime()
{
  TestAuthority authority;
  auto payload = claims(Plan::ProfessionalMax);
  payload["expires_at"] = now.addSecs(60).toString(Qt::ISODate);
  const auto signedLicense = authority.sign(payload);
  QCOMPARE(verifyProLicense(signedLicense, now.addSecs(59), authority.ring).status, ProLicenseStatus::Valid);
  QCOMPARE(verifyProLicense(signedLicense, now.addSecs(60), authority.ring).status, ProLicenseStatus::Expired);
  QCOMPARE(verifyProLicense(signedLicense, now.addSecs(-1), authority.ring).status, ProLicenseStatus::NotYetValid);
  QCOMPARE(verifyProLicense(signedLicense, {}, authority.ring).status, ProLicenseStatus::Malformed);
  payload["expires_at"] = now.addSecs(-1).toString(Qt::ISODate);
  QCOMPARE(verifyProLicense(authority.sign(payload), now, authority.ring).status, ProLicenseStatus::Malformed);
}

void PlanLicenseTests::cannotPromoteLegacyLicense()
{
  TestAuthority authority;
  auto payload = claims(Plan::ProfessionalMax);
  payload["schema"] = "ieum.pro-license.v1";
  payload["product"] = "pro-local";
  QCOMPARE(verifyProLicense(authority.sign(payload), now, authority.ring).status, ProLicenseStatus::Unsupported);
  payload["features"] = QJsonArray{"file-transfer"};
  const auto license = verifyProLicense(authority.sign(payload), now, authority.ring);
  QCOMPARE(license.status, ProLicenseStatus::Valid);
  QCOMPARE(license.plan, Plan::ProfessionalLocal);
  QVERIFY(!license.permits(QStringLiteral("audio-sharing")));
}

void PlanLicenseTests::preservesLegacyValidityWindow()
{
  TestAuthority authority;
  auto payload = claims(Plan::ProfessionalLocal);
  payload["schema"] = "ieum.pro-license.v1";
  payload["product"] = "pro-local";
  payload["not_before"] = now.addSecs(-300).toString(Qt::ISODate);
  payload["expires_at"] = now.addSecs(60).toString(Qt::ISODate);
  const auto license = authority.sign(payload);
  QCOMPARE(verifyProLicense(license, now, authority.ring).status, ProLicenseStatus::Valid);
  QCOMPARE(verifyProLicense(license, now.addSecs(60), authority.ring).status, ProLicenseStatus::Valid);
  QCOMPARE(verifyProLicense(license, now.addSecs(61), authority.ring).status, ProLicenseStatus::Expired);
}

QTEST_MAIN(PlanLicenseTests)
#include "PlanLicenseTests.moc"
