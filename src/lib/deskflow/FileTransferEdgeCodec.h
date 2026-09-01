/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"
#include "deskflow/FileTransferTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace deskflow::filetransfer {

enum class FileTransferEdgeStatus : std::uint8_t
{
  Resolved = 1,
  NoTarget = 2,
  Rejected = 3,
};

struct FileTransferEdgeCapabilities
{
  std::uint32_t activeSides = 0;
  bool operator==(const FileTransferEdgeCapabilities &) const = default;
};

struct FileTransferEdgeTargetRequest
{
  TransferId requestId{};
  std::string sourceScreen;
  Direction direction = Direction::NoDirection;
  std::int32_t x = 0;
  std::int32_t y = 0;
  bool operator==(const FileTransferEdgeTargetRequest &) const = default;
};

struct FileTransferEdgeTargetResponse
{
  TransferId requestId{};
  std::string sourceScreen;
  FileTransferEdgeStatus status = FileTransferEdgeStatus::Rejected;
  std::string targetScreen;
  bool operator==(const FileTransferEdgeTargetResponse &) const = default;
};

using FileTransferEdgeMessage =
    std::variant<FileTransferEdgeCapabilities, FileTransferEdgeTargetRequest, FileTransferEdgeTargetResponse>;

struct FileTransferEdgeLimits
{
  std::size_t maxPayloadBytes = 4096;
  std::size_t maxScreenNameBytes = 255;
};

enum class FileTransferEdgeCodecError
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
  InvalidEnum,
  InvalidSides,
  EmptyRequestId,
  InvalidScreen,
  InvalidTarget,
};

struct FileTransferEdgeEncodeResult
{
  FileTransferEdgeCodecError error = FileTransferEdgeCodecError::None;
  std::vector<std::uint8_t> bytes;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferEdgeCodecError::None;
  }
};

struct FileTransferEdgeDecodeResult
{
  FileTransferEdgeCodecError error = FileTransferEdgeCodecError::None;
  std::optional<FileTransferEdgeMessage> message;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferEdgeCodecError::None;
  }
};

[[nodiscard]] FileTransferEdgeEncodeResult
encodeFileTransferEdge(const FileTransferEdgeMessage &message, const FileTransferEdgeLimits &limits = {});

[[nodiscard]] FileTransferEdgeDecodeResult
decodeFileTransferEdge(std::span<const std::uint8_t> bytes, const FileTransferEdgeLimits &limits = {});

} // namespace deskflow::filetransfer
