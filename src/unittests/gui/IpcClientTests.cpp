/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "IpcClientTests.h"

#include "common/VersionInfo.h"
#include "gui/ipc/IpcClient.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTimer>
#include <QUuid>

using deskflow::gui::ipc::IpcClient;

namespace {

class TestIpcClient : public IpcClient
{
public:
  TestIpcClient(const QString &socketName, const int retryLimit, const int retryDelayMs)
      : IpcClient(nullptr, socketName, QStringLiteral("test"), retryLimit, retryDelayMs)
  {
  }
};

QString uniqueSocketName()
{
  return QStringLiteral("ieum-ipc-test-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

} // namespace

void IpcClientTests::connectsWhenServerStartsDuringRetryWindow()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  QLocalServer server;
  TestIpcClient client(socketName, 12, 25);
  QSignalSpy connectedSpy(&client, &IpcClient::connected);
  QByteArray received;
  int connectionCount = 0;

  connect(&server, &QLocalServer::newConnection, this, [&] {
    auto *socket = server.nextPendingConnection();
    QVERIFY(socket != nullptr);
    connectionCount++;
    connect(socket, &QLocalSocket::readyRead, this, [&, socket] {
      received.append(socket->readAll());
      if (!received.contains('\n')) {
        return;
      }
      const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
      socket->write(QStringLiteral("hello=%1\n").arg(versionId).toUtf8());
      socket->flush();
    });
  });

  client.connectToServer();
  QTimer::singleShot(100, &server, [&] { QVERIFY(server.listen(socketName)); });

  QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 2000);
  QTest::qWait(100);
  QCOMPARE(connectionCount, 1);
  QCOMPARE(received.count("hello="), 1);

  client.disconnectFromServer();
  server.close();
  QLocalServer::removeServer(socketName);
}

void IpcClientTests::emitsFailureAfterRetryLimit()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestIpcClient client(socketName, 3, 20);
  QSignalSpy failureSpy(&client, &IpcClient::connectionFailed);
  client.connectToServer();

  QTRY_COMPARE_WITH_TIMEOUT(failureSpy.count(), 1, 1000);
  QVERIFY(!client.isConnected());
}

QTEST_MAIN(IpcClientTests)
