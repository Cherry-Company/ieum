/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/Settings.h"

#include <QRect>
#include <QTest>

class SettingsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  // Test are run in order top to bottom
  void setSettingsFile();
  void migratesImeSettingsToCore();
  void migratesLegacyCertificatePath();
  void setStateFile();
  void preservesKnownStateKeysAndRemovesUnknownKeys();
  void removesInvalidWindowGeometry_data();
  void removesInvalidWindowGeometry();
  void settingsFile();
  void settingsPath();
  void tlsDir();
  void tlsTrustedServersDb();
  void tlsTrustedClientsDb();
  void checkValidSettings();
  void checkCleanScreenName();
  void checkCleanScreenName_LongName();
  void cleanupTestCase();

private:
  inline static const QString m_settingsPathTemp = QStringLiteral("tmp/test");
  inline static const QString m_settingsFile = QStringLiteral("%1/Deskflow.conf").arg(m_settingsPathTemp);
  inline static const QString m_stateFile = QStringLiteral("%1/Deskflow.state").arg(m_settingsPathTemp);
  inline static const QString m_unknownStateKey = QStringLiteral("legacy/obsoleteState");
  inline static const QRect m_validWindowGeometry = QRect(20, 30, 800, 600);
  QString m_portableSettingsFile;

  // Keep this test process in portable mode so singleton construction never
  // reads or migrates the developer's real user settings.
  inline static const QString m_settingsPath = m_settingsPathTemp;

  inline static const QString m_expectedTlsDir = QStringLiteral("%1/tls").arg(m_settingsPath);
  inline static const QString m_expectedTlsServerDB = QStringLiteral("%1/trusted-servers").arg(m_expectedTlsDir);
  inline static const QString m_expectedTlsClientDB = QStringLiteral("%1/trusted-clients").arg(m_expectedTlsDir);
};
