/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once
#include "licensing/PlanCatalog.h"
#include <QObject>
#include <QUrl>
#include <optional>

namespace deskflow::gui {
struct SubscriptionPurchase
{
  licensing::Plan plan;
  QString key;
};
[[nodiscard]] std::optional<SubscriptionPurchase> parseSubscriptionLink(const QUrl &url);

// macOS delivers a URL event, including while the application is starting.
class SubscriptionLinkHandler final : public QObject
{
  Q_OBJECT
public:
  explicit SubscriptionLinkHandler(QObject *parent);
  void setReady();
  void receive(const QUrl &url);
Q_SIGNALS:
  void received(const QUrl &url);

protected:
  bool eventFilter(QObject *object, QEvent *event) override;

private:
  bool m_ready = false;
  QUrl m_pending;
};
} // namespace deskflow::gui
