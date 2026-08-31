/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/FileTransferPlatformFactory.h"

#if defined(_WIN32)
#include "platform/MSWindowsFileTransferPlatform.h"
#endif

namespace deskflow::filetransfer {

std::unique_ptr<IFileTransferPlatform> createFileTransferPlatform()
{
#if defined(_WIN32)
  return std::make_unique<MSWindowsFileTransferPlatform>();
#else
  return nullptr;
#endif
}

std::filesystem::path fileTransferPathFromQString(const QString &value)
{
#if defined(_WIN32)
  return std::filesystem::path(value.toStdWString());
#else
  return std::filesystem::path(value.toStdString());
#endif
}

bool supportsFileTransferPlatform() noexcept
{
#if defined(_WIN32)
  return true;
#else
  return false;
#endif
}

} // namespace deskflow::filetransfer
