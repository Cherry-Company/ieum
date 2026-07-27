/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QHostAddress>
#include <QList>
#include <QNetworkInterface>
#include <QString>
#include <QStringList>

namespace deskflow::network {

struct InterfaceAddress
{
  QString systemName;
  QString displayName;
  QNetworkInterface::InterfaceType type = QNetworkInterface::Unknown;
  QNetworkInterface::InterfaceFlags flags;
  QHostAddress address;
};

class NetworkInterfaces
{
public:
  static bool isVirtualInterfaceName(const QString &interfaceName);
  static bool isVirtualInterface(const InterfaceAddress &interfaceAddress);

  static QList<InterfaceAddress> localAddresses();
  static QStringList orderedAddresses(const QList<InterfaceAddress> &addresses, bool includeVirtual = true);
  static QStringList validAddresses();
  static QStringList physicalAddresses();
  static QString preferredPhysicalAddress();
  static QString preferredPhysicalAddress(const QList<InterfaceAddress> &addresses);

  static QString resolveServerBindAddress(const QString &configuredAddress, bool preferPhysical);
};

} // namespace deskflow::network
