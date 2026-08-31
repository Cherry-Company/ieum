/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <optional>

namespace deskflow::licensing {

inline constexpr auto kFileTransferFeature = "file-transfer";
inline constexpr qsizetype kMaxProLicenseBytes = 4096;
inline constexpr qsizetype kMaxProLicensePayloadBytes = 2048;

enum class ProLicenseStatus
{
  Missing,
  TooLarge,
  Malformed,
  Unsupported,
  InvalidSignature,
  NotYetValid,
  Expired,
  Valid,
};

struct ProLicenseEntitlement
{
  ProLicenseStatus status{ProLicenseStatus::Missing};
  QString licenseId;
  QString recipient;
  QStringList features;
  QDateTime notBefore;
  std::optional<QDateTime> expiresAt;

  [[nodiscard]] bool permits(QStringView feature) const noexcept;
};

using ProLicenseKeyRing = QHash<QString, QByteArray>;

[[nodiscard]] ProLicenseEntitlement
verifyProLicense(const QString &licenseText, const QDateTime &nowUtc, const ProLicenseKeyRing &keyRing);

[[nodiscard]] ProLicenseEntitlement
verifyProductionProLicense(const QString &licenseText, const QDateTime &nowUtc = QDateTime::currentDateTimeUtc());

[[nodiscard]] bool
permitsProductionFileTransfer(const QString &licenseText, const QDateTime &nowUtc = QDateTime::currentDateTimeUtc());

[[nodiscard]] QString proLicenseStatusText(ProLicenseStatus status);

} // namespace deskflow::licensing
