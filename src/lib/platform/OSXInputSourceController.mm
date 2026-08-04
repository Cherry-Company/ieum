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

#import <AppKit/AppKit.h>

#include <chrono>
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

bool isCjkvInputSource(TISInputSourceRef source)
{
  const auto languages = static_cast<CFArrayRef>(TISGetInputSourceProperty(source, kTISPropertyInputSourceLanguages));
  if (languages == nullptr) {
    return isKorean(source);
  }

  for (CFIndex i = 0; i < CFArrayGetCount(languages); ++i) {
    const auto language = static_cast<CFStringRef>(CFArrayGetValueAtIndex(languages, i));
    if (language != nullptr && (CFStringHasPrefix(language, CFSTR("ko")) || CFStringHasPrefix(language, CFSTR("zh")) ||
                                CFStringHasPrefix(language, CFSTR("ja")) || CFStringHasPrefix(language, CFSTR("vi")))) {
      return true;
    }
  }
  return false;
}

void commitCjkvInputSourceSelection()
{
  if (pthread_main_np() == 0) {
    return;
  }

  // macOS can update the menu-bar source without committing a CJKV input
  // method to the focused application. A short accessory-window activation
  // makes the input context commit before queued remote keystrokes continue.
  // This follows the independently implemented workaround documented by
  // macism: https://github.com/laishulu/macism
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  @autoreleasepool {
    NSRunningApplication *previousApplication = NSWorkspace.sharedWorkspace.frontmostApplication;
    NSApplication *application = NSApplication.sharedApplication;
    const NSApplicationActivationPolicy previousPolicy = application.activationPolicy;
    [application setActivationPolicy:NSApplicationActivationPolicyAccessory];

    NSScreen *screen = NSScreen.mainScreen;
    const NSRect frame = screen != nil ? screen.visibleFrame : NSMakeRect(0.0, 0.0, 3.0, 3.0);
    const NSRect windowFrame = NSMakeRect(NSMaxX(frame) - 11.0, NSMinY(frame) + 8.0, 3.0, 3.0);
    NSWindow *window = [[NSWindow alloc] initWithContentRect:windowFrame
                                                   styleMask:NSWindowStyleMaskTitled
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    [window setReleasedWhenClosed:NO];
    [window setAlphaValue:0.01];
    [window setOpaque:YES];
    [window setBackgroundColor:NSColor.blackColor];
    [window setHasShadow:NO];
    [window setIgnoresMouseEvents:YES];
    [window setLevel:NSScreenSaverWindowLevel];
    [window setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary];
    [window makeKeyAndOrderFront:nil];
    [application activateIgnoringOtherApps:YES];

    [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.15]];

    [window orderOut:nil];
    [window close];
    [window release];
    [application setActivationPolicy:previousPolicy];

    if (previousApplication != nil && !previousApplication.terminated &&
        previousApplication.processIdentifier != NSRunningApplication.currentApplication.processIdentifier) {
      [previousApplication activateWithOptions:NSApplicationActivateIgnoringOtherApps];
    }
  }
#pragma clang diagnostic pop
}

} // namespace

struct OSXInputSourceController::Request
{
  OSXInputSourceController *m_controller;
  deskflow::InputLanguageAction m_action;
  const std::string *m_target;
  std::string m_expectedSourceId;
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
  const auto initialStatus = status();
  m_observedSourceId = initialStatus.m_sourceId;
}

OSXInputSourceController::~OSXInputSourceController()
{
  CFNotificationCenterRemoveObserver(
      CFNotificationCenterGetDistributedCenter(), this, kTISNotifySelectedKeyboardInputSourceChanged, nullptr
  );
}

void OSXInputSourceController::control(deskflow::InputLanguageAction action, const std::string &target)
{
  {
    std::lock_guard<std::mutex> lock(m_changeMutex);
    m_controlInProgress = true;
  }

  Request request{this, action, &target, {}};
  if (pthread_main_np() != 0) {
    applyRequest(&request);
  } else {
    dispatch_sync_f(dispatch_get_main_queue(), &request, applyRequest);
  }

  if (!request.m_expectedSourceId.empty() && !waitForSource(request.m_expectedSourceId)) {
    LOG_WARN("input source did not become active in time: %s", request.m_expectedSourceId.c_str());
  }

  {
    std::lock_guard<std::mutex> lock(m_changeMutex);
    m_controlInProgress = false;
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
  request->m_expectedSourceId = request->m_controller->apply(request->m_action, *request->m_target);
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
  static_cast<OSXInputSourceController *>(observer)->handleInputSourceChanged();
}

std::string OSXInputSourceController::apply(deskflow::InputLanguageAction action, const std::string &target)
{
  if (action == deskflow::InputLanguageAction::Query) {
    return {};
  }

  std::string resolvedTarget = target;
  if (action == deskflow::InputLanguageAction::Toggle) {
    resolvedTarget = status().isInputMethod() ? "en" : "ko";
  }

  AutoTISInputSourceRef source(resolve(resolvedTarget), CFRelease);
  if (!source || !boolProperty(source.get(), kTISPropertyInputSourceIsSelectCapable)) {
    LOG_WARN("input source is unavailable or not selectable: %s", resolvedTarget.c_str());
    return {};
  }

  const auto expectedSourceId = sourceId(source.get());
  // Selection runs on the main thread. Do not hold g_tisMutex here because
  // TIS may synchronously deliver its change notification on this thread.
  const OSStatus result = TISSelectInputSource(source.get());
  if (result != noErr) {
    LOG_WARN("failed to select input source %s: %d", expectedSourceId.c_str(), result);
    return {};
  }
  if (isCjkvInputSource(source.get())) {
    commitCjkvInputSourceSelection();
  }
  return expectedSourceId;
}

void OSXInputSourceController::handleInputSourceChanged()
{
  const auto currentStatus = readStatus();
  bool reportChange = false;
  {
    std::lock_guard<std::mutex> lock(m_changeMutex);
    m_observedSourceId = currentStatus.m_sourceId;
    reportChange = !m_controlInProgress;
  }
  m_changeCondition.notify_all();
  if (reportChange) {
    m_events->addEvent(
        Event(EventTypes::InputLanguageChanged, m_eventTarget, new deskflow::InputLanguageStatus(currentStatus))
    );
  }
}

bool OSXInputSourceController::waitForSource(const std::string &expectedSourceId)
{
  using namespace std::chrono_literals;
  if (pthread_main_np() != 0) {
    return readStatus().m_sourceId == expectedSourceId;
  }

  std::unique_lock<std::mutex> lock(m_changeMutex);
  if (m_changeCondition.wait_for(lock, 500ms, [this, &expectedSourceId] {
        return m_observedSourceId == expectedSourceId;
      })) {
    return true;
  }

  lock.unlock();
  return status().m_sourceId == expectedSourceId;
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
      CFRetain(source);
      return source;
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
  CFRetain(source);
  return source;
}
