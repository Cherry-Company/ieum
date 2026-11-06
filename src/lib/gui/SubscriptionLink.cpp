/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "gui/SubscriptionLink.h"
#include <QFileOpenEvent>
#include <QRegularExpression>
#include <QUrlQuery>

namespace deskflow::gui {
std::optional<SubscriptionPurchase> parseSubscriptionLink(const QUrl &url)
{
  if (!url.isValid() || url.toEncoded().size() > 512 || url.scheme() != QLatin1String("ieum") ||
      url.host() != QLatin1String("activate") || !url.userInfo().isEmpty() || url.port() != -1 ||
      (!url.path().isEmpty() && url.path() != QLatin1String("/")) || url.hasFragment())
    return std::nullopt;
  const auto items = QUrlQuery(url).queryItems(QUrl::FullyDecoded);
  if (items.size() != 2)
    return std::nullopt;
  QString planText;
  QString key;
  for (const auto &[name, value] : items) {
    if (name == QLatin1String("plan") && planText.isEmpty())
      planText = value;
    else if (name == QLatin1String("key") && key.isEmpty())
      key = value;
    else
      return std::nullopt;
  }
  static const QRegularExpression keyPattern(QStringLiteral("\\A[A-Za-z0-9_-]{16,128}\\z"));
  const auto plan = licensing::parsePlan(planText);
  if (!plan || *plan == licensing::Plan::Free || !keyPattern.match(key).hasMatch())
    return std::nullopt;
  return SubscriptionPurchase{*plan, key};
}
SubscriptionLinkHandler::SubscriptionLinkHandler(QObject *parent) : QObject(parent)
{
  parent->installEventFilter(this);
}
void SubscriptionLinkHandler::setReady()
{
  m_ready = true;
  const auto pending = m_pending;
  m_pending.clear();
  if (!pending.isEmpty())
    Q_EMIT received(pending);
}
void SubscriptionLinkHandler::receive(const QUrl &url)
{
  if (!parseSubscriptionLink(url))
    return;
  if (m_ready)
    Q_EMIT received(url);
  else
    m_pending = url;
}
bool SubscriptionLinkHandler::eventFilter(QObject *object, QEvent *event)
{
  if (event->type() == QEvent::FileOpen) {
    const auto url = static_cast<QFileOpenEvent *>(event)->url();
    if (parseSubscriptionLink(url)) {
      receive(url);
      event->accept();
      return true;
    }
  }
  return QObject::eventFilter(object, event);
}
} // namespace deskflow::gui
