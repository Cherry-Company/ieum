/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class UpdateShutdownTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void waitsUntilPreviousInstanceHasExited();
  void reportsTimeoutWhenPreviousInstanceRemains();
};
