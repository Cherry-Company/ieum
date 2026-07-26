/*
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class CanonicalScancodeTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void mapsKoreanTypingKeys();
  void roundTripsExtendedKeys();
  void rejectsImeCommandKeys();
  void tableIsUnambiguous();
  void canonicalFlagSurvivesTheWire();
  void canonicalFlagMustBeStrippedBeforeKeyState();
  void rejectsNonSet1Scancodes();
};
