// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "SessionPlatform.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <QPointer>
#include <algorithm>
#include <atomic>
#include <audioclient.h>
#include <deque>
#include <mmdeviceapi.h>
#include <mutex>
#include <thread>
#include <windows.h>
#include <wrl/client.h>

namespace deskflow::gui::session {
namespace {
using Microsoft::WRL::ComPtr;
constexpr ULONG_PTR kInjectedTag = 0x4945554d;
WAVEFORMATEX audioFormat()
{
  WAVEFORMATEX format{};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 2;
  format.nSamplesPerSec = 48000;
  format.wBitsPerSample = 16;
  format.nBlockAlign = 4;
  format.nAvgBytesPerSec = 192000;
  return format;
}
bool openAudio(EDataFlow direction, ComPtr<IAudioClient> &client)
{
  ComPtr<IMMDeviceEnumerator> enumerator;
  ComPtr<IMMDevice> device;
  return SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator))) &&
         SUCCEEDED(enumerator->GetDefaultAudioEndpoint(direction, eConsole, &device)) &&
         SUCCEEDED(device->Activate(
             __uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(client.GetAddressOf())
         ));
}
class WindowsSessionPlatform final : public SessionPlatform
{
public:
  ~WindowsSessionPlatform() override
  {
    stopAudioCapture();
    stopAudioPlayback();
    stopInputCapture();
    releaseInput();
  }
  bool startAudioCapture() override
  {
    stopAudioCapture();
    const auto generation = m_audioGeneration.load();
    m_capture = std::jthread([this, generation](std::stop_token stop) {
      const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      if (FAILED(initialized)) {
        audioFailure();
        return;
      }
      {
        ComPtr<IAudioClient> client;
        ComPtr<IAudioCaptureClient> capture;
        auto format = audioFormat();
        const DWORD flags =
            AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        if (!openAudio(eRender, client) ||
            FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 200000, 0, &format, nullptr)) ||
            FAILED(client->GetService(IID_PPV_ARGS(&capture))) || FAILED(client->Start())) {
          audioFailure();
        } else {
          QByteArray pending;
          while (!stop.stop_requested()) {
            UINT32 frames = 0;
            const auto next = capture->GetNextPacketSize(&frames);
            if (FAILED(next)) {
              audioFailure();
              break;
            }
            if (!frames) {
              Sleep(4);
              continue;
            }
            BYTE *data = nullptr;
            DWORD flagsRead = 0;
            UINT32 available = 0;
            if (FAILED(capture->GetBuffer(&data, &available, &flagsRead, nullptr, nullptr))) {
              audioFailure();
              break;
            }
            if (available <= 48000) {
              if (flagsRead & AUDCLNT_BUFFERFLAGS_SILENT)
                pending.append(QByteArray(static_cast<int>(available * 4), '\0'));
              else
                pending.append(reinterpret_cast<const char *>(data), static_cast<int>(available * 4));
            }
            capture->ReleaseBuffer(available);
            while (pending.size() >= 3840) {
              auto pcm = pending.left(3840);
              pending.remove(0, 3840);
              if (m_pendingAudio.fetch_add(1) >= 4) {
                m_pendingAudio.fetch_sub(1);
                continue;
              }
              QMetaObject::invokeMethod(
                  this,
                  [this, pcm, generation] {
                    m_pendingAudio.fetch_sub(1);
                    if (m_audioGeneration.load() == generation)
                      Q_EMIT audioCaptured(pcm);
                  },
                  Qt::QueuedConnection
              );
            }
          }
          client->Stop();
        }
      }
      CoUninitialize();
    });
    return true;
  }
  void stopAudioCapture() override
  {
    ++m_audioGeneration;
    if (m_capture.joinable()) {
      m_capture.request_stop();
      m_capture.join();
    }
  }
  bool playAudio(const QByteArray &pcm) override
  {
    if (pcm.isEmpty() || pcm.size() > 15360 || pcm.size() % 4)
      return false;
    {
      std::lock_guard lock(m_audioMutex);
      while (m_audioBytes + pcm.size() > 38400 && !m_audio.empty()) {
        m_audioBytes -= m_audio.front().size();
        m_audio.pop_front();
      }
      m_audio.push_back(pcm);
      m_audioBytes += pcm.size();
    }
    if (m_render.joinable())
      return true;
    m_render = std::jthread([this](std::stop_token stop) {
      if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        audioFailure();
        return;
      }
      {
        ComPtr<IAudioClient> client;
        ComPtr<IAudioRenderClient> render;
        auto format = audioFormat();
        UINT32 capacity = 0;
        if (!openAudio(eRender, client) ||
            FAILED(client->Initialize(
                AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                400000, 0, &format, nullptr
            )) ||
            FAILED(client->GetBufferSize(&capacity)) || FAILED(client->GetService(IID_PPV_ARGS(&render))) ||
            FAILED(client->Start())) {
          audioFailure();
        } else {
          QByteArray current;
          while (!stop.stop_requested()) {
            if (current.isEmpty()) {
              std::lock_guard lock(m_audioMutex);
              if (!m_audio.empty()) {
                current = std::move(m_audio.front());
                m_audio.pop_front();
                m_audioBytes -= current.size();
              }
            }
            UINT32 padding = 0;
            if (FAILED(client->GetCurrentPadding(&padding))) {
              audioFailure();
              break;
            }
            const auto frames = std::min<UINT32>(capacity - padding, static_cast<UINT32>(current.size() / 4));
            if (!frames) {
              Sleep(4);
              continue;
            }
            BYTE *output = nullptr;
            if (FAILED(render->GetBuffer(frames, &output))) {
              audioFailure();
              break;
            }
            memcpy(output, current.constData(), frames * 4);
            current.remove(0, frames * 4);
            if (FAILED(render->ReleaseBuffer(frames, 0))) {
              audioFailure();
              break;
            }
          }
          client->Stop();
        }
      }
      CoUninitialize();
    });
    return true;
  }
  void stopAudioPlayback() override
  {
    if (m_render.joinable()) {
      m_render.request_stop();
      m_render.join();
    }
    std::lock_guard lock(m_audioMutex);
    m_audio.clear();
    m_audioBytes = 0;
  }
  bool startInputCapture(InputMode mode) override
  {
    stopInputCapture();
    if (s_capture && s_capture != this)
      return fail(SessionPlatform::tr("Another input sharing session is active."));
    s_capture = this;
    m_mode = mode;
    m_keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHook, GetModuleHandleW(nullptr), 0);
    m_mouse = SetWindowsHookExW(WH_MOUSE_LL, mouseHook, GetModuleHandleW(nullptr), 0);
    if (!m_keyboard || !m_mouse) {
      stopInputCapture();
      return fail(SessionPlatform::tr("Windows did not allow input capture."));
    }
    return true;
  }
  void stopInputCapture() override
  {
    if (m_keyboard)
      UnhookWindowsHookEx(m_keyboard);
    if (m_mouse)
      UnhookWindowsHookEx(m_mouse);
    m_keyboard = nullptr;
    m_mouse = nullptr;
    m_physicalKeys.clear();
    if (s_capture == this)
      s_capture = nullptr;
  }
  bool inject(const Input &input) override
  {
    INPUT event{};
    if (input.kind == Input::Kind::Key) {
      event.type = INPUT_KEYBOARD;
      event.ki.dwExtraInfo = kInjectedTag;
      if (input.code == 0xf2 || input.code == 0xf1)
        event.ki.wVk = input.code == 0xf2 ? VK_HANGUL : VK_HANJA;
      else {
        event.ki.wScan = static_cast<WORD>(input.code & 0xff);
        event.ki.dwFlags = KEYEVENTF_SCANCODE | (input.code & 0x100 ? KEYEVENTF_EXTENDEDKEY : 0);
      }
      if (!input.down)
        event.ki.dwFlags |= KEYEVENTF_KEYUP;
      if (SendInput(1, &event, sizeof(event)) != 1)
        return fail(
            SessionPlatform::tr("Windows refused input. Run Ieum at the same privilege level as the target app.")
        );
      if (input.down)
        m_keys.insert(input.code);
      else
        m_keys.remove(input.code);
      return true;
    }
    event.type = INPUT_MOUSE;
    event.mi.dwExtraInfo = kInjectedTag;
    if (input.kind == Input::Kind::Move) {
      event.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
      event.mi.dx = input.x;
      event.mi.dy = input.y;
    } else if (input.kind == Input::Kind::Button) {
      const DWORD down[] = {
          0, MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_XDOWN, MOUSEEVENTF_XDOWN
      };
      const DWORD up[] = {
          0, MOUSEEVENTF_LEFTUP, MOUSEEVENTF_MIDDLEUP, MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_XUP, MOUSEEVENTF_XUP
      };
      if (input.code < 1 || input.code > 5)
        return false;
      event.mi.dwFlags = input.down ? down[input.code] : up[input.code];
      if (input.code >= 4)
        event.mi.mouseData = input.code == 4 ? XBUTTON1 : XBUTTON2;
    } else if (input.kind == Input::Kind::Wheel) {
      if (input.x) {
        event.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        event.mi.mouseData = static_cast<DWORD>(input.x);
        if (SendInput(1, &event, sizeof(event)) != 1)
          return false;
      }
      if (!input.y)
        return true;
      event.mi.dwFlags = MOUSEEVENTF_WHEEL;
      event.mi.mouseData = static_cast<DWORD>(input.y);
    } else
      return false;
    if (SendInput(1, &event, sizeof(event)) != 1)
      return fail(SessionPlatform::tr("Windows refused pointer input."));
    if (input.kind == Input::Kind::Button) {
      if (input.down)
        m_buttons.insert(input.code);
      else
        m_buttons.remove(input.code);
    }
    return true;
  }
  void releaseInput() override
  {
    const auto keys = m_keys, buttons = m_buttons;
    for (int key : keys)
      (void)inject({Input::Kind::Key, key, false});
    for (int button : buttons)
      (void)inject({Input::Kind::Button, button, false});
    m_keys.clear();
    m_buttons.clear();
  }
  bool lockDesktop() override
  {
    return LockWorkStation() || fail(SessionPlatform::tr("Windows could not lock this session."));
  }
  bool wakeDisplays() override
  {
    SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    DWORD_PTR result = 0;
    return SendMessageTimeoutW(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, -1, SMTO_ABORTIFHUNG, 200, &result) != 0;
  }
  bool locked() override
  {
    const auto desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!desktop)
      return true;
    wchar_t name[128]{};
    DWORD bytes = 0;
    const bool unlocked =
        GetUserObjectInformationW(desktop, UOI_NAME, name, sizeof(name), &bytes) && _wcsicmp(name, L"Default") == 0;
    CloseDesktop(desktop);
    return !unlocked;
  }

private:
  void audioFailure()
  {
    QMetaObject::invokeMethod(
        this,
        [this] {
          Q_EMIT failure(
              "audio-sharing",
              SessionPlatform::tr("The Windows audio device is unavailable or changed. Restart audio sharing.")
          );
        },
        Qt::QueuedConnection
    );
  }
  static LRESULT CALLBACK keyboardHook(int code, WPARAM message, LPARAM data)
  {
    auto *self = s_capture;
    if (code != HC_ACTION || !self)
      return CallNextHookEx(nullptr, code, message, data);
    const auto &key = *reinterpret_cast<KBDLLHOOKSTRUCT *>(data);
    if (key.flags & LLKHF_INJECTED)
      return CallNextHookEx(nullptr, code, message, data);
    const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    int scan = static_cast<int>(key.scanCode & 0xff) | (key.flags & LLKHF_EXTENDED ? 0x100 : 0);
    if (key.vkCode == VK_HANGUL)
      scan = 0xf2;
    if (key.vkCode == VK_HANJA)
      scan = 0xf1;
    if (!scan)
      return CallNextHookEx(nullptr, code, message, data);
    if (down)
      self->m_physicalKeys.insert(scan);
    else
      self->m_physicalKeys.remove(scan);
    Input input{Input::Kind::Key, scan, down};
    if (down && scan == 1 && (self->m_physicalKeys.contains(0x1d) || self->m_physicalKeys.contains(0x11d)) &&
        (self->m_physicalKeys.contains(0x38) || self->m_physicalKeys.contains(0x138)))
      input.kind = Input::Kind::Escape;
    const bool suppress = self->m_mode == InputMode::Control;
    Q_EMIT self->inputCaptured(input);
    return suppress ? 1 : CallNextHookEx(nullptr, code, message, data);
  }
  static LRESULT CALLBACK mouseHook(int code, WPARAM message, LPARAM data)
  {
    auto *self = s_capture;
    if (code != HC_ACTION || !self)
      return CallNextHookEx(nullptr, code, message, data);
    const auto &mouse = *reinterpret_cast<MSLLHOOKSTRUCT *>(data);
    if (mouse.flags & LLMHF_INJECTED)
      return CallNextHookEx(nullptr, code, message, data);
    Input input;
    if (message == WM_MOUSEMOVE) {
      input.x = std::clamp(
          static_cast<int>(
              (static_cast<qint64>(mouse.pt.x) - GetSystemMetrics(SM_XVIRTUALSCREEN)) * 65535 /
              std::max(1, GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1)
          ),
          0, 65535
      );
      input.y = std::clamp(
          static_cast<int>(
              (static_cast<qint64>(mouse.pt.y) - GetSystemMetrics(SM_YVIRTUALSCREEN)) * 65535 /
              std::max(1, GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1)
          ),
          0, 65535
      );
    } else if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
      input.kind = Input::Kind::Wheel;
      const int delta = std::clamp(static_cast<int>(static_cast<short>(HIWORD(mouse.mouseData))), -1200, 1200);
      if (message == WM_MOUSEHWHEEL)
        input.x = delta;
      else
        input.y = delta;
    } else {
      input.kind = Input::Kind::Button;
      switch (message) {
      case WM_LBUTTONDOWN:
        input.code = 1;
        input.down = true;
        break;
      case WM_LBUTTONUP:
        input.code = 1;
        break;
      case WM_MBUTTONDOWN:
        input.code = 2;
        input.down = true;
        break;
      case WM_MBUTTONUP:
        input.code = 2;
        break;
      case WM_RBUTTONDOWN:
        input.code = 3;
        input.down = true;
        break;
      case WM_RBUTTONUP:
        input.code = 3;
        break;
      case WM_XBUTTONDOWN:
        input.code = HIWORD(mouse.mouseData) == XBUTTON1 ? 4 : 5;
        input.down = true;
        break;
      case WM_XBUTTONUP:
        input.code = HIWORD(mouse.mouseData) == XBUTTON1 ? 4 : 5;
        break;
      default:
        return CallNextHookEx(nullptr, code, message, data);
      }
    }
    const bool suppress = self->m_mode == InputMode::Control && input.kind != Input::Kind::Move;
    Q_EMIT self->inputCaptured(input);
    return suppress ? 1 : CallNextHookEx(nullptr, code, message, data);
  }
  inline static WindowsSessionPlatform *s_capture = nullptr;
  HHOOK m_keyboard = nullptr, m_mouse = nullptr;
  InputMode m_mode = InputMode::Observe;
  QSet<int> m_keys, m_buttons, m_physicalKeys;
  std::jthread m_capture, m_render;
  std::atomic<int> m_pendingAudio{0};
  std::atomic<unsigned> m_audioGeneration{0};
  std::mutex m_audioMutex;
  std::deque<QByteArray> m_audio;
  qsizetype m_audioBytes = 0;
};
} // namespace
std::unique_ptr<SessionPlatform> createSessionPlatform()
{
  return std::make_unique<WindowsSessionPlatform>();
}
} // namespace deskflow::gui::session
