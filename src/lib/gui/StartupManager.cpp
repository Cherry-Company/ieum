/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StartupManager.h"

#include "common/Constants.h"
#include "common/Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#if defined(Q_OS_MACOS)
#include "OSXHelpers.h"
#endif

namespace deskflow::gui {

namespace {

#if defined(Q_OS_WIN)
const auto kRunRegistryPath = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");

QString windowsStartupCommand()
{
  return QStringLiteral("\"%1\" --background").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
}
#endif

} // namespace

bool StartupManager::isSupported()
{
#if defined(Q_OS_WIN)
  return !Settings::isPortableMode();
#elif defined(Q_OS_MACOS)
  return macOSStartAtLoginSupported();
#else
  return false;
#endif
}

bool StartupManager::isEnabled()
{
  if (!isSupported()) {
    return false;
  }

#if defined(Q_OS_WIN)
  QSettings registry(kRunRegistryPath, QSettings::NativeFormat);
  return registry.value(QString::fromLatin1(kAppName)).toString() == windowsStartupCommand();
#elif defined(Q_OS_MACOS)
  return macOSStartAtLoginEnabled();
#else
  return false;
#endif
}

bool StartupManager::requiresApproval()
{
#if defined(Q_OS_MACOS)
  return macOSStartAtLoginRequiresApproval();
#else
  return false;
#endif
}

bool StartupManager::setEnabled(bool enabled, QString *error)
{
  if (!isSupported()) {
    if (error != nullptr) {
      *error =
          QCoreApplication::translate("StartupManager", "Automatic startup is not supported on this operating system.");
    }
    return false;
  }

#if defined(Q_OS_WIN)
  QSettings registry(kRunRegistryPath, QSettings::NativeFormat);
  const auto valueName = QString::fromLatin1(kAppName);
  if (enabled) {
    registry.setValue(valueName, windowsStartupCommand());
  } else {
    registry.remove(valueName);
  }
  registry.sync();
  if (registry.status() == QSettings::NoError) {
    return enabled ? isEnabled() : !registry.contains(valueName);
  }
  if (error != nullptr) {
    *error = QCoreApplication::translate(
        "StartupManager", "Windows could not update the current user's startup registration."
    );
  }
  return false;
#elif defined(Q_OS_MACOS)
  return macOSSetStartAtLogin(enabled, error);
#else
  Q_UNUSED(enabled)
  Q_UNUSED(error)
  return false;
#endif
}

bool StartupManager::reconcile(QString *error)
{
  if (!isSupported()) {
    return true;
  }
  return setEnabled(Settings::value(Settings::Gui::StartAtLogin).toBool(), error);
}

void StartupManager::openSystemSettings()
{
#if defined(Q_OS_MACOS)
  macOSOpenLoginItemsSettings();
#endif
}

} // namespace deskflow::gui
