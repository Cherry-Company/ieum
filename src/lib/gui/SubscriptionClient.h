/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include "licensing/PlanCatalog.h"
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QUrl>

namespace deskflow::gui {

class SubscriptionClient final : public QObject
{
  Q_OBJECT
public:
  static SubscriptionClient *instance();
  static QUrl serviceUrl();
  void activate(licensing::Plan plan, const QString &purchaseKey);
  void refresh();
  void disconnectSubscription();
  [[nodiscard]] bool busy() const
  {
    return m_busy;
  }

Q_SIGNALS:
  void changed();
  void completed(bool success, const QString &message);

private:
  explicit SubscriptionClient(QObject *parent);
  void verify(licensing::Plan plan, const QString &key, bool activation);
  QNetworkAccessManager m_network;
  QTimer m_timer;
  bool m_busy = false;
};

} // namespace deskflow::gui
