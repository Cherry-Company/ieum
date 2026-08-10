/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

class QObject;

void requestOSXNotificationPermission();
bool isOSXDevelopmentBuild();
bool showOSXNotification(const QString &title, const QString &body);
bool isOSXInterfaceStyleDark();
void forceAppActive();
bool macOSStartAtLoginSupported();
bool macOSStartAtLoginEnabled();
bool macOSStartAtLoginRequiresApproval();
bool macOSSetStartAtLogin(bool enabled, QString *error = nullptr);
void macOSOpenLoginItemsSettings();
void macOSOpenAccessibilitySettings();
void macOSRevealCurrentApplication();
void macOSInstallApplicationReopenHandler(QObject *receiver);
void macOSRemoveApplicationReopenHandler();
