/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Messages.h"

#include "Diagnostic.h"
#include "Logger.h"
#include "ProductIdentity.h"

#include "common/Settings.h"
#include "common/UrlConstants.h"

#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <memory>

namespace deskflow::gui::messages {

struct Errors
{
  static std::unique_ptr<QMessageBox> s_criticalMessage;
  static QStringList s_ignoredErrors;
};

std::unique_ptr<QMessageBox> Errors::s_criticalMessage;
QStringList Errors::s_ignoredErrors;

void raiseCriticalDialog()
{
  if (Errors::s_criticalMessage) {
    Errors::s_criticalMessage->raise();
  }
}

void showUnexpectedExit(QWidget *parent, const diagnostic::PreviousSession &previousSession)
{
  const auto previousVersion = previousSession.version.isEmpty() ? QObject::tr("unknown") : previousSession.version;
  const auto previousStart = previousSession.startedAt.isEmpty() ? QObject::tr("unknown") : previousSession.startedAt;
  const auto detail = QStringLiteral(
                          "The previous Ieum session did not complete a clean shutdown. It may have crashed or been "
                          "force-terminated. Previous version: %1. Session started (UTC): %2."
  )
                          .arg(previousVersion, previousStart);
  const auto report = diagnostic::createReport(diagnostic::ReportKind::UnexpectedExit, detail);

  QMessageBox dialog(parent);
  dialog.setIcon(QMessageBox::Warning);
  dialog.setWindowTitle(QObject::tr("Ieum Previous Session Ended Unexpectedly"));
  dialog.setText(
      QObject::tr(
          "<p>The previous Ieum session did not close normally. It may have crashed, lost power, or been ended from "
          "Task Manager/Force Quit.</p><p>A privacy-filtered report was saved locally. Nothing was uploaded "
          "automatically.</p><p>Report ID: <code>%1</code><br>Local copy: <code>%2</code></p>"
      )
          .arg(
              report.id, report.saved ? QDir::toNativeSeparators(report.filePath).toHtmlEscaped()
                                      : QObject::tr("could not save the local file")
          )
  );
  auto *copyButton = dialog.addButton(QObject::tr("Copy Diagnostic"), QMessageBox::ActionRole);
  auto *issueButton = dialog.addButton(QObject::tr("Open GitHub Issue"), QMessageBox::ActionRole);
#if defined(Q_OS_MACOS)
  auto *nativeReportsButton = dialog.addButton(QObject::tr("Open macOS Crash Reports"), QMessageBox::ActionRole);
#endif
  dialog.addButton(QMessageBox::Ok);
  dialog.exec();

  if (dialog.clickedButton() == copyButton) {
    diagnostic::copyToClipboard(report);
  } else if (dialog.clickedButton() == issueButton) {
    diagnostic::copyToClipboard(report);
    diagnostic::openIssue(report);
  }
#if defined(Q_OS_MACOS)
  else if (dialog.clickedButton() == nativeReportsButton) {
    diagnostic::openNativeCrashReports();
  }
#endif
}

void showErrorDialog(const QString &message, const QString &fileLine, QtMsgType type)
{
  if (type != QtFatalMsg && Errors::s_ignoredErrors.contains(message)) {
    return;
  }

  const auto fatal = type == QtFatalMsg;
  const auto report = diagnostic::createReport(
      fatal ? diagnostic::ReportKind::FatalError : diagnostic::ReportKind::CriticalError, message, fileLine
  );
  auto errorType = fatal ? QObject::tr("fatal error") : QObject::tr("error");
  auto title = QStringLiteral("%1 %2").arg(productDisplayName(), errorType);
  auto text = QObject::tr(
                  "<p>Ieum created a privacy-filtered diagnostic passport for this error.</p>"
                  "<p><b>Nothing was uploaded automatically.</b> You can copy it or open a prefilled GitHub issue. "
                  "Submitting on GitHub requires an account.</p>"
                  "<p>Report ID: <code>%1</code><br>Saved locally: <code>%2</code></p>"
  )
                  .arg(
                      report.id, report.saved ? QDir::toNativeSeparators(report.filePath).toHtmlEscaped()
                                              : QObject::tr("could not save the local file")
                  );

  if (fatal) {
    text.prepend(QObject::tr("<p>Sorry, a fatal error has occurred and the application must now exit.</p>\n"));
    QMessageBox dialog(QMessageBox::Critical, title, text, QMessageBox::NoButton);
    auto *copyButton = dialog.addButton(QObject::tr("Copy Diagnostic"), QMessageBox::ActionRole);
    auto *issueButton = dialog.addButton(QObject::tr("Open GitHub Issue"), QMessageBox::ActionRole);
    dialog.addButton(QMessageBox::Abort);
    dialog.exec();
    if (dialog.clickedButton() == copyButton) {
      diagnostic::copyToClipboard(report);
    } else if (dialog.clickedButton() == issueButton) {
      diagnostic::copyToClipboard(report);
      diagnostic::openIssue(report);
    }
    return;
  }

  text.prepend(QObject::tr("<p>Sorry, a critical error has occurred.</p>\n"));
  Errors::s_criticalMessage.reset();
  Errors::s_criticalMessage = std::make_unique<QMessageBox>(QMessageBox::Critical, title, text, QMessageBox::NoButton);
  auto *copyButton = Errors::s_criticalMessage->addButton(QObject::tr("Copy Diagnostic"), QMessageBox::ActionRole);
  auto *issueButton = Errors::s_criticalMessage->addButton(QObject::tr("Open GitHub Issue"), QMessageBox::ActionRole);
  Errors::s_criticalMessage->addButton(QMessageBox::Ok);
  auto *ignoreButton = Errors::s_criticalMessage->addButton(QMessageBox::Ignore);

  QObject::connect(copyButton, &QPushButton::clicked, [report] { diagnostic::copyToClipboard(report); });
  QObject::connect(issueButton, &QPushButton::clicked, [report] {
    diagnostic::copyToClipboard(report);
    diagnostic::openIssue(report);
  });
  QObject::connect(Errors::s_criticalMessage.get(), &QMessageBox::finished, [message, ignoreButton](int) {
    if (Errors::s_criticalMessage && Errors::s_criticalMessage->clickedButton() == ignoreButton) {
      Errors::s_ignoredErrors.append(message);
    }
  });
  Errors::s_criticalMessage->open();
}

QString fileLine(const QMessageLogContext &context)
{
  if (!context.file) {
    return {};
  }
  return QStringLiteral("%1:%2").arg(
      QFileInfo(QString::fromUtf8(context.file)).fileName(), QString::number(context.line)
  );
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
  const auto fileLine = messages::fileLine(context);
  Logger::instance()->handleMessage(type, fileLine, message);

  if (type == QtFatalMsg || type == QtCriticalMsg) {
    showErrorDialog(message, fileLine, type);
  }

  if (type == QtFatalMsg) {
    // developers: if you hit this line in your debugger, traverse the stack
    // to find the cause of the fatal error. important: crash the app on fatal
    // error to prevent the app being used in a broken state.
    //
    // hint: if you don't want to crash, but still want to show an error
    // message, use `qCritical()` instead of `qFatal()`. you should use
    // fatal errors when the app is in an unrecoverable state; i.e. it cannot
    // function correctly in it's current state and must be restarted.
    abort();
  }
}

void showCloseReminder(QWidget *parent)
{
  auto message = QObject::tr(
                     "<p>%1 will continue to run in the background and can be accessed via the %1 icon in your "
                     "system notifications area. This setting can be disabled.</p>"
  )
                     .arg(productDisplayName());

#if defined(Q_OS_LINUX)
  message.append(
      QObject::tr(
          "<p>On Linux systems using GNOME 3, the notification area might be disabled. "
          R"(You may need to <a href="%1">enable an extension</a> to see the %2 tray icon.</p>)"
      )
          .arg(kUrlGnomeTrayFix, productDisplayName())
  );
#endif

  QMessageBox::information(parent, productDisplayName(), message);
}

void showFirstServerStartMessage(QWidget *parent)
{
  QMessageBox::information(
      parent, QObject::tr("%1 Server").arg(productDisplayName()),
      QObject::tr(
          "<p>Great, the %1 server is now running.</p>"
          "<p>Now you can connect your client computers to this server. "
          "You should see a prompt here on the server when a new client tries to connect.</p>"
      )
          .arg(productDisplayName())
  );
}

void showFirstConnectedMessage(QWidget *parent)
{
  auto message = QObject::tr("<p>%1 is now connected!</p>").arg(productDisplayName());

  if (Settings::value(Settings::Core::CoreMode).value<Settings::CoreMode>() == Settings::Server) {
    message.append(
        QObject::tr(
            "<p>Try moving your mouse to your other computer. Once there, go ahead "
            "and type something.</p>"
            "<p>Don't forget, you can copy and paste between computers too.</p>"
        )
    );
  } else {
    message.append(QObject::tr("<p>Try controlling this computer remotely.</p>"));
  }

  using ProcessMode = Settings::ProcessMode;

  if (Settings::value(Settings::Core::ProcessMode).value<ProcessMode>() == ProcessMode::Desktop &&
      !Settings::value(Settings::Gui::CloseToTray).toBool()) {
    message.append(
        QObject::tr(
            "<p>As you do not have the setting enabled to keep %1 running in "
            "the background, you'll need to keep this window open or minimized "
            "to keep %1 running.</p>"
        )
            .arg(productDisplayName())
    );
  } else {
    message.append(
        QObject::tr(
            "<p>You can now close this window and %1 will continue to run in "
            "the background. This setting can be disabled.</p>"
        )
            .arg(productDisplayName())
    );
  }

  const auto title = QObject::tr("%1 Connected").arg(productDisplayName());
  QMessageBox::information(parent, title, message);
}

bool showNewClientPrompt(QWidget *parent, const QString &clientName)
{
  if (Settings::value(Settings::Security::TlsEnabled).toBool() &&
      Settings::value(Settings::Security::CheckPeers).toBool()) {
    // When peer auth is enabled you will be prompted to allow the connection before seeing this dialog.
    // This is why we do not show a dialog with an option to ignore the new client
    QMessageBox::information(
        parent, QObject::tr("%1 - New Client").arg(productDisplayName()),
        QObject::tr("A new client called '%1' has been accepted. You'll need to add it to your server's screen layout.")
            .arg(clientName)
    );
    return true;
  }
  QMessageBox message(parent);
  message.addButton(QObject::tr("Ignore"), QMessageBox::RejectRole);
  message.addButton(QObject::tr("Add client"), QMessageBox::AcceptRole);
  message.setText(QObject::tr("A new client called '%1' wants to connect").arg(clientName));
  message.exec();
  return message.buttonRole(message.clickedButton()) == QMessageBox::AcceptRole;
}

bool showClearSettings(QWidget *parent)
{
  const auto title = QObject::tr("%1 Clear Settings").arg(productDisplayName());
  const auto message = QObject::tr(
                           "<p>Are you sure you want to clear all settings and restart %1?</p>"
                           "<p>This action cannot be undone.</p>"
  )
                           .arg(productDisplayName());
  return QMessageBox::question(parent, title, message) == QMessageBox::Yes;
}

void showReadOnlySettings(QWidget *parent, const QString &systemSettingsPath)
{
  const auto title = QObject::tr("%1 Read-only settings").arg(productDisplayName());
  const auto message = QObject::tr(
                           "<p>Settings are read-only because you only have read access "
                           "to the file:</p><p>%1</p>"
  )
                           .arg(QDir::toNativeSeparators(systemSettingsPath));
  QMessageBox::information(parent, title, message);
}

bool showUpdateCheckOption(QWidget *parent)
{
  QMessageBox message(parent);
  message.addButton(QObject::tr("No thanks"), QMessageBox::RejectRole);
  const auto checkButton = message.addButton(QObject::tr("Check for updates"), QMessageBox::AcceptRole);
  message.setText(
      QObject::tr(
          "<p>Would you like to check for updates when %1 starts?</p>"
          "<p>Checking for updates requires an Internet connection.</p>"
          "<p>URL: <pre>%2</pre></p>"
      )
          .arg(productDisplayName(), Settings::value(Settings::Gui::UpdateCheckUrl).toString())
  );

  message.exec();
  return message.clickedButton() == checkButton;
}

bool showDaemonOffline(QWidget *parent)
{
  QMessageBox message(parent);
  message.setIcon(QMessageBox::Warning);
  message.setWindowTitle(QObject::tr("Background service offline"));

  message.addButton(QObject::tr("Retry"), QMessageBox::AcceptRole);
  const auto ignore = message.addButton(QObject::tr("Ignore"), QMessageBox::RejectRole);
  const auto disable = message.addButton(QObject::tr("Disable"), QMessageBox::NoRole);

  message.setText(
      QObject::tr(
          "<p>There was a problem finding the %1 background service (daemon).</p>"
          "<p>The background service makes %1 work with UAC prompts and the login screen.</p>"
          "<p>If don't want to use the background service and intentionally stopped it, "
          "you can prevent it's use by disabling this feature.</p>"
          "<p>If you did not stop the background service intentionally, there may be a problem with it. "
          "Please retry or try restarting the %1 service from the Windows services program.</p>"
      )
          .arg(productDisplayName())
  );
  message.exec();

  if (message.clickedButton() == ignore) {
    return false;
  } else if (message.clickedButton() == disable) {
    Settings::setValue(Settings::Core::ProcessMode, Settings::ProcessMode::Desktop);
    return false;
  }

  return true;
}

} // namespace deskflow::gui::messages
