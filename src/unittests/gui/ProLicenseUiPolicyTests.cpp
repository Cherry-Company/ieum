/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/dialogs/ProLicenseUiPolicy.h"

#include <QTest>

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
  void validLicenseUnlocksFileTransferWhenRuntimeRequirementsAreMet()
  {
    const auto state = proFileTransferUiState(entitlement(ProLicenseStatus::Valid), true, true, true, true);

    QVERIFY(state.licenseActionsEnabled);
    QVERIFY(state.removeLicenseEnabled);
    QVERIFY(state.transferControlsEnabled);
  }

  void invalidLicenseStatesStayLocked_data()
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

  void invalidLicenseStatesStayLocked()
  {
    QFETCH(ProLicenseStatus, status);
    const auto state = proFileTransferUiState(entitlement(status), true, true, true, true);

    QVERIFY(!state.transferControlsEnabled);
    QVERIFY(!shouldActivateProFileTransferLicense(entitlement(status)));
  }

  void missingFeatureStaysLocked()
  {
    const auto license = entitlement(ProLicenseStatus::Valid, false);
    const auto state = proFileTransferUiState(license, true, true, true, true);

    QVERIFY(!state.transferControlsEnabled);
    QVERIFY(!shouldActivateProFileTransferLicense(license));
  }

  void runtimeRequirementsKeepFileTransferLocked_data()
  {
    QTest::addColumn<bool>("writable");
    QTest::addColumn<bool>("windows");
    QTest::addColumn<bool>("tls");
    QTest::newRow("read-only") << false << true << true;
    QTest::newRow("other-platform") << true << false << true;
    QTest::newRow("tls-disabled") << true << true << false;
  }

  void runtimeRequirementsKeepFileTransferLocked()
  {
    QFETCH(bool, writable);
    QFETCH(bool, windows);
    QFETCH(bool, tls);
    const auto state = proFileTransferUiState(entitlement(ProLicenseStatus::Valid), true, writable, windows, tls);

    QVERIFY(!state.transferControlsEnabled);
  }

  void licenseActionsRequireWritableWindowsSettings()
  {
    const auto license = entitlement(ProLicenseStatus::Valid);

    QVERIFY(!proFileTransferUiState(license, true, false, true, true).licenseActionsEnabled);
    QVERIFY(!proFileTransferUiState(license, true, true, false, true).licenseActionsEnabled);
    QVERIFY(proFileTransferUiState(license, true, true, true, false).licenseActionsEnabled);
    QVERIFY(!proFileTransferUiState(license, false, true, true, true).removeLicenseEnabled);
  }
};

QTEST_GUILESS_MAIN(ProLicenseUiPolicyTests)

#include "ProLicenseUiPolicyTests.moc"
