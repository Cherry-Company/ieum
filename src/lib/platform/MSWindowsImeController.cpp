/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsImeController.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/Log.h"

#include <Imm.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace {

// Desktop Windows SDKs omit these legacy WM_IME_CONTROL command constants.
constexpr WPARAM kImcGetOpenStatus = 0x0005;
constexpr WPARAM kImcSetOpenStatus = 0x0006;

// Upper bound on how long an IME query may hold the event loop.
constexpr UINT kImeQueryTimeoutMs = 20;

} // namespace

MSWindowsImeController::MSWindowsImeController(IEventQueue *events, void *eventTarget)
    : m_events(events),
      m_eventTarget(eventTarget)
{
  if (const auto current = queryStatus(); current.has_value()) {
    m_lastStatus = *current;
  }
}

void MSWindowsImeController::control(deskflow::InputLanguageAction action, const std::string &target)
{
  if (action == deskflow::InputLanguageAction::Query) {
    emitStatus(status());
    return;
  }

  if (target == "hanja") {
    sendImeKey(VK_HANJA);
  } else if (action == deskflow::InputLanguageAction::Toggle) {
    if (!sendImeKey(VK_HANGUL)) {
      const auto wasOpen = openStatus();
      if (!wasOpen.has_value() || !setOpenStatus(!*wasOpen)) {
        LOG_WARN("failed to toggle Windows IME open state");
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  } else if (target == "ko") {
    setOpenStatus(true);
  } else if (target.empty() || target == "en") {
    setOpenStatus(false);
  } else {
    LOG_WARN("unsupported Windows input language target: %s", target.c_str());
  }

  emitStatus(status());
}

bool MSWindowsImeController::hasImeOpenState(HKL layout)
{
  return ImmIsIME(layout) != FALSE;
}

deskflow::InputLanguageStatus MSWindowsImeController::status() const
{
  return queryStatus().value_or(m_lastStatus);
}

std::optional<deskflow::InputLanguageStatus> MSWindowsImeController::queryStatus() const
{
  const HWND foreground = GetForegroundWindow();
  DWORD threadId = foreground != nullptr ? GetWindowThreadProcessId(foreground, nullptr) : 0;
  const HKL layout = GetKeyboardLayout(threadId);
  const LANGID language = LOWORD(reinterpret_cast<ULONG_PTR>(layout));

  // openStatus() is a cross-process send. Layouts that can never have an open
  // IME answer from the layout alone, which keeps the poll off that path
  // entirely for Latin keyboards.
  bool imeOpen = false;
  if (hasImeOpenState(layout)) {
    const auto currentOpenStatus = openStatus();
    if (!currentOpenStatus.has_value()) {
      return std::nullopt;
    }
    imeOpen = *currentOpenStatus;
  }

  std::ostringstream source;
  source << (imeOpen ? "windows.ime." : "windows.keylayout.") << std::hex << std::setw(4) << std::setfill('0')
         << language;
  return deskflow::InputLanguageStatus{
      source.str(), imeOpen ? deskflow::InputLanguageCategory::InputMethod : deskflow::InputLanguageCategory::KeyLayout,
      false};
}

void MSWindowsImeController::poll()
{
  const auto current = queryStatus();
  if (current.has_value() && !(*current == m_lastStatus)) {
    emitStatus(*current);
  }
}

HWND MSWindowsImeController::imeWindow() const
{
  const HWND foreground = GetForegroundWindow();
  return foreground == nullptr ? nullptr : ImmGetDefaultIMEWnd(foreground);
}

std::optional<bool> MSWindowsImeController::openStatus() const
{
  const HWND ime = imeWindow();
  if (ime == nullptr) {
    return std::nullopt;
  }
  DWORD_PTR result = 0;
  // Block re-entrant nonqueued messages while the query is in flight. The
  // timeout is short because this runs on the input event loop.
  if (SendMessageTimeout(
          ime, WM_IME_CONTROL, kImcGetOpenStatus, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, kImeQueryTimeoutMs, &result
      ) == 0) {
    return std::nullopt;
  }
  return result != 0;
}

bool MSWindowsImeController::setOpenStatus(bool open) const
{
  const HWND ime = imeWindow();
  if (ime == nullptr) {
    return false;
  }
  DWORD_PTR result = 0;
  return SendMessageTimeout(
             ime, WM_IME_CONTROL, kImcSetOpenStatus, static_cast<LPARAM>(open), SMTO_ABORTIFHUNG | SMTO_BLOCK,
             kImeQueryTimeoutMs, &result
         ) != 0;
}

bool MSWindowsImeController::sendImeKey(WORD virtualKey) const
{
  INPUT input[2]{};
  input[0].type = INPUT_KEYBOARD;
  input[0].ki.wVk = virtualKey;
  input[0].ki.wScan = static_cast<WORD>(MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC));
  input[1] = input[0];
  input[1].ki.dwFlags = KEYEVENTF_KEYUP;
  return SendInput(2, input, sizeof(INPUT)) == 2;
}

void MSWindowsImeController::emitStatus(const deskflow::InputLanguageStatus &current)
{
  m_lastStatus = current;
  m_events->addEvent(Event(EventTypes::InputLanguageChanged, m_eventTarget, new deskflow::InputLanguageStatus(current))
  );
}
