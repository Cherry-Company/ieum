/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

class QLocalSocket;

namespace deskflow::core::ipc {

class CoreIpcClientValidator final
{
public:
  [[nodiscard]] bool isAuthorized(const QLocalSocket &clientSocket) const noexcept;
};

} // namespace deskflow::core::ipc
