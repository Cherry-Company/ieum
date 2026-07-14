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

MSWindowsImeController::MSWindowsImeController(IEventQueue *events, void *eventTarget)
    : m_events(events),
      m_eventTarget(eventTarget),
      m_lastStatus(status())
{
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
    const bool wasOpen = openStatus();
    if (!sendImeKey(VK_HANGUL)) {
      setOpenStatus(!wasOpen);
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

deskflow::InputLanguageStatus MSWindowsImeController::status() const
{
  const HWND foreground = GetForegroundWindow();
  DWORD threadId = foreground != nullptr ? GetWindowThreadProcessId(foreground, nullptr) : 0;
  const HKL layout = GetKeyboardLayout(threadId);
  const LANGID language = LOWORD(reinterpret_cast<ULONG_PTR>(layout));
  const bool imeOpen = openStatus();

  std::ostringstream source;
  source << (imeOpen ? "windows.ime." : "windows.keylayout.") << std::hex << std::setw(4) << std::setfill('0')
         << language;
  return {
      source.str(), imeOpen ? deskflow::InputLanguageCategory::InputMethod : deskflow::InputLanguageCategory::KeyLayout,
      false
  };
}

void MSWindowsImeController::poll()
{
  const auto current = status();
  if (!(current == m_lastStatus)) {
    emitStatus(current);
  }
}

HWND MSWindowsImeController::imeWindow() const
{
  const HWND foreground = GetForegroundWindow();
  return foreground == nullptr ? nullptr : ImmGetDefaultIMEWnd(foreground);
}

bool MSWindowsImeController::openStatus() const
{
  const HWND ime = imeWindow();
  if (ime == nullptr) {
    return false;
  }
  DWORD_PTR result = 0;
  if (SendMessageTimeout(ime, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &result) == 0) {
    return false;
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
             ime, WM_IME_CONTROL, IMC_SETOPENSTATUS, static_cast<LPARAM>(open), SMTO_ABORTIFHUNG | SMTO_BLOCK, 100,
             &result
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
