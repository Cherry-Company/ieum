/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LogWidget.h"

#include <gui/Logger.h>
#include <gui/StyleUtils.h>

#include <QPlainTextEdit>
#include <QScrollBar>
#include <QVBoxLayout>

LogWidget::LogWidget(QWidget *parent) : QWidget{parent}, m_textLog{new QPlainTextEdit(this)}
{
  static constexpr auto kLogFlushIntervalMs = 50;

  m_textLog->setReadOnly(true);
  m_textLog->setMaximumBlockCount(10000);
  m_textLog->setLineWrapMode(QPlainTextEdit::NoWrap);
  m_textLog->setFont(fixedFont());

  auto layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_textLog);

  setLayout(layout);

  m_flushTimer.setInterval(kLogFlushIntervalMs);
  m_flushTimer.setSingleShot(true);
  connect(&m_flushTimer, &QTimer::timeout, this, &LogWidget::flushPendingLines);
  connect(deskflow::gui::Logger::instance(), &deskflow::gui::Logger::newLine, this, &LogWidget::appendLine);
}

void LogWidget::appendLine(const QString &msg)
{
  static constexpr qsizetype kMaxPendingLines = 2000;

  if (m_pendingLines.size() >= kMaxPendingLines) {
    ++m_droppedLineCount;
  } else {
    m_pendingLines.append(msg);
  }

  if (!m_flushTimer.isActive()) {
    m_flushTimer.start();
  }
}

void LogWidget::flushPendingLines()
{
  if (m_droppedLineCount > 0) {
    m_pendingLines.append(
        tr("%1 additional log lines were omitted to keep the interface responsive.").arg(m_droppedLineCount)
    );
    m_droppedLineCount = 0;
  }

  if (m_pendingLines.isEmpty()) {
    return;
  }

  m_textLog->appendPlainText(m_pendingLines.join(QLatin1Char('\n')));
  m_pendingLines.clear();
}

void LogWidget::findNext(const QString &text)
{
  flushPendingLines();
  if (text.isEmpty())
    return;

  if (!m_textLog->find(text)) {
    m_textLog->moveCursor(QTextCursor::Start);
    m_textLog->find(text);
  }
}

void LogWidget::findPrevious(const QString &text)
{
  flushPendingLines();
  if (text.isEmpty())
    return;

  if (!m_textLog->find(text, QTextDocument::FindBackward)) {
    m_textLog->moveCursor(QTextCursor::End);
    m_textLog->find(text, QTextDocument::FindBackward);
  }
}

void LogWidget::scrollToBottom() const
{
  auto sb = m_textLog->verticalScrollBar();
  sb->setValue(sb->maximum());
}
