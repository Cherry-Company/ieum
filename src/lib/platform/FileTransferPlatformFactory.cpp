/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/FileTransferPlatformFactory.h"

#if defined(_WIN32)
#include "platform/MSWindowsFileTransferPlatform.h"
#elif defined(__APPLE__)
#include "platform/OSXFileTransferPlatform.h"
#endif

namespace deskflow::filetransfer {

std::unique_ptr<IFileTransferPlatform> createFileTransferPlatform()
{
#if defined(_WIN32)
  return std::make_unique<MSWindowsFileTransferPlatform>();
#elif defined(__APPLE__)
  return std::make_unique<OSXFileTransferPlatform>();
#else
  return nullptr;
#endif
}

std::filesystem::path fileTransferPathFromQString(const QString &value)
{
#if defined(_WIN32)
  return std::filesystem::path(value.toStdWString());
#elif defined(__APPLE__)
  const auto normalized = value.normalized(QString::NormalizationForm_C).toUtf8();
  return std::filesystem::path(std::string(normalized.constData(), static_cast<std::size_t>(normalized.size())));
#else
  return std::filesystem::path(value.toStdString());
#endif
}

bool supportsFileTransferPlatform() noexcept
{
#if defined(_WIN32) || defined(__APPLE__)
  return true;
#else
  return false;
#endif
}

} // namespace deskflow::filetransfer
