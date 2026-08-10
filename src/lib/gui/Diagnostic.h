/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QUrl>

namespace deskflow::gui::diagnostic {

enum class ReportKind
{
  UserInitiated,
  CriticalError,
  FatalError,
  UnexpectedExit
};

struct PreviousSession
{
  bool unexpectedExit = false;
  QString startedAt;
  QString version;
};

struct Report
{
  QString id;
  QString markdown;
  QString filePath;
  QUrl issueUrl;
  bool saved = false;
};

void clearSettings(bool enableRestart);
PreviousSession beginSession();
void completeSession();
QString redactSensitiveText(QString text);
Report createReport(ReportKind kind, const QString &message = {}, const QString &source = {});
void copyToClipboard(const Report &report);
bool openIssue(const Report &report);
bool openNativeCrashReports();

} // namespace deskflow::gui::diagnostic
