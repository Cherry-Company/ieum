/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "IpcClientTests.h"

#include "common/FileTransferEdgeDropIpc.h"
#include "common/VersionInfo.h"
#include "gui/config/ServerConfig.h"
#include "gui/core/CoreProcess.h"
#include "gui/ipc/CoreIpcClient.h"
#include "gui/ipc/DaemonIpcClient.h"
#include "gui/ipc/IpcClient.h"

#include <QDebug>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTimer>
#include <QUuid>

using deskflow::gui::ipc::IpcClient;

namespace {

QStringList *g_messages = nullptr;

void captureMessage(QtMsgType, const QMessageLogContext &, const QString &message)
{
  if (g_messages != nullptr) {
    g_messages->append(message);
  }
}

class MessageCapture final
{
public:
  explicit MessageCapture(QStringList &messages) : m_previous(qInstallMessageHandler(captureMessage))
  {
    g_messages = &messages;
  }

  ~MessageCapture()
  {
    g_messages = nullptr;
    qInstallMessageHandler(m_previous);
  }

private:
  QtMessageHandler m_previous;
};

class TestIpcClient : public IpcClient
{
public:
  TestIpcClient(const QString &socketName, const int retryLimit, const int retryDelayMs)
      : IpcClient(nullptr, socketName, QStringLiteral("test"), retryLimit, retryDelayMs)
  {
  }

  using IpcClient::sendMessage;

  const QList<QStringList> &commands() const
  {
    return m_commands;
  }

protected:
  void processCommand(const QString &, const QStringList &parts) override
  {
    m_commands.append(parts);
  }

private:
  QList<QStringList> m_commands;
};

class TestDaemonIpcClient final : public deskflow::gui::ipc::DaemonIpcClient
{
public:
  explicit TestDaemonIpcClient(const QString &socketName) : DaemonIpcClient(nullptr, socketName, 1, 0)
  {
  }
};

class TestCoreIpcClient final : public deskflow::gui::ipc::CoreIpcClient
{
public:
  TestCoreIpcClient(QObject *parent, const QString &socketName) : CoreIpcClient(parent, socketName, 1, 0)
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

void IpcClientTests::preservesCommandArgumentBoundaryFromServer()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  QLocalServer server;
  QVERIFY(server.listen(socketName));

  TestIpcClient client(socketName, 1, 0);
  const auto value = QStringLiteral("/tmp/이음=로그=현재.log");
  QByteArray received;
  bool responseSent = false;

  connect(&server, &QLocalServer::newConnection, this, [&] {
    auto *socket = server.nextPendingConnection();
    QVERIFY(socket != nullptr);
    connect(socket, &QLocalSocket::readyRead, this, [&, socket] {
      received.append(socket->readAll());
      if (responseSent || !received.contains('\n')) {
        return;
      }

      responseSent = true;
      const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
      const auto response = QStringLiteral("hello=%1\nnoArgs\nempty=\nlogPath=%2\n").arg(versionId, value).toUtf8();
      QCOMPARE(socket->write(response), response.size());
      socket->flush();
    });
  });

  client.connectToServer();

  QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 1000);
  QTRY_COMPARE_WITH_TIMEOUT(client.commands().size(), static_cast<qsizetype>(3), 1000);

  const auto &commands = client.commands();
  QCOMPARE(commands.at(0), QStringList{QStringLiteral("noArgs")});
  QCOMPARE(commands.at(1).size(), static_cast<qsizetype>(2));
  QCOMPARE(commands.at(1).at(0), QStringLiteral("empty"));
  QVERIFY(commands.at(1).at(1).isEmpty());
  QCOMPARE(commands.at(2).size(), static_cast<qsizetype>(2));
  QCOMPARE(commands.at(2).at(0), QStringLiteral("logPath"));
  QCOMPARE(commands.at(2).at(1), value);

  client.disconnectFromServer();
  server.close();
  QLocalServer::removeServer(socketName);
}

void IpcClientTests::redactsFileTransferEdgeDropInClientLogs()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  QLocalServer server;
  QVERIFY(server.listen(socketName));

  TestIpcClient client(socketName, 1, 0);
  QByteArray received;
  QLocalSocket *serverSocket = nullptr;
  const auto payload = QStringLiteral(R"(C:\Users\tester\한글 경로\edge one.txt|/tmp/edge drop/two.txt)");
  const auto sensitiveMessage = QStringLiteral("fileTransferEdgeDrop=%1").arg(payload);

  connect(&server, &QLocalServer::newConnection, this, [&] {
    serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket != nullptr);
    connect(serverSocket, &QLocalSocket::readyRead, this, [&] {
      received.append(serverSocket->readAll());
      if (received.startsWith("hello=") && received.contains('\n')) {
        const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
        serverSocket->write(QStringLiteral("hello=%1\n").arg(versionId).toUtf8());
        serverSocket->flush();
      }
    });
  });

  QStringList messages;
  {
    MessageCapture capture(messages);
    client.connectToServer();
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 1000);
    received.clear();

    client.sendMessage(sensitiveMessage);
    QTRY_VERIFY_WITH_TIMEOUT(received.contains("fileTransferEdgeDrop="), 1000);
    QCOMPARE(QString::fromUtf8(received), sensitiveMessage + QStringLiteral("\n"));

    serverSocket->write(sensitiveMessage.toUtf8() + '\n');
    serverSocket->flush();
    QTRY_COMPARE_WITH_TIMEOUT(client.commands().size(), static_cast<qsizetype>(1), 1000);
    QCOMPARE(client.commands().constFirst(), QStringList({QStringLiteral("fileTransferEdgeDrop"), payload}));
  }

  QVERIFY(std::ranges::any_of(messages, [](const QString &message) {
    return message.contains(QStringLiteral("ipc client sent message: fileTransferEdgeDrop=<redacted>"));
  }));
  QVERIFY(std::ranges::any_of(messages, [](const QString &message) {
    return message.contains(QStringLiteral("ipc client message: fileTransferEdgeDrop=<redacted>"));
  }));
  for (const auto &message : messages) {
    QVERIFY2(!message.contains(payload), qPrintable(message));
    QVERIFY2(!message.contains(QStringLiteral("한글 경로")), qPrintable(message));
  }

  client.disconnectFromServer();
  server.close();
  QLocalServer::removeServer(socketName);
}

void IpcClientTests::acceptsVersionMismatchHandshake()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  QLocalServer server;
  QVERIFY(server.listen(socketName));

  TestIpcClient client(socketName, 1, 0);
  QSignalSpy mismatchSpy(&client, &IpcClient::versionMismatch);
  QByteArray received;

  connect(&server, &QLocalServer::newConnection, this, [&] {
    auto *socket = server.nextPendingConnection();
    QVERIFY(socket != nullptr);
    connect(socket, &QLocalSocket::readyRead, this, [&, socket] {
      received.append(socket->readAll());
      if (!received.contains('\n')) {
        return;
      }

      socket->write("versionMismatch=server-version\n");
      socket->flush();
    });
  });

  client.connectToServer();

  QTRY_COMPARE_WITH_TIMEOUT(mismatchSpy.count(), 1, 1000);
  QVERIFY(client.isConnected());

  client.disconnectFromServer();
  server.close();
  QLocalServer::removeServer(socketName);
}

void IpcClientTests::coreRequestsActiveSidesAfterExactHandshake()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  QLocalServer server;
  QVERIFY(server.listen(socketName));
  TestCoreIpcClient client(nullptr, socketName);
  QByteArray received;
  QLocalSocket *serverSocket = nullptr;

  connect(&server, &QLocalServer::newConnection, this, [&] {
    serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket != nullptr);
    connect(serverSocket, &QLocalSocket::readyRead, this, [&] { received.append(serverSocket->readAll()); });
  });
  connect(&client, &IpcClient::connected, &client, &deskflow::gui::ipc::CoreIpcClient::requestFileTransferActiveSides);

  client.connectToServer();
  QTRY_VERIFY_WITH_TIMEOUT(received.contains('\n'), 1000);
  QVERIFY(!received.contains("fileTransferActiveSides\n"));

  const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
  serverSocket->write(QStringLiteral("hello=%1\n").arg(versionId).toUtf8());
  serverSocket->flush();
  QTRY_VERIFY_WITH_TIMEOUT(received.contains("fileTransferActiveSides\n"), 1000);
  QCOMPARE(received.count("fileTransferActiveSides\n"), 1);

  client.disconnectFromServer();
  server.close();
  QLocalServer::removeServer(socketName);
}

void IpcClientTests::corePublishesValidActiveSidesAndClearsOnDisconnect()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  QLocalServer server;
  QVERIFY(server.listen(socketName));
  TestCoreIpcClient client(nullptr, socketName);
  QSignalSpy connectedSpy(&client, &IpcClient::connected);
  QSignalSpy sidesSpy(&client, &deskflow::gui::ipc::CoreIpcClient::fileTransferActiveSidesChanged);
  QLocalSocket *serverSocket = nullptr;

  connect(&server, &QLocalServer::newConnection, this, [&] {
    serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket != nullptr);
    connect(serverSocket, &QLocalSocket::readyRead, this, [&, serverSocket] {
      const auto request = serverSocket->readAll();
      if (request.startsWith("hello=")) {
        const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
        serverSocket->write(QStringLiteral("hello=%1\n").arg(versionId).toUtf8());
        serverSocket->flush();
      }
    });
  });

  client.connectToServer();
  QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 1000);
  serverSocket->write("fileTransferActiveSides=30\n");
  serverSocket->flush();
  QTRY_COMPARE_WITH_TIMEOUT(sidesSpy.count(), 1, 1000);
  QCOMPARE(sidesSpy.at(0).at(0).toUInt(), 30U);

  serverSocket->disconnectFromServer();
  QTRY_COMPARE_WITH_TIMEOUT(sidesSpy.count(), 2, 1000);
  QCOMPARE(sidesSpy.at(1).at(0).toUInt(), 0U);

  server.close();
  QLocalServer::removeServer(socketName);
}

void IpcClientTests::coreSendsOnlyConnectedBoundedEdgeDrops()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  QLocalServer server;
  QVERIFY(server.listen(socketName));
  TestCoreIpcClient client(nullptr, socketName);
  QByteArray received;
  QLocalSocket *serverSocket = nullptr;
  const auto encoded = deskflow::ipc::encodeFileTransferEdgeDropIpc(
      {.direction = Direction::Left,
       .x = -12,
       .y = 34,
       .paths = {QDir::toNativeSeparators(QDir::current().absoluteFilePath(QStringLiteral("edge drop/한글.txt")))}}
  );
  QVERIFY(encoded.ok());
  QVERIFY(!client.sendFileTransferEdgeDrop(encoded.encodedValue));

  connect(&server, &QLocalServer::newConnection, this, [&] {
    serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket != nullptr);
    connect(serverSocket, &QLocalSocket::readyRead, this, [&] {
      received.append(serverSocket->readAll());
      if (received.startsWith("hello=") && received.contains('\n')) {
        const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
        serverSocket->write(QStringLiteral("hello=%1\n").arg(versionId).toUtf8());
        serverSocket->flush();
      }
    });
  });

  client.connectToServer();
  QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 1000);
  received.clear();
  QVERIFY(!client.sendFileTransferEdgeDrop(QString(60 * 1024 + 1, QLatin1Char('A'))));
  QVERIFY(client.sendFileTransferEdgeDrop(encoded.encodedValue));
  QTRY_VERIFY_WITH_TIMEOUT(received.contains("fileTransferEdgeDrop="), 1000);
  QCOMPARE(QString::fromUtf8(received), QStringLiteral("fileTransferEdgeDrop=%1\n").arg(encoded.encodedValue));

  client.disconnectFromServer();
  server.close();
  QLocalServer::removeServer(socketName);
}

void IpcClientTests::correlatesDaemonCommandResults()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  QLocalServer server;
  QVERIFY(server.listen(socketName));

  TestDaemonIpcClient client(socketName);
  QSignalSpy connectedSpy(&client, &IpcClient::connected);
  QSignalSpy resultSpy(&client, &deskflow::gui::ipc::DaemonIpcClient::commandResult);
  QByteArray received;
  QLocalSocket *serverSocket = nullptr;

  connect(&server, &QLocalServer::newConnection, this, [&] {
    serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket != nullptr);
    connect(serverSocket, &QLocalSocket::readyRead, this, [&] {
      received.append(serverSocket->readAll());
      if (received.startsWith("hello=") && received.contains('\n')) {
        const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
        serverSocket->write(QStringLiteral("hello=%1\n").arg(versionId).toUtf8());
        serverSocket->flush();
      }
    });
  });

  client.connectToServer();
  QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 1000);
  received.clear();

  const auto configFile = QStringLiteral("C:\\설정=이음.conf");
  const auto requestId = client.sendConfigFile(configFile);
  QTRY_VERIFY_WITH_TIMEOUT(received.contains("configFile="), 1000);
  QCOMPARE(QString::fromUtf8(received), QStringLiteral("configFile=%1\t%2\n").arg(requestId, configFile));

  serverSocket->write(QStringLiteral("commandResult=%1\tconfigFile\terror\tconfig rejected\n").arg(requestId).toUtf8());
  serverSocket->flush();

  QTRY_COMPARE_WITH_TIMEOUT(resultSpy.count(), 1, 1000);
  const auto result = resultSpy.takeFirst();
  QCOMPARE(result.at(0).toString(), requestId);
  QCOMPARE(result.at(1).toString(), QStringLiteral("configFile"));
  QCOMPARE(result.at(2).toBool(), false);
  QCOMPARE(result.at(3).toString(), QStringLiteral("config rejected"));

  received.clear();
  const auto emptyRequestId = client.sendConfigFile({});
  QTRY_VERIFY_WITH_TIMEOUT(received.contains("configFile="), 1000);
  QCOMPARE(QString::fromUtf8(received), QStringLiteral("configFile=%1\t\n").arg(emptyRequestId));

  client.disconnectFromServer();
  server.close();
  QLocalServer::removeServer(socketName);
}

QTEST_MAIN(IpcClientTests)
