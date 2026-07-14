/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXInputSourceController.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "platform/OSXAutoTypes.h"

#include <cstring>
#include <dispatch/dispatch.h>
#include <pthread.h>

namespace {

std::string toUtf8(CFStringRef value)
{
  if (value == nullptr) {
    return {};
  }
  const CFIndex length = CFStringGetLength(value);
  const CFIndex size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  std::string result(static_cast<size_t>(size), '\0');
  if (!CFStringGetCString(value, result.data(), size, kCFStringEncodingUTF8)) {
    return {};
  }
  result.resize(strlen(result.c_str()));
  return result;
}

std::string sourceId(TISInputSourceRef source)
{
  return source == nullptr
             ? std::string()
             : toUtf8(static_cast<CFStringRef>(TISGetInputSourceProperty(source, kTISPropertyInputSourceID)));
}

bool boolProperty(TISInputSourceRef source, CFStringRef property)
{
  const auto value = static_cast<CFBooleanRef>(TISGetInputSourceProperty(source, property));
  return value != nullptr && CFBooleanGetValue(value);
}

deskflow::InputLanguageCategory sourceCategory(TISInputSourceRef source)
{
  if (source == nullptr) {
    return deskflow::InputLanguageCategory::Unknown;
  }
  const auto type = static_cast<CFStringRef>(TISGetInputSourceProperty(source, kTISPropertyInputSourceType));
  if ((type != nullptr && CFEqual(type, kTISTypeKeyboardInputMode)) ||
      TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData) == nullptr) {
    return deskflow::InputLanguageCategory::InputMethod;
  }
  return deskflow::InputLanguageCategory::KeyLayout;
}

bool isKorean(TISInputSourceRef source)
{
  const auto languages = static_cast<CFArrayRef>(TISGetInputSourceProperty(source, kTISPropertyInputSourceLanguages));
  if (languages != nullptr) {
    for (CFIndex i = 0; i < CFArrayGetCount(languages); ++i) {
      const auto language = static_cast<CFStringRef>(CFArrayGetValueAtIndex(languages, i));
      if (language != nullptr && CFStringHasPrefix(language, CFSTR("ko"))) {
        return true;
      }
    }
  }
  return sourceId(source).find("Korean") != std::string::npos;
}

} // namespace

struct OSXInputSourceController::Request
{
  OSXInputSourceController *m_controller;
  deskflow::InputLanguageAction m_action;
  const std::string *m_target;
};

struct OSXInputSourceController::StatusRequest
{
  const OSXInputSourceController *m_controller;
  deskflow::InputLanguageStatus *m_status;
};

OSXInputSourceController::OSXInputSourceController(IEventQueue *events, void *eventTarget)
    : m_events(events),
      m_eventTarget(eventTarget)
{
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(), this, inputSourceChanged,
      kTISNotifySelectedKeyboardInputSourceChanged, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately
  );
  status();
}

OSXInputSourceController::~OSXInputSourceController()
{
  CFNotificationCenterRemoveObserver(
      CFNotificationCenterGetDistributedCenter(), this, kTISNotifySelectedKeyboardInputSourceChanged, nullptr
  );
}

void OSXInputSourceController::control(deskflow::InputLanguageAction action, const std::string &target)
{
  Request request{this, action, &target};
  if (pthread_main_np() != 0) {
    applyRequest(&request);
  } else {
    dispatch_sync_f(dispatch_get_main_queue(), &request, applyRequest);
  }
}

deskflow::InputLanguageStatus OSXInputSourceController::status() const
{
  if (pthread_main_np() != 0) {
    return readStatus();
  }

  deskflow::InputLanguageStatus result;
  StatusRequest request{this, &result};
  dispatch_sync_f(dispatch_get_main_queue(), &request, readStatusRequest);
  return result;
}

deskflow::InputLanguageStatus OSXInputSourceController::readStatus() const
{
  std::lock_guard<std::mutex> lock(g_tisMutex);
  AutoTISInputSourceRef current(TISCopyCurrentKeyboardInputSource(), CFRelease);
  deskflow::InputLanguageStatus result{sourceId(current.get()), sourceCategory(current.get()), false};
  if (result.m_category == deskflow::InputLanguageCategory::KeyLayout && !result.m_sourceId.empty()) {
    m_lastLatinSourceId = result.m_sourceId;
  }
  return result;
}

void OSXInputSourceController::applyRequest(void *context)
{
  auto *request = static_cast<Request *>(context);
  request->m_controller->apply(request->m_action, *request->m_target);
}

void OSXInputSourceController::readStatusRequest(void *context)
{
  auto *request = static_cast<StatusRequest *>(context);
  *request->m_status = request->m_controller->readStatus();
}

void OSXInputSourceController::inputSourceChanged(
    CFNotificationCenterRef, void *observer, CFStringRef, const void *, CFDictionaryRef
)
{
  static_cast<OSXInputSourceController *>(observer)->emitStatus();
}

void OSXInputSourceController::apply(deskflow::InputLanguageAction action, const std::string &target)
{
  if (action == deskflow::InputLanguageAction::Query) {
    emitStatus();
    return;
  }

  std::string resolvedTarget = target;
  if (action == deskflow::InputLanguageAction::Toggle) {
    resolvedTarget = status().isInputMethod() ? "en" : "ko";
  }

  AutoTISInputSourceRef source(resolve(resolvedTarget), CFRelease);
  if (!source || !boolProperty(source.get(), kTISPropertyInputSourceIsSelectCapable)) {
    LOG_WARN("input source is unavailable or not selectable: %s", resolvedTarget.c_str());
    emitStatus();
    return;
  }

  OSStatus result = noErr;
  {
    std::lock_guard<std::mutex> lock(g_tisMutex);
    result = TISSelectInputSource(source.get());
  }
  if (result != noErr) {
    LOG_WARN("failed to select input source %s: %d", sourceId(source.get()).c_str(), result);
  }
  emitStatus();
}

void OSXInputSourceController::emitStatus() const
{
  m_events->addEvent(Event(EventTypes::InputLanguageChanged, m_eventTarget, new deskflow::InputLanguageStatus(status()))
  );
}

TISInputSourceRef OSXInputSourceController::resolve(const std::string &target) const
{
  if (target == "ko") {
    return resolveKorean();
  }
  if (target.empty() || target == "en") {
    return resolveLatin();
  }
  if (target == "hanja") {
    return nullptr;
  }
  return resolveId(target);
}

TISInputSourceRef OSXInputSourceController::resolveKorean() const
{
  std::lock_guard<std::mutex> lock(g_tisMutex);
  AutoCFArray sources(TISCreateInputSourceList(nullptr, false), CFRelease);
  if (!sources) {
    return nullptr;
  }
  for (CFIndex i = 0; i < CFArrayGetCount(sources.get()); ++i) {
    auto source = static_cast<TISInputSourceRef>(const_cast<void *>(CFArrayGetValueAtIndex(sources.get(), i)));
    if (sourceCategory(source) == deskflow::InputLanguageCategory::InputMethod &&
        boolProperty(source, kTISPropertyInputSourceIsSelectCapable) && isKorean(source)) {
      return static_cast<TISInputSourceRef>(CFRetain(source));
    }
  }
  return nullptr;
}

TISInputSourceRef OSXInputSourceController::resolveLatin() const
{
  if (!m_lastLatinSourceId.empty()) {
    if (auto source = resolveId(m_lastLatinSourceId); source != nullptr) {
      return source;
    }
  }
  std::lock_guard<std::mutex> lock(g_tisMutex);
  return TISCopyCurrentASCIICapableKeyboardInputSource();
}

TISInputSourceRef OSXInputSourceController::resolveId(const std::string &wantedSourceId) const
{
  const auto sourceIdString =
      CFStringCreateWithCString(kCFAllocatorDefault, wantedSourceId.c_str(), kCFStringEncodingUTF8);
  if (sourceIdString == nullptr) {
    return nullptr;
  }
  const void *keys[] = {kTISPropertyInputSourceID};
  const void *values[] = {sourceIdString};
  AutoCFDictionary filter(
      CFDictionaryCreate(
          kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
      ),
      CFRelease
  );
  CFRelease(sourceIdString);

  std::lock_guard<std::mutex> lock(g_tisMutex);
  AutoCFArray sources(TISCreateInputSourceList(filter.get(), false), CFRelease);
  if (!sources || CFArrayGetCount(sources.get()) == 0) {
    return nullptr;
  }
  auto source = static_cast<TISInputSourceRef>(const_cast<void *>(CFArrayGetValueAtIndex(sources.get(), 0)));
  return static_cast<TISInputSourceRef>(CFRetain(source));
}
