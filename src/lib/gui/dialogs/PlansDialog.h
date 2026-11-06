/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once
#include "licensing/PlanCatalog.h"
#include <QDialog>
#include <QMap>
#include <QUrl>
class QLabel;
class QPushButton;
class QComboBox;
class QLineEdit;
class QToolButton;
namespace deskflow::gui {
class PlansDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit PlansDialog(QWidget *parent = nullptr);
  void preparePurchase(licensing::Plan plan, const QString &key);

private:
  void loadCatalog();
  void updateStatus();
  QLabel *m_status = nullptr;
  QLabel *m_availability = nullptr;
  QMap<QString, QPushButton *> m_purchaseButtons;
  QComboBox *m_restorePlan = nullptr;
  QLineEdit *m_restoreKey = nullptr;
  QToolButton *m_restoreToggle = nullptr;
};
} // namespace deskflow::gui
