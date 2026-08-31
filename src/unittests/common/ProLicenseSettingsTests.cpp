/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

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

} // namespace

class ProLicenseSettingsTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void reportsNoEntitlementForMissingAndMalformedLicenses();
  void clearsAlpha20TransferPreferencesDuringLoad();
  void clearingUnauthorizedPreferencesIsIdempotent();

private:
  QString settingsFile(const QString &name) const;
  void seedAlpha20Settings(const QString &path, const QString &license = QString()) const;

  QTemporaryDir m_temp;
  QString m_portableSettingsFile;
};

QString ProLicenseSettingsTests::settingsFile(const QString &name) const
{
  return QStringLiteral("%1/%2.conf").arg(m_temp.path(), name);
}

void ProLicenseSettingsTests::seedAlpha20Settings(const QString &path, const QString &license) const
{
  QSettings settings(path, QSettings::IniFormat);
  settings.setValue(Settings::FileTransfer::Enabled, true);
  settings.setValue(Settings::FileTransfer::ReceiveEnabled, true);
  settings.setValue(Settings::FileTransfer::DownloadDirectory, QStringLiteral("C:/Ieum/Keep-This-Destination"));
  if (!license.isEmpty()) {
    settings.setValue(Settings::Pro::LicenseKey, license);
  }
  settings.sync();
}

void ProLicenseSettingsTests::initTestCase()
{
  QVERIFY(m_temp.isValid());

  m_portableSettingsFile = Settings::portableSettingsFile();
  QVERIFY(QDir().mkpath(QFileInfo(m_portableSettingsFile).absolutePath()));
  QFile portableSettings(m_portableSettingsFile);
  QVERIFY(portableSettings.open(QFile::WriteOnly | QFile::Truncate));
  portableSettings.close();

  Settings::setSettingsFile(settingsFile(QStringLiteral("initial")));
}

void ProLicenseSettingsTests::cleanupTestCase()
{
  QFile::remove(m_portableSettingsFile);
}

void ProLicenseSettingsTests::reportsNoEntitlementForMissingAndMalformedLicenses()
{
  const auto missingPath = settingsFile(QStringLiteral("missing"));
  Settings::setSettingsFile(missingPath);
  QVERIFY(!Settings::hasProFileTransferEntitlement());
  QVERIFY(Settings::value(Settings::Pro::LicenseKey).toString().isEmpty());

  const auto malformedPath = settingsFile(QStringLiteral("malformed"));
  QSettings malformed(malformedPath, QSettings::IniFormat);
  malformed.setValue(Settings::Pro::LicenseKey, QStringLiteral("IEUM1.not-a-license.invalid"));
  malformed.sync();

  Settings::setSettingsFile(malformedPath);
  QVERIFY(!Settings::hasProFileTransferEntitlement());
  QCOMPARE(Settings::value(Settings::Pro::LicenseKey).toString(), QStringLiteral("IEUM1.not-a-license.invalid"));
}

void ProLicenseSettingsTests::clearsAlpha20TransferPreferencesDuringLoad()
{
  const auto candidate = QStringLiteral("IEUM1.private-payload.private-signature");
  const auto path = settingsFile(QStringLiteral("alpha20-upgrade"));
  seedAlpha20Settings(path, candidate);

  QStringList messages;
  {
    MessageCapture capture(messages);
    Settings::setSettingsFile(path);
  }

  QVERIFY(!Settings::value(Settings::FileTransfer::Enabled).toBool());
  QVERIFY(!Settings::value(Settings::FileTransfer::ReceiveEnabled).toBool());
  QCOMPARE(
      Settings::value(Settings::FileTransfer::DownloadDirectory).toString(),
      QStringLiteral("C:/Ieum/Keep-This-Destination")
  );
  QCOMPARE(Settings::value(Settings::Pro::LicenseKey).toString(), candidate);

  QSettings migrated(path, QSettings::IniFormat);
  QVERIFY(!migrated.contains(Settings::FileTransfer::Enabled));
  QVERIFY(!migrated.contains(Settings::FileTransfer::ReceiveEnabled));
  QCOMPARE(
      migrated.value(Settings::FileTransfer::DownloadDirectory).toString(),
      QStringLiteral("C:/Ieum/Keep-This-Destination")
  );

  for (const auto &message : messages) {
    QVERIFY2(!message.contains(candidate), qPrintable(message));
    QVERIFY2(!message.contains(QStringLiteral("private-payload")), qPrintable(message));
    QVERIFY2(!message.contains(QStringLiteral("private-signature")), qPrintable(message));
  }
}

void ProLicenseSettingsTests::clearingUnauthorizedPreferencesIsIdempotent()
{
  const auto path = settingsFile(QStringLiteral("idempotent"));
  Settings::setSettingsFile(path);
  Settings::setValue(Settings::FileTransfer::Enabled, true);
  Settings::setValue(Settings::FileTransfer::ReceiveEnabled, true);

  QVERIFY(Settings::clearUnauthorizedFileTransferPreferences());
  QVERIFY(!Settings::value(Settings::FileTransfer::Enabled).toBool());
  QVERIFY(!Settings::value(Settings::FileTransfer::ReceiveEnabled).toBool());
  QVERIFY(!Settings::clearUnauthorizedFileTransferPreferences());
}

QTEST_MAIN(ProLicenseSettingsTests)

#include "ProLicenseSettingsTests.moc"
