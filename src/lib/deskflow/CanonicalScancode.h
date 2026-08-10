/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/KeyTypes.h"

#include <cstdint>
#include <optional>

namespace deskflow::scancode {

//! Marks a wire KeyButton as a canonical PC Set-1 scancode.
/*!
Canonical scancodes occupy the low 9 bits, the same range \c KeyState masks a
server key handle down to, so this bit is free on every platform. A primary sets
it only when it can actually produce Set-1 codes, which makes the raw-scancode
path opt-in per key: a secondary that sees an unmarked button falls back to the
translated key path instead of reading a foreign platform key button as Set-1.
The primary sends the flag only to peers that negotiated protocol 1.10, so
older peers never see it and never take the flagged raw path.
*/
inline constexpr KeyButton kCanonicalFlag = 0x8000;

constexpr bool isCanonical(KeyButton button)
{
  return (button & kCanonicalFlag) != 0;
}

constexpr KeyButton markCanonical(KeyButton scancode)
{
  return static_cast<KeyButton>(scancode | kCanonicalFlag);
}

constexpr KeyButton stripCanonical(KeyButton button)
{
  return static_cast<KeyButton>(button & ~kCanonicalFlag);
}

std::optional<uint16_t> macVirtualKeyFromSet1(KeyButton scancode);
std::optional<KeyButton> set1FromMacVirtualKey(uint16_t virtualKey);

//! True when \p scancode is a well-formed canonical Set-1 code.
bool isSet1Scancode(KeyButton scancode);

} // namespace deskflow::scancode
