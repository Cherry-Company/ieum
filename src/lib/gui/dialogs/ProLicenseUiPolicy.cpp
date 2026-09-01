/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ProLicenseUiPolicy.h"

namespace deskflow::gui {

bool shouldActivateProFileTransferLicense(const licensing::ProLicenseEntitlement &entitlement) noexcept
{
  return entitlement.status == licensing::ProLicenseStatus::Valid &&
         entitlement.permits(QString::fromLatin1(licensing::kFileTransferFeature));
}

ProFileTransferUiState proFileTransferUiState(
    const licensing::ProLicenseEntitlement &entitlement, bool hasStoredLicense, bool settingsWritable,
    FileTransferPlatformSupport platform, bool tlsEnabled
) noexcept
{
  const auto supportedPlatform = platform != FileTransferPlatformSupport::Unsupported;
  const auto entitled = shouldActivateProFileTransferLicense(entitlement);
  const auto licenseActionsEnabled = settingsWritable && supportedPlatform;
  return {
      .licenseActionsEnabled = licenseActionsEnabled,
      .removeLicenseEnabled = licenseActionsEnabled && hasStoredLicense,
      .transferControlsEnabled = licenseActionsEnabled && tlsEnabled && entitled,
      .supportedPlatform = supportedPlatform,
      .purchaseActionsVisible = supportedPlatform && !entitled,
  };
}

} // namespace deskflow::gui
