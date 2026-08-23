/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "IpcServerTests.h"

#include "common/VersionInfo.h"
#include "deskflow/ipc/IpcServer.h"

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>

using deskflow::core::ipc::IpcServer;

namespace {

class TestIpcServer final : public IpcServer
{
public:
  explicit TestIpcServer(const QString &socketName) : IpcServer(nullptr, socketName, QStringLiteral("test"))
  {
  }

  const QList<QStringList> &commands() const
  {
    return m_commands;
  }

private:
  void processCommand(QLocalSocket *, const QString &, const QStringList &parts) override
  {
    m_commands.append(parts);
  }

  QList<QStringList> m_commands;
};

QString uniqueSocketName()
{
  // Keep the full path below macOS's short Unix-domain socket limit.
  return QStringLiteral("ieum-ipc-test-%1").arg(QCoreApplication::applicationPid());
}

} // namespace

void IpcServerTests::refusesDuplicateWithoutDisruptingFirstServer()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestIpcServer firstServer(socketName);
  QVERIFY(firstServer.listen());

  QLocalSocket firstClient;
  firstClient.connectToServer(socketName);
  QVERIFY(firstClient.waitForConnected(1000));

  QSignalSpy readyReadSpy(&firstClient, &QLocalSocket::readyRead);
  const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
  const auto partialHello = QStringLiteral("hello=%1").arg(versionId).toUtf8();
  QCOMPARE(firstClient.write(partialHello), partialHello.size());
  firstClient.flush();
  QTest::qWait(50);
  QVERIFY(readyReadSpy.isEmpty());

  QCOMPARE(firstClient.write("\r\n"), 2);
  firstClient.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QVERIFY(firstClient.readAll().startsWith("hello="));

  auto *firstListener = firstServer.findChild<QLocalServer *>();
  QVERIFY(firstListener != nullptr);
  QVERIFY(firstListener->isListening());

  TestIpcServer duplicateServer(socketName);
  QVERIFY(!duplicateServer.listen());
  QVERIFY(firstListener->isListening());
  QCOMPARE(firstClient.state(), QLocalSocket::ConnectedState);

  QLocalSocket secondClient;
  secondClient.connectToServer(socketName);
  QVERIFY(secondClient.waitForConnected(1000));
  QCOMPARE(firstClient.state(), QLocalSocket::ConnectedState);
}

void IpcServerTests::preservesCommandArgumentBoundaryFromClient()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestIpcServer server(socketName);
  QVERIFY(server.listen());

  QLocalSocket client;
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));

  const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
  const auto value = QStringLiteral("C:\\경로=설정\\이음=공유.conf");
  const auto request = QStringLiteral("hello=%1\nnoArgs\nempty=\nconfigFile=%2\n").arg(versionId, value).toUtf8();
  QCOMPARE(client.write(request), request.size());
  client.flush();

  QTRY_COMPARE_WITH_TIMEOUT(server.commands().size(), static_cast<qsizetype>(3), 1000);

  const auto &commands = server.commands();
  QCOMPARE(commands.at(0), QStringList{QStringLiteral("noArgs")});
  QCOMPARE(commands.at(1).size(), static_cast<qsizetype>(2));
  QCOMPARE(commands.at(1).at(0), QStringLiteral("empty"));
  QVERIFY(commands.at(1).at(1).isEmpty());
  QCOMPARE(commands.at(2).size(), static_cast<qsizetype>(2));
  QCOMPARE(commands.at(2).at(0), QStringLiteral("configFile"));
  QCOMPARE(commands.at(2).at(1), value);
}

void IpcServerTests::returnsVersionMismatchForDifferentClientVersion()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestIpcServer server(socketName);
  QVERIFY(server.listen());

  QLocalSocket client;
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));
  QSignalSpy readyReadSpy(&client, &QLocalSocket::readyRead);

  QCOMPARE(client.write("hello=different-version\n"), 24);
  client.flush();

  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("versionMismatch=%1\n").arg(versionId));
}

QTEST_GUILESS_MAIN(IpcServerTests)
