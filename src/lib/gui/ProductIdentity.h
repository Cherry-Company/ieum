/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QCoreApplication>
#include <QString>

namespace deskflow::gui {

inline QString productDisplayName()
{
  return QCoreApplication::translate("ProductIdentity", "Ieum");
}

inline QString productTagline()
{
  return QCoreApplication::translate("ProductIdentity", "Software KVM across Windows, macOS, and Linux");
}

} // namespace deskflow::gui
