/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "HotkeyDialog.h"
#include "gui/StyleUtils.h"
#include "ui_HotkeyDialog.h"

#include <QLabel>
#include <QVBoxLayout>

HotkeyDialog::HotkeyDialog(QWidget *parent, Hotkey &hotkey)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::HotkeyDialog>()},
      m_Hotkey(hotkey)
{
  ui->setupUi(this);
  deskflow::gui::applyIeumDialogStyle(*this);
  auto *hint = new QLabel(
      tr("Click the shortcut field, then press the keys you want to use. Configure its action after saving."), this
  );
  hint->setWordWrap(true);
  qobject_cast<QVBoxLayout *>(layout())->insertWidget(0, hint);
  setMinimumWidth(420);

  ui->m_pKeySequenceWidgetHotkey->setText(m_Hotkey.text());
}

HotkeyDialog::~HotkeyDialog() = default;

void HotkeyDialog::accept()
{
  if (!sequenceWidget()->valid())
    return;

  hotkey().setKeySequence(sequenceWidget()->keySequence());
  QDialog::accept();
}

const KeySequenceWidget *HotkeyDialog::sequenceWidget() const
{
  return ui->m_pKeySequenceWidgetHotkey;
}
