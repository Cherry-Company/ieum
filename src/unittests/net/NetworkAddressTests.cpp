/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "arch/Arch.h"
#include "net/NetworkAddress.h"
#include "net/SocketException.h"

#include <QTest>

#include <memory>

class NetworkAddressTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase()
  {
    m_arch = std::make_unique<Arch>();
    m_arch->init();
  }

  void parsesCompleteDecimalPort_data()
  {
    QTest::addColumn<QString>("endpoint");
    QTest::addColumn<QString>("hostname");

    QTest::newRow("hostname") << QStringLiteral("host.example:24800") << QStringLiteral("host.example");
    QTest::newRow("ipv4") << QStringLiteral("127.0.0.1:24800") << QStringLiteral("127.0.0.1");
    QTest::newRow("ipv6") << QStringLiteral("[::1]:24800") << QStringLiteral("::1");
  }

  void parsesCompleteDecimalPort()
  {
    QFETCH(QString, endpoint);
    QFETCH(QString, hostname);

    const NetworkAddress address(endpoint.toStdString(), 1);

    QCOMPARE(address.getHostname(), hostname.toStdString());
    QCOMPARE(address.getPort(), 24800);
  }

  void rejectsMalformedPortSuffix_data()
  {
    QTest::addColumn<QString>("endpoint");

    QTest::newRow("hostname-trailing-text") << QStringLiteral("host.example:24800junk");
    QTest::newRow("ipv4-trailing-text") << QStringLiteral("127.0.0.1:24800junk");
    QTest::newRow("ipv6-trailing-text") << QStringLiteral("[::1]:24800junk");
    QTest::newRow("empty") << QStringLiteral("host.example:");
    QTest::newRow("ipv6-empty") << QStringLiteral("[::1]:");
    QTest::newRow("plus-sign") << QStringLiteral("host.example:+24800");
    QTest::newRow("leading-space") << QStringLiteral("host.example: 24800");
    QTest::newRow("negative") << QStringLiteral("host.example:-1");
    QTest::newRow("negative-zero") << QStringLiteral("host.example:-0");
    QTest::newRow("out-of-range") << QStringLiteral("host.example:65536");
  }

  void rejectsMalformedPortSuffix()
  {
    QFETCH(QString, endpoint);

    try {
      const NetworkAddress address(endpoint.toStdString(), 1);
      Q_UNUSED(address);
      QFAIL("malformed port suffix was accepted");
    } catch (const SocketAddressException &error) {
      QVERIFY(error.getError() == SocketAddressException::SocketError::BadPort);
    }
  }

  void comparesNullAddressCombinations()
  {
    const NetworkAddress unresolvedLeft("left.example", 24800);
    const NetworkAddress unresolvedRight("right.example", 24800);
    QVERIFY(unresolvedLeft == unresolvedRight);

    const NetworkAddress resolved(24800);
    QVERIFY(!(resolved == unresolvedLeft));
    QVERIFY(!(unresolvedLeft == resolved));
  }

private:
  std::unique_ptr<Arch> m_arch;
};

QTEST_GUILESS_MAIN(NetworkAddressTests)

#include "NetworkAddressTests.moc"
