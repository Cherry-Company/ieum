/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: MIT
 */

#include "base/LogOutputters.h"
#include "gui/FileTail.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using deskflow::gui::FileTail;

namespace {

constexpr auto kOversizedLogLength = 1024 * 1024;

bool writeFile(const QString &path, const QByteArray &contents, QIODevice::OpenMode mode = QIODevice::WriteOnly)
{
  QFile file(path);
  return file.open(mode) && file.write(contents) == contents.size() && file.flush();
}

bool containsLine(const QSignalSpy &spy, const QString &expected)
{
  for (const auto &arguments : spy) {
    if (!arguments.isEmpty() && arguments.constFirst().toString() == expected)
      return true;
  }

  return false;
}

} // namespace

class FileTailTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void emitsOnlyAppendedLines();
  void resumesAfterFileReplacement();
  void resetsAfterTruncation();
  void resumesAfterDeleteAndRecreate();
  void resumesAfterLogRotation();
};

void FileTailTests::emitsOnlyAppendedLines()
{
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  const auto logPath = temporaryDirectory.filePath(QStringLiteral("ieum.log"));
  QVERIFY(writeFile(logPath, QByteArrayLiteral("existing line\n")));

  FileTail tail(logPath);
  QSignalSpy spy(&tail, &FileTail::newLine);
  QVERIFY(spy.isValid());

  QVERIFY(writeFile(logPath, QByteArrayLiteral("appended line\n"), QIODevice::WriteOnly | QIODevice::Append));

  QTRY_VERIFY_WITH_TIMEOUT(containsLine(spy, QStringLiteral("appended line")), 3000);
  QVERIFY(!containsLine(spy, QStringLiteral("existing line")));
}

void FileTailTests::resumesAfterFileReplacement()
{
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  const auto logPath = temporaryDirectory.filePath(QStringLiteral("ieum.log"));
  const auto replacementPath = temporaryDirectory.filePath(QStringLiteral("replacement.log"));
  QVERIFY(writeFile(logPath, QByteArrayLiteral("old\n")));
  QVERIFY(writeFile(replacementPath, QByteArrayLiteral("replacement line\n")));

  FileTail tail(logPath);
  QSignalSpy spy(&tail, &FileTail::newLine);
  QVERIFY(spy.isValid());

  QVERIFY(QFile::remove(logPath));
  QVERIFY(QFile::rename(replacementPath, logPath));

  QTRY_VERIFY_WITH_TIMEOUT(containsLine(spy, QStringLiteral("replacement line")), 3000);

  QVERIFY(writeFile(logPath, QByteArrayLiteral("after replacement\n"), QIODevice::WriteOnly | QIODevice::Append));
  QTRY_VERIFY_WITH_TIMEOUT(containsLine(spy, QStringLiteral("after replacement")), 3000);
}

void FileTailTests::resetsAfterTruncation()
{
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  const auto logPath = temporaryDirectory.filePath(QStringLiteral("ieum.log"));
  QVERIFY(writeFile(logPath, QByteArray(256, 'x') + '\n'));

  FileTail tail(logPath);
  QSignalSpy spy(&tail, &FileTail::newLine);
  QVERIFY(spy.isValid());

  QVERIFY(writeFile(logPath, QByteArrayLiteral("fresh line\n"), QIODevice::WriteOnly | QIODevice::Truncate));

  QTRY_VERIFY_WITH_TIMEOUT(containsLine(spy, QStringLiteral("fresh line")), 3000);
}

void FileTailTests::resumesAfterDeleteAndRecreate()
{
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  const auto logPath = temporaryDirectory.filePath(QStringLiteral("ieum.log"));
  QVERIFY(writeFile(logPath, QByteArrayLiteral("old line\n")));

  FileTail tail(logPath);
  QSignalSpy spy(&tail, &FileTail::newLine);
  QVERIFY(spy.isValid());

  QVERIFY(QFile::remove(logPath));
  QTest::qWait(100);
  QVERIFY(writeFile(logPath, QByteArrayLiteral("recreated line\n")));

  QTRY_VERIFY_WITH_TIMEOUT(containsLine(spy, QStringLiteral("recreated line")), 3000);

  QVERIFY(writeFile(logPath, QByteArrayLiteral("after recreation\n"), QIODevice::WriteOnly | QIODevice::Append));
  QTRY_VERIFY_WITH_TIMEOUT(containsLine(spy, QStringLiteral("after recreation")), 3000);
}

void FileTailTests::resumesAfterLogRotation()
{
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  const auto logPath = temporaryDirectory.filePath(QStringLiteral("ieum.log"));
  QVERIFY(writeFile(logPath, QByteArrayLiteral("initial line\n")));

  FileTail tail(logPath);
  QSignalSpy spy(&tail, &FileTail::newLine);
  QVERIFY(spy.isValid());

  FileLogOutputter outputter(logPath);
  QVERIFY(outputter.write(LogLevel::Level::Info, QString(kOversizedLogLength, QLatin1Char('z'))));
  QVERIFY(QFile::exists(logPath + QStringLiteral(".1")));
  QVERIFY(outputter.write(LogLevel::Level::Info, QStringLiteral("after rotation")));

  QTRY_VERIFY_WITH_TIMEOUT(containsLine(spy, QStringLiteral("after rotation")), 3000);
}

QTEST_GUILESS_MAIN(FileTailTests)
#include "FileTailTests.moc"
