/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "licensing/ProEarlyAccessClaim.h"

#include "common/UrlConstants.h"

#include <QUrlQuery>

#include <openssl/rand.h>

namespace deskflow::licensing {

QString createProEarlyAccessClaim()
{
  QByteArray entropy(kProEarlyAccessClaimBytes, Qt::Uninitialized);
  if (RAND_bytes(reinterpret_cast<unsigned char *>(entropy.data()), static_cast<int>(entropy.size())) != 1) {
    return {};
  }
  return createProEarlyAccessClaim(entropy);
}

QString createProEarlyAccessClaim(const QByteArray &entropy)
{
  if (entropy.size() != kProEarlyAccessClaimBytes) {
    return {};
  }
  return QString::fromLatin1(entropy.toHex());
}

bool isValidProEarlyAccessClaim(QStringView claim) noexcept
{
  if (claim.size() != kProEarlyAccessClaimCharacters) {
    return false;
  }
  for (const auto character : claim) {
    if (!((character >= u'0' && character <= u'9') || (character >= u'a' && character <= u'f'))) {
      return false;
    }
  }
  return true;
}

QUrl proEarlyAccessSponsorUrl(QStringView claim)
{
  if (!isValidProEarlyAccessClaim(claim)) {
    return {};
  }
  QUrl url(kUrlProEarlyAccessSponsor);
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("metadata_campaign"), QStringLiteral("ieum_pro_local_ea"));
  query.addQueryItem(QStringLiteral("metadata_source"), QStringLiteral("desktop_app"));
  query.addQueryItem(QStringLiteral("metadata_claim"), claim.toString());
  url.setQuery(query);
  return url;
}

QUrl proEarlyAccessMailtoUrl(QStringView claim)
{
  if (!isValidProEarlyAccessClaim(claim)) {
    return {};
  }
  QUrl url;
  url.setScheme(QStringLiteral("mailto"));
  url.setPath(kProEarlyAccessEmail);

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("subject"), QStringLiteral("Ieum Pro Local Early Access license request"));
  query.addQueryItem(
      QStringLiteral("body"),
      QStringLiteral(
          "GitHub username:\nClaim code: %1\n\nI completed the one-time USD 30 GitHub Sponsors payment. "
          "Please verify it and email my perpetual Pro Local file-transfer license file."
      )
          .arg(claim)
  );
  url.setQuery(query);
  return url;
}

} // namespace deskflow::licensing
