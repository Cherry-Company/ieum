/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "IpcServerTests.h"

#include "base/ILogOutputter.h"
#include "common/FileTransferEdgeDropIpc.h"
#include "common/VersionInfo.h"
#include "deskflow/ipc/CoreIpcServer.h"
#include "deskflow/ipc/DaemonIpcServer.h"
#include "deskflow/ipc/IpcServer.h"

#include <QCoreApplication>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QSignalSpy>

#include <utility>

using deskflow::core::ipc::IpcServer;

namespace {

class CapturingLogOutputter final : public ILogOutputter
{
public:
  explicit CapturingLogOutputter(QStringList &messages) : m_messages(messages)
  {
  }

  void open(const QString &) override
  {
  }

  void close() override
  {
  }

  bool write(LogLevel::Level, const QString &message) override
  {
    m_messages.append(message);
    return false;
  }

private:
  QStringList &m_messages;
};

class LogCapture final
{
public:
  explicit LogCapture(QStringList &messages) : m_outputter(new CapturingLogOutputter(messages))
  {
    CLOG->insert(m_outputter);
  }

  ~LogCapture()
  {
    CLOG->remove(m_outputter);
    delete m_outputter;
  }

private:
  CapturingLogOutputter *m_outputter;
};

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

  QLocalSocket *serverSocket() const
  {
    const auto sockets = findChildren<QLocalSocket *>();
    return sockets.size() == 1 ? sockets.constFirst() : nullptr;
  }

  bool receivedCurrentVersionHello(const QLocalSocket *socket) const
  {
    return hasCurrentVersionHello(socket);
  }

private:
  void processCommand(QLocalSocket *, const QString &, const QStringList &parts) override
  {
    m_commands.append(parts);
  }

  QList<QStringList> m_commands;
};

class TestCoreIpcServer final : public deskflow::core::ipc::CoreIpcServer
{
public:
  TestCoreIpcServer(const QString &socketName, ClientValidator validator)
      : CoreIpcServer(nullptr, socketName, std::move(validator))
  {
  }
};

struct CapturedDrop
{
  int count = 0;
  Direction direction = Direction::NoDirection;
  qint32 x = 0;
  qint32 y = 0;
  QStringList paths;
};

QString uniqueSocketName()
{
  // Keep the full path below macOS's short Unix-domain socket limit.
  return QStringLiteral("ieum-ipc-test-%1").arg(QCoreApplication::applicationPid());
}

QString currentVersionId()
{
  return QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
}

QByteArray currentHello()
{
  return QStringLiteral("hello=%1\n").arg(currentVersionId()).toUtf8();
}

deskflow::ipc::FileTransferEdgeDropIpcValue dropFixture()
{
  return {
      .direction = Direction::Bottom,
      .x = -120,
      .y = 2048,
      .paths = {
          QDir::toNativeSeparators(QDir::current().absoluteFilePath(QStringLiteral("edge drop/alpha one.txt"))),
          QDir::toNativeSeparators(QDir::current().absoluteFilePath(QStringLiteral("edge drop/한글 둘.txt"))),
      },
  };
}

void captureDrops(TestCoreIpcServer &server, QObject *context, CapturedDrop &capture)
{
  QObject::connect(
      &server, &deskflow::core::ipc::CoreIpcServer::fileTransferEdgeDropRequested, context,
      [&capture](Direction direction, qint32 x, qint32 y, const QStringList &paths) {
        ++capture.count;
        capture.direction = direction;
        capture.x = x;
        capture.y = y;
        capture.paths = paths;
      }
  );
}

} // namespace

void IpcServerTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Verbose);
}

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

void IpcServerTests::redactsFileTransferEdgeDropInServerLogs()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestIpcServer server(socketName);
  QVERIFY(server.listen());

  QLocalSocket client;
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));

  const auto payload = QStringLiteral(R"(C:\Users\tester\한글 경로\edge one.txt|/tmp/edge drop/two.txt)");
  const auto sensitiveMessage = QStringLiteral("fileTransferEdgeDrop=%1").arg(payload);
  QStringList logs;
  {
    LogCapture capture(logs);

    const auto request = QStringLiteral("%1\n").arg(sensitiveMessage).toUtf8();
    QCOMPARE(client.write(request), request.size());
    client.flush();
    QTRY_COMPARE_WITH_TIMEOUT(server.commands().size(), static_cast<qsizetype>(1), 1000);
    QCOMPARE(server.commands().constFirst(), QStringList({QStringLiteral("fileTransferEdgeDrop"), payload}));

    QSignalSpy readyReadSpy(&client, &QLocalSocket::readyRead);
    server.broadcastCommand(QStringLiteral("fileTransferEdgeDrop"), payload);
    QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
    QCOMPARE(QString::fromUtf8(client.readAll()), sensitiveMessage + QStringLiteral("\n"));
  }

  QVERIFY(std::ranges::any_of(logs, [](const QString &message) {
    return message.contains(QStringLiteral("ipc server got message: fileTransferEdgeDrop=<redacted>"));
  }));
  QVERIFY(std::ranges::any_of(logs, [](const QString &message) {
    return message.contains(QStringLiteral("ipc server broadcasting message to 1 clients: fileTransferEdgeDrop=<redacted>"));
  }));
  QVERIFY(std::ranges::any_of(logs, [](const QString &message) {
    return message.contains(QStringLiteral("ipc server wrote message to client socket: fileTransferEdgeDrop=<redacted>"));
  }));
  for (const auto &message : logs) {
    QVERIFY2(!message.contains(payload), qPrintable(message));
    QVERIFY2(!message.contains(QStringLiteral("한글 경로")), qPrintable(message));
  }
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

void IpcServerTests::tracksExactHelloAndClearsStateOnMismatchAndDisconnect()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestIpcServer server(socketName);
  QVERIFY(server.listen());

  QLocalSocket client;
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));
  QTRY_VERIFY_WITH_TIMEOUT(server.serverSocket() != nullptr, 1000);
  auto *serverSocket = server.serverSocket();
  QVERIFY(!server.receivedCurrentVersionHello(serverSocket));

  QSignalSpy readyReadSpy(&client, &QLocalSocket::readyRead);
  const auto hello = currentHello();
  QCOMPARE(client.write(hello), hello.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("hello=%1\n").arg(currentVersionId()));
  QVERIFY(server.receivedCurrentVersionHello(serverSocket));
  readyReadSpy.clear();

  const auto mismatch = QByteArrayLiteral("hello=legacy-version\n");
  QCOMPARE(client.write(mismatch), mismatch.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("versionMismatch=%1\n").arg(currentVersionId()));
  QVERIFY(!server.receivedCurrentVersionHello(serverSocket));
  readyReadSpy.clear();

  QCOMPARE(client.write(hello), hello.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  client.readAll();
  QVERIFY(server.receivedCurrentVersionHello(serverSocket));

  bool disconnectObserved = false;
  bool exactAtDisconnect = true;
  connect(serverSocket, &QLocalSocket::disconnected, this, [&] {
    disconnectObserved = true;
    exactAtDisconnect = server.receivedCurrentVersionHello(serverSocket);
  });
  QPointer<QLocalSocket> guardedServerSocket(serverSocket);
  client.disconnectFromServer();
  QTRY_VERIFY_WITH_TIMEOUT(disconnectObserved, 1000);
  QVERIFY(!exactAtDisconnect);
  QTRY_VERIFY_WITH_TIMEOUT(guardedServerSocket.isNull(), 1000);
}

void IpcServerTests::authorizedCurrentCoreClientEmitsDecodedDrop()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestCoreIpcServer server(socketName, [](QLocalSocket *) { return true; });
  QVERIFY(server.listen());
  CapturedDrop capture;
  captureDrops(server, this, capture);

  QLocalSocket client;
  QSignalSpy readyReadSpy(&client, &QLocalSocket::readyRead);
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));

  const auto hello = currentHello();
  QCOMPARE(client.write(hello), hello.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("hello=%1\n").arg(currentVersionId()));
  readyReadSpy.clear();

  const auto fixture = dropFixture();
  const auto encoded = deskflow::ipc::encodeFileTransferEdgeDropIpc(fixture);
  QCOMPARE(encoded.error, deskflow::ipc::FileTransferEdgeDropIpcError::None);
  const auto request = QStringLiteral("fileTransferEdgeDrop=%1\n").arg(encoded.encodedValue).toUtf8();
  QCOMPARE(client.write(request), request.size());
  client.flush();

  QTRY_COMPARE_WITH_TIMEOUT(capture.count, 1, 1000);
  QCOMPARE(capture.direction, fixture.direction);
  QCOMPARE(capture.x, fixture.x);
  QCOMPARE(capture.y, fixture.y);
  QCOMPARE(capture.paths, fixture.paths);
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("ok\n"));
  QTest::qWait(50);
  QCOMPARE(capture.count, 1);
}

void IpcServerTests::deniedCurrentCoreClientReceivesGenericError()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestCoreIpcServer server(socketName, [](QLocalSocket *) { return false; });
  QVERIFY(server.listen());
  CapturedDrop capture;
  captureDrops(server, this, capture);

  QLocalSocket client;
  QSignalSpy readyReadSpy(&client, &QLocalSocket::readyRead);
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));
  const auto hello = currentHello();
  QCOMPARE(client.write(hello), hello.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  client.readAll();
  readyReadSpy.clear();

  const auto fixture = dropFixture();
  const auto encoded = deskflow::ipc::encodeFileTransferEdgeDropIpc(fixture);
  QCOMPARE(encoded.error, deskflow::ipc::FileTransferEdgeDropIpcError::None);
  QStringList logs;
  {
    LogCapture logCapture(logs);
    const auto request = QStringLiteral("fileTransferEdgeDrop=%1\n").arg(encoded.encodedValue).toUtf8();
    QCOMPARE(client.write(request), request.size());
    client.flush();
    QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
    QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("error\n"));
  }

  QCOMPARE(capture.count, 0);
  QCOMPARE(client.state(), QLocalSocket::ConnectedState);
  for (const auto &message : logs) {
    QVERIFY2(!message.contains(encoded.encodedValue), qPrintable(message));
    for (const auto &path : fixture.paths) {
      QVERIFY2(!message.contains(path), qPrintable(message));
    }
  }
}

void IpcServerTests::legacyCoreClientRetainsCommandsButCannotSubmitDrop()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestCoreIpcServer server(socketName, [](QLocalSocket *) { return true; });
  QVERIFY(server.listen());
  CapturedDrop capture;
  captureDrops(server, this, capture);
  QSignalSpy reloadSpy(&server, &deskflow::core::ipc::CoreIpcServer::reloadConfigRequested);
  QSignalSpy stopSpy(&server, &deskflow::core::ipc::IpcServer::stopProcessRequested);

  QLocalSocket client;
  QSignalSpy readyReadSpy(&client, &QLocalSocket::readyRead);
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));
  const auto mismatch = QByteArrayLiteral("hello=alpha.24-compatible-client\n");
  QCOMPARE(client.write(mismatch), mismatch.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("versionMismatch=%1\n").arg(currentVersionId()));
  readyReadSpy.clear();

  const auto encoded = deskflow::ipc::encodeFileTransferEdgeDropIpc(dropFixture());
  QCOMPARE(encoded.error, deskflow::ipc::FileTransferEdgeDropIpcError::None);
  const auto dropRequest = QStringLiteral("fileTransferEdgeDrop=%1\n").arg(encoded.encodedValue).toUtf8();
  QCOMPARE(client.write(dropRequest), dropRequest.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("error\n"));
  QCOMPARE(capture.count, 0);
  readyReadSpy.clear();

  QCOMPARE(client.write("reloadConfig\n"), 13);
  client.flush();
  QTRY_COMPARE_WITH_TIMEOUT(reloadSpy.count(), 1, 1000);
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("ok\n"));
  readyReadSpy.clear();

  QCOMPARE(client.write("stop\n"), 5);
  client.flush();
  QTRY_COMPARE_WITH_TIMEOUT(stopSpy.count(), 1, 1000);
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  const auto response = QString::fromUtf8(client.readAll());
  QVERIFY(response.contains(QStringLiteral("ok\n")));
  QVERIFY(response.contains(QStringLiteral("bye\n")));
  QCOMPARE(client.state(), QLocalSocket::ConnectedState);
}

void IpcServerTests::preHelloCoreClientCannotSubmitDrop()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestCoreIpcServer server(socketName, [](QLocalSocket *) { return true; });
  QVERIFY(server.listen());
  CapturedDrop capture;
  captureDrops(server, this, capture);

  QLocalSocket client;
  QSignalSpy readyReadSpy(&client, &QLocalSocket::readyRead);
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));
  const auto encoded = deskflow::ipc::encodeFileTransferEdgeDropIpc(dropFixture());
  QCOMPARE(encoded.error, deskflow::ipc::FileTransferEdgeDropIpcError::None);
  const auto request = QStringLiteral("fileTransferEdgeDrop=%1\n").arg(encoded.encodedValue).toUtf8();
  QCOMPARE(client.write(request), request.size());
  client.flush();

  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("error\n"));
  QCOMPARE(capture.count, 0);
}

void IpcServerTests::malformedAndOversizedCoreDropsAreRejected()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestCoreIpcServer server(socketName, [](QLocalSocket *) { return true; });
  QVERIFY(server.listen());
  CapturedDrop capture;
  captureDrops(server, this, capture);

  QLocalSocket client;
  QSignalSpy readyReadSpy(&client, &QLocalSocket::readyRead);
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));
  const auto hello = currentHello();
  QCOMPARE(client.write(hello), hello.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  client.readAll();
  readyReadSpy.clear();

  const auto malformed = QByteArrayLiteral("fileTransferEdgeDrop=%not-base64\n");
  QCOMPARE(client.write(malformed), malformed.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("error\n"));
  QCOMPARE(capture.count, 0);
  readyReadSpy.clear();

  const auto oversized =
      QStringLiteral("fileTransferEdgeDrop=%1\n").arg(QString(60 * 1024 + 1, QLatin1Char('A'))).toUtf8();
  QCOMPARE(client.write(oversized), oversized.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("error\n"));
  QCOMPARE(capture.count, 0);
}

void IpcServerTests::missingCoreDropArgumentIsRejected()
{
  const auto socketName = uniqueSocketName();
  QLocalServer::removeServer(socketName);

  TestCoreIpcServer server(socketName, [](QLocalSocket *) { return true; });
  QVERIFY(server.listen());
  CapturedDrop capture;
  captureDrops(server, this, capture);

  QLocalSocket client;
  QSignalSpy readyReadSpy(&client, &QLocalSocket::readyRead);
  client.connectToServer(socketName);
  QVERIFY(client.waitForConnected(1000));
  const auto hello = currentHello();
  QCOMPARE(client.write(hello), hello.size());
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  client.readAll();
  readyReadSpy.clear();

  QCOMPARE(client.write("fileTransferEdgeDrop\n"), 21);
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("error\n"));
  QCOMPARE(capture.count, 0);
  readyReadSpy.clear();

  QCOMPARE(client.write("fileTransferEdgeDrop=\n"), 22);
  client.flush();
  QTRY_VERIFY_WITH_TIMEOUT(!readyReadSpy.isEmpty(), 1000);
  QCOMPARE(QString::fromUtf8(client.readAll()), QStringLiteral("error\n"));
  QCOMPARE(capture.count, 0);
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
