/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace deskflow::ipc {

struct FileTransferEdgeDropIpcValue
{
  Direction direction = Direction::NoDirection;
  std::int32_t x = 0;
  std::int32_t y = 0;
  QStringList paths;

  bool operator==(const FileTransferEdgeDropIpcValue &) const = default;
};

enum class FileTransferEdgeDropIpcError : std::uint8_t
{
  None,
  InvalidBase64,
  InvalidJson,
  MissingField,
  UnknownField,
  InvalidFieldType,
  InvalidVersion,
  InvalidDirection,
  InvalidInteger,
  InvalidPathType,
  EmptyPath,
  RelativePath,
  TooManyPaths,
  EncodedValueTooLarge,
};

struct FileTransferEdgeDropIpcEncodeResult
{
  FileTransferEdgeDropIpcError error = FileTransferEdgeDropIpcError::None;
  QString encodedValue;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferEdgeDropIpcError::None;
  }
};

struct FileTransferEdgeDropIpcDecodeResult
{
  FileTransferEdgeDropIpcError error = FileTransferEdgeDropIpcError::None;
  std::optional<FileTransferEdgeDropIpcValue> value;

  [[nodiscard]] bool ok() const noexcept
  {
    return error == FileTransferEdgeDropIpcError::None;
  }
};

[[nodiscard]] FileTransferEdgeDropIpcEncodeResult
encodeFileTransferEdgeDropIpc(const FileTransferEdgeDropIpcValue &value);

[[nodiscard]] FileTransferEdgeDropIpcDecodeResult decodeFileTransferEdgeDropIpc(const QString &encodedValue);

} // namespace deskflow::ipc
