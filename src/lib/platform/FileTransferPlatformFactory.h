/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferPlatform.h"

#include <QString>

#include <filesystem>
#include <memory>

namespace deskflow::filetransfer {

[[nodiscard]] std::unique_ptr<IFileTransferPlatform> createFileTransferPlatform();
[[nodiscard]] std::filesystem::path fileTransferPathFromQString(const QString &value);
[[nodiscard]] bool supportsFileTransferPlatform() noexcept;

} // namespace deskflow::filetransfer
