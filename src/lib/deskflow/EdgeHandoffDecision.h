/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"

#include <chrono>
#include <optional>
#include <string>

namespace deskflow::filetransfer {

struct EdgeTarget
{
  Direction direction = Direction::NoDirection;
  std::string screen;

  bool operator==(const EdgeTarget &) const = default;
};

enum class EdgeHandoffState
{
  Idle,
  Candidate,
  Armed,
};

class EdgeHandoffDecision
{
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  explicit EdgeHandoffDecision(std::chrono::milliseconds dwell = std::chrono::milliseconds{250});

  [[nodiscard]] EdgeHandoffState observe(const std::optional<EdgeTarget> &target, TimePoint now);
  [[nodiscard]] std::optional<EdgeTarget> release();
  void cancel() noexcept;

  [[nodiscard]] EdgeHandoffState state() const noexcept;
  [[nodiscard]] const std::optional<EdgeTarget> &target() const noexcept;

private:
  [[nodiscard]] static bool isValid(const EdgeTarget &target) noexcept;
  void beginCandidate(const EdgeTarget &target, TimePoint now);

  std::chrono::milliseconds m_dwell;
  EdgeHandoffState m_state = EdgeHandoffState::Idle;
  std::optional<EdgeTarget> m_target;
  TimePoint m_candidateSince{};
  TimePoint m_lastObserved{};
  bool m_hasObservation = false;
};

} // namespace deskflow::filetransfer
