/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Cherry Inc.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class DiagnosticTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void redactSensitiveText_removesHomePathsAndAddresses();
  void redactSensitiveText_limitsUntrustedMessages();
  void sessionMarker_detectsUncleanExit();
};
