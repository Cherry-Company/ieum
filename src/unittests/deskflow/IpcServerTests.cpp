/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "IpcServerTests.h"

#include "common/VersionInfo.h"
#include "deskflow/ipc/DaemonIpcServer.h"
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

void IpcServerTests::correlatesDelayedDaemonCommandResults()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  deskflow::core::ipc::DaemonIpcServer server(nullptr, QStringLiteral("daemon.log"), socketName);
  QVERIFY(server.listen());

  QSignalSpy configSpy(&server, &deskflow::core::ipc::DaemonIpcServer::configFileRequested);
  QSignalSpy startSpy(&server, &deskflow::core::ipc::DaemonIpcServer::startCommandRequested);
  QLocalSocket client;
  QSignalSpy readyReadSpy(&client, &QLocalSocket::readyRead);
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));

  const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
  const auto hello = QStringLiteral("hello=%1\n").arg(versionId).toUtf8();
  QCOMPARE(client.write(hello), hello.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QVERIFY(client.readAll().startsWith("hello="));
  readyReadSpy.clear();

  const auto configRequestId = QStringLiteral("config-request-1");
  const auto configFile = QStringLiteral("C:\\설정=이음.conf");
  const auto configRequest = QStringLiteral("configFile=%1\t%2\n").arg(configRequestId, configFile).toUtf8();
  QCOMPARE(client.write(configRequest), configRequest.size());
  client.flush();

  QTRY_COMPARE_WITH_TIMEOUT(configSpy.count(), 1, 1000);
  QCOMPARE(configSpy.at(0).at(0).toString(), configRequestId);
  QCOMPARE(configSpy.at(0).at(1).toString(), configFile);
  QTest::qWait(50);
  QVERIFY(readyReadSpy.isEmpty());

  server.completeCommand(configRequestId, false, QStringLiteral("config rejected"));
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(
      QString::fromUtf8(client.readAll()),
      QStringLiteral("commandResult=%1\tconfigFile\terror\tconfig rejected\n").arg(configRequestId)
  );
  readyReadSpy.clear();

  const auto emptyRequestId = QStringLiteral("empty-config-request");
  const auto emptyRequest = QStringLiteral("configFile=%1\t\n").arg(emptyRequestId).toUtf8();
  QCOMPARE(client.write(emptyRequest), emptyRequest.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(
      QString::fromUtf8(client.readAll()),
      QStringLiteral("commandResult=%1\tconfigFile\terror\tconfig file path is empty\n").arg(emptyRequestId)
  );
  QCOMPARE(configSpy.count(), 1);
  readyReadSpy.clear();

  const auto startRequestId = QStringLiteral("start-request-1");
  const auto startRequest = QStringLiteral("start=%1\n").arg(startRequestId).toUtf8();
  QCOMPARE(client.write(startRequest), startRequest.size());
  client.flush();

  QTRY_COMPARE_WITH_TIMEOUT(startSpy.count(), 1, 1000);
  QCOMPARE(startSpy.at(0).at(0).toString(), startRequestId);
  QTest::qWait(50);
  QVERIFY(readyReadSpy.isEmpty());

  server.completeCommand(startRequestId, true);
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("commandResult=%1\tstart\tok\t\n").arg(startRequestId));
}

void IpcServerTests::acknowledgesLegacyDaemonCommandsAfterCompletion()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  deskflow::core::ipc::DaemonIpcServer server(nullptr, QStringLiteral("daemon.log"), socketName);
  QVERIFY(server.listen());

  QSignalSpy configSpy(&server, &deskflow::core::ipc::DaemonIpcServer::configFileRequested);
  QSignalSpy startSpy(&server, &deskflow::core::ipc::DaemonIpcServer::startCommandRequested);
  QSignalSpy stopSpy(&server, &deskflow::core::ipc::DaemonIpcServer::stopCommandRequested);
  QLocalSocket client;
  QSignalSpy readyReadSpy(&client, &QLocalSocket::readyRead);
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));

  const auto hello = QByteArrayLiteral("hello=ieum-runtime-coexistence-test\n");
  QCOMPARE(client.write(hello), hello.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QVERIFY(client.readAll().startsWith("versionMismatch="));
  readyReadSpy.clear();

  const auto configFile = QStringLiteral("C:\\legacy-ieum.conf");
  const auto configRequest = QStringLiteral("configFile=%1\n").arg(configFile).toUtf8();
  QCOMPARE(client.write(configRequest), configRequest.size());
  client.flush();
  QTRY_COMPARE_WITH_TIMEOUT(configSpy.count(), 1, 1000);
  const auto configRequestId = configSpy.at(0).at(0).toString();
  QVERIFY(!configRequestId.isEmpty());
  QCOMPARE(configSpy.at(0).at(1).toString(), configFile);
  QTest::qWait(50);
  QVERIFY(readyReadSpy.isEmpty());
  server.completeCommand(configRequestId, true);
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("ok\n"));
  readyReadSpy.clear();

  QCOMPARE(client.write("start\n"), 6);
  client.flush();
  QTRY_COMPARE_WITH_TIMEOUT(startSpy.count(), 1, 1000);
  const auto startRequestId = startSpy.at(0).at(0).toString();
  QVERIFY(!startRequestId.isEmpty());
  QTest::qWait(50);
  QVERIFY(readyReadSpy.isEmpty());
  server.completeCommand(startRequestId, true);
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("ok\n"));
  readyReadSpy.clear();

  QCOMPARE(client.write("stop\n"), 5);
  client.flush();
  QTRY_COMPARE_WITH_TIMEOUT(stopSpy.count(), 1, 1000);
  const auto stopRequestId = stopSpy.at(0).at(0).toString();
  QVERIFY(!stopRequestId.isEmpty());
  QTest::qWait(50);
  QVERIFY(readyReadSpy.isEmpty());
  server.completeCommand(stopRequestId, true);
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("ok\n"));
}

QTEST_GUILESS_MAIN(IpcServerTests)
