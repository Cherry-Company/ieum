/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ConnectionGuideDialog.h"
#include "gui/StyleUtils.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#ifdef Q_OS_MACOS
#include "gui/OSXHelpers.h"
#endif

ConnectionGuideDialog::ConnectionGuideDialog(QWidget *parent) : QDialog(parent)
{
  setObjectName(QStringLiteral("connectionGuide"));
  setWindowTitle(tr("First connection"));
  setMinimumWidth(620);
  resize(700, 640);
  deskflow::gui::applyIeumDialogStyle(*this);
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(24, 24, 24, 20);
  root->setSpacing(16);
  auto *title = new QLabel(tr("One keyboard. A connected desk."), this);
  title->setObjectName(QStringLiteral("guideTitle"));
  root->addWidget(title);
  auto *intro = new QLabel(
      tr("Install Ieum on every computer. Follow these steps on both sides; you can reopen this guide at any time."),
      this
  );
  intro->setWordWrap(true);
  root->addWidget(intro);
  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  auto *contents = new QWidget;
  auto *steps = new QVBoxLayout(contents);
  steps->setContentsMargins(0, 0, 8, 0);
  steps->setSpacing(16);
  auto section = [steps, contents](const QString &heading, const QString &description) {
    auto *card = new QGroupBox(heading, contents);
    auto *layout = new QVBoxLayout(card);
    layout->setSpacing(12);
    auto *label = new QLabel(description, card);
    label->setWordWrap(true);
    layout->addWidget(label);
    steps->addWidget(card);
    return layout;
  };
  auto *role = section(
      tr("1. Which computer has your keyboard?"),
      tr("Host shares its keyboard and mouse. Client receives them. A client uses one host at a time.")
  );
  auto *roles = new QHBoxLayout;
  auto *host = new QPushButton(tr("Share this keyboard\nHost"), this);
  auto *client = new QPushButton(tr("Use another keyboard\nClient"), this);
  host->setObjectName(QStringLiteral("guideChooseHost"));
  client->setObjectName(QStringLiteral("guideChooseClient"));
  auto *group = new QButtonGroup(this);
  for (auto *button : {host, client}) {
    button->setCheckable(true);
    button->setAutoDefault(false);
    button->setMinimumHeight(52);
    group->addButton(button);
    roles->addWidget(button);
  }
  host->setChecked(Settings::value(Settings::Core::CoreMode) == Settings::CoreMode::Server);
  client->setChecked(Settings::value(Settings::Core::CoreMode) == Settings::CoreMode::Client);
  connect(host, &QPushButton::clicked, this, [this] { Q_EMIT roleSelected(Settings::CoreMode::Server); });
  connect(client, &QPushButton::clicked, this, [this] { Q_EMIT roleSelected(Settings::CoreMode::Client); });
  role->addLayout(roles);

  auto *network = section(
      tr("2. Choose how they connect"),
      tr("On the same Wi-Fi or wired network, enter the host’s name or IP on the client. "
         "For Tailscale, sign in to the same Tailnet on both computers, then select the host on the client. "
         "An online Tailscale device still needs Ieum running as a host.")
  );
  auto *networkButton = new QPushButton(tr("Open connection settings"), this);
  networkButton->setObjectName(QStringLiteral("guideNetworkSettings"));
  networkButton->setAutoDefault(false);
  connect(networkButton, &QPushButton::clicked, this, [this] {
    accept();
    Q_EMIT networkSettingsRequested();
  });
  network->addWidget(networkButton, 0, Qt::AlignLeft);

  auto *sharing = section(
      tr("3. Start, approve, and arrange"),
      tr("Press Start on the host and Connect on the client. Compare and approve the certificate fingerprints. "
         "On the host, add the client using its exact “This device” name and choose its position. "
         "Move the mouse across that screen edge to begin.")
  );
  auto *arrange = new QPushButton(tr("Arrange computers on this host"), this);
  arrange->setObjectName(QStringLiteral("guideArrange"));
  arrange->setAutoDefault(false);
  arrange->setEnabled(host->isChecked());
  connect(host, &QPushButton::toggled, arrange, &QPushButton::setEnabled);
  connect(arrange, &QPushButton::clicked, this, [this] {
    accept();
    Q_EMIT arrangementRequested();
  });
  sharing->addWidget(arrange, 0, Qt::AlignLeft);

#ifdef Q_OS_MACOS
  auto *permission = section(
      tr("On Mac: allow input access"),
      tr("In System Settings → Privacy & Security → Accessibility, allow the current Ieum app. "
         "If you replaced an older app, remove the old entry and add the current one from Applications.")
  );
  auto *openPermissions = new QPushButton(tr("Open Accessibility settings"), this);
  openPermissions->setAutoDefault(false);
  connect(openPermissions, &QPushButton::clicked, this, [] { macOSOpenAccessibilitySettings(); });
  permission->addWidget(openPermissions, 0, Qt::AlignLeft);
#endif

  auto *switching = new QLabel(
      tr("Already connected elsewhere? Change the host on the client. Ieum closes the old connection and opens the new "
         "one automatically."),
      contents
  );
  switching->setWordWrap(true);
  steps->addWidget(switching);
  steps->addStretch();
  scroll->setWidget(contents);
  root->addWidget(scroll, 1);
  auto *buttons = new QDialogButtonBox(this);
  auto *done = buttons->addButton(tr("Continue in main window"), QDialogButtonBox::AcceptRole);
  done->setDefault(true);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  root->addWidget(buttons);
}
