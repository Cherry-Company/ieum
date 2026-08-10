/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <sithord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2024 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/Constants.h"
#include "common/ExitCodes.h"
#include "common/I18N.h"
#include "common/PlatformInfo.h"
#include "common/UrlConstants.h"
#include "common/VersionInfo.h"
#include "gui/Diagnostic.h"
#include "gui/MainWindow.h"
#include "gui/Messages.h"
#include "gui/OSXHelpers.h"
#include "gui/ProductIdentity.h"
#include "gui/StartupManager.h"
#include "gui/StyleUtils.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QEventLoop>
#include <QLocalSocket>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSharedMemory>
#include <QTimer>

#if defined(Q_OS_MACOS)
#include <Carbon/Carbon.h>
#include <cstdlib>
#endif

#if defined(Q_OS_UNIX) && defined(QT_DEBUG)
#include <QLoggingCategory>
#endif

#if !defined(Q_OS_MAC) && !defined(Q_OS_WIN)
#include "platform/XDGPortalRegistry.h"
#endif

using namespace deskflow::gui;

#if defined(Q_OS_MACOS)
bool checkMacAssistiveDevices();

namespace {

bool requestMacAccessibility()
{
  const void *keys[] = {kAXTrustedCheckOptionPrompt};
  const void *trueValue[] = {kCFBooleanTrue};
  const auto options = CFDictionaryCreate(nullptr, keys, trueValue, 1, nullptr, nullptr);
  const auto trusted = AXIsProcessTrustedWithOptions(options);
  CFRelease(options);
  return trusted;
}

bool resetMacAccessibility(QString &error)
{
  QProcess process;
  process.start(
      QStringLiteral("/usr/bin/tccutil"),
      {QStringLiteral("reset"), QStringLiteral("Accessibility"), QString::fromLatin1(kMacBundleId)}
  );

  if (!process.waitForStarted(3000)) {
    error = process.errorString();
    return false;
  }
  if (!process.waitForFinished(10000)) {
    process.kill();
    process.waitForFinished();
    error = QCoreApplication::translate("MacAccessibility", "The macOS permission reset timed out.");
    return false;
  }
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    error = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (error.isEmpty()) {
      error = QCoreApplication::translate("MacAccessibility", "tccutil exited with code %1.").arg(process.exitCode());
    }
    return false;
  }

  return true;
}

void waitForMacAccessibilityState(int milliseconds)
{
  QEventLoop loop;
  QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
  loop.exec(QEventLoop::ExcludeUserInputEvents);
}

} // namespace
#endif

const static auto kHeader = QStringLiteral("%1: %2\n").arg(kAppName, kDisplayVersion);

int main(int argc, char *argv[])
{
#if defined(Q_OS_UNIX) && defined(QT_DEBUG)
  // Fixes Fedora bug where qDebug() messages aren't printed.
  QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true\nqt.*=false"));
#endif

#if !defined(Q_OS_MAC) && !defined(Q_OS_WIN)
  deskflow::platform::setAppId();
#endif

  QCoreApplication::setApplicationName(kAppName);
  QCoreApplication::setOrganizationName(kAppName);
  QCoreApplication::setApplicationVersion(kVersion);
  QCoreApplication::setOrganizationDomain(kOrgDomain); // used in prefix, can't be a url
  QGuiApplication::setDesktopFileName(kRevFqdnName);

  QApplication app(argc, argv);
  QApplication::setQuitOnLastWindowClosed(false);

  // Ensure the I18N object is made before strings
  QTextStream(stdout) << "initial language: " << I18N::currentLanguage() << '\n';
  QGuiApplication::setApplicationDisplayName(productDisplayName());
  QObject::connect(I18N::instance(), &I18N::languageChanged, &app, [] {
    QGuiApplication::setApplicationDisplayName(productDisplayName());
  });

  // Add Command Line Options
  auto helpOption = QCommandLineOption({"h", "help"}, "Display Help on the command line");
  auto versionOption = QCommandLineOption({"v", "version"}, "Display version information");
  auto resetOption = QCommandLineOption("reset", "Reset all settings");
  auto showOption = QCommandLineOption("show", "Show the main window even when background startup is enabled");
  auto backgroundOption = QCommandLineOption("background", "Start in the background without opening the main window");
  auto prepareUpdateOption =
      QCommandLineOption("prepare-update", "Ask the running Ieum instance to exit before an update");
  auto unregisterStartupOption =
      QCommandLineOption("unregister-startup", "Remove the current user's automatic startup registration");

  QCommandLineParser parser;
  parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
  parser.addOption(helpOption);
  parser.addOption(versionOption);
  parser.addOption(resetOption);
  parser.addOption(showOption);
  parser.addOption(backgroundOption);
  parser.addOption(prepareUpdateOption);
  parser.addOption(unregisterStartupOption);
  parser.parse(QCoreApplication::arguments());

  if (!parser.errorText().isEmpty()) {
    qCritical().noquote() << parser.errorText() << "\nUse --help for more information.";
    return s_exitArgs;
  }

  if (parser.isSet(helpOption)) {
    QTextStream(stdout) << kHeader << QStringLiteral("  %1\n\n").arg(kAppDescription)
                        << parser.helpText().replace(QApplication::applicationFilePath(), kRuntimeId);
    return s_exitSuccess;
  }

  if (parser.isSet(versionOption)) {
    QTextStream(stdout) << kHeader << kCopyright << Qt::endl;
    return s_exitSuccess;
  }

  if (parser.isSet(unregisterStartupOption)) {
    QString startupError;
    if (!StartupManager::isSupported() || StartupManager::setEnabled(false, &startupError)) {
      return s_exitSuccess;
    }
    qCritical().noquote() << startupError;
    return s_exitFailed;
  }

  const auto shmId = QString::fromLatin1(kGuiIpcName);
  // Create a shared memory segment with a unique key
  // This is to prevent a new instance from running if one is already running
  QSharedMemory sharedMemory(shmId);

  // Attempt to attach first and detach in order to clean up stale shm chunks
  // This can happen if the previous instance was killed or crashed
  if (sharedMemory.attach())
    sharedMemory.detach();

  // If we can create 1 byte of SHM we are the only instance
  if (!sharedMemory.create(1)) {
    if (parser.isSet(backgroundOption)) {
      return s_exitSuccess;
    }

    // Ask the running instance to show itself or exit cleanly for an update.
    QLocalSocket socket;
    socket.connectToServer(shmId, QLocalSocket::ReadWrite);
    if (!socket.waitForConnected(2000)) {
      if (parser.isSet(prepareUpdateOption)) {
        return s_exitFailed;
      }
      // If we can't connect to the other instance tell the user its running.
      // This should never happen but just incase we should show something
      QMessageBox::information(
          nullptr, productDisplayName(), QObject::tr("%1 is already running").arg(productDisplayName())
      );
    } else {
      const auto command =
          parser.isSet(prepareUpdateOption) ? QByteArrayLiteral("prepare-update\n") : QByteArrayLiteral("show\n");
      socket.write(command);
      if (!socket.waitForBytesWritten(2000)) {
        return parser.isSet(prepareUpdateOption) ? s_exitFailed : s_exitDuplicate;
      }
      if (parser.isSet(prepareUpdateOption)) {
        if (!socket.waitForReadyRead(3000) || socket.readAll().trimmed() != QByteArrayLiteral("ok")) {
          return s_exitFailed;
        }
      }
    }
    socket.disconnectFromServer();
    return parser.isSet(prepareUpdateOption) ? s_exitSuccess : s_exitDuplicate;
  }

  // An installer may probe for a running instance when none exists.
  if (parser.isSet(prepareUpdateOption)) {
    return s_exitSuccess;
  }

  if (!deskflow::platform::isMac() && qEnvironmentVariable("XDG_CURRENT_DESKTOP") != QLatin1String("KDE")) {
    QApplication::setStyle("fusion");
  }

  // Sets the fallback icon path and fallback theme
  updateIconTheme();

  qInstallMessageHandler(deskflow::gui::messages::messageHandler);
  qInfo("%s v%s", kAppName, kDisplayVersion);

#if defined(Q_OS_MACOS)

  if (app.applicationDirPath().startsWith("/Volumes/")) {
    QString msgBody = QStringLiteral(
        "Please drag %1 to the Applications folder, "
        "and open it from there."
    );
    QMessageBox::information(nullptr, productDisplayName(), msgBody.arg(productDisplayName()));
    return 1;
  }

  if (!checkMacAssistiveDevices()) {
    return 1;
  }
#endif

  const auto previousSession = diagnostic::beginSession();
  QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [] { diagnostic::completeSession(); });

  // --no-reset
  if (parser.isSet(resetOption)) {
    diagnostic::clearSettings(false);
  }

  QString startupError;
  if (!StartupManager::reconcile(&startupError)) {
    qWarning().noquote() << "could not reconcile automatic startup:" << startupError;
  } else if (StartupManager::requiresApproval()) {
    qWarning("automatic startup is registered but still requires approval in macOS Login Items");
  }

  MainWindow mainWindow;
  mainWindow.open(parser.isSet(showOption), parser.isSet(backgroundOption));

  if (previousSession.unexpectedExit) {
    QTimer::singleShot(0, &mainWindow, [&mainWindow, previousSession] {
      messages::showUnexpectedExit(&mainWindow, previousSession);
    });
  }

#if defined(Q_OS_MACOS)
  if (!parser.isSet(backgroundOption) && StartupManager::requiresApproval()) {
    QTimer::singleShot(0, &mainWindow, [&mainWindow] {
      const auto choice = QMessageBox::warning(
          &mainWindow, QObject::tr("Allow Ieum at login"),
          QObject::tr(
              "macOS has not yet allowed Ieum to start at login. Open System Settings > General > Login Items "
              "and allow Ieum so its menu bar icon and KVM core return after sign-in."
          ),
          QMessageBox::Open | QMessageBox::Cancel, QMessageBox::Open
      );
      if (choice == QMessageBox::Open) {
        StartupManager::openSystemSettings();
      }
    });
  }
#endif

  return QApplication::exec();
}

#if defined(Q_OS_MACOS)
bool checkMacAssistiveDevices()
{
  if (AXIsProcessTrusted()) {
    return true;
  }

  auto message = QCoreApplication::translate(
      "MacAccessibility", "Ieum needs Accessibility access to share the keyboard and mouse.\n\n"
                          "In System Settings, turn on Ieum under Privacy & Security > Accessibility. "
                          "After granting access, return here and choose Check Again."
  );

#if !defined(IEUM_MACOS_STABLE_CODE_IDENTITY)
  message += QCoreApplication::translate(
      "MacAccessibility",
      "\n\nThis alpha build has a temporary code identity. If an older enabled Ieum entry remains but this version is "
      "still not trusted, choose Reset Previous Approval. Ieum will clear only its own Accessibility record and ask "
      "macOS to register the current /Applications/Ieum.app."
  );
#else
  message += QCoreApplication::translate(
      "MacAccessibility",
      "\n\nIf an older enabled Ieum entry remains but this version is still not trusted, choose Reset Previous "
      "Approval to register the current /Applications/Ieum.app."
  );
#endif

  while (!AXIsProcessTrusted()) {
    requestMacAccessibility();

    QMessageBox dialog(QMessageBox::Warning, productDisplayName(), message, QMessageBox::NoButton);
    auto *checkButton =
        dialog.addButton(QCoreApplication::translate("MacAccessibility", "Check Again"), QMessageBox::AcceptRole);
    auto *openSettingsButton = dialog.addButton(
        QCoreApplication::translate("MacAccessibility", "Open Accessibility Settings"), QMessageBox::ActionRole
    );
    auto *resetButton = dialog.addButton(
        QCoreApplication::translate("MacAccessibility", "Reset Previous Approval"), QMessageBox::ActionRole
    );
    auto *revealButton = dialog.addButton(
        QCoreApplication::translate("MacAccessibility", "Show Ieum in Applications"), QMessageBox::HelpRole
    );
    auto *cancelButton = dialog.addButton(QMessageBox::Cancel);
    dialog.setDefaultButton(checkButton);
    dialog.setEscapeButton(cancelButton);
    dialog.exec();

    if (dialog.clickedButton() == cancelButton || dialog.clickedButton() == nullptr) {
      return false;
    }
    if (dialog.clickedButton() == openSettingsButton) {
      macOSOpenAccessibilitySettings();
      continue;
    }
    if (dialog.clickedButton() == revealButton) {
      macOSRevealCurrentApplication();
      continue;
    }
    if (dialog.clickedButton() != resetButton) {
      continue;
    }

    const auto confirmation = QCoreApplication::translate(
        "MacAccessibility", "Reset Ieum's previous Accessibility approval?\n\n"
                            "macOS will remove only Ieum's Accessibility record. No other app permissions are changed. "
                            "You must approve the current Ieum app again."
    );
    if (QMessageBox::question(
            nullptr, productDisplayName(), confirmation, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel
        ) != QMessageBox::Yes) {
      continue;
    }

    // System Settings caches the TCC list. Open the exact pane first so the
    // removal and the following registration are reflected in one place.
    macOSOpenAccessibilitySettings();
    waitForMacAccessibilityState(600);

    QString error;
    if (!resetMacAccessibility(error)) {
      QMessageBox::critical(
          nullptr, productDisplayName(),
          QCoreApplication::translate("MacAccessibility", "Could not reset Ieum's Accessibility approval:\n%1")
              .arg(error)
      );
      continue;
    }

    waitForMacAccessibilityState(800);
    requestMacAccessibility();
    waitForMacAccessibilityState(300);
    macOSOpenAccessibilitySettings();
  }

  return true;
}
#endif
