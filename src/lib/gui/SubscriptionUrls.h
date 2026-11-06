/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once
#include <QUrl>

namespace deskflow::gui {
inline bool isSubscriptionServiceUrl(const QUrl &url)
{
  return url.isValid() && url.scheme() == QLatin1String("https") && !url.host().isEmpty() && url.userInfo().isEmpty() &&
         url.port() == -1 && !url.hasQuery() && !url.hasFragment() &&
         (url.path().isEmpty() || url.path() == QLatin1String("/"));
}
inline bool isSubscriptionCheckoutUrl(const QUrl &url)
{
  if (!url.isValid() || url.scheme() != QLatin1String("https") || !url.userInfo().isEmpty() || url.port() != -1 ||
      url.hasQuery() || url.hasFragment())
    return false;
  const auto parts = url.path().split(QLatin1Char('/'));
  if (parts.size() != 3 || parts[2].isEmpty() || parts[2] == QLatin1String(".") || parts[2] == QLatin1String(".."))
    return false;
  return (url.host() == QLatin1String("polar.sh") && parts[1] == QLatin1String("checkout")) ||
         (url.host().endsWith(QLatin1String(".gumroad.com")) && parts[1] == QLatin1String("l"));
}
} // namespace deskflow::gui
