/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Event.h"

#include <cstdint>
#include <string>
#include <utility>

namespace deskflow {

enum class InputLanguageAction : int8_t
{
  Toggle = 0,
  Set = 1,
  Query = 2
};

enum class InputLanguageCategory : int8_t
{
  KeyLayout = 0,
  InputMethod = 1,
  Unknown = 2
};

struct InputLanguageStatus : public EventData
{
  InputLanguageStatus() = default;
  InputLanguageStatus(std::string sourceId, InputLanguageCategory category, bool composing)
      : m_sourceId(std::move(sourceId)),
        m_category(category),
        m_composing(composing)
  {
  }

  std::string m_sourceId;
  InputLanguageCategory m_category = InputLanguageCategory::Unknown;
  bool m_composing = false;

  bool isInputMethod() const
  {
    return m_category == InputLanguageCategory::InputMethod;
  }

  bool operator==(const InputLanguageStatus &other) const
  {
    return m_sourceId == other.m_sourceId && m_category == other.m_category && m_composing == other.m_composing;
  }
};

} // namespace deskflow
