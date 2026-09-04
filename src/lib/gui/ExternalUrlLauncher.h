/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QUrl>

#include <functional>

class QWidget;

namespace deskflow::gui {

struct ExternalUrlLaunchResult
{
  bool opened = false;
  QString fallbackUrl;
};

using ExternalUrlOpener = std::function<bool(const QUrl &)>;

[[nodiscard]] ExternalUrlLaunchResult launchExternalUrl(const QUrl &url, ExternalUrlOpener opener = {});
void openExternalUrlOrShowFallback(QWidget *parent, const QUrl &url, const QString &title);

} // namespace deskflow::gui
