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
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace deskflow::filetransfer {

using FileTransferDigest = std::array<std::uint8_t, 32>;

struct FileTransferDataRoute
{
  TransferId id{};
  std::string sourceScreen;
  std::string targetScreen;

  bool operator==(const FileTransferDataRoute &) const = default;
};

struct FileTransferDataBegin
{
  FileTransferDataRoute route;

  bool operator==(const FileTransferDataBegin &) const = default;
};

struct FileTransferDataChunk
{
  FileTransferDataRoute route;
  std::uint32_t itemIndex = 0;
  std::uint64_t offset = 0;
  std::vector<std::uint8_t> bytes;

  bool operator==(const FileTransferDataChunk &) const = default;
};

struct FileTransferDataItemEnd
{
  FileTransferDataRoute route;
  std::uint32_t itemIndex = 0;
  std::uint64_t size = 0;
  FileTransferDigest sha256{};

  bool operator==(const FileTransferDataItemEnd &) const = default;
};

struct FileTransferDataFinish
{
  FileTransferDataRoute route;
  std::uint32_t itemCount = 0;
  std::uint64_t totalBytes = 0;

  bool operator==(const FileTransferDataFinish &) const = default;
};

using FileTransferDataMessage =
    std::variant<FileTransferDataBegin, FileTransferDataChunk, FileTransferDataItemEnd, FileTransferDataFinish>;

struct FileTransferDataLimits
{
  std::size_t maxPayloadBytes = 64 * 1024;
  std::size_t maxChunkBytes = 60 * 1024;
  FileTransferLimits route;
};

enum class FileTransferDataCodecError
{
  None,
  MessageTooLarge,
  InvalidMagic,
  UnsupportedVersion,
  InvalidKind,
  InvalidFlags,
  LengthMismatch,
  Truncated,
  TrailingData,
  StringTooLong,
  ValueTooLarge,
  EmptyChunk,
  ChunkTooLarge,
  InvalidValue,
  PolicyRejected,
};

struct FileTransferDataEncodeResult
{
  FileTransferDataCodecError error = FileTransferDataCodecError::None;
  std::vector<std::uint8_t> bytes;
  OfferValidation validation;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferDataCodecError::None;
  }
};

struct FileTransferDataDecodeResult
{
  FileTransferDataCodecError error = FileTransferDataCodecError::None;
  std::optional<FileTransferDataMessage> message;
  OfferValidation validation;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferDataCodecError::None;
  }
};

[[nodiscard]] FileTransferDataEncodeResult
encodeFileTransferData(const FileTransferDataMessage &message, const FileTransferDataLimits &limits = {});

[[nodiscard]] FileTransferDataDecodeResult
decodeFileTransferData(std::span<const std::uint8_t> bytes, const FileTransferDataLimits &limits = {});

} // namespace deskflow::filetransfer
