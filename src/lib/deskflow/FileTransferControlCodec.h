/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileTransferTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace deskflow::filetransfer {

enum class FileTransferDecisionValue : std::uint8_t
{
  Accept = 1,
  Reject = 2,
};

enum class FileTransferCancelReason : std::uint8_t
{
  SenderCancelled = 1,
  ReceiverCancelled = 2,
  Timeout = 3,
  Disconnected = 4,
};

enum class FileTransferResultStatus : std::uint8_t
{
  Completed = 1,
  Failed = 2,
  Cancelled = 3,
};

enum class FileTransferResultCode : std::uint16_t
{
  None = 0,
  NetworkUnavailable = 1,
  AuthorizationFailed = 2,
  SourceChanged = 3,
  DestinationDenied = 4,
  DiskFull = 5,
  IntegrityMismatch = 6,
  ProtocolError = 7,
};

struct FileTransferDecision
{
  TransferId id{};
  std::string sourceScreen;
  std::string targetScreen;
  FileTransferDecisionValue decision = FileTransferDecisionValue::Reject;

  bool operator==(const FileTransferDecision &) const = default;
};

struct FileTransferCancel
{
  TransferId id{};
  std::string sourceScreen;
  std::string targetScreen;
  FileTransferCancelReason reason = FileTransferCancelReason::SenderCancelled;

  bool operator==(const FileTransferCancel &) const = default;
};

struct FileTransferResult
{
  TransferId id{};
  std::string sourceScreen;
  std::string targetScreen;
  FileTransferResultStatus status = FileTransferResultStatus::Failed;
  FileTransferResultCode code = FileTransferResultCode::None;

  bool operator==(const FileTransferResult &) const = default;
};

using FileTransferControlMessage =
    std::variant<FileTransferOffer, FileTransferDecision, FileTransferCancel, FileTransferResult>;

struct FileTransferControlLimits
{
  std::size_t maxPayloadBytes = 64 * 1024;
  FileTransferLimits offer;
};

enum class FileTransferCodecError
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
  TooManyItems,
  ValueTooLarge,
  InvalidEnum,
  PolicyRejected,
};

struct FileTransferControlEncodeResult
{
  FileTransferCodecError error = FileTransferCodecError::None;
  std::vector<std::uint8_t> bytes;
  OfferValidation validation;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferCodecError::None;
  }
};

struct FileTransferControlDecodeResult
{
  FileTransferCodecError error = FileTransferCodecError::None;
  std::optional<FileTransferControlMessage> message;
  OfferValidation validation;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferCodecError::None;
  }
};

[[nodiscard]] FileTransferControlEncodeResult
encodeFileTransferControl(const FileTransferControlMessage &message, const FileTransferControlLimits &limits = {});

[[nodiscard]] FileTransferControlDecodeResult
decodeFileTransferControl(std::span<const std::uint8_t> bytes, const FileTransferControlLimits &limits = {});

} // namespace deskflow::filetransfer
