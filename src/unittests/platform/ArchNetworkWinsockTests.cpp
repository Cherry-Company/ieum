/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "arch/win32/ArchNetworkWinsock.h"

#include <QTest>

class ArchNetworkWinsockTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void requestsFreshWritableNotification()
  {
    ArchNetworkWinsock network;
    ArchSocketImpl socket{INVALID_SOCKET, 1, WSA_INVALID_EVENT, false};

    network.resetPollWriteOnSocket(&socket);

    QVERIFY(socket.m_pollWrite);
  }

  void reArmsHintAfterPollerConsumedIt()
  {
    ArchNetworkWinsock network;
    ArchSocketImpl socket{INVALID_SOCKET, 1, WSA_INVALID_EVENT, true};
    socket.m_pollWrite = false;

    network.resetPollWriteOnSocket(&socket);

    QVERIFY(socket.m_pollWrite);
  }

  void recognizesIpv4AnyAddress()
  {
    ArchNetworkWinsock network;
    const auto address = network.newAnyAddr(IArchNetwork::AddressFamily::INet);
    QVERIFY(address != nullptr);

    const bool isAnyAddress = network.isAnyAddr(address);
    network.closeAddr(address);

    QVERIFY(isAnyAddress);
  }

  void recognizesIpv6AnyAddress()
  {
    ArchNetworkWinsock network;
    const auto address = network.newAnyAddr(IArchNetwork::AddressFamily::INet6);
    QVERIFY(address != nullptr);

    const bool isAnyAddress = network.isAnyAddr(address);
    network.closeAddr(address);

    QVERIFY(isAnyAddress);
  }
};

QTEST_GUILESS_MAIN(ArchNetworkWinsockTests)

#include "ArchNetworkWinsockTests.moc"
