/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "licensing/ProLicense.h"

namespace deskflow::gui {

struct ProFileTransferUiState
{
  bool licenseActionsEnabled{false};
  bool removeLicenseEnabled{false};
  bool transferControlsEnabled{false};
};

[[nodiscard]] bool shouldActivateProFileTransferLicense(const licensing::ProLicenseEntitlement &entitlement) noexcept;

[[nodiscard]] ProFileTransferUiState proFileTransferUiState(
    const licensing::ProLicenseEntitlement &entitlement, bool hasStoredLicense, bool settingsWritable,
    bool windowsPlatform, bool tlsEnabled
) noexcept;

} // namespace deskflow::gui
