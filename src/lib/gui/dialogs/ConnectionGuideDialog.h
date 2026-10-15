/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Settings.h"

#include <QDialog>

class ConnectionGuideDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit ConnectionGuideDialog(QWidget *parent = nullptr);

Q_SIGNALS:
  void roleSelected(Settings::CoreMode role);
  void networkSettingsRequested();
  void arrangementRequested();
};
