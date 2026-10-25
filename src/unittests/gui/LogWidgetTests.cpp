/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: MIT
 */

#include "gui/widgets/LogWidget.h"

#include <QPlainTextEdit>
#include <QTest>

class LogWidgetTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void batchesRapidLines();
  void boundsPendingBurst();
  void defersRenderingWhileHidden();
  void boundsOversizedLines();
  void boundsHiddenCharactersAndKeepsNewestLines();
  void boundsRenderedCharactersAcrossBatches();
};

void LogWidgetTests::batchesRapidLines()
{
  LogWidget widget;
  auto *textLog = widget.findChild<QPlainTextEdit *>();
  QVERIFY(textLog);

  for (int i = 0; i < 100; ++i) {
    widget.appendLine(QStringLiteral("line %1").arg(i));
  }

  QTRY_COMPARE_WITH_TIMEOUT(textLog->blockCount(), 100, 1000);
  QVERIFY(textLog->toPlainText().contains(QStringLiteral("line 99")));
}

void LogWidgetTests::boundsPendingBurst()
{
  LogWidget widget;
  auto *textLog = widget.findChild<QPlainTextEdit *>();
  QVERIFY(textLog);

  for (int i = 0; i < 2500; ++i) {
    widget.appendLine(QStringLiteral("burst %1").arg(i));
  }

  QTRY_COMPARE_WITH_TIMEOUT(textLog->blockCount(), 2001, 1000);
  QVERIFY(textLog->toPlainText().contains(QStringLiteral("500 additional log lines were omitted")));
  QVERIFY(textLog->toPlainText().endsWith(QStringLiteral("burst 2499")));
  QVERIFY(!textLog->toPlainText().contains(QStringLiteral("burst 0\n")));
}

void LogWidgetTests::defersRenderingWhileHidden()
{
  QWidget hiddenPanel;
  LogWidget widget(&hiddenPanel);
  auto *textLog = widget.findChild<QPlainTextEdit *>();
  QVERIFY(textLog);

  widget.appendLine(QStringLiteral("render after opening"));
  QTest::qWait(100);
  QVERIFY(!textLog->toPlainText().contains(QStringLiteral("render after opening")));

  widget.setParent(nullptr);
  widget.appendLine(QStringLiteral("resume rendering"));
  QTRY_VERIFY_WITH_TIMEOUT(textLog->toPlainText().contains(QStringLiteral("render after opening")), 1000);
}

void LogWidgetTests::boundsOversizedLines()
{
  LogWidget widget;
  auto *textLog = widget.findChild<QPlainTextEdit *>();
  widget.appendLine(QString(1024 * 1024, QLatin1Char('x')));
  QTRY_VERIFY_WITH_TIMEOUT(textLog->toPlainText().endsWith(QStringLiteral("[truncated]")), 1000);
  QVERIFY(textLog->toPlainText().size() < 4200);
}

void LogWidgetTests::boundsHiddenCharactersAndKeepsNewestLines()
{
  QWidget hiddenPanel;
  LogWidget widget(&hiddenPanel);
  auto *textLog = widget.findChild<QPlainTextEdit *>();
  for (int i = 0; i < 2000; ++i) {
    widget.appendLine(QStringLiteral("%1:").arg(i) + QString(4000, QLatin1Char('x')));
  }
  widget.appendLine(QStringLiteral("latest failure"));
  QVERIFY(textLog->toPlainText().isEmpty());
  widget.findNext({}); // Flush the retained queue through the public search API.
  const auto text = textLog->toPlainText();
  QVERIFY(text.size() < 257 * 1024);
  QVERIFY(text.contains(QStringLiteral("additional log lines were omitted")));
  QVERIFY(text.endsWith(QStringLiteral("latest failure")));
  QVERIFY(text.contains(QStringLiteral("1999:")));
}

void LogWidgetTests::boundsRenderedCharactersAcrossBatches()
{
  LogWidget widget;
  auto *textLog = widget.findChild<QPlainTextEdit *>();
  for (int batch = 0; batch < 20; ++batch) {
    for (int i = 0; i < 50; ++i) {
      widget.appendLine(QStringLiteral("%1:").arg(batch * 50 + i) + QString(4000, QLatin1Char('x')));
    }
    widget.findNext({});
    QVERIFY(textLog->document()->characterCount() <= 1024 * 1024);
  }
  const auto text = textLog->toPlainText();
  QVERIFY(text.contains(QStringLiteral("999:")));
  QVERIFY(!text.contains(QStringLiteral("\n0:")));
  QVERIFY(text.indexOf(QLatin1Char(':')) < 5); // Trim whole lines, never a partial prefix.
}

QTEST_MAIN(LogWidgetTests)
#include "LogWidgetTests.moc"
