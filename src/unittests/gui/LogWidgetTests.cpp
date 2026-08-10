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

QTEST_MAIN(LogWidgetTests)
#include "LogWidgetTests.moc"
