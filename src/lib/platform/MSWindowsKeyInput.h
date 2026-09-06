/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/KeyTypes.h"

#include <Windows.h>

namespace deskflow::win32 {

inline bool isModifierRepeat(KeyModifierMask oldState, KeyModifierMask state, UINT virtualKey, bool down, bool wasDown)
{
  // Equal aggregate masks also occur when pressing the other side or
  // releasing one of two held modifiers. Never discard those transitions.
  if (!down || !wasDown || oldState != state) {
    return false;
  }
  return ((state & KeyModifierShift) != 0 && (virtualKey == VK_LSHIFT || virtualKey == VK_RSHIFT)) ||
         ((state & KeyModifierControl) != 0 && (virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL)) ||
         ((state & KeyModifierAlt) != 0 && (virtualKey == VK_LMENU || virtualKey == VK_RMENU)) ||
         ((state & KeyModifierSuper) != 0 && (virtualKey == VK_LWIN || virtualKey == VK_RWIN));
}

} // namespace deskflow::win32
