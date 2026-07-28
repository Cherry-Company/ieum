/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Cherry Inc.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

class QString;

namespace deskflow::gui {

class StartupManager final
{
public:
  [[nodiscard]] static bool isSupported();
  [[nodiscard]] static bool isEnabled();
  [[nodiscard]] static bool requiresApproval();
  static bool setEnabled(bool enabled, QString *error = nullptr);
  static bool reconcile(QString *error = nullptr);
  static void openSystemSettings();
};

} // namespace deskflow::gui
