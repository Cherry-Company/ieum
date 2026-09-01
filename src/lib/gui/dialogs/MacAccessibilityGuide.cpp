/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacAccessibilityGuide.h"

#include "gui/ProductIdentity.h"

#include <QCoreApplication>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace deskflow::gui {

namespace {

QString stepLabel(MacAccessibilityStep step)
{
  switch (step) {
  case MacAccessibilityStep::ShowApplication:
    return QCoreApplication::translate("MacAccessibility", "1. Show Ieum in Applications");
  case MacAccessibilityStep::ResetPreviousApproval:
    return QCoreApplication::translate("MacAccessibility", "2. Reset Previous Approval (if needed)");
  case MacAccessibilityStep::RegisterCurrentApplication:
    return QCoreApplication::translate("MacAccessibility", "3. Register This Ieum App");
  case MacAccessibilityStep::OpenAccessibilitySettings:
    return QCoreApplication::translate("MacAccessibility", "4. Open Accessibility Settings");
  case MacAccessibilityStep::CheckPermission:
    return QCoreApplication::translate("MacAccessibility", "5. Check Permission");
  }
  return {};
}

QString stepObjectName(MacAccessibilityStep step)
{
  switch (step) {
  case MacAccessibilityStep::ShowApplication:
    return QStringLiteral("macAccessibilityShowApplication");
  case MacAccessibilityStep::ResetPreviousApproval:
    return QStringLiteral("macAccessibilityResetApproval");
  case MacAccessibilityStep::RegisterCurrentApplication:
    return QStringLiteral("macAccessibilityRegisterApplication");
  case MacAccessibilityStep::OpenAccessibilitySettings:
    return QStringLiteral("macAccessibilityOpenSettings");
  case MacAccessibilityStep::CheckPermission:
    return QStringLiteral("macAccessibilityCheckPermission");
  }
  return {};
}

} // namespace

QList<MacAccessibilityStep> macAccessibilityGuideSteps()
{
  return {
      MacAccessibilityStep::ShowApplication,
      MacAccessibilityStep::ResetPreviousApproval,
      MacAccessibilityStep::RegisterCurrentApplication,
      MacAccessibilityStep::OpenAccessibilitySettings,
      MacAccessibilityStep::CheckPermission,
  };
}

MacAccessibilityGuideDialog::MacAccessibilityGuideDialog(const QString &message, QWidget *parent) : QDialog(parent)
{
  setWindowTitle(productDisplayName());
  setModal(true);
  setMinimumWidth(520);

  auto *layout = new QVBoxLayout(this);
  layout->setSpacing(10);

  auto *instructions = new QLabel(message, this);
  instructions->setWordWrap(true);
  instructions->setTextFormat(Qt::PlainText);
  layout->addWidget(instructions);

  for (const auto step : macAccessibilityGuideSteps()) {
    auto *button = new QPushButton(stepLabel(step), this);
    button->setObjectName(stepObjectName(step));
    button->setMinimumHeight(36);
    connect(button, &QPushButton::clicked, this, [this, step] {
      m_selectedStep = step;
      accept();
    });
    layout->addWidget(button);
  }

  auto *cancelButton = new QPushButton(QCoreApplication::translate("MacAccessibility", "Cancel"), this);
  cancelButton->setObjectName(QStringLiteral("macAccessibilityCancel"));
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  layout->addWidget(cancelButton);
}

} // namespace deskflow::gui
