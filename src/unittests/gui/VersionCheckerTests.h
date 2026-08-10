/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class VersionCheckerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void compareVersions_ordersPrereleaseStages();
  void compareVersions_ordersPrereleaseSequence();
  void packageFileName_selectsPlatformAndArchitecture();
  void packageFileName_rejectsUntrustedVersionText();
};
