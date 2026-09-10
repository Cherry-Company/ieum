/*
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <ApplicationServices/ApplicationServices.h>
#include <mutex>

// Quartz counts hide/show calls. Own at most one hide, including when the
// event-tap thread restores the cursor while the screen thread changes focus.
class OSXCursorVisibility
{
public:
  template <typename Apply> void setHidden(bool hidden, Apply apply)
  {
    std::lock_guard lock(m_mutex);
    if (hidden != m_hidden && apply() == kCGErrorSuccess) {
      m_hidden = hidden;
    }
  }

  static bool isLocalPointerEvent(CGEventType type, CGEventRef event)
  {
    switch (type) {
    case kCGEventMouseMoved:
    case kCGEventLeftMouseDragged:
    case kCGEventRightMouseDragged:
    case kCGEventOtherMouseDragged:
    case kCGEventLeftMouseDown:
    case kCGEventRightMouseDown:
    case kCGEventOtherMouseDown:
    case kCGEventLeftMouseUp:
    case kCGEventRightMouseUp:
    case kCGEventOtherMouseUp:
    case kCGEventScrollWheel:
      // Hardware events have no originating user process. In particular,
      // Ieum's synthetic parking move must not undo leave()/enable().
      return event && CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID) == 0;
    default:
      return false;
    }
  }

private:
  std::mutex m_mutex;
  bool m_hidden = false;
};
