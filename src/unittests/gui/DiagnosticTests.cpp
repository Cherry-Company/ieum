/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DiagnosticTests.h"

#include "gui/Diagnostic.h"
#include "gui/SessionLifecycle.h"

#include <QAbstractEventDispatcher>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

void DiagnosticTests::redactSensitiveText_removesHomePathsAndAddresses()
{
  const auto input = QStringLiteral("%1/private.log from 192.168.50.238 and fd7a:115c:a1e0::1")
                         .arg(QDir::toNativeSeparators(QDir::homePath()));
  const auto redacted = deskflow::gui::diagnostic::redactSensitiveText(input);

  QVERIFY(!redacted.contains(QDir::fromNativeSeparators(QDir::homePath()), Qt::CaseInsensitive));
  QVERIFY(redacted.startsWith(QLatin1Char('~')));
  QVERIFY(!redacted.contains(QStringLiteral("192.168.50.238")));
  QVERIFY(!redacted.contains(QStringLiteral("fd7a:115c:a1e0::1"), Qt::CaseInsensitive));
  QCOMPARE(redacted.count(QStringLiteral("<ip-address>")), 2);
}

void DiagnosticTests::redactSensitiveText_limitsUntrustedMessages()
{
  const auto redacted = deskflow::gui::diagnostic::redactSensitiveText(
      QStringLiteral("```untrusted\n") + QString(3000, QLatin1Char('x'))
  );
  QVERIFY(redacted.size() < 2100);
  QVERIFY(redacted.endsWith(QStringLiteral("[truncated]")));
  QVERIFY(!redacted.contains(QStringLiteral("```")));
}

void DiagnosticTests::sessionMarker_detectsUncleanExit()
{
  using namespace deskflow::gui::diagnostic;
  QStandardPaths::setTestModeEnabled(true);
  completeSession();

  const auto firstSession = beginSession("test-boot");
  QVERIFY(!firstSession.unexpectedExit);

  const auto interruptedSession = beginSession("test-boot");
  QVERIFY(interruptedSession.unexpectedExit);
  QCOMPARE(interruptedSession.version, QCoreApplication::applicationVersion());

  completeSession();
}

void DiagnosticTests::sessionMarker_previousBootIsNotAnAppCrash()
{
  using namespace deskflow::gui::diagnostic;
  QStandardPaths::setTestModeEnabled(true);
  completeSession();
  beginSession("previous-boot");
  QVERIFY(beginSession("previous-boot").unexpectedExit);
  const auto previous = beginSession("current-boot");
  completeSession();
  QVERIFY(!previous.unexpectedExit);
}

void DiagnosticTests::sessionMarker_cleanExit()
{
  using namespace deskflow::gui::diagnostic;
  QStandardPaths::setTestModeEnabled(true);
  beginSession();
  completeSession();
  const auto previous = beginSession();
  completeSession();
  QVERIFY(!previous.unexpectedExit);
}

void DiagnosticTests::sessionMarker_missingBootIdIsNotCrashEvidence()
{
  using namespace deskflow::gui::diagnostic;
  QStandardPaths::setTestModeEnabled(true);
  completeSession();
  beginSession({});
  QVERIFY(!beginSession({}).unexpectedExit);
  QVERIFY(!beginSession("known-boot").unexpectedExit);
  QVERIFY(!beginSession({}).unexpectedExit);
  completeSession();
}

void DiagnosticTests::sessionMarker_corruptMarkerIsIgnored()
{
  using namespace deskflow::gui::diagnostic;
  QStandardPaths::setTestModeEnabled(true);
  completeSession();
  beginSession("known-boot");
  QFile marker(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/session-active.json");
  QVERIFY(marker.open(QIODevice::WriteOnly | QIODevice::Truncate));
  marker.write("{broken");
  marker.close();
  QVERIFY(!beginSession("known-boot").unexpectedExit);
  completeSession();
}

void DiagnosticTests::report_issueUrlIsBoundedAndContainsNoErrorText()
{
  using namespace deskflow::gui::diagnostic;
  QStandardPaths::setTestModeEnabled(true);
  const auto report = createReport(ReportKind::CriticalError, QString(2000, QChar(0xD55C)) + " private-message");
  QVERIFY(report.saved);
  QVERIFY(report.markdown.contains("private-message") || report.markdown.contains("truncated"));
  QVERIFY(report.issueUrl.toEncoded().size() < 1800);
  QVERIFY(!report.issueUrl.toString().contains("private-message"));
  QVERIFY(report.issueUrl.toString().contains(report.id));
  QFile::remove(report.filePath);
}

void DiagnosticTests::sessionShutdown_onlyConfirmedShutdownClearsMarker()
{
#ifdef Q_OS_WIN
  using namespace deskflow::gui;
  QStandardPaths::setTestModeEnabled(true);
  diagnostic::completeSession();
  diagnostic::beginSession();
  SessionLifecycle lifecycle(*qApp);
  auto *dispatcher = QAbstractEventDispatcher::instance();
  QVERIFY(dispatcher != nullptr);
  MSG message{};
  qintptr result = 0;
  message.message = WM_QUERYENDSESSION;
  QVERIFY(!dispatcher->filterNativeEvent("windows_generic_MSG", &message, &result));
  QVERIFY(diagnostic::beginSession().unexpectedExit);
  message.message = WM_ENDSESSION;
  message.wParam = FALSE;
  QVERIFY(!dispatcher->filterNativeEvent("windows_generic_MSG", &message, &result));
  QVERIFY(diagnostic::beginSession().unexpectedExit);
  message.wParam = TRUE;
  QVERIFY(!dispatcher->filterNativeEvent("windows_generic_MSG", &message, &result));
  const auto previous = diagnostic::beginSession();
  diagnostic::completeSession();
  QVERIFY(!previous.unexpectedExit);
#else
  QSKIP("Windows session shutdown messages");
#endif
}

QTEST_MAIN(DiagnosticTests)
