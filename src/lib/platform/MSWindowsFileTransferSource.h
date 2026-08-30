/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferSourceManifest.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace deskflow::filetransfer {

enum class MSWindowsFileTransferSourceError
{
  None,
  EmptyPaths,
  TooManyPaths,
  EmptyPath,
  RelativePath,
  OpenFailed,
  MetadataReadFailed,
  ReparsePoint,
  NotRegularFile,
  FilenameConversionFailed,
};

struct MSWindowsFileTransferSourceResult
{
  MSWindowsFileTransferSourceError error = MSWindowsFileTransferSourceError::None;
  std::optional<std::size_t> itemPosition;
  std::vector<FileTransferSourceCandidate> sources;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == MSWindowsFileTransferSourceError::None;
  }
};

[[nodiscard]] MSWindowsFileTransferSourceResult
inspectMSWindowsFileTransferSources(const std::vector<std::filesystem::path> &paths, std::size_t maxItems = 100);

} // namespace deskflow::filetransfer
