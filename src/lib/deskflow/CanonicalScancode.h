/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/KeyTypes.h"

#include <cstdint>
#include <optional>

namespace deskflow::scancode {

std::optional<uint16_t> macVirtualKeyFromSet1(KeyButton scancode);
std::optional<KeyButton> set1FromMacVirtualKey(uint16_t virtualKey);

} // namespace deskflow::scancode
