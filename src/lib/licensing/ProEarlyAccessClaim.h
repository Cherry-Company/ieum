/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringView>
#include <QUrl>

namespace deskflow::licensing {

inline constexpr qsizetype kProEarlyAccessClaimBytes = 16;
inline constexpr qsizetype kProEarlyAccessClaimCharacters = kProEarlyAccessClaimBytes * 2;

[[nodiscard]] QString createProEarlyAccessClaim();
[[nodiscard]] QString createProEarlyAccessClaim(const QByteArray &entropy);
[[nodiscard]] bool isValidProEarlyAccessClaim(QStringView claim) noexcept;
[[nodiscard]] QUrl proEarlyAccessSponsorUrl(QStringView claim);
[[nodiscard]] QUrl proEarlyAccessMailtoUrl(QStringView claim);

} // namespace deskflow::licensing
