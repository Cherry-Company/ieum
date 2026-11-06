/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "gui/dialogs/PlansDialog.h"
#include "common/Settings.h"
#include "gui/ExternalUrlLauncher.h"
#include "gui/StyleUtils.h"
#include "gui/SubscriptionClient.h"
#include "gui/SubscriptionUrls.h"
#include "licensing/PlanCatalog.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>

namespace deskflow::gui {
using namespace deskflow::licensing;

PlansDialog::PlansDialog(QWidget *parent) : QDialog(parent)
{
  setWindowTitle(tr("Plans and subscription"));
  resize(940, 720);
  applyIeumDialogStyle(*this);
  const auto palette = QGuiApplication::palette();
  setStyleSheet(
      styleSheet() +
      QStringLiteral(R"(
QWidget#planCards { background: transparent; }
QGroupBox[planCard="true"] {
  background: %1; border: 1px solid %2; border-radius: 16px;
  margin-top: 0; padding: 18px 14px;
}
QLabel#planTitle { font-size: 15px; font-weight: 600; }
QLabel#planPrice { font-size: 25px; font-weight: 600; }
QLabel#planFeatures { font-size: 14px; }
QPushButton[planPurchase="true"] { border: 1px solid %2; border-radius: 8px; padding: 5px 10px; }
)")
          .arg(
              cssColor(palette.color(QPalette::Base), 180),
              cssColor(blendedColor(palette.color(QPalette::Window), palette.color(QPalette::WindowText), 0.16))
          )
  );
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 24, 24, 18);
  layout->setSpacing(8);
  auto *title = new QLabel(tr("One desk. More connected."), this);
  title->setObjectName(QStringLiteral("guideTitle"));
  layout->addWidget(title);
  m_status = new QLabel(this);
  layout->addWidget(m_status);
  m_availability = new QLabel(tr("Subscriptions are being prepared. Existing Local licenses continue to work."), this);
  m_availability->setWordWrap(true);
  layout->addWidget(m_availability);
  auto *preview =
      new QLabel(tr("Additional sharing features are in preview. Subscriptions are not on sale yet."), this);
  preview->setWordWrap(true);
  layout->addWidget(preview);

  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  auto *content = new QWidget(scroll);
  content->setObjectName(QStringLiteral("planCards"));
  auto *cards = new QHBoxLayout(content);
  cards->setContentsMargins(0, 12, 0, 12);
  for (const auto plan : {Plan::ProfessionalLocal, Plan::Professional, Plan::ProfessionalMax}) {
    auto *card = new QGroupBox(content);
    card->setProperty("planCard", true);
    auto *column = new QVBoxLayout(card);
    column->setSpacing(8);
    auto *name = new QLabel(planName(plan), card);
    name->setObjectName(QStringLiteral("planTitle"));
    column->addWidget(name);
    const auto price = planPrice(plan);
    auto *annual = new QLabel(tr("US$%1 / year").arg(QString::number(price.annualUsdCents / 100.0, 'f', 2)), card);
    annual->setObjectName(QStringLiteral("planPrice"));
    column->addWidget(annual);
    column->addWidget(
        new QLabel(tr("or US$%1 / month").arg(QString::number(price.monthlyUsdCents / 100.0, 'f', 2)), card)
    );
    QStringList items{tr("Keyboard, mouse and text clipboard"), tr("Direct file transfer")};
    if (plan != Plan::ProfessionalLocal)
      items.append(tr("Computer audio sharing"));
    if (plan == Plan::ProfessionalMax)
      items.append(
          {tr("Control from either keyboard"), tr("Remote screen viewing"), tr("Image clipboard"),
           tr("Coordinated lock and wake"), tr("Simultaneous input")}
      );
    auto *included = new QLabel(items.join(QStringLiteral("\n")), card);
    included->setObjectName(QStringLiteral("planFeatures"));
    included->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    included->setWordWrap(true);
    column->addWidget(included);
    column->addStretch();
    auto *button = new QPushButton(tr("Not available yet"), card);
    button->setProperty("planPurchase", true);
    button->setObjectName(QStringLiteral("purchase-%1").arg(planId(plan)));
    button->setEnabled(false);
    column->addWidget(button);
    m_purchaseButtons.insert(planId(plan), button);
    cards->addWidget(card, 1);
  }
  scroll->setWidget(content);
  layout->addWidget(scroll, 1);
  auto *terms = new QLabel(
      tr("Free input sharing remains available. Annual prices are charged once per year. Taxes, if applicable, appear "
         "at checkout."),
      this
  );
  terms->setWordWrap(true);
  layout->addWidget(terms);

  m_restoreToggle = new QToolButton(this);
  m_restoreToggle->setText(tr("Restore a subscription purchase"));
  m_restoreToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_restoreToggle->setArrowType(Qt::RightArrow);
  m_restoreToggle->setCheckable(true);
  layout->addWidget(m_restoreToggle);
  auto *restore = new QWidget(this);
  restore->setVisible(false);
  connect(m_restoreToggle, &QToolButton::toggled, this, [this, restore](bool expanded) {
    restore->setVisible(expanded);
    m_restoreToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
  });
  auto *row = new QHBoxLayout(restore);
  row->setContentsMargins(0, 0, 0, 0);
  auto *plan = new QComboBox(restore);
  m_restorePlan = plan;
  plan->setObjectName(QStringLiteral("restorePlan"));
  for (const auto value : {Plan::ProfessionalLocal, Plan::Professional, Plan::ProfessionalMax})
    plan->addItem(planName(value), planId(value));
  auto *key = new QLineEdit(restore);
  m_restoreKey = key;
  key->setObjectName(QStringLiteral("restoreKey"));
  key->setEchoMode(QLineEdit::Password);
  key->setMaxLength(128);
  key->setPlaceholderText(tr("Subscription recovery code"));
  auto *activate = new QPushButton(tr("Restore"), restore);
  activate->setObjectName(QStringLiteral("restoreSubscription"));
  auto *client = SubscriptionClient::instance();
  const auto update = [activate, client] {
    activate->setEnabled(!client->busy() && Settings::isWritable() && !SubscriptionClient::serviceUrl().isEmpty());
  };
  connect(client, &SubscriptionClient::changed, this, update);
  connect(client, &SubscriptionClient::changed, this, &PlansDialog::updateStatus);
  connect(client, &SubscriptionClient::completed, this, [this](bool, const QString &message) {
    m_availability->setText(message);
  });
  connect(activate, &QPushButton::clicked, this, [plan, key, client] {
    if (const auto chosen = parsePlan(plan->currentData().toString()))
      client->activate(*chosen, key->text());
  });
  row->addWidget(plan);
  row->addWidget(key, 1);
  row->addWidget(activate);
  layout->addWidget(restore);
  auto *help = new QLabel(
      tr("Event and existing license keys can be entered in Settings → Files. Manage or cancel billing from your "
         "purchase receipt."),
      this
  );
  help->setWordWrap(true);
  layout->addWidget(help);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
  update();
  updateStatus();
  loadCatalog();
}

void PlansDialog::preparePurchase(Plan plan, const QString &key)
{
  const auto index = m_restorePlan->findData(planId(plan));
  if (index < 0)
    return;
  m_restorePlan->setCurrentIndex(index);
  m_restoreToggle->setChecked(true);
  m_restoreKey->setText(key);
  m_availability->setText(tr("Your purchase is ready. Select Restore to activate it on this computer."));
  m_restoreKey->setFocus();
}

void PlansDialog::updateStatus()
{
  const auto entitlement = Settings::proEntitlement();
  m_status->setText(tr("Current plan: %1").arg(planName(entitlement.plan)));
}

void PlansDialog::loadCatalog()
{
  const auto base = SubscriptionClient::serviceUrl();
  if (base.isEmpty())
    return;
  auto *network = new QNetworkAccessManager(this);
  QNetworkRequest request(base.resolved(QUrl(QStringLiteral("/api/catalog"))));
  request.setTransferTimeout(10000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
  auto *reply = network->get(request);
  reply->setReadBufferSize(16385);
  auto bytes = std::make_shared<QByteArray>();
  connect(reply, &QIODevice::readyRead, this, [reply, bytes] {
    bytes->append(reply->readAll());
    if (bytes->size() > 16384)
      reply->abort();
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, base] {
    bytes->append(reply->readAll());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError || bytes->size() > 16384)
      return;
    const auto data = QJsonDocument::fromJson(*bytes).object();
    if (data.value(QStringLiteral("currency")).toString() != QLatin1String("USD"))
      return;
    bool available = false;
    for (const auto value : data.value(QStringLiteral("plans")).toArray()) {
      const auto entry = value.toObject();
      const auto id = entry.value(QStringLiteral("id")).toString();
      const auto plan = parsePlan(id);
      if (!plan || !m_purchaseButtons.contains(id) || !entry.value(QStringLiteral("available")).toBool())
        continue;
      const auto expected = planPrice(*plan);
      const auto prices = entry.value(QStringLiteral("prices")).toObject();
      if (prices.value(QStringLiteral("monthly")).toInt() != expected.monthlyUsdCents ||
          prices.value(QStringLiteral("annually")).toInt() != expected.annualUsdCents)
        continue;
      const QUrl url(entry.value(QStringLiteral("checkout_url")).toString());
      if (!isSubscriptionCheckoutUrl(url, base))
        continue;
      auto *button = m_purchaseButtons.value(id);
      button->setText(tr("View subscription"));
      button->setEnabled(true);
      connect(button, &QPushButton::clicked, this, [this, url] {
        openExternalUrlOrShowFallback(this, url, tr("Open checkout"));
      });
      available = true;
    }
    if (available)
      m_availability->setText(tr("Choose a plan, then select monthly or annual billing at checkout."));
  });
}
} // namespace deskflow::gui
