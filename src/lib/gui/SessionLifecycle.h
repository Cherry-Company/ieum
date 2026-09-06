/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "Diagnostic.h"

#include <QAbstractNativeEventFilter>
#include <QGuiApplication>

#ifdef Q_OS_WIN
#include <QWindow>
#include <qt_windows.h>
#endif

namespace deskflow::gui {

class SessionLifecycle : public QAbstractNativeEventFilter
{
public:
  explicit SessionLifecycle(QGuiApplication &app) : m_app(app)
  {
    m_app.installNativeEventFilter(this);
#ifdef Q_OS_WIN
    // Background startup may never create the main widget's native window.
    // Qt's tray HWND has its own window procedure, bypassing native filters.
    // Keep a hidden top-level Qt HWND to receive broadcast session messages.
    m_shutdownWindow.create();
#endif
  }

  ~SessionLifecycle() override
  {
    m_app.removeNativeEventFilter(this);
  }

  bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override
  {
    Q_UNUSED(result)
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
      const auto *msg = static_cast<const MSG *>(message);
      // WM_QUERYENDSESSION can be cancelled by another application. Windows
      // can terminate us without another event-loop turn after WM_ENDSESSION.
      if (msg->message == WM_ENDSESSION && msg->wParam != FALSE) {
        diagnostic::completeSession();
      }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
#endif
    return false;
  }

private:
  QGuiApplication &m_app;
#ifdef Q_OS_WIN
  QWindow m_shutdownWindow;
#endif
};

} // namespace deskflow::gui
