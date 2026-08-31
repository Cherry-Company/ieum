/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "licensing/ProLicense.h"

#include "licensing/ProLicensePublicKey.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QUuid>

#include <openssl/evp.h>

#include <memory>

namespace deskflow::licensing {
namespace {

constexpr auto kEnvelopePrefix = "IEUM1";
constexpr auto kSchema = "ieum.pro-license.v1";
constexpr auto kProduct = "pro-local";
constexpr qsizetype kEd25519PublicKeyBytes = 32;
constexpr qsizetype kEd25519SignatureBytes = 64;

ProLicenseEntitlement rejected(ProLicenseStatus status)
{
  return {.status = status};
}

bool isBase64Url(const QString &value)
{
  if (value.isEmpty()) {
    return false;
  }

  for (const auto character : value) {
    const auto unicode = character.unicode();
    const auto alphaNumeric =
        (unicode >= 'A' && unicode <= 'Z') || (unicode >= 'a' && unicode <= 'z') || (unicode >= '0' && unicode <= '9');
    if (!alphaNumeric && unicode != '-' && unicode != '_') {
      return false;
    }
  }
  return true;
}

std::optional<QByteArray> decodeBase64Url(const QString &value)
{
  if (!isBase64Url(value)) {
    return std::nullopt;
  }

  const auto encoded = value.toLatin1();
  const auto result =
      QByteArray::fromBase64Encoding(encoded, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
  if (!result) {
    return std::nullopt;
  }

  if (result.decoded.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals) != encoded) {
    return std::nullopt;
  }
  return result.decoded;
}

std::optional<QString> requiredString(const QJsonObject &object, const QString &name)
{
  const auto value = object.value(name);
  if (!value.isString() || value.toString().isEmpty()) {
    return std::nullopt;
  }
  return value.toString();
}

std::optional<QDateTime> parseStrictUtc(const QString &text)
{
  static const QRegularExpression format(
      QStringLiteral(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)"), QRegularExpression::DontCaptureOption
  );
  if (!format.match(text).hasMatch()) {
    return std::nullopt;
  }

  const auto value = QDateTime::fromString(text, Qt::ISODate);
  if (!value.isValid() || value.timeSpec() != Qt::UTC || value.toString(Qt::ISODate) != text) {
    return std::nullopt;
  }
  return value;
}

bool isLowercaseUuid(const QString &text)
{
  static const QRegularExpression format(
      QStringLiteral(R"(^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$)"),
      QRegularExpression::DontCaptureOption
  );
  return format.match(text).hasMatch() && !QUuid::fromString(text).isNull();
}

bool verifyEd25519(const QByteArray &publicKey, const QByteArray &payload, const QByteArray &signature)
{
  if (publicKey.size() != kEd25519PublicKeyBytes || signature.size() != kEd25519SignatureBytes) {
    return false;
  }

  using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
  using Context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

  Key key(
      EVP_PKEY_new_raw_public_key_ex(
          nullptr, "ED25519", nullptr, reinterpret_cast<const unsigned char *>(publicKey.constData()),
          static_cast<std::size_t>(publicKey.size())
      ),
      EVP_PKEY_free
  );
  Context context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!key || !context || EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1) {
    return false;
  }

  return EVP_DigestVerify(
             context.get(), reinterpret_cast<const unsigned char *>(signature.constData()),
             static_cast<std::size_t>(signature.size()), reinterpret_cast<const unsigned char *>(payload.constData()),
             static_cast<std::size_t>(payload.size())
         ) == 1;
}

} // namespace

bool ProLicenseEntitlement::permits(QStringView feature) const noexcept
{
  return status == ProLicenseStatus::Valid && features.contains(feature.toString());
}

ProLicenseEntitlement
verifyProLicense(const QString &licenseText, const QDateTime &nowUtc, const ProLicenseKeyRing &keyRing)
{
  if (licenseText.toUtf8().size() > kMaxProLicenseBytes) {
    return rejected(ProLicenseStatus::TooLarge);
  }

  const auto trimmed = licenseText.trimmed();
  if (trimmed.isEmpty()) {
    return rejected(ProLicenseStatus::Missing);
  }

  const auto sections = trimmed.split(QLatin1Char('.'), Qt::KeepEmptyParts);
  if (sections.size() != 3 || sections[0] != QLatin1String(kEnvelopePrefix)) {
    return rejected(ProLicenseStatus::Malformed);
  }

  const auto payload = decodeBase64Url(sections[1]);
  const auto signature = decodeBase64Url(sections[2]);
  if (!payload || !signature) {
    return rejected(ProLicenseStatus::Malformed);
  }
  if (payload->size() > kMaxProLicensePayloadBytes) {
    return rejected(ProLicenseStatus::TooLarge);
  }
  if (signature->size() != kEd25519SignatureBytes) {
    return rejected(ProLicenseStatus::Malformed);
  }

  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(*payload, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return rejected(ProLicenseStatus::Malformed);
  }
  const auto object = document.object();

  const auto schema = requiredString(object, QStringLiteral("schema"));
  const auto keyId = requiredString(object, QStringLiteral("key_id"));
  const auto licenseId = requiredString(object, QStringLiteral("license_id"));
  const auto product = requiredString(object, QStringLiteral("product"));
  const auto recipient = requiredString(object, QStringLiteral("recipient"));
  const auto issuedAtText = requiredString(object, QStringLiteral("issued_at"));
  const auto notBeforeText = requiredString(object, QStringLiteral("not_before"));
  const auto featuresValue = object.value(QStringLiteral("features"));
  if (!schema || !keyId || !licenseId || !product || !recipient || !issuedAtText || !notBeforeText ||
      !featuresValue.isArray()) {
    return rejected(ProLicenseStatus::Malformed);
  }

  if (*schema != QLatin1String(kSchema) || *product != QLatin1String(kProduct)) {
    return rejected(ProLicenseStatus::Unsupported);
  }

  const auto key = keyRing.constFind(*keyId);
  if (key == keyRing.cend() || key->size() != kEd25519PublicKeyBytes) {
    return rejected(ProLicenseStatus::Unsupported);
  }
  if (!isLowercaseUuid(*licenseId)) {
    return rejected(ProLicenseStatus::Malformed);
  }

  QStringList features;
  for (const auto &value : featuresValue.toArray()) {
    if (!value.isString() || value.toString().isEmpty()) {
      return rejected(ProLicenseStatus::Malformed);
    }
    features.append(value.toString());
  }
  if (!features.contains(QLatin1String(kFileTransferFeature))) {
    return rejected(ProLicenseStatus::Unsupported);
  }

  const auto issuedAt = parseStrictUtc(*issuedAtText);
  const auto notBefore = parseStrictUtc(*notBeforeText);
  if (!issuedAt || !notBefore) {
    return rejected(ProLicenseStatus::Malformed);
  }

  std::optional<QDateTime> expiresAt;
  const auto expiresValue = object.value(QStringLiteral("expires_at"));
  if (expiresValue.isString()) {
    expiresAt = parseStrictUtc(expiresValue.toString());
    if (!expiresAt) {
      return rejected(ProLicenseStatus::Malformed);
    }
  } else if (!expiresValue.isNull()) {
    return rejected(ProLicenseStatus::Malformed);
  }

  if (!verifyEd25519(*key, *payload, *signature)) {
    return rejected(ProLicenseStatus::InvalidSignature);
  }

  const auto current = nowUtc.toUTC();
  if (current < *notBefore) {
    return rejected(ProLicenseStatus::NotYetValid);
  }
  if (expiresAt && current > *expiresAt) {
    return rejected(ProLicenseStatus::Expired);
  }

  return {
      .status = ProLicenseStatus::Valid,
      .licenseId = *licenseId,
      .recipient = *recipient,
      .features = std::move(features),
      .notBefore = *notBefore,
      .expiresAt = expiresAt,
  };
}

ProLicenseEntitlement verifyProductionProLicense(const QString &licenseText, const QDateTime &nowUtc)
{
  const ProLicenseKeyRing keyRing = {{
      QString::fromLatin1(kProductionProLicenseKeyId),
      QByteArray::fromHex(kProductionProLicensePublicKeyHex),
  }};
  return verifyProLicense(licenseText, nowUtc, keyRing);
}

bool permitsProductionFileTransfer(const QString &licenseText, const QDateTime &nowUtc)
{
  return verifyProductionProLicense(licenseText, nowUtc).permits(QStringLiteral("file-transfer"));
}

QString proLicenseStatusText(ProLicenseStatus status)
{
  switch (status) {
  case ProLicenseStatus::Missing:
    return QStringLiteral("No Pro Local license is active.");
  case ProLicenseStatus::TooLarge:
    return QStringLiteral("The license is too large.");
  case ProLicenseStatus::Malformed:
    return QStringLiteral("The license format is invalid.");
  case ProLicenseStatus::Unsupported:
    return QStringLiteral("The license does not enable this product or feature.");
  case ProLicenseStatus::InvalidSignature:
    return QStringLiteral("The license signature is invalid.");
  case ProLicenseStatus::NotYetValid:
    return QStringLiteral("The license is not active yet.");
  case ProLicenseStatus::Expired:
    return QStringLiteral("The license has expired.");
  case ProLicenseStatus::Valid:
    return QStringLiteral("Pro Local is active.");
  }
  return QStringLiteral("The license status is unknown.");
}

} // namespace deskflow::licensing
