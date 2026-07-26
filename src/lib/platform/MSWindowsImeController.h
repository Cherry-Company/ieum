/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/InputLanguageTypes.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class IEventQueue;

class MSWindowsImeController
{
public:
  MSWindowsImeController(IEventQueue *events, void *eventTarget);

  void control(deskflow::InputLanguageAction action, const std::string &target);
  deskflow::InputLanguageStatus status() const;
  void poll();

private:
  HWND imeWindow() const;
  bool openStatus() const;
  bool setOpenStatus(bool open) const;
  bool sendImeKey(WORD virtualKey) const;
  void emitStatus(const deskflow::InputLanguageStatus &status);

  //! True for the primary languages that actually carry an IME open state.
  static bool hasImeOpenState(LANGID language);

  IEventQueue *m_events;
  void *m_eventTarget;
  deskflow::InputLanguageStatus m_lastStatus;
};
