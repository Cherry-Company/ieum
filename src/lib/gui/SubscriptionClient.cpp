/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "gui/SubscriptionClient.h"
#include "common/Settings.h"
#include "gui/SubscriptionUrls.h"
#include "licensing/ProLicense.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QPointer>
#include <memory>

namespace deskflow::gui {
using namespace deskflow::licensing;

SubscriptionClient *SubscriptionClient::instance()
{
  static QPointer<SubscriptionClient> client;
  if (!client)
    client = new SubscriptionClient(QCoreApplication::instance());
  return client;
}

QUrl SubscriptionClient::serviceUrl()
{
  // A release-time constant, never a setting or a receipt parameter that could
  // redirect a customer's key to another server.
#ifdef IEUM_SUBSCRIPTION_SERVICE_URL
  const QUrl url(QStringLiteral(IEUM_SUBSCRIPTION_SERVICE_URL), QUrl::StrictMode);
  return isSubscriptionServiceUrl(url) ? url : QUrl{};
#else
  return {};
#endif
}

SubscriptionClient::SubscriptionClient(QObject *parent) : QObject(parent), m_network(this), m_timer(this)
{
  m_timer.setInterval(60 * 60 * 1000);
  m_timer.setSingleShot(true);
  connect(&m_timer, &QTimer::timeout, this, &SubscriptionClient::refresh);
  if (!Settings::value(Settings::Pro::SubscriptionKey).toString().isEmpty())
    QTimer::singleShot(0, this, &SubscriptionClient::refresh);
}

void SubscriptionClient::activate(Plan plan, const QString &purchaseKey)
{
  if (m_busy || !Settings::isWritable())
    return;
  if (plan == Plan::Free || purchaseKey.trimmed().isEmpty() || purchaseKey.size() > 128) {
    Q_EMIT completed(false, tr("Choose your purchased plan and enter the key from your receipt."));
    return;
  }
  verify(plan, purchaseKey.trimmed(), true);
}

void SubscriptionClient::refresh()
{
  if (m_busy || !Settings::isWritable())
    return;
  const auto key = Settings::value(Settings::Pro::SubscriptionKey).toString();
  const auto plan = parsePlan(Settings::value(Settings::Pro::SubscriptionPlan).toString());
  if (key.isEmpty() || !plan || *plan == Plan::Free)
    return;
  verify(*plan, key, false);
}

void SubscriptionClient::disconnectSubscription()
{
  if (m_busy || !Settings::isWritable())
    return;
  m_timer.stop();
  Settings::setValue(Settings::Pro::SubscriptionLease, QString{});
  Settings::setValue(Settings::Pro::SubscriptionKey, QString{});
  Settings::setValue(Settings::Pro::SubscriptionPlan, QString{});
  Q_EMIT changed();
}

void SubscriptionClient::verify(Plan plan, const QString &key, bool activation)
{
  const auto base = serviceUrl();
  if (base.scheme() != QLatin1String("https") || base.host().isEmpty()) {
    Q_EMIT completed(false, tr("Subscription activation is not available in this build yet."));
    return;
  }
  m_busy = true;
  m_timer.stop();
  Q_EMIT changed();
  QNetworkRequest request(base.resolved(QUrl(QStringLiteral("/api/activate"))));
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  request.setRawHeader("Accept", "application/json");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
  request.setTransferTimeout(12000);
  auto *reply = m_network.post(
      request, QJsonDocument(QJsonObject{{"plan", planId(plan)}, {"key", key}}).toJson(QJsonDocument::Compact)
  );
  reply->setReadBufferSize(8193);
  auto bytes = std::make_shared<QByteArray>();
  auto oversized = std::make_shared<bool>(false);
  connect(reply, &QIODevice::readyRead, this, [reply, bytes, oversized] {
    bytes->append(reply->readAll());
    if (bytes->size() > 8192) {
      *oversized = true;
      reply->abort();
    }
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, oversized, plan, key, activation] {
    bytes->append(reply->readAll());
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto error = reply->error();
    reply->deleteLater();
    m_busy = false;
    const auto object = QJsonDocument::fromJson(*bytes).object();
    const auto lease = object.value(QStringLiteral("license")).toString();
    const auto entitlement = verifyProductionProLicense(lease);
    const bool activeCredential = key == Settings::value(Settings::Pro::SubscriptionKey).toString();
    if (!*oversized && bytes->size() <= 8192 && error == QNetworkReply::NoError && status == 200 &&
        entitlement.status == ProLicenseStatus::Valid && entitlement.kind == QLatin1String("subscription") &&
        entitlement.plan == plan && (activation || activeCredential)) {
      Settings::setValue(Settings::Pro::SubscriptionLease, lease);
      Settings::setValue(Settings::Pro::SubscriptionKey, key);
      Settings::setValue(Settings::Pro::SubscriptionPlan, planId(plan));
      m_timer.start(60 * 60 * 1000);
      Q_EMIT completed(true, tr("%1 is active on this computer.").arg(planName(entitlement.plan)));
    } else if (!activation && activeCredential && !*oversized && bytes->size() <= 8192 && status == 403 &&
               object.value(QStringLiteral("error")).toString() == QLatin1String("inactive")) {
      Settings::setValue(Settings::Pro::SubscriptionLease, QString{});
      m_timer.start(60 * 60 * 1000);
      Q_EMIT completed(
          false,
          tr("This subscription has ended. Free features and any valid event or legacy license remain available.")
      );
    } else {
      if (!Settings::value(Settings::Pro::SubscriptionKey).toString().isEmpty())
        m_timer.start(15 * 60 * 1000);
      Q_EMIT completed(
          false,
          status == 403
              ? tr("This purchase key does not have an active subscription for the selected plan.")
              : tr("Could not verify the subscription. Your existing signed access remains valid until its expiry.")
      );
    }
    Q_EMIT changed();
  });
}

} // namespace deskflow::gui
