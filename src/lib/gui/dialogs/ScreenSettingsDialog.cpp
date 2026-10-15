/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenSettingsDialog.h"
#include "gui/StyleUtils.h"
#include "ui_ScreenSettingsDialog.h"

#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "gui/config/Screen.h"
#include "validators/AliasValidator.h"
#include "validators/ScreenNameValidator.h"
#include "validators/ValidationError.h"

#include <QMessageBox>

using enum ScreenConfig::Modifier;
using enum ScreenConfig::SwitchCorner;
using enum ScreenConfig::Fix;

ScreenSettingsDialog::~ScreenSettingsDialog() = default;

ScreenSettingsDialog::ScreenSettingsDialog(QWidget *parent, Screen *screen, const ScreenList *screens)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::ScreenSettingsDialog>()},
      m_screen(screen)
{

  ui->setupUi(this);
  ui->buttonBox->button(QDialogButtonBox::Cancel)->setFocus();

  ui->lineNameEdit->setText(m_screen->name());

  const auto valNameError = new validators::ValidationError(this, ui->lblNameError);
  const auto valName = new validators::ScreenNameValidator(ui->lineNameEdit, valNameError, screens);
  ui->lineNameEdit->setValidator(valName);

  const auto valAliasError = new validators::ValidationError(this, ui->lblAliasError);
  const auto valAlias = new validators::AliasValidator(ui->lineAddAlias, valAliasError);
  ui->lineAddAlias->setValidator(valAlias);

  for (int i = 0; i < m_screen->aliases().count(); i++)
    new QListWidgetItem(m_screen->aliases()[i], ui->listAliases);

  ui->comboShift->setCurrentIndex(m_screen->modifier(static_cast<int>(Shift)));
  ui->comboCtrl->setCurrentIndex(m_screen->modifier(static_cast<int>(Ctrl)));
  ui->comboAlt->setCurrentIndex(m_screen->modifier(static_cast<int>(Alt)));
  ui->comboMeta->setCurrentIndex(m_screen->modifier(static_cast<int>(Meta)));
  ui->comboSuper->setCurrentIndex(m_screen->modifier(static_cast<int>(Super)));
  ui->comboAltGr->setCurrentIndex(m_screen->modifier(static_cast<int>(AltGr)));

  ui->chkDeadTopLeft->setChecked(m_screen->switchCorner(static_cast<int>(TopLeft)));
  ui->chkDeadTopRight->setChecked(m_screen->switchCorner(static_cast<int>(TopRight)));
  ui->chkDeadBottomLeft->setChecked(m_screen->switchCorner(static_cast<int>(BottomLeft)));
  ui->chkDeadBottomRight->setChecked(m_screen->switchCorner(static_cast<int>(BottomRight)));
  ui->sbSwitchCornerSize->setValue(m_screen->switchCornerSize());

  ui->chkFixCapsLock->setChecked(m_screen->fix(CapsLock));
  ui->chkFixNumLock->setChecked(m_screen->fix(NumLock));
  ui->chkFixScrollLock->setChecked(m_screen->fix(ScrollLock));
  ui->chkFixXTest->setChecked(m_screen->fix(XTest));

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ScreenSettingsDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ScreenSettingsDialog::reject);
  connect(ui->btnAddAlias, &QPushButton::clicked, this, &ScreenSettingsDialog::addAlias);
  connect(ui->btnRemoveAlias, &QPushButton::clicked, this, &ScreenSettingsDialog::removeAlias);
  connect(ui->lineAddAlias, &QLineEdit::textChanged, this, &ScreenSettingsDialog::checkNewAliasName);
  connect(ui->listAliases, &QListWidget::itemSelectionChanged, this, &ScreenSettingsDialog::aliasSelected);
  createNavigation();
}

void ScreenSettingsDialog::createNavigation()
{
  deskflow::gui::applyIeumDialogStyle(*this);
  setMinimumSize(740, 500);
  resize(820, 600);
  auto *root = qobject_cast<QVBoxLayout *>(layout());
  root->removeWidget(ui->groupComputerName);
  root->removeItem(ui->layoutOptions);
  delete ui->layoutOptions;
  auto *heading = new QLabel(tr("Settings for %1").arg(m_screen->name()), this);
  heading->setTextFormat(Qt::PlainText);
  heading->setProperty("sectionHeading", true);
  root->insertWidget(0, heading);
  auto *navigation = new QListWidget(this);
  navigation->setObjectName(QStringLiteral("settingsNavigation"));
  navigation->setAccessibleName(tr("Computer settings categories"));
  navigation->setFixedWidth(180);
  auto *pages = new QStackedWidget(this);
  pages->setObjectName(QStringLiteral("computerSettingsPages"));
  const QStringList titles{tr("Name & aliases"), tr("Keyboard mapping"), tr("Pointer & compatibility")};
  const QStringList descriptions{
      tr("Use the exact “This device” name shown on that computer. Aliases let this layout recognize another name for "
         "the same computer."),
      tr("Remap modifier keys for this computer. Keep the defaults unless your keyboards or shortcuts need a different "
         "mapping."),
      tr("Dead corners keep the pointer on this screen near its corners. Compatibility fixes apply only to this "
         "computer.")
  };
  const QList<QList<QWidget *>> groups{
      {ui->groupComputerName, ui->groupAliases}, {ui->groupModifers}, {ui->groupDeadCorners, ui->groupFixes}
  };
  for (int i = 0; i < titles.size(); ++i) {
    auto *item = new QListWidgetItem(titles[i], navigation);
    item->setSizeHint(QSize(160, 48));
    auto *content = new QWidget;
    auto *pageLayout = new QVBoxLayout(content);
    auto *description = new QLabel(descriptions[i], content);
    description->setWordWrap(true);
    pageLayout->addWidget(description);
    for (auto *group : groups[i]) {
      pageLayout->addWidget(group);
    }
    pageLayout->addStretch();
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(content);
    pages->addWidget(scroll);
  }
  auto *body = new QHBoxLayout;
  body->addWidget(navigation);
  body->addWidget(pages, 1);
  root->insertLayout(1, body, 1);
  connect(navigation, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
  navigation->setCurrentRow(0);
}

void ScreenSettingsDialog::accept()
{
  if (ui->lineNameEdit->text().isEmpty()) {
    QMessageBox::warning(
        this, tr("Screen name is empty"),
        tr("The screen name cannot be empty. "
           "Please either fill in a name or cancel the dialog.")
    );
    return;
  }
  if (!ui->lblNameError->text().isEmpty()) {
    return;
  }

  m_screen->setName(ui->lineNameEdit->text());

  m_screen->aliases().clear();

  for (int i = 0; i < ui->listAliases->count(); i++) {
    QString alias(ui->listAliases->item(i)->text());
    if (alias == ui->lineNameEdit->text()) {
      QMessageBox::warning(
          this, tr("Screen name matches alias"),
          tr("The screen name cannot be the same as an alias. "
             "Please either remove the alias or change the screen name.")
      );
      return;
    }
    if (!m_screen->aliases().contains(alias))
      m_screen->addAlias(alias);
  }

  m_screen->setModifier(Shift, ui->comboShift->currentIndex());
  m_screen->setModifier(Ctrl, ui->comboCtrl->currentIndex());
  m_screen->setModifier(Alt, ui->comboAlt->currentIndex());
  m_screen->setModifier(Meta, ui->comboMeta->currentIndex());
  m_screen->setModifier(Super, ui->comboSuper->currentIndex());
  m_screen->setModifier(AltGr, ui->comboAltGr->currentIndex());

  m_screen->setSwitchCorner(TopLeft, ui->chkDeadTopLeft->isChecked());
  m_screen->setSwitchCorner(TopRight, ui->chkDeadTopRight->isChecked());
  m_screen->setSwitchCorner(BottomLeft, ui->chkDeadBottomLeft->isChecked());
  m_screen->setSwitchCorner(BottomRight, ui->chkDeadBottomRight->isChecked());
  m_screen->setSwitchCornerSize(ui->sbSwitchCornerSize->value());

  m_screen->setFix(CapsLock, ui->chkFixCapsLock->isChecked());
  m_screen->setFix(NumLock, ui->chkFixNumLock->isChecked());
  m_screen->setFix(ScrollLock, ui->chkFixScrollLock->isChecked());
  m_screen->setFix(XTest, ui->chkFixXTest->isChecked());

  QDialog::accept();
}

void ScreenSettingsDialog::addAlias()
{
  if (!ui->lineAddAlias->text().isEmpty() &&
      ui->listAliases->findItems(ui->lineAddAlias->text(), Qt::MatchFixedString).isEmpty()) {
    new QListWidgetItem(ui->lineAddAlias->text(), ui->listAliases);
    ui->lineAddAlias->clear();
  }
}

void ScreenSettingsDialog::removeAlias() const
{
  QList<QListWidgetItem *> items = ui->listAliases->selectedItems();
  qDeleteAll(items);
}

void ScreenSettingsDialog::checkNewAliasName(const QString &text)
{
  ui->btnAddAlias->setEnabled(!text.isEmpty() && ui->lblAliasError->text().isEmpty());
}

void ScreenSettingsDialog::aliasSelected()
{
  ui->btnRemoveAlias->setEnabled(!ui->listAliases->selectedItems().isEmpty());
}
