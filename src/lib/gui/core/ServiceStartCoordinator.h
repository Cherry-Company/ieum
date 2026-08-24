/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QTimer>

namespace deskflow::gui {

class ServiceStartCoordinator : public QObject
{
  Q_OBJECT

public:
  explicit ServiceStartCoordinator(QObject *parent, int timeoutMs);

  void begin(quint64 generation);
  void daemonConnected(quint64 generation);
  void configResult(quint64 generation, bool success);
  void startResult(quint64 generation, bool success);
  void coreConnected(quint64 generation);
  void coreConnectionFailed(quint64 generation);
  void transportFailed(quint64 generation);
  void cancel(quint64 generation);

  bool isActive() const;

Q_SIGNALS:
  void configRequested(quint64 generation);
  void startRequested(quint64 generation);
  void coreConnectionRequested(quint64 generation);
  void succeeded(quint64 generation);
  void failed(quint64 generation);

private:
  enum class Stage
  {
    Idle,
    WaitingForDaemon,
    WaitingForConfig,
    WaitingForStart,
    WaitingForCore,
  };

  bool accepts(quint64 generation, Stage stage) const;
  void finishFailure();

  Stage m_stage = Stage::Idle;
  quint64 m_generation = 0;
  QTimer m_timeout;
};

} // namespace deskflow::gui
