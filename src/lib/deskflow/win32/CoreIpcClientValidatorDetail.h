/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QtTypes>

#include <optional>

class QLocalSocket;

namespace deskflow::core::ipc::detail {

[[nodiscard]] bool hasInteractiveSidMembership(quintptr primaryToken) noexcept;
[[nodiscard]] std::optional<quint32> namedPipeClientProcessId(const QLocalSocket &clientSocket) noexcept;

} // namespace deskflow::core::ipc::detail
