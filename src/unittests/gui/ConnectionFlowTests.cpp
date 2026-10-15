/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/TailscaleIntegration.h"
#include "gui/MainWindow.h"
#include "gui/StyleUtils.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/dialogs/ConnectionGuideDialog.h"
#include "gui/dialogs/DiagnosticDialog.h"
#include "gui/dialogs/ScreenSettingsDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/dialogs/SettingsDialog.h"

#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTextEdit>
#include <QTranslator>

using deskflow::core::ProcessState;
using deskflow::network::TailscalePeer;
using deskflow::network::TailscaleState;
using deskflow::network::TailscaleStatus;

class ConnectionFlowTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase()
  {
    qInfo("Initializing connection-flow tests");
    QApplication::setQuitOnLastWindowClosed(false);
    if (qEnvironmentVariableIsSet("IEUM_PREVIEW_DARK")) {
      QPalette dark;
      dark.setColor(QPalette::Window, QColor("#1c2026"));
      dark.setColor(QPalette::Base, QColor("#252a33"));
      dark.setColor(QPalette::Button, QColor("#313741"));
      dark.setColor(QPalette::Mid, QColor("#697386"));
      for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        dark.setColor(role, QColor("#edf0f5"));
        dark.setColor(QPalette::Disabled, role, QColor("#89929f"));
      }
      dark.setColor(QPalette::PlaceholderText, QColor("#a4aebb"));
      dark.setColor(QPalette::Highlight, QColor("#5ba5ed"));
      dark.setColor(QPalette::HighlightedText, QColor("#111923"));
      qApp->setPalette(dark);
    }
    deskflow::gui::updateIconTheme();
    QVERIFY(m_directory.isValid());
    Settings::setSettingsFile(m_directory.filePath("settings.ini"));
    Settings::setStateFile(m_directory.filePath("state.ini"));
    Settings::setValue(Settings::Security::TlsEnabled, false);
    Settings::setValue(Settings::Core::ProcessMode, Settings::ProcessMode::Desktop);
    Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
    Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
    Settings::setValue(Settings::Gui::AutoUpdateCheck, false);
    Settings::setValue(Settings::Gui::CloseReminder, false);
    Settings::setValue(Settings::Gui::LogExpanded, false);
    Settings::setValue(Settings::Core::ComputerName, "Studio-PC");
    QFile hostConfig(m_directory.filePath("host.conf"));
    QVERIFY(hostConfig.open(QIODevice::WriteOnly | QIODevice::Text));
    ServerConfig config;
    config.save(hostConfig);
    hostConfig.close();
    // Windows desktop tests must not depend on a pre-existing ProgramData
    // directory created by an installed application or service.
    Settings::setValue(Settings::Server::ExternalConfigFile, hostConfig.fileName());
    Settings::setValue(Settings::Server::ExternalConfig, true);
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

  void init()
  {
    qInfo("Running connection-flow case: %s", QTest::currentTestFunction());
    Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Client);
    Settings::setValue(Settings::Core::UseTailscale, true);
    Settings::setValue(Settings::Gui::AutoStartCore, false);
    Settings::setValue(Settings::Client::RemoteHost, "100.64.0.1");
  }

  void selectingAnotherHostReconnectsAndStopCancels()
  {
    MainWindow window(helperPath());
    window.populateTailscalePeers(status());
    savePreview(window, "client-ready.png");
    window.startCoreProcess();
    QTRY_COMPARE(window.m_coreProcess.processState(), ProcessState::Started);
    auto *picker = window.findChild<QComboBox *>("comboTailscalePeer");
    QVERIFY(picker != nullptr);
    const auto second = picker->findData("100.64.0.2");
    QVERIFY(second > 0);
    picker->setCurrentIndex(second);
    window.tailscalePeerSelected(second);
    QCOMPARE(Settings::value(Settings::Client::RemoteHost).toString(), QStringLiteral("100.64.0.2"));
    QVERIFY(window.findChild<QLabel *>("lblConnectionTitle")->text().contains("100.64.0.2"));
    QTRY_COMPARE_WITH_TIMEOUT(window.m_coreProcess.processState(), ProcessState::Started, 8000);
    QTRY_COMPARE(window.m_activeRemoteHost, QStringLiteral("100.64.0.2"));
    // A queued Started update must keep the running destination even if the
    // user has begun typing the next address without committing it yet.
    window.findChild<QLineEdit *>("lineHostname")->setText(QStringLiteral("100.64.0.3"));
    window.coreProcessStateChanged(ProcessState::Started);
    QCOMPARE(window.m_activeRemoteHost, QStringLiteral("100.64.0.2"));
    window.stopCore();
    QTRY_COMPARE_WITH_TIMEOUT(window.m_coreProcess.processState(), ProcessState::Stopped, 8000);
    QVERIFY(!Settings::value(Settings::Gui::AutoStartCore).toBool());
  }

  void refreshDoesNotReplaceSavedHostWithSoleOnlinePeer()
  {
    MainWindow window(helperPath());
    auto peers = status();
    peers.peers.removeFirst();
    window.populateTailscalePeers(peers);
    QCOMPARE(window.findChild<QLineEdit *>("lineHostname")->text(), QStringLiteral("100.64.0.1"));
    QCOMPARE(window.findChild<QComboBox *>("comboTailscalePeer")->currentIndex(), 0);
  }

  void roleChangeAfterStartCancelsThePreviousCore()
  {
    Settings::setValue(Settings::Core::UseTailscale, false);
    MainWindow window(helperPath());
    window.startCoreProcess();
    QVERIFY(
        window.m_coreProcess.processState() == ProcessState::Starting ||
        window.m_coreProcess.processState() == ProcessState::Started
    );
    window.findChild<QRadioButton *>("rbModeServer")->click();
    QCOMPARE(window.coreMode(), Settings::CoreMode::Server);
    QTRY_COMPARE_WITH_TIMEOUT(window.m_coreProcess.processState(), ProcessState::Stopped, 7000);
    QTest::qWait(1200);
    QCOMPARE(window.m_coreProcess.processState(), ProcessState::Stopped);
    QVERIFY(!window.m_coreProcess.hasForegroundProcess());
  }

  void waitingHostAlwaysExplainsOtherHostConnections()
  {
    Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Server);
    Settings::setValue(Settings::Core::UseTailscale, false);
    MainWindow window(helperPath());
    window.startCoreProcess();
    QTRY_COMPARE(window.m_coreProcess.processState(), ProcessState::Started);
    window.updateConnectionGuide();
    QVERIFY(window.findChild<QLabel *>("lblConnectionDetail")->text().contains("another host"));
    savePreview(window, "host-waiting.png");
    window.stopCore();
    QTRY_COMPARE_WITH_TIMEOUT(window.m_coreProcess.processState(), ProcessState::Stopped, 8000);
  }

  void settingsSearchAndShortcutsSelectTheRightPage()
  {
    ServerConfig config;
    SettingsDialog dialog(nullptr, config);
    dialog.selectPage("tabInput");
    auto *tabs = dialog.findChild<QTabWidget *>();
    QCOMPARE(tabs->currentWidget()->objectName(), QStringLiteral("tabInput"));
    auto *search = dialog.findChild<QLineEdit *>("settingsSearch");
    search->setText("Tailscale");
    QCOMPARE(tabs->currentWidget()->objectName(), QStringLiteral("tabNetwork"));
    search->setText("no-such-setting-0239");
    QVERIFY(tabs->isHidden());
    search->clear();
    QVERIFY(!tabs->isHidden());
    savePreview(dialog, "settings.png");
  }

  void searchResultJumpsToTheActualControl()
  {
    Settings::setValue(Settings::Core::UseTailscale, false);
    ServerConfig config;
    SettingsDialog dialog(nullptr, config);
    QSignalSpy accepted(&dialog, &QDialog::accepted);
    dialog.show();
    auto *search = dialog.findChild<QLineEdit *>("settingsSearch");
    auto *results = dialog.findChild<QListWidget *>("settingsSearchResults");
    search->setText("Port");
    QListWidgetItem *port = nullptr;
    for (int i = 0; i < results->count(); ++i) {
      if (results->item(i)->data(Qt::UserRole).toString() == QStringLiteral("lblPort")) {
        port = results->item(i);
      }
    }
    QVERIFY(port != nullptr);
    results->setCurrentItem(port);
    results->setFocus();
    QTest::keyClick(results, Qt::Key_Return);
    QCOMPARE(accepted.count(), 0);
    QVERIFY(dialog.isVisible());
    QCOMPARE(dialog.findChild<QTabWidget *>()->currentWidget()->objectName(), QStringLiteral("tabNetwork"));
    QCOMPARE(dialog.focusWidget(), dialog.findChild<QSpinBox *>("sbPort"));
    QVERIFY(dialog.findChild<QLabel *>("settingsPageDescription")->text().contains("Port"));
    search->setFocus();
    QTest::keyClick(search, Qt::Key_Enter);
    QCOMPARE(accepted.count(), 0);
    QVERIFY(dialog.isVisible());
    dialog.selectPage("tabInput");
    QVERIFY(search->text().isEmpty());
    QCOMPARE(dialog.findChild<QTabWidget *>()->currentWidget()->objectName(), QStringLiteral("tabInput"));
  }

  void guideChangesRoleAndRemainsAvailable()
  {
    Settings::setValue(Settings::Core::UseTailscale, false);
    Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::None);
    MainWindow window(helperPath());
    window.openConnectionGuide();
    auto *guide = window.findChild<ConnectionGuideDialog *>();
    QVERIFY(guide != nullptr);
    QVERIFY(!guide->findChild<QPushButton *>("guideArrange")->isEnabled());
    guide->findChild<QPushButton *>("guideChooseHost")->click();
    QCOMPARE(window.coreMode(), Settings::CoreMode::Server);
    QVERIFY(guide->findChild<QPushButton *>("guideArrange")->isEnabled());
    guide->findChild<QPushButton *>("guideChooseClient")->click();
    QCOMPARE(window.coreMode(), Settings::CoreMode::Client);
    QVERIFY(!guide->findChild<QPushButton *>("guideArrange")->isEnabled());
    guide->reject();
  }

  void diagnosticActionsPreserveTheReportWhenBrowserFails()
  {
    QStandardPaths::setTestModeEnabled(true);
    const auto report = deskflow::gui::diagnostic::createReport(deskflow::gui::diagnostic::ReportKind::UserInitiated);
    int opens = 0;
    DiagnosticDialog dialog(report, nullptr, {}, [&opens](const QUrl &) {
      ++opens;
      return false;
    });
    dialog.show();
    QCOMPARE(dialog.findChild<QTextEdit *>("diagnosticPreview")->toPlainText(), report.markdown);
    dialog.findChild<QPushButton *>("copyDiagnostic")->click();
    QCOMPARE(QApplication::clipboard()->text(), report.markdown);
    QVERIFY(dialog.isVisible());
    dialog.findChild<QPushButton *>("openDiagnosticIssue")->click();
    QCOMPARE(opens, 1);
    QCOMPARE(QApplication::clipboard()->text(), report.markdown);
    QVERIFY(dialog.findChild<QLabel *>("diagnosticActionStatus")->text().contains("could not be opened"));
    QVERIFY(!dialog.findChild<QLineEdit *>("diagnosticIssueAddress")->text().isEmpty());
    QVERIFY(dialog.isVisible());
    dialog.reject();
    QFile::remove(report.filePath);
  }

  void captureKoreanPreviews()
  {
    const auto translation = qEnvironmentVariable("IEUM_PREVIEW_TRANSLATION");
    if (translation.isEmpty()) {
      return;
    }
    QTranslator translator;
    QVERIFY(translator.load(translation));
    QCoreApplication::installTranslator(&translator);
    Settings::setValue(Settings::Server::ExternalConfig, false);
    Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Server);
    Settings::setValue(Settings::Core::UseTailscale, false);
    Settings::setValue(Settings::Core::Interface, "192.0.2.10");
    Settings::setValue(Settings::Security::Certificate, "~/.config/Ieum/tls/ieum.pem");
    MainWindow window(helperPath());
    savePreview(window, "home-ko.png");
    SettingsDialog settings(&window, window.serverConfig());
    settings.selectPage("tabInput");
    savePreview(settings, "settings-ko.png");
    settings.findChild<QLineEdit *>("settingsSearch")->setText(QStringLiteral("Tailscale"));
    savePreview(settings, "search-ko.png");
    ConnectionGuideDialog guide(&window);
    savePreview(guide, "guide-ko.png");
    ServerConfigDialog desk(&window, window.serverConfig());
    desk.findChild<QLineEdit *>("newComputerName")->setText(QStringLiteral("MacBook"));
    desk.findChild<QPushButton *>("addComputer")->click();
    savePreview(desk, "desk-ko.png");
    Screen demo(QStringLiteral("MacBook"));
    const ServerConfig &config = window.serverConfig();
    ScreenSettingsDialog computer(&window, &demo, &config.screens());
    savePreview(computer, "computer-ko.png");
    ClientConfigDialog client(&window);
    savePreview(client, "client-options-ko.png");
    QCoreApplication::removeTranslator(&translator);
  }

private:
  TailscaleStatus status() const
  {
    TailscaleStatus result;
    result.state = TailscaleState::Running;
    result.localAddresses = {QStringLiteral("100.64.0.9")};
    result.peers = {
        TailscalePeer{"a", "Office", "office.example", "windows", {"100.64.0.1"}, true},
        TailscalePeer{"b", "Studio", "studio.example", "macOS", {"100.64.0.2"}, true}
    };
    return result;
  }

  QString helperPath() const
  {
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + "/CoreProcessTestHelper.exe";
#else
    return QCoreApplication::applicationDirPath() + "/CoreProcessTestHelper";
#endif
  }

  void savePreview(QWidget &widget, const QString &file)
  {
    const auto directory = qEnvironmentVariable("IEUM_SCREENSHOT_DIR");
    if (directory.isEmpty()) {
      return;
    }
    QDir().mkpath(directory);
    widget.show();
    QTest::qWait(100);
    widget.grab().save(directory + QLatin1Char('/') + file);
    widget.hide();
  }

  QTemporaryDir m_directory;
};

QTEST_MAIN(ConnectionFlowTests)
#include "ConnectionFlowTests.moc"
