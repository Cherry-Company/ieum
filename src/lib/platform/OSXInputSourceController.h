/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/InputLanguageTypes.h"

#include <Carbon/Carbon.h>

#include <condition_variable>
#include <mutex>
#include <string>

class IEventQueue;

class OSXInputSourceController
{
public:
  OSXInputSourceController(IEventQueue *events, void *eventTarget);
  ~OSXInputSourceController();

  void control(deskflow::InputLanguageAction action, const std::string &target);
  deskflow::InputLanguageStatus status() const;

private:
  struct Request;
  struct StatusRequest;

  static void applyRequest(void *context);
  static void readStatusRequest(void *context);
  static void inputSourceChanged(CFNotificationCenterRef, void *observer, CFStringRef, const void *, CFDictionaryRef);

  std::string apply(deskflow::InputLanguageAction action, const std::string &target);
  deskflow::InputLanguageStatus readStatus() const;
  void handleInputSourceChanged();
  bool waitForSource(const std::string &expectedSourceId);
  TISInputSourceRef resolve(const std::string &target) const;
  TISInputSourceRef resolveKorean() const;
  TISInputSourceRef resolveLatin() const;
  TISInputSourceRef resolveId(const std::string &sourceId) const;

  IEventQueue *m_events;
  void *m_eventTarget;
  mutable std::string m_lastLatinSourceId;
  std::mutex m_changeMutex;
  std::condition_variable m_changeCondition;
  std::string m_observedSourceId;
  bool m_controlInProgress = false;
};
