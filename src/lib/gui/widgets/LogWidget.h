/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QWidget>

class QPlainTextEdit;
class QTemporaryFile;

class LogWidget : public QWidget
{
  Q_OBJECT
public:
  explicit LogWidget(QWidget *parent = nullptr);
  void appendLine(const QString &msg);
  void findNext(const QString &text);
  void findPrevious(const QString &text);
  void scrollToBottom() const;

private:
  void flushPendingLines();

  QPlainTextEdit *m_textLog = nullptr;
  QTimer m_flushTimer;
  QStringList m_pendingLines;
  qsizetype m_droppedLineCount = 0;
};
