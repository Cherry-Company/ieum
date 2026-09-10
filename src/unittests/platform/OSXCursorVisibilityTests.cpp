/*
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXCursorVisibility.h"

#include <cstdlib>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

void require(bool condition, const char *message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void repeatedTransitionsStayBalanced()
{
  OSXCursorVisibility cursor;
  int depth = 0;
  int hides = 0;
  int shows = 0;
  const auto hide = [&] {
    ++depth;
    ++hides;
    require(depth == 1, "duplicate hide must not accumulate Quartz hide count");
    return kCGErrorSuccess;
  };
  const auto show = [&] {
    --depth;
    ++shows;
    require(depth == 0, "show must balance only an owned hide");
    return kCGErrorSuccess;
  };
  cursor.setHidden(false, show);
  for (int i = 0; i < 100; ++i) {
    cursor.setHidden(true, hide);  // enable / leave
    cursor.setHidden(true, hide);  // repeated leave
    cursor.setHidden(false, show); // local mouse / enter
    cursor.setHidden(false, show); // disable / destructor
  }
  require(depth == 0 && hides == 100 && shows == 100, "all transitions must balance");
}

void failuresRemainRetryable()
{
  OSXCursorVisibility cursor;
  int calls = 0;
  const auto fail = [&] {
    ++calls;
    return kCGErrorFailure;
  };
  const auto success = [&] {
    ++calls;
    return kCGErrorSuccess;
  };
  cursor.setHidden(true, fail);
  cursor.setHidden(false, success);
  require(calls == 1, "failed hide must not acquire a hide count");
  cursor.setHidden(true, success);
  cursor.setHidden(false, fail);
  cursor.setHidden(false, success);
  cursor.setHidden(false, success);
  require(calls == 4, "failed show must retain ownership for the next restore");
}

void eventTapAndScreenTransitionsStayBalanced()
{
  OSXCursorVisibility cursor;
  int depth = 0;
  const auto hide = [&] {
    require(++depth == 1, "concurrent hides must serialize");
    return kCGErrorSuccess;
  };
  const auto show = [&] {
    require(--depth == 0, "concurrent restores must serialize");
    return kCGErrorSuccess;
  };
  std::vector<std::thread> workers;
  for (int i = 0; i < 4; ++i) {
    workers.emplace_back([&] {
      for (int j = 0; j < 1000; ++j) {
        cursor.setHidden(true, hide);
        cursor.setHidden(false, show);
      }
    });
  }
  for (auto &worker : workers) {
    worker.join();
  }
  cursor.setHidden(false, show);
  require(depth == 0, "shutdown must leave no owned hide");
}

void onlyHardwarePointerInputRestoresCursor()
{
  CGEventRef event = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, CGPointZero, kCGMouseButtonLeft);
  require(event != nullptr, "mouse event fixture must exist");
  // This event is never posted: these tests do not move or hide the real cursor.
  CGEventSetIntegerValueField(event, kCGEventSourceUnixProcessID, getpid());
  require(
      !OSXCursorVisibility::isLocalPointerEvent(kCGEventMouseMoved, event),
      "Ieum's synthetic parking move must stay hidden"
  );
  CGEventSetIntegerValueField(event, kCGEventSourceUnixProcessID, 0);
  for (auto type :
       {kCGEventMouseMoved, kCGEventLeftMouseDragged, kCGEventRightMouseDragged, kCGEventOtherMouseDragged,
        kCGEventLeftMouseDown, kCGEventRightMouseDown, kCGEventOtherMouseDown, kCGEventLeftMouseUp,
        kCGEventRightMouseUp, kCGEventOtherMouseUp, kCGEventScrollWheel}) {
    require(OSXCursorVisibility::isLocalPointerEvent(type, event), "local pointer input must restore visibility");
  }
  require(!OSXCursorVisibility::isLocalPointerEvent(kCGEventKeyDown, event), "typing must not restore the cursor");
  require(
      !OSXCursorVisibility::isLocalPointerEvent(kCGEventTapDisabledByTimeout, nullptr),
      "tap notifications must not be read as input events"
  );
  CFRelease(event);
}

} // namespace

int main()
{
  repeatedTransitionsStayBalanced();
  failuresRemainRetryable();
  eventTapAndScreenTransitionsStayBalanced();
  onlyHardwarePointerInputRestoresCursor();
  std::cout << "OSX cursor visibility tests passed\n";
}
