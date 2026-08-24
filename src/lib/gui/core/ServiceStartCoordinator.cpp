/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServiceStartCoordinator.h"

namespace deskflow::gui {

ServiceStartCoordinator::ServiceStartCoordinator(QObject *parent, const int timeoutMs) : QObject(parent)
{
  m_timeout.setInterval(timeoutMs);
  m_timeout.setSingleShot(true);
  connect(&m_timeout, &QTimer::timeout, this, [this] {
    if (isActive()) {
      finishFailure();
    }
  });
}

void ServiceStartCoordinator::begin(const quint64 generation)
{
  m_timeout.stop();
  m_generation = generation;
  m_stage = Stage::WaitingForDaemon;
  m_timeout.start();
}

void ServiceStartCoordinator::daemonConnected(const quint64 generation)
{
  if (!accepts(generation, Stage::WaitingForDaemon)) {
    return;
  }

  m_stage = Stage::WaitingForConfig;
  Q_EMIT configRequested(generation);
}

void ServiceStartCoordinator::configResult(const quint64 generation, const bool success)
{
  if (!accepts(generation, Stage::WaitingForConfig)) {
    return;
  }

  if (!success) {
    finishFailure();
    return;
  }

  m_stage = Stage::WaitingForStart;
  Q_EMIT startRequested(generation);
}

void ServiceStartCoordinator::startResult(const quint64 generation, const bool success)
{
  if (!accepts(generation, Stage::WaitingForStart)) {
    return;
  }

  if (!success) {
    finishFailure();
    return;
  }

  m_stage = Stage::WaitingForCore;
  Q_EMIT coreConnectionRequested(generation);
}

void ServiceStartCoordinator::coreConnected(const quint64 generation)
{
  if (!accepts(generation, Stage::WaitingForCore)) {
    return;
  }

  m_timeout.stop();
  m_stage = Stage::Idle;
  Q_EMIT succeeded(generation);
}

void ServiceStartCoordinator::coreConnectionFailed(const quint64 generation)
{
  if (accepts(generation, Stage::WaitingForCore)) {
    finishFailure();
  }
}

void ServiceStartCoordinator::transportFailed(const quint64 generation)
{
  if (generation == m_generation && isActive()) {
    finishFailure();
  }
}

void ServiceStartCoordinator::cancel(const quint64 generation)
{
  if (generation != m_generation || !isActive()) {
    return;
  }

  m_timeout.stop();
  m_stage = Stage::Idle;
}

bool ServiceStartCoordinator::isActive() const
{
  return m_stage != Stage::Idle;
}

bool ServiceStartCoordinator::accepts(const quint64 generation, const Stage stage) const
{
  return generation == m_generation && m_stage == stage;
}

void ServiceStartCoordinator::finishFailure()
{
  const auto generation = m_generation;
  m_timeout.stop();
  m_stage = Stage::Idle;
  Q_EMIT failed(generation);
}

} // namespace deskflow::gui
