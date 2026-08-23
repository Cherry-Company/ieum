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
};

QTEST_GUILESS_MAIN(ArchNetworkWinsockTests)

#include "ArchNetworkWinsockTests.moc"
