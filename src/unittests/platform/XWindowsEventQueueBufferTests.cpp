/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QElapsedTimer>
#include <QTest>

#include "arch/Arch.h"
#include "base/EventQueue.h"
#include "base/Log.h"
#include "platform/XWindowsEventQueueBuffer.h"

#include <sys/resource.h>
#include <thread>

class XWindowsEventQueueBufferTests final : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase()
  {
    m_arch.init();
    XInitThreads();
    m_display = XOpenDisplay(nullptr);
    QVERIFY2(m_display != nullptr, "Run on an owned X display using xvfb-run.");
    m_window = XCreateSimpleWindow(m_display, DefaultRootWindow(m_display), 0, 0, 1, 1, 0, 0, 0);
    XSync(m_display, False);
  }

  void earlyReturnDoesNotLeaveTheBufferInWaitingState()
  {
    EventQueue events;
    XWindowsEventQueueBuffer buffer(m_display, m_window, &events);
    XEvent pending{};
    pending.type = ClientMessage;
    pending.xclient.window = m_window;
    XPutBackEvent(m_display, &pending);
    buffer.waitForEvent(1.0);
    XEvent discarded{};
    XNextEvent(m_display, &discarded);
    buffer.addEvent(73);
    XSync(m_display, False);
    // A non-waiting buffer retains posted events until its next flush. A
    // stale waiting flag would send this event from the producer immediately.
    QCOMPARE(XPending(m_display), 0);
  }

  void init()
  {
    XSync(m_display, False);
    while (XPending(m_display) > 0) {
      XEvent discarded{};
      XNextEvent(m_display, &discarded);
    }
  }

  void postedEventWakesImmediatelyWithoutWaitingForFallbackPoll()
  {
    EventQueue events;
    XWindowsEventQueueBuffer buffer(m_display, m_window, &events);
    std::jthread sender([&buffer] {
      std::this_thread::sleep_for(std::chrono::milliseconds(40));
      buffer.addEvent(42);
    });
    QElapsedTimer timer;
    timer.start();
    buffer.waitForEvent(2.0);
    const auto elapsed = timer.elapsed();
    sender.join();
    qInfo("Posted event woke the buffer in %lld ms, including the 40 ms sender delay", static_cast<long long>(elapsed));
    QVERIFY2(elapsed < 180, qPrintable(QStringLiteral("Event delivery took %1 ms").arg(elapsed)));
    Event event;
    uint32_t id = 0;
    QCOMPARE(buffer.getEvent(event, id), IEventQueueBuffer::Type::User);
    QCOMPARE(id, 42U);
  }

  void idleWaitDoesNotPollFortyTimesPerSecond()
  {
    EventQueue events;
    XWindowsEventQueueBuffer buffer(m_display, m_window, &events);
    XSync(m_display, False);
    rusage before{}, after{};
    getrusage(RUSAGE_THREAD, &before);
    QElapsedTimer timer;
    timer.start();
    buffer.waitForEvent(0.5);
    getrusage(RUSAGE_THREAD, &after);
    const auto switches = after.ru_nvcsw - before.ru_nvcsw;
    QVERIFY(timer.elapsed() >= 450);
    QVERIFY2(
        switches <= 10, qPrintable(QStringLiteral("Idle wait caused %1 voluntary context switches").arg(switches))
    );
    qInfo("500 ms idle wait: %ld voluntary context switches", switches);
  }

  void cleanupTestCase()
  {
    if (m_display != nullptr) {
      XDestroyWindow(m_display, m_window);
      XCloseDisplay(m_display);
    }
  }

private:
  Arch m_arch;
  Log m_log;
  Display *m_display = nullptr;
  Window m_window = 0;
};

QTEST_GUILESS_MAIN(XWindowsEventQueueBufferTests)
#include "XWindowsEventQueueBufferTests.moc"
