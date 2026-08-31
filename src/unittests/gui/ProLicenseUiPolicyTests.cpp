/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/dialogs/ProLicenseUiPolicy.h"

#include <QTest>

using deskflow::gui::FileTransferPlatformSupport;
using deskflow::gui::proFileTransferUiState;
using deskflow::gui::shouldActivateProFileTransferLicense;
using deskflow::licensing::kFileTransferFeature;
using deskflow::licensing::ProLicenseEntitlement;
using deskflow::licensing::ProLicenseStatus;

class ProLicenseUiPolicyTests : public QObject
{
  Q_OBJECT

private:
  static ProLicenseEntitlement entitlement(ProLicenseStatus status, bool includesFileTransfer = true)
  {
    ProLicenseEntitlement result;
    result.status = status;
    if (includesFileTransfer) {
      result.features = {QString::fromLatin1(kFileTransferFeature)};
    }
    return result;
  }

private Q_SLOTS:
  void supportedPlatformsExposeLockedPurchaseFlow_data()
  {
    QTest::addColumn<FileTransferPlatformSupport>("platform");
    QTest::newRow("windows") << FileTransferPlatformSupport::Windows;
    QTest::newRow("macos") << FileTransferPlatformSupport::MacOS;
  }

  void supportedPlatformsExposeLockedPurchaseFlow()
  {
    QFETCH(FileTransferPlatformSupport, platform);
    const auto state = proFileTransferUiState(entitlement(ProLicenseStatus::Missing), false, true, platform, true);

    QVERIFY(state.supportedPlatform);
    QVERIFY(state.licenseActionsEnabled);
    QVERIFY(!state.removeLicenseEnabled);
    QVERIFY(!state.transferControlsEnabled);
    QVERIFY(state.purchaseActionsVisible);
  }

  void perpetualEntitlementUnlocksWindowsAndMac_data()
  {
    QTest::addColumn<FileTransferPlatformSupport>("platform");
    QTest::newRow("windows") << FileTransferPlatformSupport::Windows;
    QTest::newRow("macos") << FileTransferPlatformSupport::MacOS;
  }

  void perpetualEntitlementUnlocksWindowsAndMac()
  {
    QFETCH(FileTransferPlatformSupport, platform);
    const auto state = proFileTransferUiState(entitlement(ProLicenseStatus::Valid), true, true, platform, true);

    QVERIFY(state.supportedPlatform);
    QVERIFY(state.licenseActionsEnabled);
    QVERIFY(state.removeLicenseEnabled);
    QVERIFY(state.transferControlsEnabled);
    QVERIFY(!state.purchaseActionsVisible);
  }

  void unsupportedPlatformHidesFilesAndPurchaseFlow()
  {
    const auto state = proFileTransferUiState(
        entitlement(ProLicenseStatus::Missing), false, true, FileTransferPlatformSupport::Unsupported, true
    );

    QVERIFY(!state.supportedPlatform);
    QVERIFY(!state.licenseActionsEnabled);
    QVERIFY(!state.transferControlsEnabled);
    QVERIFY(!state.purchaseActionsVisible);
  }

  void invalidAndMissingFeatureLicensesStayLocked_data()
  {
    QTest::addColumn<ProLicenseStatus>("status");
    QTest::newRow("missing") << ProLicenseStatus::Missing;
    QTest::newRow("too-large") << ProLicenseStatus::TooLarge;
    QTest::newRow("malformed") << ProLicenseStatus::Malformed;
    QTest::newRow("unsupported") << ProLicenseStatus::Unsupported;
    QTest::newRow("bad-signature") << ProLicenseStatus::InvalidSignature;
    QTest::newRow("not-yet-valid") << ProLicenseStatus::NotYetValid;
    QTest::newRow("expired") << ProLicenseStatus::Expired;
  }

  void invalidAndMissingFeatureLicensesStayLocked()
  {
    QFETCH(ProLicenseStatus, status);
    const auto state =
        proFileTransferUiState(entitlement(status), true, true, FileTransferPlatformSupport::Windows, true);
    QVERIFY(!state.transferControlsEnabled);
    QVERIFY(state.purchaseActionsVisible);
    QVERIFY(!shouldActivateProFileTransferLicense(entitlement(status)));

    const auto missingFeature = entitlement(ProLicenseStatus::Valid, false);
    QVERIFY(!proFileTransferUiState(missingFeature, true, true, FileTransferPlatformSupport::MacOS, true)
                 .transferControlsEnabled);
    QVERIFY(!shouldActivateProFileTransferLicense(missingFeature));
  }

  void runtimeRequirementsKeepTransferControlsLocked_data()
  {
    QTest::addColumn<bool>("writable");
    QTest::addColumn<bool>("tls");
    QTest::newRow("read-only") << false << true;
    QTest::newRow("tls-disabled") << true << false;
  }

  void runtimeRequirementsKeepTransferControlsLocked()
  {
    QFETCH(bool, writable);
    QFETCH(bool, tls);
    const auto state = proFileTransferUiState(
        entitlement(ProLicenseStatus::Valid), true, writable, FileTransferPlatformSupport::Windows, tls
    );

    QVERIFY(!state.transferControlsEnabled);
    QCOMPARE(state.licenseActionsEnabled, writable);
    QVERIFY(!state.purchaseActionsVisible);
  }
};

QTEST_GUILESS_MAIN(ProLicenseUiPolicyTests)

#include "ProLicenseUiPolicyTests.moc"
