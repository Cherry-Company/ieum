/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TailscaleIntegrationTests.h"

#include "common/TailscaleIntegration.h"

using deskflow::network::TailscaleIntegration;
using deskflow::network::TailscaleState;

void TailscaleIntegrationTests::parsesRunningStatus()
{
  const auto json = QByteArrayLiteral(R"json(
    {
      "Version": "1.98.4",
      "BackendState": "Running",
      "TailscaleIPs": ["fd7a:115c:a1e0::1", "100.68.129.102"],
      "Self": {"DNSName": "this-pc.example.ts.net."},
      "Peer": {
        "nodekey:mac": {
          "ID": "mac-id",
          "DNSName": "studio-mac.example.ts.net.",
          "HostName": "Mac Studio",
          "OS": "macOS",
          "TailscaleIPs": ["fd7a:115c:a1e0::2", "100.92.158.1"],
          "Online": true
        },
        "nodekey:phone": {
          "ID": "phone-id",
          "DNSName": "phone.example.ts.net.",
          "OS": "iOS",
          "TailscaleIPs": ["100.85.45.87"],
          "Online": true
        },
        "nodekey:offline": {
          "ID": "offline-id",
          "DNSName": "offline-pc.example.ts.net.",
          "OS": "windows",
          "TailscaleIPs": ["100.117.91.86"],
          "Online": false
        }
      }
    }
  )json");

  const auto status = TailscaleIntegration::parseStatus(json);
  QCOMPARE(status.state, TailscaleState::Running);
  QVERIFY(status.isReady());
  QCOMPARE(status.localDnsName, QStringLiteral("this-pc.example.ts.net"));
  QCOMPARE(status.preferredLocalAddress(), QStringLiteral("100.68.129.102"));

  const auto peers = status.onlineDesktopPeers();
  QCOMPARE(peers.size(), 1);
  QCOMPARE(peers.first().name, QStringLiteral("studio-mac"));
  QCOMPARE(peers.first().preferredAddress(), QStringLiteral("100.92.158.1"));
}

void TailscaleIntegrationTests::parsesLoginState()
{
  const auto status =
      TailscaleIntegration::parseStatus(QByteArrayLiteral(R"json({"BackendState":"NeedsLogin","Peer":{}})json"));
  QCOMPARE(status.state, TailscaleState::NeedsLogin);
  QVERIFY(!status.isReady());
}

void TailscaleIntegrationTests::parsesMachineAuthorizationState()
{
  const auto status =
      TailscaleIntegration::parseStatus(QByteArrayLiteral(R"json({"BackendState":"NeedsMachineAuth","Peer":{}})json"));
  QCOMPARE(status.state, TailscaleState::NeedsLogin);
  QVERIFY(!status.isReady());
}

void TailscaleIntegrationTests::rejectsMalformedStatus()
{
  const auto status = TailscaleIntegration::parseStatus(QByteArrayLiteral("{not-json"));
  QCOMPARE(status.state, TailscaleState::Error);
  QVERIFY(!status.error.isEmpty());
}

QTEST_MAIN(TailscaleIntegrationTests)
