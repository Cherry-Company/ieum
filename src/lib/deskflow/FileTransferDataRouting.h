/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferControlRouting.h"
#include "deskflow/FileTransferDataCodec.h"

#include <string_view>

namespace deskflow::filetransfer {

[[nodiscard]] FileTransferRouteResult resolveFileTransferDataDestination(
    const FileTransferDataMessage &message, std::string_view senderScreen, const FileTransferDataLimits &limits = {}
);

} // namespace deskflow::filetransfer
