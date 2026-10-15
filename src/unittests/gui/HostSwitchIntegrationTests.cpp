/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/TailscaleIntegration.h"
#include "gui/MainWindow.h"
#include "gui/StyleUtils.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include <signal.h>

using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;
using deskflow::network::TailscalePeer;
using deskflow::network::TailscaleState;
using deskflow::network::TailscaleStatus;

class HostSwitchIntegrationTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase()
  {
    if (qEnvironmentVariable("IEUM_INTEGRATION_CLIENT_SETTINGS").isEmpty()) {
      QSKIP("Run with tools/tests/run_host_switch.py; this test requires isolated X displays and real TLS cores.");
    }
    QApplication::setQuitOnLastWindowClosed(false);
    deskflow::gui::updateIconTheme();
    Settings::setSettingsFile(qEnvironmentVariable("IEUM_INTEGRATION_CLIENT_SETTINGS"));
    Settings::setStateFile(Settings::settingsFile() + ".state");
    Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
    Settings::setValue(Settings::Core::ProcessMode, Settings::ProcessMode::Desktop);
    Settings::setValue(Settings::Core::UseTailscale, true);
    Settings::setValue(Settings::Client::RemoteHost, "127.0.0.1");
    Settings::setValue(Settings::Gui::AutoStartCore, false);
    Settings::setValue(Settings::Gui::AutoUpdateCheck, false);
    Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
    Settings::setValue(Settings::Gui::CloseReminder, false);
    Settings::setValue(Settings::Gui::LogExpanded, false);
    Settings::save(false);
  }

  void dropdownSwitchesRealTlsCoresAndRoutesInput()
  {
    MainWindow window(qEnvironmentVariable("IEUM_INTEGRATION_CORE"));
    TailscaleStatus peers;
    peers.state = TailscaleState::Running;
    peers.localAddresses = {"127.0.0.3"};
    peers.peers = {
        TailscalePeer{"host-a", "Host A", "host-a.example", "linux", {"127.0.0.1"}, true},
        TailscalePeer{"host-b", "Host B", "host-b.example", "linux", {"127.0.0.2"}, true}
    };
    window.populateTailscalePeers(peers);
    window.startCoreProcess();
    auto *picker = window.findChild<QComboBox *>("comboTailscalePeer");
    QVERIFY(picker != nullptr);
    QSignalSpy secure(&window.m_coreProcess, &deskflow::gui::CoreProcess::secureSocket);
    constexpr int switches = 10;
    for (int iteration = 0; iteration < switches; ++iteration) {
      const bool hostA = iteration % 2 == 0;
      const QString address = hostA ? "127.0.0.1" : "127.0.0.2";
      picker->setCurrentIndex(picker->findData(address));
      window.tailscalePeerSelected(picker->currentIndex());
      QTRY_COMPARE_WITH_TIMEOUT(window.m_coreProcess.connectionState(), ConnectionState::Connected, 15000);
      QCOMPARE(window.m_activeRemoteHost, address);
      QVERIFY(window.m_secureSocket);
      QProcess inputProbe;
      inputProbe.start(
          "python3", {qEnvironmentVariable("IEUM_INTEGRATION_INPUT_PROBE"),
                      hostA ? qEnvironmentVariable("IEUM_INTEGRATION_HOST_A_DISPLAY")
                            : qEnvironmentVariable("IEUM_INTEGRATION_HOST_B_DISPLAY"),
                      qEnvironmentVariable("DISPLAY")}
      );
      QTRY_COMPARE_WITH_TIMEOUT(inputProbe.state(), QProcess::NotRunning, 8000);
      const auto output = inputProbe.readAllStandardOutput();
      QVERIFY2(inputProbe.exitCode() == 0, qPrintable(QString::fromUtf8(output + inputProbe.readAllStandardError())));
      const auto result = QJsonDocument::fromJson(output).object();
      QVERIFY(result.value("keyReceived").toBool());
      QVERIFY(result.value("pointerMoved").toBool());
      qInfo().noquote(
      ) << QStringLiteral("Verified switch %1: %2, TLS, remote key and pointer received").arg(iteration).arg(address);
    }
    QVERIFY(!secure.isEmpty());

    // Change targets without giving the old process time to exit. Only the
    // final selection may start, and only one foreground core may survive.
    QSignalSpy restarts(&window.m_coreProcess, &deskflow::gui::CoreProcess::processStateChanged);
    for (const auto &address : {"127.0.0.1", "127.0.0.2", "127.0.0.1", "127.0.0.2", "127.0.0.1"}) {
      picker->setCurrentIndex(picker->findData(address));
      window.tailscalePeerSelected(picker->currentIndex());
    }
    QTRY_COMPARE_WITH_TIMEOUT(window.m_coreProcess.connectionState(), ConnectionState::Connected, 15000);
    QCOMPARE(window.m_activeRemoteHost, QStringLiteral("127.0.0.1"));
    int starts = 0;
    for (const auto &state : restarts) {
      starts += state.first().value<ProcessState>() == ProcessState::Started;
    }
    QCOMPARE(starts, 1);
    int children = 0;
    for (auto *child : window.m_coreProcess.findChildren<QProcess *>()) {
      children += child->state() != QProcess::NotRunning;
    }
    QCOMPARE(children, 1);
    verifyInput(qEnvironmentVariable("IEUM_INTEGRATION_HOST_A_DISPLAY"));
    qInfo("Verified rapid selection: only the final host started; one client core remains");

    // Terminate only the host PID created by this test's Python harness.
    const auto hostPid = qEnvironmentVariable("IEUM_INTEGRATION_HOST_A_PID").toLongLong();
    QVERIFY(hostPid > 0);
    QVERIFY(::kill(static_cast<pid_t>(hostPid), SIGTERM) == 0);
    QTRY_VERIFY_WITH_TIMEOUT(window.m_coreProcess.connectionState() != ConnectionState::Connected, 15000);
    QCOMPARE(window.m_coreProcess.processState(), ProcessState::Started);
    QProcess replacement;
    const auto settings = qEnvironmentVariable("IEUM_INTEGRATION_HOST_A_SETTINGS");
    const QDir directory(QFileInfo(settings).absoluteDir().absoluteFilePath(QStringLiteral("../..")));
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("TMPDIR", directory.filePath("tmp"));
    environment.insert("XDG_CONFIG_HOME", directory.filePath("config"));
    environment.insert("XDG_STATE_HOME", directory.filePath("state"));
    environment.insert("XDG_RUNTIME_DIR", directory.filePath("runtime"));
    environment.insert("DISPLAY", qEnvironmentVariable("IEUM_INTEGRATION_HOST_A_DISPLAY"));
    replacement.setProcessEnvironment(environment);
    replacement.start(
        qEnvironmentVariable("IEUM_INTEGRATION_CORE"), {"server", "--new-instance", "--settings", settings}
    );
    QTRY_COMPARE_WITH_TIMEOUT(window.m_coreProcess.connectionState(), ConnectionState::Connected, 20000);
    QVERIFY(window.m_secureSocket);
    verifyInput(qEnvironmentVariable("IEUM_INTEGRATION_HOST_A_DISPLAY"));
    qInfo("Verified host termination and recovery: TLS and input restored without pressing Connect");

    picker->setCurrentIndex(picker->findData("127.0.0.2"));
    window.tailscalePeerSelected(picker->currentIndex());
    QCoreApplication::processEvents();
    auto *cancel = window.findChild<QPushButton *>("btnToggleCore");
    QVERIFY(cancel->isEnabled());
    QVERIFY(cancel->text() == QStringLiteral("Cancel") || cancel->text() == QStringLiteral("Cancel switch"));
    cancel->click();
    QTRY_COMPARE_WITH_TIMEOUT(window.m_coreProcess.processState(), ProcessState::Stopped, 8000);
    QVERIFY(!Settings::value(Settings::Gui::AutoStartCore).toBool());
    QTest::qWait(2500);
    QCOMPARE(window.m_coreProcess.processState(), ProcessState::Stopped);
    QVERIFY(!window.m_coreProcess.hasForegroundProcess());
    qInfo("Verified Stop during switch cancels the replacement connection");

    picker->setCurrentIndex(picker->findData("127.0.0.1"));
    window.tailscalePeerSelected(picker->currentIndex());
    window.startCoreProcess();
    QTRY_COMPARE_WITH_TIMEOUT(window.m_coreProcess.connectionState(), ConnectionState::Connected, 15000);
    QTRY_VERIFY(Settings::value(Settings::Gui::AutoStartCore).toBool());
    window.quitApplication();
    QTRY_VERIFY_WITH_TIMEOUT(!window.m_coreProcess.hasForegroundProcess(), 7000);
    QCOMPARE(window.m_coreProcess.processState(), ProcessState::Stopped);
    QVERIFY(Settings::value(Settings::Gui::AutoStartCore).toBool());
    qInfo("Verified application quit waits for the foreground core and preserves startup intent");
    replacement.terminate();
    QTRY_COMPARE_WITH_TIMEOUT(replacement.state(), QProcess::NotRunning, 5000);
  }

private:
  void verifyInput(const QString &hostDisplay)
  {
    QProcess probe;
    probe.start(
        "python3", {qEnvironmentVariable("IEUM_INTEGRATION_INPUT_PROBE"), hostDisplay, qEnvironmentVariable("DISPLAY")}
    );
    QTRY_COMPARE_WITH_TIMEOUT(probe.state(), QProcess::NotRunning, 8000);
    const auto output = probe.readAllStandardOutput();
    QVERIFY2(probe.exitCode() == 0, qPrintable(QString::fromUtf8(output + probe.readAllStandardError())));
    const auto result = QJsonDocument::fromJson(output).object();
    QVERIFY(result.value("keyReceived").toBool());
    QVERIFY(result.value("pointerMoved").toBool());
  }
};

QTEST_MAIN(HostSwitchIntegrationTests)
#include "HostSwitchIntegrationTests.moc"
