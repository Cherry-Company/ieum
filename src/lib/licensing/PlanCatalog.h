/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QStringView>

#include <optional>

namespace deskflow::licensing {

enum class Plan
{
  Free,
  ProfessionalLocal,
  Professional,
  ProfessionalMax
};

inline constexpr auto kAudioSharingFeature = "audio-sharing";
inline constexpr auto kBidirectionalControlFeature = "bidirectional-control";
inline constexpr auto kRemoteViewingFeature = "remote-viewing";
inline constexpr auto kImageClipboardFeature = "image-clipboard";
inline constexpr auto kCoordinatedLockWakeFeature = "coordinated-lock-wake";
inline constexpr auto kSimultaneousInputFeature = "simultaneous-input";

struct PlanPrice
{
  int monthlyUsdCents;
  int annualUsdCents;
};

[[nodiscard]] QString planId(Plan plan);
[[nodiscard]] QString planName(Plan plan);
[[nodiscard]] std::optional<Plan> parsePlan(QStringView id);
[[nodiscard]] QStringList planFeatures(Plan plan);
[[nodiscard]] bool planPermits(Plan plan, QStringView feature);
[[nodiscard]] PlanPrice planPrice(Plan plan);

} // namespace deskflow::licensing
