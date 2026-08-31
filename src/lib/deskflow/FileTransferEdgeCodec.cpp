/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferEdgeCodec.h"

#include "base/Unicode.h"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

namespace deskflow::filetransfer {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic{'I', 'F', 'E', '1'};
constexpr std::uint8_t kWireVersion = 1;
constexpr std::size_t kHeaderSize = 12;

enum class MessageKind : std::uint8_t
{
  Capabilities = 1,
  TargetRequest = 2,
  TargetResponse = 3,
};

class Writer final
{
public:
  explicit Writer(std::size_t limit) : m_limit(limit)
  {
  }

  bool byte(std::uint8_t value)
  {
    if (!reserve(1)) {
      return false;
    }
    m_bytes.push_back(value);
    return true;
  }

  bool uint16(std::uint16_t value)
  {
    return byte(static_cast<std::uint8_t>(value >> 8U)) && byte(static_cast<std::uint8_t>(value));
  }

  bool uint32(std::uint32_t value)
  {
    return byte(static_cast<std::uint8_t>(value >> 24U)) && byte(static_cast<std::uint8_t>(value >> 16U)) &&
           byte(static_cast<std::uint8_t>(value >> 8U)) && byte(static_cast<std::uint8_t>(value));
  }

  bool data(std::span<const std::uint8_t> value)
  {
    if (!reserve(value.size())) {
      return false;
    }
    m_bytes.insert(m_bytes.end(), value.begin(), value.end());
    return true;
  }

  bool string(const std::string &value)
  {
    return uint16(static_cast<std::uint16_t>(value.size())) &&
           data(std::span{reinterpret_cast<const std::uint8_t *>(value.data()), value.size()});
  }

  [[nodiscard]] const std::vector<std::uint8_t> &bytes() const noexcept
  {
    return m_bytes;
  }

  [[nodiscard]] std::vector<std::uint8_t> take()
  {
    return std::move(m_bytes);
  }

private:
  bool reserve(std::size_t count) const noexcept
  {
    return count <= m_limit - (std::min)(m_limit, m_bytes.size());
  }

  std::size_t m_limit;
  std::vector<std::uint8_t> m_bytes;
};

class Reader final
{
public:
  explicit Reader(std::span<const std::uint8_t> bytes) : m_bytes(bytes)
  {
  }

  bool byte(std::uint8_t &value)
  {
    if (remaining() < 1) {
      return false;
    }
    value = m_bytes[m_position++];
    return true;
  }

  bool uint16(std::uint16_t &value)
  {
    std::uint8_t high = 0;
    std::uint8_t low = 0;
    if (!byte(high) || !byte(low)) {
      return false;
    }
    value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(high) << 8U) | low);
    return true;
  }

  bool uint32(std::uint32_t &value)
  {
    std::uint16_t high = 0;
    std::uint16_t low = 0;
    if (!uint16(high) || !uint16(low)) {
      return false;
    }
    value = (static_cast<std::uint32_t>(high) << 16U) | low;
    return true;
  }

  bool data(std::size_t count, std::span<const std::uint8_t> &value)
  {
    if (remaining() < count) {
      return false;
    }
    value = m_bytes.subspan(m_position, count);
    m_position += count;
    return true;
  }

  FileTransferEdgeCodecError string(std::size_t limit, std::string &value)
  {
    std::uint16_t length = 0;
    if (!uint16(length)) {
      return FileTransferEdgeCodecError::Truncated;
    }
    if (length > limit) {
      return FileTransferEdgeCodecError::StringTooLong;
    }
    std::span<const std::uint8_t> bytes;
    if (!data(length, bytes)) {
      return FileTransferEdgeCodecError::Truncated;
    }
    value.assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    return FileTransferEdgeCodecError::None;
  }

  [[nodiscard]] std::size_t remaining() const noexcept
  {
    return m_bytes.size() - m_position;
  }

private:
  std::span<const std::uint8_t> m_bytes;
  std::size_t m_position = 0;
};

FileTransferEdgeEncodeResult encodeFailure(FileTransferEdgeCodecError error)
{
  return {.error = error};
}

FileTransferEdgeDecodeResult decodeFailure(FileTransferEdgeCodecError error)
{
  return {.error = error, .message = std::nullopt};
}

std::uint32_t permittedSides()
{
  return static_cast<std::uint32_t>(DirectionMask::LeftMask) | static_cast<std::uint32_t>(DirectionMask::RightMask) |
         static_cast<std::uint32_t>(DirectionMask::TopMask) | static_cast<std::uint32_t>(DirectionMask::BottomMask);
}

bool isEmptyId(const TransferId &id)
{
  return std::ranges::all_of(id, [](std::uint8_t value) { return value == 0; });
}

bool validDirection(Direction direction)
{
  return direction >= Direction::FirstDirection && direction <= Direction::LastDirection;
}

bool validStatus(FileTransferEdgeStatus status)
{
  return status >= FileTransferEdgeStatus::Resolved && status <= FileTransferEdgeStatus::Rejected;
}

bool hasUnsafeScreenByte(std::string_view screen)
{
  return std::ranges::any_of(screen, [](char value) {
    const auto byte = static_cast<unsigned char>(value);
    return byte < 0x20 || byte == 0x7f;
  });
}

FileTransferEdgeCodecError validateScreen(const std::string &screen, const FileTransferEdgeLimits &limits)
{
  if (screen.empty() || !Unicode::isUTF8(screen) || hasUnsafeScreenByte(screen)) {
    return FileTransferEdgeCodecError::InvalidScreen;
  }
  if (screen.size() > limits.maxScreenNameBytes || screen.size() > (std::numeric_limits<std::uint16_t>::max)()) {
    return FileTransferEdgeCodecError::StringTooLong;
  }
  return FileTransferEdgeCodecError::None;
}

FileTransferEdgeCodecError validateMessage(const FileTransferEdgeMessage &message, const FileTransferEdgeLimits &limits)
{
  return std::visit(
      [&](const auto &value) -> FileTransferEdgeCodecError {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferEdgeCapabilities>) {
          return (value.activeSides & ~permittedSides()) == 0 ? FileTransferEdgeCodecError::None
                                                              : FileTransferEdgeCodecError::InvalidSides;
        } else {
          if (isEmptyId(value.requestId)) {
            return FileTransferEdgeCodecError::EmptyRequestId;
          }
          const auto sourceError = validateScreen(value.sourceScreen, limits);
          if (sourceError != FileTransferEdgeCodecError::None) {
            return sourceError;
          }

          if constexpr (std::is_same_v<Value, FileTransferEdgeTargetRequest>) {
            return validDirection(value.direction) ? FileTransferEdgeCodecError::None
                                                   : FileTransferEdgeCodecError::InvalidEnum;
          } else {
            if (!validStatus(value.status)) {
              return FileTransferEdgeCodecError::InvalidEnum;
            }
            if (value.status != FileTransferEdgeStatus::Resolved) {
              return value.targetScreen.empty() ? FileTransferEdgeCodecError::None
                                                : FileTransferEdgeCodecError::InvalidTarget;
            }
            const auto targetError = validateScreen(value.targetScreen, limits);
            if (targetError != FileTransferEdgeCodecError::None || value.targetScreen == value.sourceScreen) {
              return FileTransferEdgeCodecError::InvalidTarget;
            }
            return FileTransferEdgeCodecError::None;
          }
        }
      },
      message
  );
}

MessageKind kindOf(const FileTransferEdgeMessage &message)
{
  return std::visit(
      [](const auto &value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferEdgeCapabilities>) {
          return MessageKind::Capabilities;
        } else if constexpr (std::is_same_v<Value, FileTransferEdgeTargetRequest>) {
          return MessageKind::TargetRequest;
        } else {
          return MessageKind::TargetResponse;
        }
      },
      message
  );
}

bool writeMessageBody(Writer &writer, const FileTransferEdgeMessage &message)
{
  return std::visit(
      [&](const auto &value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferEdgeCapabilities>) {
          return writer.uint32(value.activeSides);
        } else if constexpr (std::is_same_v<Value, FileTransferEdgeTargetRequest>) {
          return writer.data(value.requestId) && writer.string(value.sourceScreen) &&
                 writer.byte(static_cast<std::uint8_t>(value.direction)) &&
                 writer.uint32(std::bit_cast<std::uint32_t>(value.x)) &&
                 writer.uint32(std::bit_cast<std::uint32_t>(value.y));
        } else {
          return writer.data(value.requestId) && writer.string(value.sourceScreen) &&
                 writer.byte(static_cast<std::uint8_t>(value.status)) && writer.string(value.targetScreen);
        }
      },
      message
  );
}

std::optional<MessageKind> messageKind(std::uint8_t raw)
{
  switch (static_cast<MessageKind>(raw)) {
  case MessageKind::Capabilities:
  case MessageKind::TargetRequest:
  case MessageKind::TargetResponse:
    return static_cast<MessageKind>(raw);
  }
  return std::nullopt;
}

std::uint32_t readHeaderLength(std::span<const std::uint8_t> bytes)
{
  return (static_cast<std::uint32_t>(bytes[8]) << 24U) | (static_cast<std::uint32_t>(bytes[9]) << 16U) |
         (static_cast<std::uint32_t>(bytes[10]) << 8U) | bytes[11];
}

FileTransferEdgeCodecError
readIdAndSource(Reader &reader, TransferId &requestId, std::string &sourceScreen, const FileTransferEdgeLimits &limits)
{
  std::span<const std::uint8_t> id;
  if (!reader.data(requestId.size(), id)) {
    return FileTransferEdgeCodecError::Truncated;
  }
  std::ranges::copy(id, requestId.begin());
  return reader.string(limits.maxScreenNameBytes, sourceScreen);
}

} // namespace

FileTransferEdgeEncodeResult
encodeFileTransferEdge(const FileTransferEdgeMessage &message, const FileTransferEdgeLimits &limits)
{
  const auto validation = validateMessage(message, limits);
  if (validation != FileTransferEdgeCodecError::None) {
    return encodeFailure(validation);
  }
  if (limits.maxPayloadBytes < kHeaderSize) {
    return encodeFailure(FileTransferEdgeCodecError::MessageTooLarge);
  }

  Writer body(limits.maxPayloadBytes - kHeaderSize);
  if (!writeMessageBody(body, message) || body.bytes().size() > (std::numeric_limits<std::uint32_t>::max)()) {
    return encodeFailure(FileTransferEdgeCodecError::MessageTooLarge);
  }

  Writer output(limits.maxPayloadBytes);
  if (!output.data(kMagic) || !output.byte(kWireVersion) || !output.byte(static_cast<std::uint8_t>(kindOf(message))) ||
      !output.uint16(0) || !output.uint32(static_cast<std::uint32_t>(body.bytes().size())) ||
      !output.data(body.bytes())) {
    return encodeFailure(FileTransferEdgeCodecError::MessageTooLarge);
  }
  return {.bytes = output.take()};
}

FileTransferEdgeDecodeResult
decodeFileTransferEdge(std::span<const std::uint8_t> bytes, const FileTransferEdgeLimits &limits)
{
  if (bytes.size() > limits.maxPayloadBytes) {
    return decodeFailure(FileTransferEdgeCodecError::MessageTooLarge);
  }
  if (bytes.size() < kHeaderSize) {
    return decodeFailure(FileTransferEdgeCodecError::Truncated);
  }
  if (!std::ranges::equal(kMagic, bytes.first(kMagic.size()))) {
    return decodeFailure(FileTransferEdgeCodecError::InvalidMagic);
  }
  if (bytes[4] != kWireVersion) {
    return decodeFailure(FileTransferEdgeCodecError::UnsupportedVersion);
  }
  const auto kind = messageKind(bytes[5]);
  if (!kind.has_value()) {
    return decodeFailure(FileTransferEdgeCodecError::InvalidKind);
  }
  if (bytes[6] != 0 || bytes[7] != 0) {
    return decodeFailure(FileTransferEdgeCodecError::InvalidFlags);
  }
  if (readHeaderLength(bytes) != bytes.size() - kHeaderSize) {
    return decodeFailure(FileTransferEdgeCodecError::LengthMismatch);
  }

  Reader reader(bytes.subspan(kHeaderSize));
  FileTransferEdgeMessage message;
  FileTransferEdgeCodecError error = FileTransferEdgeCodecError::None;

  switch (*kind) {
  case MessageKind::Capabilities: {
    FileTransferEdgeCapabilities value;
    if (!reader.uint32(value.activeSides)) {
      error = FileTransferEdgeCodecError::Truncated;
    }
    message = value;
    break;
  }
  case MessageKind::TargetRequest: {
    FileTransferEdgeTargetRequest value;
    error = readIdAndSource(reader, value.requestId, value.sourceScreen, limits);
    std::uint8_t direction = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    if (error == FileTransferEdgeCodecError::None &&
        (!reader.byte(direction) || !reader.uint32(x) || !reader.uint32(y))) {
      error = FileTransferEdgeCodecError::Truncated;
    }
    value.direction = static_cast<Direction>(direction);
    value.x = std::bit_cast<std::int32_t>(x);
    value.y = std::bit_cast<std::int32_t>(y);
    message = std::move(value);
    break;
  }
  case MessageKind::TargetResponse: {
    FileTransferEdgeTargetResponse value;
    error = readIdAndSource(reader, value.requestId, value.sourceScreen, limits);
    std::uint8_t status = 0;
    if (error == FileTransferEdgeCodecError::None && !reader.byte(status)) {
      error = FileTransferEdgeCodecError::Truncated;
    }
    value.status = static_cast<FileTransferEdgeStatus>(status);
    if (error == FileTransferEdgeCodecError::None) {
      error = reader.string(limits.maxScreenNameBytes, value.targetScreen);
    }
    message = std::move(value);
    break;
  }
  }

  if (error != FileTransferEdgeCodecError::None) {
    return decodeFailure(error);
  }
  if (reader.remaining() != 0) {
    return decodeFailure(FileTransferEdgeCodecError::TrailingData);
  }
  const auto validation = validateMessage(message, limits);
  if (validation != FileTransferEdgeCodecError::None) {
    return decodeFailure(validation);
  }
  return {.message = std::move(message)};
}

} // namespace deskflow::filetransfer
