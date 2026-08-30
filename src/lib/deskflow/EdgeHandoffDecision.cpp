/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/EdgeHandoffDecision.h"

#include <algorithm>
#include <cstdint>

namespace deskflow::filetransfer {

EdgeHandoffDecision::EdgeHandoffDecision(const std::chrono::milliseconds dwell)
    : m_dwell(std::max(dwell, std::chrono::milliseconds::zero()))
{
}

EdgeHandoffState EdgeHandoffDecision::observe(const std::optional<EdgeTarget> &target, const TimePoint now)
{
  if (!target.has_value() || !isValid(*target)) {
    cancel();
    return m_state;
  }

  if (!m_target.has_value() || *m_target != *target || (m_hasObservation && now < m_lastObserved)) {
    beginCandidate(*target, now);
    return m_state;
  }

  m_lastObserved = now;
  if (m_state == EdgeHandoffState::Candidate && now - m_candidateSince >= m_dwell) {
    m_state = EdgeHandoffState::Armed;
  }

  return m_state;
}

std::optional<EdgeTarget> EdgeHandoffDecision::release()
{
  auto released = m_state == EdgeHandoffState::Armed ? m_target : std::nullopt;
  cancel();
  return released;
}

void EdgeHandoffDecision::cancel() noexcept
{
  m_state = EdgeHandoffState::Idle;
  m_target.reset();
  m_candidateSince = {};
  m_lastObserved = {};
  m_hasObservation = false;
}

EdgeHandoffState EdgeHandoffDecision::state() const noexcept
{
  return m_state;
}

const std::optional<EdgeTarget> &EdgeHandoffDecision::target() const noexcept
{
  return m_target;
}

bool EdgeHandoffDecision::isValid(const EdgeTarget &target) noexcept
{
  const auto direction = static_cast<std::uint8_t>(target.direction);
  return !target.screen.empty() && direction >= static_cast<std::uint8_t>(Direction::Left) &&
         direction <= static_cast<std::uint8_t>(Direction::Bottom);
}

void EdgeHandoffDecision::beginCandidate(const EdgeTarget &target, const TimePoint now)
{
  m_target = target;
  m_candidateSince = now;
  m_lastObserved = now;
  m_hasObservation = true;
  m_state = m_dwell == std::chrono::milliseconds::zero() ? EdgeHandoffState::Armed : EdgeHandoffState::Candidate;
}

} // namespace deskflow::filetransfer
