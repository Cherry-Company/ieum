/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "base/Log.h"

#include <QTest>

class UnicodeTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void UTF16ToUTF8();
  void acceptsRFC3629Boundaries();
  void rejectsInvalidUTF8_data();
  void rejectsInvalidUTF8();
  void maximumScalarRoundTrips();
  void encoderRejectsNonScalarValues_data();
  void encoderRejectsNonScalarValues();

private:
  Log m_log;
};
