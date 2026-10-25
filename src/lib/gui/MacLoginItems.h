/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <functional>

namespace deskflow::gui {

enum class MacLoginItemStatus
{
  NotRegistered,
  Enabled,
  RequiresApproval,
  NotFound
};

struct MacLoginItemOperations
{
  std::function<MacLoginItemStatus()> status;
  std::function<bool(bool enabled, QString *error)> setEnabled;
};

inline bool reconcileMacLoginItems(
    bool enabled, const MacLoginItemOperations &mainApp, const MacLoginItemOperations &legacyAgent, QString *error
)
{
  const auto status = mainApp.status();
  const bool alreadyRegistered =
      status == MacLoginItemStatus::Enabled || status == MacLoginItemStatus::RequiresApproval;
  const bool changeMainApp = enabled ? !alreadyRegistered : status != MacLoginItemStatus::NotRegistered;
  QString mainError;
  const bool mainOk = !changeMainApp || mainApp.setEnabled(enabled, &mainError);

  // Register the replacement first: unregistering an agent can terminate its
  // running process. Always retire the old job, even when the main app already
  // has the desired status or registration failed. Otherwise it can keep
  // producing launch-constraint crash reports on every login.
  QString legacyError;
  const bool legacyOk =
      legacyAgent.status() == MacLoginItemStatus::NotRegistered || legacyAgent.setEnabled(false, &legacyError);
  if (error != nullptr) {
    *error = mainError;
    if (!legacyOk) {
      if (!error->isEmpty()) {
        error->append(QLatin1Char('\n'));
      }
      error->append(QStringLiteral("Could not remove the previous Ieum login agent: %1").arg(legacyError));
    }
  }
  return mainOk && legacyOk;
}

} // namespace deskflow::gui
