/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/ExitCodes.h"
#include "gui/core/CoreProcess.h"
#include "gui/ipc/CoreIpcClient.h"

#include <QElapsedTimer>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using deskflow::core::ProcessState;
using deskflow::gui::CoreProcess;

class CoreProcessTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase()
  {
    QVERIFY(m_directory.isValid());
    Settings::setSettingsFile(m_directory.filePath("settings.ini"));
    Settings::setStateFile(m_directory.filePath("state.ini"));
    Settings::setValue(Settings::Core::ProcessMode, Settings::ProcessMode::Desktop);
    Settings::setValue(Settings::Client::RemoteHost, "127.0.0.1");
    QProcess probe;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("IEUM_TEST_CORE_EXIT_CODE"), QStringLiteral("0"));
    probe.setProcessEnvironment(environment);
    probe.start(helperPath());
    QVERIFY2(probe.waitForStarted(5000), qPrintable(probe.errorString()));
    QVERIFY2(probe.waitForFinished(5000), qPrintable(probe.errorString()));
    QCOMPARE(probe.exitStatus(), QProcess::NormalExit);
    QCOMPARE(probe.exitCode(), 0);
  }

  void stopImmediatelyAfterStartCancelsWithoutBlocking()
  {
    ServerConfig config;
    CoreProcess core(config, helperPath());
    core.setMode(Settings::CoreMode::Client);
    core.setAddress("127.0.0.1");
    core.start();
    // Windows can emit QProcess::started synchronously inside start().
    QVERIFY(core.processState() == ProcessState::Starting || core.processState() == ProcessState::Started);
    QElapsedTimer timer;
    timer.start();
    core.stop();
    QVERIFY(timer.elapsed() < 500);
    QTRY_COMPARE_WITH_TIMEOUT(core.processState(), ProcessState::Stopped, 8000);
    QTest::qWait(1100);
    QCOMPARE(core.processState(), ProcessState::Stopped);
  }

  void rapidRestartsSerializeAndStopCancelsRetry()
  {
    ServerConfig config;
    CoreProcess core(config, helperPath());
    core.setMode(Settings::CoreMode::Client);
    core.setAddress("127.0.0.1");
    core.start();
    QTRY_COMPARE(core.processState(), ProcessState::Started);
    QSignalSpy states(&core, &CoreProcess::processStateChanged);
    core.restart();
    core.setAddress("127.0.0.2");
    core.restart();
    core.setAddress("127.0.0.3");
    core.restart();
    QTRY_COMPARE_WITH_TIMEOUT(core.processState(), ProcessState::RetryPending, 8000);
    QTRY_COMPARE_WITH_TIMEOUT(core.processState(), ProcessState::Started, 3000);
    int starts = 0;
    for (const auto &state : states) {
      starts += state.first().value<ProcessState>() == ProcessState::Started;
    }
    QCOMPARE(starts, 1);
    core.restart();
    QTRY_COMPARE_WITH_TIMEOUT(core.processState(), ProcessState::RetryPending, 8000);
    core.stop();
    QTest::qWait(1200);
    QCOMPARE(core.processState(), ProcessState::Stopped);
  }

  void stopCancelsLateDuplicateRecovery_data()
  {
    QTest::addColumn<QString>("signal");
    QTest::newRow("connection-failed") << QStringLiteral("connectionFailed");
    QTest::newRow("old-server-shutdown") << QStringLiteral("serverShutdown");
  }

  void stopCancelsLateDuplicateRecovery()
  {
    QFETCH(QString, signal);
    ServerConfig config;
    CoreProcess core(config, helperPath());
    core.setMode(Settings::CoreMode::Client);
    core.setAddress("127.0.0.1");
    qputenv("IEUM_TEST_CORE_EXIT_CODE", QByteArray::number(s_exitDuplicate));
    core.start();
    // QProcess inherits the environment in start(), before this is removed.
    qunsetenv("IEUM_TEST_CORE_EXIT_CODE");
    using deskflow::gui::ipc::CoreIpcClient;
    QTRY_VERIFY(core.findChild<CoreIpcClient *>() != nullptr);
    auto *oldRecovery = core.findChild<CoreIpcClient *>();
    core.stop();
    QCOMPARE(core.processState(), ProcessState::Stopped);
    QVERIFY(QMetaObject::invokeMethod(oldRecovery, qPrintable(signal), Qt::DirectConnection));
    QCOMPARE(core.processState(), ProcessState::Stopped);
    QTest::qWait(1200);
    QCOMPARE(core.processState(), ProcessState::Stopped);
    QVERIFY(!core.hasForegroundProcess());
  }

private:
  QString helperPath() const
  {
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + "/CoreProcessTestHelper.exe";
#else
    return QCoreApplication::applicationDirPath() + "/CoreProcessTestHelper";
#endif
  }
  QTemporaryDir m_directory;
};

QTEST_MAIN(CoreProcessTests)
#include "CoreProcessTests.moc"
