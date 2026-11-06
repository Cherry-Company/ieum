// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "SessionPlatform.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QProcess>
#include <QSignalBlocker>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include <array>
#ifdef IEUM_SESSION_X11
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/dpms.h>
#endif

namespace deskflow::gui::session {
namespace {
int set1FromEvdev(int code)
{
  if (code > 0 && code <= 88)
    return code;
  switch (code) {
  case 96:
    return 0x11c;
  case 97:
    return 0x11d;
  case 98:
    return 0x135;
  case 99:
    return 0x137;
  case 100:
    return 0x138;
  case 102:
    return 0x147;
  case 103:
    return 0x148;
  case 104:
    return 0x149;
  case 105:
    return 0x14b;
  case 106:
    return 0x14d;
  case 107:
    return 0x14f;
  case 108:
    return 0x150;
  case 109:
    return 0x151;
  case 110:
    return 0x152;
  case 111:
    return 0x153;
  case 125:
    return 0x15b;
  case 126:
    return 0x15c;
  case 127:
    return 0x15d;
  default:
    return 0;
  }
}
int evdevFromSet1(int code)
{
  for (int i = 1; i < 256; ++i)
    if (set1FromEvdev(i) == code)
      return i;
  return 0;
}
class LinuxSessionPlatform final : public SessionPlatform
{
public:
  LinuxSessionPlatform()
  {
    m_capture.setStandardErrorFile(QProcess::nullDevice());
    m_playback.setStandardErrorFile(QProcess::nullDevice());
    m_playback.setStandardOutputFile(QProcess::nullDevice());
    refreshLockStatus();
    connect(&m_capture, &QProcess::readyReadStandardOutput, this, [this] {
      m_pcm.append(m_capture.readAllStandardOutput());
      if (m_pcm.size() > 15360)
        m_pcm = m_pcm.right(15360 + m_pcm.size() % 4);
      while (m_pcm.size() >= 3840) {
        Q_EMIT audioCaptured(m_pcm.left(3840));
        m_pcm.remove(0, 3840);
      }
    });
    connect(&m_capture, &QProcess::errorOccurred, this, [this] {
      reportAudioFailure(SessionPlatform::tr("System audio capture stopped. Check the PulseAudio/PipeWire device."));
    });
    connect(&m_capture, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
      if (m_capturing) {
        m_capturing = false;
        reportAudioFailure(SessionPlatform::tr("System audio capture ended."));
      }
    });
    connect(&m_playback, &QProcess::errorOccurred, this, [this] {
      reportAudioFailure(SessionPlatform::tr("Audio output is unavailable."));
    });
    connect(&m_playback, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
      if (m_playing) {
        m_playing = false;
        reportAudioFailure(SessionPlatform::tr("Audio output is unavailable."));
      }
    });
  }
  ~LinuxSessionPlatform() override
  {
    stopAudioCapture();
    stopAudioPlayback();
    stopInputCapture();
    releaseInput();
#ifdef IEUM_SESSION_X11
    if (m_display)
      XCloseDisplay(m_display);
#endif
  }
  bool requestScreenPermission() override
  {
    return QGuiApplication::platformName() == "xcb" ||
           fail(SessionPlatform::tr("Screen sharing currently requires an X11 desktop on Linux."));
  }
  bool startAudioCapture() override
  {
    stopAudioCapture();
    const auto program = QStandardPaths::findExecutable("parec");
    const auto pactl = QStandardPaths::findExecutable("pactl");
    if (program.isEmpty() || pactl.isEmpty())
      return fail(SessionPlatform::tr("Install PulseAudio utilities (parec, pactl and paplay) to share system audio."));
    QProcess query;
    query.start(pactl, {"get-default-sink"});
    if (!query.waitForFinished(1000) || query.exitCode() != 0)
      return fail(SessionPlatform::tr("The default audio output could not be found."));
    const auto sink = QString::fromUtf8(query.readAllStandardOutput()).trimmed();
    if (sink.isEmpty() || sink.size() > 512 || sink.contains('\n'))
      return fail(SessionPlatform::tr("The default audio output is invalid."));
    m_capture.start(
        program, {"--raw", "--format=s16le", "--rate=48000", "--channels=2", "--latency-msec=20",
                  "--device=" + sink + ".monitor"}
    );
    m_capturing = m_capture.waitForStarted(500);
    return m_capturing || fail(SessionPlatform::tr("Could not start system audio capture."));
  }
  void stopAudioCapture() override
  {
    m_capturing = false;
    const QSignalBlocker blocker(&m_capture);
    if (m_capture.state() != QProcess::NotRunning) {
      m_capture.kill();
      m_capture.waitForFinished(500);
    }
    m_pcm.clear();
  }
  bool playAudio(const QByteArray &pcm) override
  {
    if (pcm.isEmpty() || pcm.size() > 15360 || pcm.size() % 4)
      return false;
    if (m_playback.state() == QProcess::NotRunning) {
      const auto program = QStandardPaths::findExecutable("paplay");
      if (program.isEmpty())
        return fail(SessionPlatform::tr("Install paplay to play shared audio."));
      m_playback.start(program, {"--raw", "--format=s16le", "--rate=48000", "--channels=2", "--latency-msec=40"});
      if (!m_playback.waitForStarted(500))
        return fail(SessionPlatform::tr("Audio output could not be started."));
      m_playing = true;
    }
    if (m_playback.bytesToWrite() + pcm.size() > 38400)
      return true; // Drop late audio rather than accumulating latency.
    return m_playback.write(pcm) == pcm.size();
  }
  void stopAudioPlayback() override
  {
    m_playing = false;
    const QSignalBlocker blocker(&m_playback);
    if (m_playback.state() != QProcess::NotRunning) {
      m_playback.kill();
      m_playback.waitForFinished(500);
    }
  }
  bool startInputCapture(InputMode mode) override
  {
    stopInputCapture();
#ifdef IEUM_SESSION_X11
    if (!display())
      return false;
    int event = 0, error = 0;
    if (!XQueryExtension(m_display, "XInputExtension", &m_opcode, &event, &error))
      return fail(SessionPlatform::tr("XInput2 is unavailable."));
    int major = 2, minor = 0;
    if (XIQueryVersion(m_display, &major, &minor) != Success)
      return fail(SessionPlatform::tr("XInput2 is unavailable."));
    m_mode = mode;
    m_devices.clear();
    int count = 0;
    auto *devices = XIQueryDevice(m_display, XIAllDevices, &count);
    for (int i = 0; i < count; ++i)
      if (devices[i].use != XIMasterPointer && devices[i].use != XIMasterKeyboard &&
          !QString::fromUtf8(devices[i].name).contains("XTEST", Qt::CaseInsensitive))
        m_devices.insert(devices[i].deviceid);
    if (devices)
      XIFreeDeviceInfo(devices);
    unsigned char mask[XIMaskLen(XI_RawMotion)]{};
    for (int type : {XI_RawKeyPress, XI_RawKeyRelease, XI_RawButtonPress, XI_RawButtonRelease, XI_RawMotion})
      XISetMask(mask, type);
    XIEventMask selection{XIAllMasterDevices, sizeof(mask), mask};
    XISelectEvents(m_display, DefaultRootWindow(m_display), &selection, 1);
    if (mode == InputMode::Control) {
      if (XGrabKeyboard(m_display, DefaultRootWindow(m_display), False, GrabModeAsync, GrabModeAsync, CurrentTime) !=
          GrabSuccess)
        return fail(SessionPlatform::tr("Another application owns the keyboard."));
      if (XGrabPointer(
              m_display, DefaultRootWindow(m_display), False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
              GrabModeAsync, GrabModeAsync, None, None, CurrentTime
          ) != GrabSuccess) {
        XUngrabKeyboard(m_display, CurrentTime);
        return fail(SessionPlatform::tr("Another application owns the pointer."));
      }
    }
    m_notifier = std::make_unique<QSocketNotifier>(ConnectionNumber(m_display), QSocketNotifier::Read, this);
    connect(m_notifier.get(), &QSocketNotifier::activated, this, [this] { readInput(); });
    XFlush(m_display);
    return true;
#else
    (void)mode;
    return fail(SessionPlatform::tr("Physical input sharing requires X11 on this Linux build."));
#endif
  }
  void stopInputCapture() override
  {
#ifdef IEUM_SESSION_X11
    m_notifier.reset();
    if (m_display) {
      XUngrabKeyboard(m_display, CurrentTime);
      XUngrabPointer(m_display, CurrentTime);
      unsigned char mask[1]{};
      XIEventMask selection{XIAllMasterDevices, sizeof(mask), mask};
      XISelectEvents(m_display, DefaultRootWindow(m_display), &selection, 1);
      XFlush(m_display);
    }
    m_physicalKeys.clear();
#endif
  }
  bool inject(const Input &input) override
  {
#ifdef IEUM_SESSION_X11
    if (!display())
      return false;
    switch (input.kind) {
    case Input::Kind::Key: {
      const auto key = evdevFromSet1(input.code);
      if (!key)
        return false;
      XTestFakeKeyEvent(m_display, key + 8, input.down, CurrentTime);
      if (input.down)
        m_keys.insert(input.code);
      else
        m_keys.remove(input.code);
      break;
    }
    case Input::Kind::Button:
      XTestFakeButtonEvent(m_display, input.code <= 3 ? input.code : input.code + 4, input.down, CurrentTime);
      if (input.down)
        m_buttons.insert(input.code);
      else
        m_buttons.remove(input.code);
      break;
    case Input::Kind::Move:
      XTestFakeMotionEvent(
          m_display, DefaultScreen(m_display),
          input.x * (DisplayWidth(m_display, DefaultScreen(m_display)) - 1) / 65535,
          input.y * (DisplayHeight(m_display, DefaultScreen(m_display)) - 1) / 65535, CurrentTime
      );
      break;
    case Input::Kind::Wheel:
      for (auto [delta, negative, positive] : {std::array<int, 3>{input.x, 6, 7}, std::array<int, 3>{input.y, 5, 4}})
        for (int i = 0; i < std::abs(delta) / 120; ++i) {
          XTestFakeButtonEvent(m_display, delta > 0 ? positive : negative, True, CurrentTime);
          XTestFakeButtonEvent(m_display, delta > 0 ? positive : negative, False, CurrentTime);
        }
      break;
    default:
      return false;
    }
    XFlush(m_display);
    return true;
#else
    (void)input;
    return false;
#endif
  }
  void releaseInput() override
  {
#ifdef IEUM_SESSION_X11
    const auto keys = m_keys, buttons = m_buttons;
    for (int key : keys)
      (void)inject({Input::Kind::Key, key, false});
    for (int button : buttons)
      (void)inject({Input::Kind::Button, button, false});
#endif
  }
  bool lockDesktop() override
  {
    const auto program = QStandardPaths::findExecutable("loginctl");
    if (program.isEmpty())
      return fail(SessionPlatform::tr("The desktop session lock service is unavailable."));
    QProcess process;
    process.start(program, {"lock-session"});
    return (process.waitForFinished(1500) && process.exitCode() == 0) ||
           fail(SessionPlatform::tr("The desktop refused the lock request."));
  }
  bool wakeDisplays() override
  {
#ifdef IEUM_SESSION_X11
    if (!display())
      return false;
    XForceScreenSaver(m_display, ScreenSaverReset);
    if (DPMSCapable(m_display))
      DPMSForceLevel(m_display, DPMSModeOn);
    XFlush(m_display);
    return true;
#else
    return fail(SessionPlatform::tr("Display wake requires X11. Sleeping computers can use Wake-on-LAN."));
#endif
  }
  bool locked() override
  {
    if (!m_lockClock.isValid() || m_lockClock.hasExpired(1000))
      refreshLockStatus();
    return m_locked;
  }

private:
  void reportAudioFailure(const QString &message)
  {
    QMetaObject::invokeMethod(
        this, [this, message] { Q_EMIT failure("audio-sharing", message); }, Qt::QueuedConnection
    );
  }
  void refreshLockStatus()
  {
    if (m_lockQueryPending)
      return;
    m_lockClock.start();
    m_lockQueryPending = true;
    const bool lookup = m_sessionPath.isEmpty();
    auto request = lookup ? QDBusMessage::createMethodCall(
                                "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager",
                                "GetSessionByPID"
                            )
                          : QDBusMessage::createMethodCall(
                                "org.freedesktop.login1", m_sessionPath, "org.freedesktop.DBus.Properties", "Get"
                            );
    if (lookup)
      request << static_cast<quint32>(QCoreApplication::applicationPid());
    else
      request << "org.freedesktop.login1.Session" << "LockedHint";
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(request, 500), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, lookup](QDBusPendingCallWatcher *call) {
      m_lockQueryPending = false;
      call->deleteLater();
      if (lookup) {
        const QDBusPendingReply<QDBusObjectPath> reply = *call;
        if (reply.isError()) {
          m_locked = true;
          return;
        }
        m_sessionPath = reply.value().path();
        refreshLockStatus();
      } else {
        const QDBusPendingReply<QDBusVariant> reply = *call;
        // Unknown lock state cannot authorize capture of a potentially locked desktop.
        m_locked = reply.isError() || reply.value().variant().toBool();
        if (reply.isError())
          m_sessionPath.clear();
      }
    });
  }
  QElapsedTimer m_lockClock;
  bool m_locked = true, m_lockQueryPending = false;
  QString m_sessionPath;
  QProcess m_capture, m_playback;
  QByteArray m_pcm;
  bool m_capturing = false, m_playing = false;
#ifdef IEUM_SESSION_X11
  bool display()
  {
    if (!m_display && QGuiApplication::platformName() == "xcb")
      m_display = XOpenDisplay(nullptr);
    return m_display || fail(SessionPlatform::tr("Physical input sharing requires an X11 desktop."));
  }
  void readInput()
  {
    for (int count = 0; m_notifier && XPending(m_display) && count < 128; ++count) {
      XEvent event;
      XNextEvent(m_display, &event);
      if (event.type != GenericEvent || event.xcookie.extension != m_opcode ||
          !XGetEventData(m_display, &event.xcookie))
        continue;
      const auto *raw = static_cast<XIRawEvent *>(event.xcookie.data);
      if (!m_devices.contains(raw->sourceid)) {
        XFreeEventData(m_display, &event.xcookie);
        continue;
      }
      std::optional<Input> input;
      const auto type = event.xcookie.evtype;
      if (type == XI_RawKeyPress || type == XI_RawKeyRelease) {
        const auto code = set1FromEvdev(raw->detail - 8);
        if (code) {
          const bool down = type == XI_RawKeyPress;
          if (down)
            m_physicalKeys.insert(code);
          else
            m_physicalKeys.remove(code);
          input = Input{Input::Kind::Key, code, down};
          if (down && code == 1 && (m_physicalKeys.contains(0x1d) || m_physicalKeys.contains(0x11d)) &&
              (m_physicalKeys.contains(0x38) || m_physicalKeys.contains(0x138)))
            input->kind = Input::Kind::Escape;
        }
      } else if (type == XI_RawMotion) {
        Window root, child;
        int x, y, winX, winY;
        unsigned int mask;
        if (XQueryPointer(m_display, DefaultRootWindow(m_display), &root, &child, &x, &y, &winX, &winY, &mask))
          input = Input{
              Input::Kind::Move, 0, false,
              std::clamp(x * 65535 / std::max(1, DisplayWidth(m_display, DefaultScreen(m_display)) - 1), 0, 65535),
              std::clamp(y * 65535 / std::max(1, DisplayHeight(m_display, DefaultScreen(m_display)) - 1), 0, 65535)
          };
      } else if (raw->detail >= 4 && raw->detail <= 7) {
        if (type == XI_RawButtonPress)
          input = Input{
              Input::Kind::Wheel, 0, false,
              raw->detail == 6   ? -120
              : raw->detail == 7 ? 120
                                 : 0,
              raw->detail == 4   ? 120
              : raw->detail == 5 ? -120
                                 : 0
          };
      } else if (raw->detail >= 1 && raw->detail <= 9) {
        input = Input{Input::Kind::Button, raw->detail <= 3 ? raw->detail : raw->detail - 4, type == XI_RawButtonPress};
      }
      XFreeEventData(m_display, &event.xcookie);
      if (input)
        Q_EMIT inputCaptured(*input);
    }
    if (m_notifier && XPending(m_display))
      QTimer::singleShot(0, this, [this] { readInput(); });
  }
  Display *m_display = nullptr;
  int m_opcode = 0;
  InputMode m_mode = InputMode::Observe;
  std::unique_ptr<QSocketNotifier> m_notifier;
  QSet<int> m_devices, m_keys, m_buttons, m_physicalKeys;
#endif
};
} // namespace
std::unique_ptr<SessionPlatform> createSessionPlatform()
{
  return std::make_unique<LinuxSessionPlatform>();
}
} // namespace deskflow::gui::session
