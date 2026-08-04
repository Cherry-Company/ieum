/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Cherry Inc.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>

namespace deskflow::reconnect {

inline double retryDelay(uint32_t retryCount, bool dynamicRetry)
{
  // Keep startup and network-resume recovery fast. Once the peer has been
  // absent for a while, two seconds is still light enough for an idle client
  // but avoids the multi-minute dead period that made manual reconnect win.
  if (retryCount < 20) {
    return 0.25;
  }
  if (!dynamicRetry || retryCount < 80) {
    return 1.0;
  }
  return 2.0;
}

} // namespace deskflow::reconnect
