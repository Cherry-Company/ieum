/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LoggerTests.h"
#include "common/Settings.h"

#include "gui/Logger.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>

using namespace deskflow::gui;

void LoggerTests::initTestCase()
{
  QDir dir;
  QVERIFY(dir.mkpath(m_settingsPath));

  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();

  Settings::setSettingsFile(m_settingsFile);
  Settings::setStateFile(m_stateFile);
}

void LoggerTests::newLine()
{
  QSignalSpy spy(Logger::instance(), &Logger::newLine);
  QVERIFY(spy.isValid());

  Settings::setValue(Settings::Log::GuiDebug, true);
  Logger::instance()->handleMessage(QtDebugMsg, "stub", "test");

  QCOMPARE(spy.count(), 1);
  QVERIFY(qvariant_cast<QString>(spy.takeFirst().at(0)).contains("test"));
  Settings::setValue(Settings::Log::GuiDebug, false);
}

void LoggerTests::noNewLine()
{
  bool newLineEmitted = false;

  QSignalSpy spy(Logger::instance(), &Logger::newLine);
  QVERIFY(spy.isValid());

  Settings::setValue(Settings::Log::GuiDebug, false);
  Logger::instance()->handleMessage(QtDebugMsg, "stub", "test");
  QCOMPARE(spy.count(), 0);
  QVERIFY(!newLineEmitted);
}

void LoggerTests::ignoredQtCursorWarning()
{
  QSignalSpy spy(Logger::instance(), &Logger::newLine);
  QVERIFY(spy.isValid());

  Logger::instance()->handleMessage(
      QtWarningMsg, "qtextcursor.cpp", "QTextCursor::setPosition: Position '42' out of range"
  );
  QCOMPARE(spy.count(), 0);
}

void LoggerTests::recursiveMessage()
{
  auto logger = Logger::instance();
  QSignalSpy spy(logger, &Logger::newLine);
  QVERIFY(spy.isValid());

  const auto connection = connect(logger, &Logger::newLine, logger, [logger]() {
    logger->handleMessage(QtWarningMsg, "stub", "nested warning");
  });
  logger->handleMessage(QtInfoMsg, "stub", "outer message");
  disconnect(connection);

  QCOMPARE(spy.count(), 1);
  QVERIFY(qvariant_cast<QString>(spy.takeFirst().at(0)).contains("outer message"));
}

QTEST_MAIN(LoggerTests)
