/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Diagnostic.h"

#include "common/Settings.h"
#include "common/UrlConstants.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrlQuery>
#include <QUuid>

namespace deskflow::gui::diagnostic {

namespace {

QString reportKindName(ReportKind kind)
{
  switch (kind) {
  case ReportKind::UserInitiated:
    return QStringLiteral("user initiated");
  case ReportKind::CriticalError:
    return QStringLiteral("critical error");
  case ReportKind::FatalError:
    return QStringLiteral("fatal error");
  case ReportKind::UnexpectedExit:
    return QStringLiteral("unexpected exit");
  }
  return QStringLiteral("unknown");
}

QString issueTitle(ReportKind kind, const QString &id)
{
  if (kind == ReportKind::UserInitiated) {
    return QStringLiteral("[Diagnostic %1] ").arg(id);
  }
  if (kind == ReportKind::UnexpectedExit) {
    return QStringLiteral("[Unexpected exit %1] Ieum %2").arg(id, QCoreApplication::applicationVersion());
  }
  return QStringLiteral("[%1 %2] Ieum %3")
      .arg(
          kind == ReportKind::FatalError ? QStringLiteral("Crash") : QStringLiteral("Error"), id,
          QCoreApplication::applicationVersion()
      );
}

} // namespace

QString sessionMarkerPath()
{
  return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
         QStringLiteral("/session-active.json");
}

PreviousSession beginSession()
{
  PreviousSession previous;
  QFile marker(sessionMarkerPath());
  if (marker.open(QIODevice::ReadOnly)) {
    const auto document = QJsonDocument::fromJson(marker.readAll());
    const auto object = document.object();
    previous.unexpectedExit = true;
    previous.startedAt = object.value(QStringLiteral("startedAt")).toString();
    previous.version = object.value(QStringLiteral("version")).toString();
    marker.close();
  }

  QFileInfo markerInfo(marker);
  QDir directory;
  if (!directory.mkpath(markerInfo.absolutePath())) {
    return previous;
  }

  QSaveFile replacement(marker.fileName());
  if (replacement.open(QIODevice::WriteOnly | QIODevice::Text)) {
    const QJsonObject object{
        {QStringLiteral("startedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("version"), QCoreApplication::applicationVersion()}
    };
    replacement.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    replacement.commit();
  }
  return previous;
}

void completeSession()
{
  QFile::remove(sessionMarkerPath());
}

QString redactSensitiveText(QString text)
{
  text.replace(QChar::Null, QLatin1Char(' '));

  const auto homePath = QDir::fromNativeSeparators(QDir::homePath());
  auto normalized = QDir::fromNativeSeparators(text);
  if (!homePath.isEmpty()) {
    normalized.replace(homePath, QStringLiteral("~"), Qt::CaseInsensitive);
  }
  normalized.replace(
      QRegularExpression(QStringLiteral(R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)")), QStringLiteral("<ip-address>")
  );
  normalized.replace(
      QRegularExpression(QStringLiteral(R"((?<![0-9A-Fa-f:])(?:[0-9A-Fa-f]{0,4}:){2,}[0-9A-Fa-f]{0,4}(?![0-9A-Fa-f:]))")
      ),
      QStringLiteral("<ip-address>")
  );
  normalized.replace(QStringLiteral("```"), QStringLiteral("` ` `"));
  if (normalized.size() > 2000) {
    normalized.truncate(2000);
    normalized.append(QStringLiteral("\n[truncated]"));
  }
  return normalized.trimmed();
}

Report createReport(ReportKind kind, const QString &message, const QString &source)
{
  Report report;
  report.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper();

  const auto safeMessage = redactSensitiveText(message);
  const auto safeSource = redactSensitiveText(source);
  const auto timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  const auto version = QCoreApplication::applicationVersion().isEmpty() ? QStringLiteral("unknown")
                                                                        : QCoreApplication::applicationVersion();

  report.markdown =
      QStringLiteral(
          "# Ieum diagnostic passport %1\n\n"
          "- Created (UTC): `%2`\n"
          "- Event: `%3`\n"
          "- Ieum: `%4`\n"
          "- Operating system: `%5`\n"
          "- CPU architecture: `%6`\n"
          "- Build ABI: `%7`\n"
          "- Qt: `%8`\n"
      )
          .arg(
              report.id, timestamp, reportKindName(kind), version, redactSensitiveText(QSysInfo::prettyProductName()),
              QSysInfo::currentCpuArchitecture(), QSysInfo::buildAbi(), QString::fromLatin1(qVersion())
          );

  if (!safeMessage.isEmpty()) {
    report.markdown += QStringLiteral("\n## Error\n\n```text\n%1\n```\n").arg(safeMessage);
  }
  if (!safeSource.isEmpty()) {
    report.markdown += QStringLiteral("\nSource: `%1`\n").arg(safeSource);
  }
  report.markdown += QStringLiteral(
      "\n## Privacy\n\n"
      "Ieum did not upload this report. It does not collect settings, clipboard contents, TLS material, or full "
      "logs. Common home-directory paths and IP addresses in the error text are redacted. Review the report before "
      "sharing it.\n"
  );

  const auto directoryPath =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/diagnostics");
  QDir directory;
  if (directory.mkpath(directoryPath)) {
    report.filePath = directoryPath + QStringLiteral("/ieum-diagnostic-%1.md").arg(report.id.toLower());
    QSaveFile file(report.filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      file.write(report.markdown.toUtf8());
      report.saved = file.commit();
    }
  }

  QUrl issueUrl(QStringLiteral("%1/new").arg(kUrlHelp));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("title"), issueTitle(kind, report.id));
  query.addQueryItem(
      QStringLiteral("body"),
      QStringLiteral(
          "## What happened?\n\nDescribe what you were doing immediately before the problem.\n\n"
          "## Diagnostic passport\n\n%1"
      )
          .arg(report.markdown)
  );
  issueUrl.setQuery(query);
  report.issueUrl = issueUrl;
  return report;
}

void copyToClipboard(const Report &report)
{
  if (auto *clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
    clipboard->setText(report.markdown);
  }
}

bool openIssue(const Report &report)
{
  return QDesktopServices::openUrl(report.issueUrl);
}

bool openNativeCrashReports()
{
#if defined(Q_OS_MACOS)
  return QDesktopServices::openUrl(
      QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/Library/Logs/DiagnosticReports"))
  );
#else
  return false;
#endif
}

void restart()
{
  QString program = QCoreApplication::applicationFilePath();
  QStringList arguments = QCoreApplication::arguments();

  // look for and remove --reset option if found
  if (int resetIndex = arguments.indexOf("--reset"); resetIndex != -1)
    arguments.remove(resetIndex);

  qInfo("launching new process: %s", qPrintable(program));
  QProcess::startDetached(program, arguments);

  qDebug("exiting current process");
  QCoreApplication::exit();
}

void clearSettings(bool enableRestart)
{
  qDebug("clearing settings");
  Settings::proxy().clear();

  // Reset the windowGeometry
  Settings::setValue(Settings::Gui::WindowGeometry);

  // save but do not emit saving signal which will prevent the current state of
  // the app config and server configs from being applied.
  Settings::save(false);

  auto profileDir = QDir(Settings::settingsPath());
  qDebug("removing profile dir: %s", qPrintable(profileDir.absolutePath()));
  profileDir.removeRecursively();

#ifdef Q_OS_WIN
  if (Settings::isPortableMode()) {
    // make a new empty portable settings file
    if (profileDir.mkpath(Settings::settingsPath())) {
      QFile file(Settings::settingsFile());
      std::ignore = file.open(QIODevice::WriteOnly);
      file.write(" ", 1);
      file.close();
    }
  }
#endif

  if (enableRestart) {
    qDebug("restarting");
    restart();
  } else {
    qDebug("skipping restart");
  }
}

} // namespace deskflow::gui::diagnostic
