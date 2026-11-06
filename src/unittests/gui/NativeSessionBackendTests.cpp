// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "gui/session/SessionPlatform.h"
#include <QGuiApplication>
#include <QLineEdit>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <cmath>
#ifdef IEUM_TEST_X11
#include <X11/Xlib.h>
#endif
using namespace deskflow::gui::session;
class NativeSessionBackendTests final : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void nativeInputReachesApplicationsReleasesKeysAndDoesNotLoop()
  {
#ifdef IEUM_TEST_X11
    if (qEnvironmentVariable("IEUM_TEST_VIRTUAL_DESKTOP") != "1" || QGuiApplication::platformName() != "xcb")
      QSKIP("Requires an isolated Xvfb display with IEUM_TEST_VIRTUAL_DESKTOP=1");
    QLineEdit edit;
    edit.resize(480, 80);
    edit.show();
    QVERIFY(QTest::qWaitForWindowExposed(&edit));
    auto *display = XOpenDisplay(nullptr);
    QVERIFY(display);
    XSetInputFocus(display, edit.winId(), RevertToParent, CurrentTime);
    XSync(display, False);
    auto platform = createSessionPlatform();
    QSignalSpy captured(platform.get(), &SessionPlatform::inputCaptured);
    QVERIFY2(platform->startInputCapture(SessionPlatform::InputMode::Observe), qPrintable(platform->lastError()));
    QVERIFY(platform->inject({Input::Kind::Key, 0x1e, true}));
    QTRY_COMPARE(edit.text(), QString("a"));
    char keys[32]{};
    XQueryKeymap(display, keys);
    QVERIFY(keys[38 / 8] & (1 << (38 % 8)));
    platform->releaseInput();
    XSync(display, False);
    QTRY_VERIFY([&] {
      XQueryKeymap(display, keys);
      return !(keys[38 / 8] & (1 << (38 % 8)));
    }());
    QVERIFY(platform->inject({Input::Kind::Move, 0, false, 32768, 32768}));
    QVERIFY(platform->inject({Input::Kind::Button, 1, true}));
    Window root, child;
    int x, y, wx, wy;
    unsigned int mask = 0;
    QTRY_VERIFY([&] {
      XQueryPointer(display, DefaultRootWindow(display), &root, &child, &x, &y, &wx, &wy, &mask);
      return mask & Button1Mask;
    }());
    platform->releaseInput();
    QTRY_VERIFY([&] {
      XQueryPointer(display, DefaultRootWindow(display), &root, &child, &x, &y, &wx, &wy, &mask);
      return !(mask & Button1Mask);
    }());
    QTest::qWait(50);
    QCOMPARE(captured.size(), 0);
    platform->stopInputCapture();
    XCloseDisplay(display);
#else
    QSKIP("X11 native fixture is not available on this platform");
#endif
  }
  void systemAudioCapturesVirtualSinkAndPlaysBoundedPcm()
  {
#ifdef Q_OS_LINUX
    if (qEnvironmentVariable("IEUM_TEST_VIRTUAL_AUDIO") != "1")
      QSKIP("Requires an isolated PulseAudio null sink");
    auto platform = createSessionPlatform();
    QSignalSpy captured(platform.get(), &SessionPlatform::audioCaptured);
    QVERIFY2(platform->startAudioCapture(), qPrintable(platform->lastError()));
    QByteArray tone(48000 * 4, Qt::Uninitialized);
    auto *samples = reinterpret_cast<qint16 *>(tone.data());
    for (int i = 0; i < 48000; ++i)
      samples[2 * i] = samples[2 * i + 1] = static_cast<qint16>(std::sin(i * 440.0 * 6.28318530718 / 48000) * 8000);
    QTemporaryFile audio;
    QVERIFY(audio.open());
    QCOMPARE(audio.write(tone), tone.size());
    audio.flush();
    QProcess player;
    player.start("paplay", {"--raw", "--format=s16le", "--rate=48000", "--channels=2", audio.fileName()});
    QVERIFY(player.waitForStarted());
    QTRY_VERIFY_WITH_TIMEOUT(
        [&] {
          for (const auto &event : captured) {
            const auto pcm = event.first().toByteArray();
            for (char byte : pcm)
              if (byte)
                return true;
          }
          return false;
        }(),
        5000
    );
    platform->stopAudioCapture();
    const auto count = captured.size();
    QTest::qWait(100);
    QCOMPARE(captured.size(), count);
    for (const auto &event : captured) {
      const auto pcm = event.first().toByteArray();
      QVERIFY(pcm.size() <= 3840);
      QCOMPARE(pcm.size() % 4, 0);
    }
    if (player.state() != QProcess::NotRunning) {
      player.kill();
      player.waitForFinished();
    }
    QTest::qWait(150);
    captured.clear();
    QVERIFY(platform->startAudioCapture());
    for (int i = 0; i < 4; ++i)
      QVERIFY2(platform->playAudio(tone.left(3840)), qPrintable(platform->lastError()));
    QTRY_VERIFY_WITH_TIMEOUT(
        [&] {
          for (const auto &event : captured)
            for (char sample : event.first().toByteArray())
              if (sample)
                return true;
          return false;
        }(),
        5000
    );
    platform->stopAudioPlayback();
    platform->stopAudioCapture();
#else
    QSKIP("PulseAudio native fixture is Linux only");
#endif
  }
};
QTEST_MAIN(NativeSessionBackendTests)
#include "NativeSessionBackendTests.moc"
