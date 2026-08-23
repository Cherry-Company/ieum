/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: MIT
 */

#include "base/LogOutputters.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

namespace {

constexpr auto kOversizedLogLength = 1024 * 1024;

bool writeFile(const QString &path, const QByteArray &contents)
{
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size() &&
         file.flush();
}

QByteArray readFile(const QString &path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};

  return file.readAll();
}

} // namespace

class LogOutputtersTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void rotationReplacesBackupWithoutLosingActiveLog();
  void rotationFailureLeavesActiveLogIntact();
};

void LogOutputtersTests::rotationReplacesBackupWithoutLosingActiveLog()
{
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  const auto logPath = temporaryDirectory.filePath(QStringLiteral("ieum.log"));
  const auto backupPath = logPath + QStringLiteral(".1");
  QVERIFY(writeFile(backupPath, QByteArrayLiteral("stale backup\n")));

  const QString message(kOversizedLogLength, QLatin1Char('x'));
  FileLogOutputter outputter(logPath);
  QVERIFY(outputter.write(LogLevel::Level::Info, message));

  QVERIFY(!QFile::exists(logPath));
  QCOMPARE(readFile(backupPath), message.toUtf8() + '\n');
}

void LogOutputtersTests::rotationFailureLeavesActiveLogIntact()
{
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  const auto logPath = temporaryDirectory.filePath(QStringLiteral("ieum.log"));
  const auto backupPath = logPath + QStringLiteral(".1");
  QVERIFY(QDir().mkpath(backupPath));

  const QString message(kOversizedLogLength, QLatin1Char('y'));
  FileLogOutputter outputter(logPath);
  QVERIFY(!outputter.write(LogLevel::Level::Info, message));

  QCOMPARE(readFile(logPath), message.toUtf8() + '\n');
  QVERIFY(QFileInfo(backupPath).isDir());
}

QTEST_GUILESS_MAIN(LogOutputtersTests)
#include "LogOutputtersTests.moc"
