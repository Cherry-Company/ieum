/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "licensing/ProLicense.h"

#include <QMetaType>

namespace deskflow::gui {

enum class FileTransferPlatformSupport
{
  Unsupported,
  Windows,
  MacOS,
};

struct ProFileTransferUiState
{
  bool licenseActionsEnabled{false};
  bool removeLicenseEnabled{false};
  bool transferControlsEnabled{false};
  bool supportedPlatform{false};
  bool purchaseActionsVisible{false};
};

[[nodiscard]] bool shouldActivateProFileTransferLicense(const licensing::ProLicenseEntitlement &entitlement) noexcept;

[[nodiscard]] ProFileTransferUiState proFileTransferUiState(
    const licensing::ProLicenseEntitlement &entitlement, bool hasStoredLicense, bool settingsWritable,
    FileTransferPlatformSupport platform, bool tlsEnabled
) noexcept;

} // namespace deskflow::gui

Q_DECLARE_METATYPE(deskflow::gui::FileTransferPlatformSupport)
