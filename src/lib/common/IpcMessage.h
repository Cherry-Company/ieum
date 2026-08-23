/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QStringList>

namespace deskflow::ipc {

inline QStringList splitCommandMessage(const QString &message)
{
  const auto delimiter = message.indexOf('=');
  if (delimiter < 0) {
    return {message};
  }

  return {message.left(delimiter), message.mid(delimiter + 1)};
}

} // namespace deskflow::ipc
