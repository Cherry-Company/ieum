/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "NetworkInterfaces.h"

#include <QAbstractSocket>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <tuple>
#include <utility>

namespace deskflow::network {

namespace {

struct RankedAddress
{
  QHostAddress address;
  bool isVirtual = false;
  int interfaceRank = 0;
};

int interfaceTypeRank(QNetworkInterface::InterfaceType type)
{
  switch (type) {
  case QNetworkInterface::Ethernet:
    return 0;
  case QNetworkInterface::Wifi:
    return 1;
  case QNetworkInterface::Unknown:
    return 2;
  default:
    return 3;
  }
}

bool isUsable(const InterfaceAddress &candidate)
{
  const auto protocol = candidate.address.protocol();
  return (candidate.flags & QNetworkInterface::IsUp) && (candidate.flags & QNetworkInterface::IsRunning) &&
         !(candidate.flags & QNetworkInterface::IsLoopBack) &&
         protocol != QAbstractSocket::UnknownNetworkLayerProtocol && !candidate.address.isNull() &&
         !candidate.address.isLoopback() && !candidate.address.isLinkLocal() && !candidate.address.isMulticast();
}

auto addressSortKey(const RankedAddress &candidate)
{
  const auto protocolRank = candidate.address.protocol() == QAbstractSocket::IPv4Protocol ? 0 : 1;
  const auto virtualRank = candidate.isVirtual ? 1 : 0;
  const auto privateRank = candidate.address.isPrivateUse() ? 0 : 1;
  const auto addressText = candidate.address.protocol() == QAbstractSocket::IPv4Protocol
                               ? QStringLiteral("%1").arg(candidate.address.toIPv4Address(), 10, 10, QLatin1Char('0'))
                               : candidate.address.toString();
  return std::tuple(protocolRank, virtualRank, privateRank, candidate.interfaceRank, addressText);
}

} // namespace

bool NetworkInterfaces::isVirtualInterfaceName(const QString &interfaceName)
{
  static const auto virtualRegEx = QRegularExpression(
      QStringLiteral(
          R"((^|[\s_()/-])(vboxnet[0-9]*|vmnet[0-9]*|vmware|docker|virbr[0-9]*|veth[0-9a-f]*|br[-_][0-9a-z]+|tun[0-9]*|utun[0-9]*|awdl[0-9]*|p2p[0-9]*|llw[0-9]*|anpi[0-9]*|tap[0-9]*|tailscale|zerotier|wireguard|wg[0-9]+|hamachi|vethernet|hyper[- ]?v|wsl|vpn|bridge)(?=$|[\s_()/-]))"
      ),
      QRegularExpression::CaseInsensitiveOption
  );
  return virtualRegEx.match(interfaceName).hasMatch();
}

bool NetworkInterfaces::isVirtualInterface(const InterfaceAddress &interfaceAddress)
{
  return interfaceAddress.type == QNetworkInterface::Virtual ||
         (interfaceAddress.flags & QNetworkInterface::IsPointToPoint) ||
         isVirtualInterfaceName(interfaceAddress.systemName) || isVirtualInterfaceName(interfaceAddress.displayName);
}

QList<InterfaceAddress> NetworkInterfaces::localAddresses()
{
  QList<InterfaceAddress> result;
  for (const auto &interface : QNetworkInterface::allInterfaces()) {
    for (const auto &entry : interface.addressEntries()) {
      result.append({interface.name(), interface.humanReadableName(), interface.type(), interface.flags(), entry.ip()});
    }
  }
  return result;
}

QStringList NetworkInterfaces::orderedAddresses(const QList<InterfaceAddress> &addresses, bool includeVirtual)
{
  QList<RankedAddress> ranked;
  for (const auto &candidate : addresses) {
    if (!isUsable(candidate)) {
      continue;
    }

    const auto isVirtual = isVirtualInterface(candidate);
    if (isVirtual && !includeVirtual) {
      continue;
    }

    ranked.append({candidate.address, isVirtual, interfaceTypeRank(candidate.type)});
  }

  std::ranges::sort(ranked, [](const auto &left, const auto &right) {
    return addressSortKey(left) < addressSortKey(right);
  });

  QStringList result;
  QSet<QHostAddress> seen;
  for (const auto &candidate : std::as_const(ranked)) {
    if (seen.contains(candidate.address)) {
      continue;
    }
    seen.insert(candidate.address);
    result.append(candidate.address.toString());
  }
  return result;
}

QStringList NetworkInterfaces::validAddresses()
{
  return orderedAddresses(localAddresses());
}

QStringList NetworkInterfaces::physicalAddresses()
{
  return orderedAddresses(localAddresses(), false);
}

QString NetworkInterfaces::preferredPhysicalAddress()
{
  return preferredPhysicalAddress(localAddresses());
}

QString NetworkInterfaces::preferredPhysicalAddress(const QList<InterfaceAddress> &addresses)
{
  const auto physical = orderedAddresses(addresses, false);
  return physical.isEmpty() ? QString() : physical.first();
}

QString NetworkInterfaces::resolveServerBindAddress(const QString &configuredAddress, bool preferPhysical)
{
  if (!configuredAddress.isEmpty() || !preferPhysical) {
    return configuredAddress;
  }
  return preferredPhysicalAddress();
}

} // namespace deskflow::network
