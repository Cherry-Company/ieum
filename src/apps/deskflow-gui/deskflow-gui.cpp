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
#include "gui/ProductIdentity.h"
#include "gui/StyleUtils.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLocalSocket>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSharedMemory>

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

  QCommandLineParser parser;
  parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
  parser.addOption(helpOption);
  parser.addOption(versionOption);
  parser.addOption(resetOption);
  parser.addOption(showOption);
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
    // Ping the running instance to have it show itself
    QLocalSocket socket;
    socket.connectToServer(shmId, QLocalSocket::ReadOnly);
    if (!socket.waitForConnected()) {
      // If we can't connect to the other instance tell the user its running.
      // This should never happen but just incase we should show something
      QMessageBox::information(
          nullptr, productDisplayName(), QObject::tr("%1 is already running").arg(productDisplayName())
      );
    }
    socket.disconnectFromServer();
    return s_exitDuplicate;
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

  // --no-reset
  if (parser.isSet(resetOption)) {
    diagnostic::clearSettings(false);
  }

  MainWindow mainWindow;
  mainWindow.open(parser.isSet(showOption));

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
    auto *resetButton = dialog.addButton(
        QCoreApplication::translate("MacAccessibility", "Reset Previous Approval"), QMessageBox::ActionRole
    );
    auto *cancelButton = dialog.addButton(QMessageBox::Cancel);
    dialog.setDefaultButton(checkButton);
    dialog.setEscapeButton(cancelButton);
    dialog.exec();

    if (dialog.clickedButton() == cancelButton || dialog.clickedButton() == nullptr) {
      return false;
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

    QString error;
    if (!resetMacAccessibility(error)) {
      QMessageBox::critical(
          nullptr, productDisplayName(),
          QCoreApplication::translate("MacAccessibility", "Could not reset Ieum's Accessibility approval:\n%1")
              .arg(error)
      );
      continue;
    }

    requestMacAccessibility();
  }

  return true;
}
#endif
