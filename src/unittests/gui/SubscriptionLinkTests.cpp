/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "gui/SubscriptionLink.h"
#include "gui/SubscriptionUrls.h"
#include <QFileOpenEvent>
#include <QSignalSpy>
#include <QTest>

using namespace deskflow::gui;
class SubscriptionLinkTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void allowsProviderBrandPrefixWithoutPermittingCredentialRedirects()
  {
    QVERIFY(
        parseSubscriptionLink(QUrl("ieum://activate?plan=professional&key=IEUM_12345678-1234-4234-8234-123456789012"))
    );
    QVERIFY(isSubscriptionServiceUrl(QUrl("https://subscriptions.example.invalid")));
    QVERIFY(isSubscriptionCheckoutUrl(QUrl("https://polar.sh/checkout/checkout-id")));
    QVERIFY(isSubscriptionCheckoutUrl(QUrl("https://store.gumroad.com/l/product")));
    for (const auto &text :
         {"http://polar.sh/checkout/id", "https://polar.sh.evil.invalid/checkout/id",
          "https://user@polar.sh/checkout/id", "https://polar.sh:444/checkout/id",
          "https://polar.sh/checkout/id?redirect=https://evil.invalid", "https://polar.sh/checkout/..",
          "https://polar.sh/checkout/id#credential", "https://polar.sh/checkout/id/extra"})
      QVERIFY(!isSubscriptionCheckoutUrl(QUrl(text)));
    for (const auto &text :
         {"http://example.invalid", "https://user@example.invalid", "https://example.invalid/path",
          "https://example.invalid?key=secret", "https://example.invalid#key", "https://example.invalid:443"})
      QVERIFY(!isSubscriptionServiceUrl(QUrl(text)));
  }
  void acceptsOnlyPurchasedPlans()
  {
    for (const auto &plan : {"professional-local", "professional", "professional-max"}) {
      const auto parsed =
          parseSubscriptionLink(QUrl(QStringLiteral("ieum://activate?plan=%1&key=12345678-ABCD1234").arg(plan)));
      QVERIFY(parsed);
      QCOMPARE(deskflow::licensing::planId(parsed->plan), QString::fromLatin1(plan));
      QCOMPARE(parsed->key, QStringLiteral("12345678-ABCD1234"));
    }
  }
  void rejectsAmbiguousAndUntrustedLinks()
  {
    const QString valid = QStringLiteral("ieum://activate?plan=professional&key=12345678-ABCD1234");
    QStringList invalid{
        QString(valid).replace("ieum:", "https:"),
        QString(valid).replace("activate?", "activate.evil.invalid?"),
        QString(valid).replace("activate?", "user@activate?"),
        QString(valid).replace("activate?", "activate:443?"),
        QString(valid).replace("activate?", "activate/other?"),
        QString(valid).replace("professional", "free"),
        QString(valid).replace("professional", "owner"),
        valid + "&plan=professional-max",
        valid + "&key=12345678-99999999",
        valid + "&url=https://example.invalid",
        valid + "#secret",
        valid + "%0A",
        QString(valid).replace("12345678-ABCD1234", "short"),
        QString(valid).replace("12345678-ABCD1234", QString(129, QLatin1Char('A'))),
        QStringLiteral("ieum://activate?key=12345678-ABCD1234&key=12345678-ABCD1234"),
    };
    for (const auto &url : invalid)
      QVERIFY(!parseSubscriptionLink(QUrl(url, QUrl::StrictMode)));
  }
  void preservesLaunchEventUntilWindowIsReady()
  {
    QObject application;
    SubscriptionLinkHandler handler(&application);
    QSignalSpy received(&handler, &SubscriptionLinkHandler::received);
    const QUrl url(QStringLiteral("ieum://activate?plan=professional&key=12345678-ABCD1234"));
    QFileOpenEvent event(url);
    QCoreApplication::sendEvent(&application, &event);
    QCOMPARE(received.count(), 0);
    handler.setReady();
    QCOMPARE(received.count(), 1);
    QCOMPARE(received.takeFirst().first().toUrl(), url);
    handler.setReady();
    QCOMPARE(received.count(), 0);
    handler.receive(QUrl(QStringLiteral("https://example.invalid/")));
    QCOMPARE(received.count(), 0);
    handler.receive(url);
    QCOMPARE(received.count(), 1);
  }
};
QTEST_MAIN(SubscriptionLinkTests)
#include "SubscriptionLinkTests.moc"
