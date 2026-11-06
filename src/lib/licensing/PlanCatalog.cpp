/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "licensing/PlanCatalog.h"
#include "licensing/ProLicense.h"

namespace deskflow::licensing {

QString planId(Plan plan)
{
  switch (plan) {
  case Plan::Free:
    return QStringLiteral("free");
  case Plan::ProfessionalLocal:
    return QStringLiteral("professional-local");
  case Plan::Professional:
    return QStringLiteral("professional");
  case Plan::ProfessionalMax:
    return QStringLiteral("professional-max");
  }
  return {};
}

QString planName(Plan plan)
{
  switch (plan) {
  case Plan::Free:
    return QStringLiteral("Free");
  case Plan::ProfessionalLocal:
    return QStringLiteral("Professional Local");
  case Plan::Professional:
    return QStringLiteral("Professional");
  case Plan::ProfessionalMax:
    return QStringLiteral("Professional Max");
  }
  return {};
}

std::optional<Plan> parsePlan(QStringView id)
{
  for (const auto plan : {Plan::Free, Plan::ProfessionalLocal, Plan::Professional, Plan::ProfessionalMax}) {
    if (id == planId(plan))
      return plan;
  }
  return std::nullopt;
}

QStringList planFeatures(Plan plan)
{
  if (plan == Plan::Free)
    return {};
  QStringList result{QString::fromLatin1(kFileTransferFeature)};
  if (plan == Plan::ProfessionalLocal)
    return result;
  result.append(QString::fromLatin1(kAudioSharingFeature));
  if (plan == Plan::Professional)
    return result;
  if (plan != Plan::ProfessionalMax)
    return {};
  result.append(
      {QString::fromLatin1(kBidirectionalControlFeature), QString::fromLatin1(kRemoteViewingFeature),
       QString::fromLatin1(kImageClipboardFeature), QString::fromLatin1(kCoordinatedLockWakeFeature),
       QString::fromLatin1(kSimultaneousInputFeature)}
  );
  return result;
}

bool planPermits(Plan plan, QStringView feature)
{
  return planFeatures(plan).contains(feature.toString());
}

PlanPrice planPrice(Plan plan)
{
  switch (plan) {
  case Plan::ProfessionalLocal:
    return {199, 1499};
  case Plan::Professional:
    return {299, 2499};
  case Plan::ProfessionalMax:
    return {499, 3999};
  case Plan::Free:
    return {0, 0};
  }
  return {0, 0};
}

} // namespace deskflow::licensing
