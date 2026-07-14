/*
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class Protocol19Tests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void inputLanguageControlRoundTrip();
  void inputLanguageStatusRoundTrip();
};
