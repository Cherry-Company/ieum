/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/ExternalUrlLauncher.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMessageBox>

namespace deskflow::gui {

ExternalUrlLaunchResult launchExternalUrl(const QUrl &url, ExternalUrlOpener opener)
{
  const auto fallbackUrl = url.toString(QUrl::FullyEncoded);
  if (!url.isValid() || fallbackUrl.isEmpty()) {
    return {.opened = false, .fallbackUrl = fallbackUrl};
  }
  if (!opener) {
    opener = [](const QUrl &value) { return QDesktopServices::openUrl(value); };
  }

  bool opened = false;
  try {
    opened = opener(url);
  } catch (...) {
    opened = false;
  }
  return {.opened = opened, .fallbackUrl = opened ? QString{} : fallbackUrl};
}

void openExternalUrlOrShowFallback(QWidget *parent, const QUrl &url, const QString &title)
{
  const auto result = launchExternalUrl(url);
  if (result.opened) {
    return;
  }
  if (!result.fallbackUrl.isEmpty()) {
    QGuiApplication::clipboard()->setText(result.fallbackUrl);
  }

  QMessageBox message(parent);
  message.setIcon(QMessageBox::Warning);
  message.setWindowTitle(title);
  message.setText(QObject::tr("The browser could not be opened. The full URL was copied to the clipboard."));
  message.setInformativeText(result.fallbackUrl);
  message.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
  message.exec();
}

} // namespace deskflow::gui
