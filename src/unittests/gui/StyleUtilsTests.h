/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class StyleUtilsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void applyMainWindowStyle_paletteChange_doesNotRecurse();
};
