/*
 * Ieum -- keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/Settings.h"
#include "gui/widgets/DeskWorkspace.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

#include <utility>

class DeskWorkspaceTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void init()
  {
    QVERIFY(m_directory.isValid());
    Settings::setSettingsFile(m_directory.filePath(QString::fromLatin1(QTest::currentTestFunction()) + ".ini"));
    Settings::setStateFile(m_directory.filePath("state.ini"));
    Settings::setValue(Settings::Core::ComputerName, "Studio-PC");
    Settings::setValue(Settings::Server::ExternalConfig, false);
  }

  void selectingAComputerShowsItsActualConnectionState()
  {
    ServerConfig config;
    config.addClient("MacBook");
    DeskWorkspace desk(config);
    desk.resize(1080, 600);
    desk.refresh(true, "Studio-PC", {});
    desk.show();
    QVERIFY(desk.findChild<QWidget *>("deviceInspector")->isHidden());
    auto selectPeer = [&desk] {
      for (auto *button : desk.findChildren<QToolButton *>()) {
        if (button->property("deskComputer").toBool() && button->text().startsWith("MacBook\n")) {
          button->click();
          return true;
        }
      }
      return false;
    };
    QVERIFY(selectPeer());
    QVERIFY(!desk.findChild<QWidget *>("deviceInspector")->isHidden());
    QCOMPARE(desk.findChild<QLabel *>("deskDeviceStatus")->text(), "Not connected");
    QVERIFY(!desk.findChild<QPushButton *>("deskDeviceConnectionHelp")->isHidden());
    desk.refresh(true, "Studio-PC", {"MacBook"});
    QVERIFY(!desk.findChild<QWidget *>("deviceInspector")->isHidden());
    QCOMPARE(desk.findChild<QLabel *>("deskDeviceStatus")->text(), "Connected");
    QVERIFY(desk.findChild<QPushButton *>("deskDeviceConnectionHelp")->isHidden());
    desk.findChild<QToolButton *>("closeDeviceInspector")->click();
    QVERIFY(desk.findChild<QWidget *>("deviceInspector")->isHidden());
  }

  void addingAndMovingOnlyPersistWhenSaved()
  {
    ServerConfig config;
    DeskWorkspace desk(config);
    desk.refresh(true, "Studio-PC", {});
    desk.findChild<QPushButton *>("deskAddComputer")->click();
    desk.findChild<QLineEdit *>("deskNewComputerName")->setText("MacBook");
    desk.findChild<QPushButton *>("deskConfirmAddComputer")->click();
    QVERIFY(!config.screenExists("MacBook"));
    desk.findChild<QPushButton *>("cancelDeskLayout")->click();
    QVERIFY(!config.screenExists("MacBook"));

    desk.findChild<QPushButton *>("deskAddComputer")->click();
    desk.findChild<QLineEdit *>("deskNewComputerName")->setText("MacBook");
    desk.findChild<QComboBox *>("deskNewComputerSide")->setCurrentIndex(1);
    desk.findChild<QPushButton *>("deskConfirmAddComputer")->click();
    desk.findChild<QToolButton *>("deskMove0")->click();
    QSignalSpy committed(&desk, &DeskWorkspace::configurationCommitted);
    desk.findChild<QPushButton *>("editDeskLayout")->click();
    QCOMPARE(committed.count(), 1);
    QVERIFY(config.screenExists("MacBook"));
    const auto &screens = std::as_const(config).screens();
    int peer = -1, host = -1;
    for (int i = 0; i < screens.size(); ++i) {
      if (screens[i].name() == "MacBook")
        peer = i;
      if (screens[i].isServer())
        host = i;
    }
    const int columns = Settings::value(Settings::Server::GridWidth).toInt();
    QCOMPARE(peer, host - columns - 1);
    ServerConfig restored;
    QVERIFY(restored.screenExists("MacBook"));
  }

  void invalidAndDuplicateNamesDoNotReplaceAComputer()
  {
    ServerConfig config;
    DeskWorkspace desk(config);
    desk.refresh(true, "Studio-PC", {});
    desk.findChild<QPushButton *>("deskAddComputer")->click();
    for (const auto &name : {QString("invalid name"), QString("studio-pc")}) {
      desk.findChild<QLineEdit *>("deskNewComputerName")->setText(name);
      desk.findChild<QPushButton *>("deskConfirmAddComputer")->click();
      QVERIFY(!desk.findChild<QLabel *>("deskFeedback")->isHidden());
    }
    desk.findChild<QPushButton *>("editDeskLayout")->click();
    QCOMPARE(config.numScreens(), 1);
  }

  void concurrentLayoutChangeIsNotOverwritten()
  {
    ServerConfig config;
    DeskWorkspace desk(config);
    desk.refresh(true, "Studio-PC", {});
    desk.findChild<QPushButton *>("deskAddComputer")->click();
    desk.findChild<QLineEdit *>("deskNewComputerName")->setText("Pending");
    desk.findChild<QPushButton *>("deskConfirmAddComputer")->click();
    config.addClient("AddedElsewhere");
    QSignalSpy committed(&desk, &DeskWorkspace::configurationCommitted);
    desk.findChild<QPushButton *>("editDeskLayout")->click();
    QCOMPARE(committed.count(), 0);
    QVERIFY(config.screenExists("AddedElsewhere"));
    QVERIFY(!config.screenExists("Pending"));
    QVERIFY(desk.findChild<QLabel *>("deskFeedback")->text().contains("changed elsewhere"));
  }

  void externalConfigurationCannotBeEditedThroughTheOverview()
  {
    ServerConfig config;
    config.addClient("InternalOnly");
    Settings::setValue(Settings::Server::ExternalConfig, true);
    DeskWorkspace desk(config);
    desk.refresh(true, "Studio-PC", {});
    QSignalSpy advanced(&desk, &DeskWorkspace::advancedSettingsRequested);
    desk.findChild<QPushButton *>("editDeskLayout")->click();
    QCOMPARE(advanced.count(), 1);
    QVERIFY(desk.findChild<QPushButton *>("cancelDeskLayout")->isHidden());
    int computers = 0;
    for (auto *button : desk.findChildren<QToolButton *>()) {
      if (button->property("deskComputer").toBool()) {
        ++computers;
        QVERIFY(!button->text().contains("InternalOnly"));
      }
    }
    QCOMPARE(computers, 1);
  }

private:
  QTemporaryDir m_directory;
};

QTEST_MAIN(DeskWorkspaceTests)
#include "DeskWorkspaceTests.moc"
