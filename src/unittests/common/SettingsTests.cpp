/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsTests.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>

void SettingsTests::initTestCase()
{
  QVERIFY(QDir().mkpath(m_settingsPathTemp));

  m_portableSettingsFile = Settings::portableSettingsFile();
  QVERIFY(QDir().mkpath(QFileInfo(m_portableSettingsFile).absolutePath()));
  QFile portableSettings(m_portableSettingsFile);
  QVERIFY(portableSettings.open(QFile::WriteOnly | QFile::Truncate));
  portableSettings.close();

  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();
  QFile::remove(m_stateFile);

  QSettings legacySettings(m_settingsFile, QSettings::IniFormat);
  legacySettings.setValue(Settings::Client::ImeSyncLegacy, false);
  legacySettings.setValue(Settings::Client::EnterScreenLangLegacy, QStringLiteral("follow-server"));
  legacySettings.setValue(Settings::Security::Certificate, QStringLiteral("%1/deskflow.pem").arg(m_expectedTlsDir));
  legacySettings.sync();

  QSettings stateSettings(m_stateFile, QSettings::IniFormat);
  stateSettings.setValue(Settings::Gui::WindowGeometry, m_validWindowGeometry);
  stateSettings.setValue(m_unknownStateKey, QStringLiteral("stale"));
  stateSettings.sync();
}

void SettingsTests::setSettingsFile()
{
  Settings::setSettingsFile(m_settingsFile);
}

void SettingsTests::migratesImeSettingsToCore()
{
  QCOMPARE(Settings::value(Settings::Core::ImeSync).toBool(), false);
  QCOMPARE(Settings::value(Settings::Core::EnterScreenLang).toString(), QStringLiteral("follow-server"));

  Settings::save(false);
  QSettings migratedSettings(m_settingsFile, QSettings::IniFormat);
  QVERIFY(!migratedSettings.contains(Settings::Client::ImeSyncLegacy));
  QVERIFY(!migratedSettings.contains(Settings::Client::EnterScreenLangLegacy));

  Settings::setValue(Settings::Core::ImeSync, Settings::defaultValue(Settings::Core::ImeSync));
  Settings::setValue(Settings::Core::EnterScreenLang, Settings::defaultValue(Settings::Core::EnterScreenLang));
}

void SettingsTests::migratesLegacyCertificatePath()
{
  const auto certificate = Settings::value(Settings::Security::Certificate).toString();
  QCOMPARE(certificate, Settings::defaultValue(Settings::Security::Certificate).toString());
  QVERIFY(certificate.endsWith(QStringLiteral("/ieum.pem")));
}

void SettingsTests::setStateFile()
{
  Settings::setStateFile(m_stateFile);
}

void SettingsTests::preservesKnownStateKeysAndRemovesUnknownKeys()
{
  Settings::save(false);
  QSettings stateSettings(m_stateFile, QSettings::IniFormat);

  QCOMPARE(stateSettings.value(Settings::Gui::WindowGeometry).toRect(), m_validWindowGeometry);
  QVERIFY(!stateSettings.contains(m_unknownStateKey));
}

void SettingsTests::removesInvalidWindowGeometry_data()
{
  QTest::addColumn<QString>("suffix");
  QTest::addColumn<QVariant>("geometry");

  QTest::newRow("empty-string") << QStringLiteral("empty-string") << QVariant(QString());
  QTest::newRow("empty-rectangle") << QStringLiteral("empty-rectangle") << QVariant(QRect());
  QTest::newRow("non-rectangle") << QStringLiteral("non-rectangle") << QVariant(QStringLiteral("not-a-rectangle"));
  QTest::newRow("zero-width") << QStringLiteral("zero-width") << QVariant(QRect(10, 20, 0, 600));
}

void SettingsTests::removesInvalidWindowGeometry()
{
  QFETCH(QString, suffix);
  QFETCH(QVariant, geometry);
  const auto invalidStateFile = QStringLiteral("%1/Invalid-%2.state").arg(m_settingsPathTemp, suffix);
  QFile::remove(invalidStateFile);

  QSettings invalidStateSettings(invalidStateFile, QSettings::IniFormat);
  invalidStateSettings.setValue(Settings::Gui::WindowGeometry, geometry);
  invalidStateSettings.sync();
  QVERIFY(invalidStateSettings.contains(Settings::Gui::WindowGeometry));

  Settings::setStateFile(invalidStateFile);
  Settings::save(false);

  QSettings cleanedStateSettings(invalidStateFile, QSettings::IniFormat);
  QVERIFY(!cleanedStateSettings.contains(Settings::Gui::WindowGeometry));

  Settings::setStateFile(m_stateFile);
}

void SettingsTests::settingsFile()
{
  QVERIFY(Settings::settingsFile().endsWith(m_settingsFile));
}

void SettingsTests::settingsPath()
{
  QVERIFY(Settings::settingsPath().endsWith(m_settingsPath));
}

void SettingsTests::tlsDir()
{
  QVERIFY(Settings::tlsDir().endsWith(m_expectedTlsDir));
}

void SettingsTests::tlsTrustedServersDb()
{
  QVERIFY(Settings::tlsTrustedServersDb().endsWith(m_expectedTlsServerDB));
}

void SettingsTests::tlsTrustedClientsDb()
{
  QVERIFY(Settings::tlsTrustedClientsDb().endsWith(m_expectedTlsClientDB));
}

void SettingsTests::checkValidSettings()
{
  QSignalSpy spy(Settings::instance(), &Settings::settingsChanged);
  QVERIFY(spy.isValid());

  const auto keysToCheck = QRegularExpression(QLatin1String("[^%1]").arg(Settings::Core::ComputerName));
  const auto validKeys = Settings::validKeys().filter(keysToCheck);
  for (const auto &setting : validKeys) {
    const auto value = Settings::value(setting).toString();
    QCOMPARE(Settings::defaultValue(setting).toString(), value);

    Settings::setValue(setting, "NEW_VALUE");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<QString>(spy.first().at(0)), setting);
    QCOMPARE(Settings::value(setting).toString(), "NEW_VALUE");

    Settings::setValue(setting);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(Settings::value(setting).toString(), value);

    // Reset the spy for the next loop
    spy.clear();
    QCOMPARE(spy.count(), 0);
  }
}

void SettingsTests::checkCleanScreenName()
{
  const auto input = QStringLiteral("--!_ _-S@c#r$e%e^&*(n)= +Name\n[1]2|3?4--5>6<,7`~/8*90\\.lan--..    ..");
  const auto expected = QStringLiteral("Screen_Name_1234--567890.lan");

  Settings::setValue(Settings::Core::ComputerName, input);

  QCOMPARE(Settings::value(Settings::Core::ComputerName).toString(), expected);
}

void SettingsTests::checkCleanScreenName_LongName()
{
  QString input;
  input.fill('f', 300);
  input.prepend('.');

  QString expected;
  expected.fill('f', 255);

  Settings::setValue(Settings::Core::ComputerName, input);

  QCOMPARE(Settings::value(Settings::Core::ComputerName).toString(), expected);
}

void SettingsTests::cleanupTestCase()
{
  QFile::remove(m_portableSettingsFile);
}

QTEST_MAIN(SettingsTests)
