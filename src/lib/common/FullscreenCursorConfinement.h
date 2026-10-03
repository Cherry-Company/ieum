/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/FullscreenGeometry.h"

#include <optional>

namespace deskflow::fullscreen {

class CursorConfinement
{
public:
  bool owns(const Bounds &current) const
  {
    return m_applied && *m_applied == current;
  }

  bool owns(const Bounds &current, const Bounds &desktop) const
  {
    if (!m_applied) {
      return false;
    }
    // Windows intersects an existing clip with the new desktop on a resolution
    // change. It remains our restriction and must still be releasable.
    const Bounds visible{
        (std::max)(m_applied->left, desktop.left), (std::max)(m_applied->top, desktop.top),
        (std::min)(m_applied->right, desktop.right), (std::min)(m_applied->bottom, desktop.bottom)
    };
    return owns(current) || (visible.right > visible.left && visible.bottom > visible.top && visible == current);
  }

  template <typename Apply>
  bool update(std::optional<Bounds> target, Bounds current, const Bounds &desktop, Apply apply)
  {
    if (m_applied && !owns(current, desktop)) {
      // Another app changed the clip. Its newer request must survive our release.
      m_applied.reset();
      m_previous.reset();
    }

    if (m_applied && (!target || *target != *m_applied)) {
      if (!apply(m_previous)) {
        return false;
      }
      current = m_previous.value_or(desktop);
      m_applied.reset();
      m_previous.reset();
    }

    if (!target || owns(current, desktop)) {
      return true;
    }

    // Preserve the game's own restriction, including a one-pixel raw-input
    // capture. On a single-display desktop no extra restriction is needed.
    if (current.left >= target->left && current.top >= target->top && current.right <= target->right &&
        current.bottom <= target->bottom) {
      return true;
    }

    if (!apply(target)) {
      return false;
    }
    m_previous = current == desktop ? std::nullopt : std::optional{current};
    m_applied = target;
    return true;
  }

private:
  std::optional<Bounds> m_applied;
  std::optional<Bounds> m_previous;
};

} // namespace deskflow::fullscreen
