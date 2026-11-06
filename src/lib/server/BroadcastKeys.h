/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once
#include "deskflow/KeyTypes.h"
#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace deskflow::server {
// Release follows the original press, even after a plan or target changes.
class BroadcastKeys
{
public:
  struct Press
  {
    KeyID key;
    KeyModifierMask mask;
    std::vector<std::string> recipients;
  };
  bool remember(KeyButton button, KeyID key, KeyModifierMask mask, std::vector<std::string> recipients)
  {
    if (recipients.empty())
      return false;
    return m_pressed.try_emplace(button, Press{key, mask, std::move(recipients)}).second;
  }
  const Press *find(KeyButton button) const
  {
    const auto it = m_pressed.find(button);
    return it == m_pressed.end() ? nullptr : &it->second;
  }
  std::optional<Press> take(KeyButton button)
  {
    auto node = m_pressed.extract(button);
    if (node.empty())
      return std::nullopt;
    return std::move(node.mapped());
  }
  auto takeAll()
  {
    return std::exchange(m_pressed, {});
  }
  auto releaseAll()
  {
    auto pressed = m_pressed;
    for (auto &[button, press] : m_pressed)
      press.recipients.clear();
    // Consume the eventual physical key-up and repeats without forwarding
    // them to a newly selected screen after a toggle or entitlement expiry.
    return pressed;
  }
  void forgetPeer(const std::string &name)
  {
    for (auto &[button, press] : m_pressed)
      std::erase(press.recipients, name);
    // Keep empty records until key-up; reconnecting the same name must not
    // inherit a key pressed in its previous connection.
  }

private:
  std::map<KeyButton, Press> m_pressed;
};
} // namespace deskflow::server
