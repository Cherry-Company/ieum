/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "NetworkInterfacesTests.h"

#include "common/NetworkInterfaces.h"

using deskflow::network::InterfaceAddress;
using deskflow::network::NetworkInterfaces;

namespace {

InterfaceAddress candidate(
    const QString &name, const QString &address, QNetworkInterface::InterfaceType type = QNetworkInterface::Ethernet,
    QNetworkInterface::InterfaceFlags flags = QNetworkInterface::IsUp | QNetworkInterface::IsRunning
)
{
  return {name, name, type, flags, QHostAddress(address)};
}

} // namespace

void NetworkInterfacesTests::ordersPhysicalNetworksBeforeVirtualNetworks()
{
  const QList<InterfaceAddress> addresses = {
      candidate(QStringLiteral("Tailscale"), QStringLiteral("100.68.129.102"), QNetworkInterface::Unknown),
      candidate(QStringLiteral("Wi-Fi"), QStringLiteral("192.168.50.20"), QNetworkInterface::Wifi),
      candidate(QStringLiteral("Ethernet"), QStringLiteral("10.5.11.69")),
      candidate(QStringLiteral("Ethernet"), QStringLiteral("2001:db8::20")),
  };

  QCOMPARE(
      NetworkInterfaces::orderedAddresses(addresses),
      QStringList(
          {QStringLiteral("10.5.11.69"), QStringLiteral("192.168.50.20"), QStringLiteral("100.68.129.102"),
           QStringLiteral("2001:db8::20")}
      )
  );
  QCOMPARE(NetworkInterfaces::preferredPhysicalAddress(addresses), QStringLiteral("10.5.11.69"));
}

void NetworkInterfacesTests::ignoresUnavailableAndLocalAddresses()
{
  const QList<InterfaceAddress> addresses = {
      candidate(
          QStringLiteral("Ethernet"), QStringLiteral("10.0.0.2"), QNetworkInterface::Ethernet,
          QNetworkInterface::IsUp
      ),
      candidate(QStringLiteral("Ethernet"), QStringLiteral("169.254.1.2")),
      candidate(
          QStringLiteral("Loopback"), QStringLiteral("127.0.0.1"), QNetworkInterface::Loopback,
          QNetworkInterface::IsUp | QNetworkInterface::IsRunning | QNetworkInterface::IsLoopBack
      ),
  };

  QVERIFY(NetworkInterfaces::orderedAddresses(addresses).isEmpty());
  QVERIFY(NetworkInterfaces::preferredPhysicalAddress(addresses).isEmpty());
}

void NetworkInterfacesTests::resolvesAutomaticServerBinding()
{
  QCOMPARE(
      NetworkInterfaces::resolveServerBindAddress(QStringLiteral("192.168.1.10"), true),
      QStringLiteral("192.168.1.10")
  );
  QVERIFY(NetworkInterfaces::resolveServerBindAddress(QString(), false).isEmpty());
}

QTEST_MAIN(NetworkInterfacesTests)
