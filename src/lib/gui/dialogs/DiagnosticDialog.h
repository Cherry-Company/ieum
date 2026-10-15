/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/Diagnostic.h"
#include "gui/ExternalUrlLauncher.h"

#include <QDialog>

class DiagnosticDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit DiagnosticDialog(
      const deskflow::gui::diagnostic::Report &report, QWidget *parent = nullptr, const QString &explanation = {},
      deskflow::gui::ExternalUrlOpener opener = {}
  );
};
