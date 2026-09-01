/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDialog>
#include <QList>

#include <optional>

namespace deskflow::gui {

enum class MacAccessibilityStep
{
  ShowApplication,
  ResetPreviousApproval,
  RegisterCurrentApplication,
  OpenAccessibilitySettings,
  CheckPermission
};

[[nodiscard]] QList<MacAccessibilityStep> macAccessibilityGuideSteps();

class MacAccessibilityGuideDialog final : public QDialog
{
public:
  explicit MacAccessibilityGuideDialog(const QString &message, QWidget *parent = nullptr);

  [[nodiscard]] std::optional<MacAccessibilityStep> selectedStep() const noexcept
  {
    return m_selectedStep;
  }

private:
  std::optional<MacAccessibilityStep> m_selectedStep;
};

} // namespace deskflow::gui
