/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
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

private:
  void processCommand(QLocalSocket *, const QString &, const QStringList &) override
  {
  }
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

QTEST_GUILESS_MAIN(IpcServerTests)
