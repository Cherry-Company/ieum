/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace deskflow::filetransfer {

struct FileTransferSourceIdentity
{
  std::array<std::uint8_t, 32> bytes{};
  std::size_t length = 0;

  bool operator==(const FileTransferSourceIdentity &) const = default;
};

struct FileTransferSourceCandidate
{
  std::filesystem::path path;
  FileTransferSourceIdentity identity;
  std::uint64_t size = 0;
  std::uint64_t lastWriteTime = 0;
  std::string name;

  bool operator==(const FileTransferSourceCandidate &) const = default;
};

struct FileTransferSourceManifest
{
  FileTransferOffer offer;
  std::vector<FileTransferSourceCandidate> sources;
};

enum class SourceManifestError
{
  None,
  EmptySourcePath,
  RelativeSourcePath,
  MissingFileIdentity,
  InvalidFileIdentity,
  InvalidOffer,
};

struct FileTransferSourceManifestResult
{
  SourceManifestError error = SourceManifestError::None;
  std::optional<std::size_t> candidatePosition;
  OfferValidation validation;
  FileTransferSourceManifest manifest;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == SourceManifestError::None;
  }
};

[[nodiscard]] FileTransferSourceManifestResult buildFileTransferSourceManifest(
    TransferId id, std::string sourceScreen, std::string targetScreen,
    std::vector<FileTransferSourceCandidate> candidates, const FileTransferLimits &limits = {}
);

} // namespace deskflow::filetransfer
