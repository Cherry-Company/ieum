/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferDataCodec.h"

#include "deskflow/FileTransferOfferValidator.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

namespace deskflow::filetransfer {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic{'I', 'F', 'T', 'D'};
constexpr std::uint8_t kWireVersion = 1;
constexpr std::size_t kHeaderSize = 12;

enum class MessageKind : std::uint8_t
{
  Begin = 1,
  Chunk = 2,
  ItemEnd = 3,
  Finish = 4,
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

  bool uint64(std::uint64_t value)
  {
    return uint32(static_cast<std::uint32_t>(value >> 32U)) && uint32(static_cast<std::uint32_t>(value));
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

  [[nodiscard]] std::size_t size() const noexcept
  {
    return m_bytes.size();
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

  bool uint64(std::uint64_t &value)
  {
    std::uint32_t high = 0;
    std::uint32_t low = 0;
    if (!uint32(high) || !uint32(low)) {
      return false;
    }
    value = (static_cast<std::uint64_t>(high) << 32U) | low;
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

  FileTransferDataCodecError string(std::size_t limit, std::string &value)
  {
    std::uint16_t length = 0;
    if (!uint16(length)) {
      return FileTransferDataCodecError::Truncated;
    }
    if (length > limit) {
      return FileTransferDataCodecError::StringTooLong;
    }

    std::span<const std::uint8_t> bytes;
    if (!data(length, bytes)) {
      return FileTransferDataCodecError::Truncated;
    }
    value.assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    return FileTransferDataCodecError::None;
  }

  [[nodiscard]] std::size_t remaining() const noexcept
  {
    return m_bytes.size() - m_position;
  }

private:
  std::span<const std::uint8_t> m_bytes;
  std::size_t m_position = 0;
};

FileTransferDataEncodeResult encodeFailure(FileTransferDataCodecError error, OfferValidation validation = {})
{
  return {.error = error, .bytes = {}, .validation = validation};
}

FileTransferDataDecodeResult decodeFailure(FileTransferDataCodecError error, OfferValidation validation = {})
{
  return {.error = error, .message = std::nullopt, .validation = validation};
}

const FileTransferDataRoute &routeOf(const FileTransferDataMessage &message)
{
  return std::visit([](const auto &value) -> const FileTransferDataRoute & { return value.route; }, message);
}

MessageKind kindOf(const FileTransferDataMessage &message)
{
  return std::visit(
      [](const auto &value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferDataBegin>) {
          return MessageKind::Begin;
        } else if constexpr (std::is_same_v<Value, FileTransferDataChunk>) {
          return MessageKind::Chunk;
        } else if constexpr (std::is_same_v<Value, FileTransferDataItemEnd>) {
          return MessageKind::ItemEnd;
        } else {
          return MessageKind::Finish;
        }
      },
      message
  );
}

std::optional<MessageKind> messageKind(std::uint8_t value)
{
  switch (static_cast<MessageKind>(value)) {
  case MessageKind::Begin:
  case MessageKind::Chunk:
  case MessageKind::ItemEnd:
  case MessageKind::Finish:
    return static_cast<MessageKind>(value);
  }
  return std::nullopt;
}

bool hasRepresentableValues(const FileTransferDataMessage &message)
{
  const auto &route = routeOf(message);
  if (route.sourceScreen.size() > std::numeric_limits<std::uint16_t>::max() ||
      route.targetScreen.size() > std::numeric_limits<std::uint16_t>::max()) {
    return false;
  }

  return std::visit(
      [](const auto &value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferDataChunk>) {
          return value.bytes.size() <= std::numeric_limits<std::uint32_t>::max();
        } else {
          return true;
        }
      },
      message
  );
}

FileTransferDataCodecError
validateMessageValues(const FileTransferDataMessage &message, const FileTransferDataLimits &limits)
{
  return std::visit(
      [&](const auto &value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferDataChunk>) {
          if (value.bytes.empty()) {
            return FileTransferDataCodecError::EmptyChunk;
          }
          if (value.bytes.size() > limits.maxChunkBytes) {
            return FileTransferDataCodecError::ChunkTooLarge;
          }
          if (value.bytes.size() > std::numeric_limits<std::uint64_t>::max() - value.offset) {
            return FileTransferDataCodecError::InvalidValue;
          }
        } else if constexpr (std::is_same_v<Value, FileTransferDataFinish>) {
          if (value.itemCount == 0) {
            return FileTransferDataCodecError::InvalidValue;
          }
        }
        return FileTransferDataCodecError::None;
      },
      message
  );
}

bool writeRoute(Writer &writer, const FileTransferDataRoute &route)
{
  return writer.data(route.id) && writer.string(route.sourceScreen) && writer.string(route.targetScreen);
}

bool writeBody(Writer &writer, const FileTransferDataMessage &message)
{
  return std::visit(
      [&](const auto &value) {
        if (!writeRoute(writer, value.route)) {
          return false;
        }

        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferDataBegin>) {
          return true;
        } else if constexpr (std::is_same_v<Value, FileTransferDataChunk>) {
          return writer.uint32(value.itemIndex) && writer.uint64(value.offset) &&
                 writer.uint32(static_cast<std::uint32_t>(value.bytes.size())) && writer.data(value.bytes);
        } else if constexpr (std::is_same_v<Value, FileTransferDataItemEnd>) {
          return writer.uint32(value.itemIndex) && writer.uint64(value.size) && writer.data(value.sha256);
        } else {
          return writer.uint32(value.itemCount) && writer.uint64(value.totalBytes);
        }
      },
      message
  );
}

FileTransferDataCodecError readRoute(Reader &reader, FileTransferDataRoute &route, const FileTransferDataLimits &limits)
{
  std::span<const std::uint8_t> idBytes;
  if (!reader.data(route.id.size(), idBytes)) {
    return FileTransferDataCodecError::Truncated;
  }
  std::ranges::copy(idBytes, route.id.begin());

  auto error = reader.string(limits.route.maxScreenNameBytes, route.sourceScreen);
  if (error != FileTransferDataCodecError::None) {
    return error;
  }
  return reader.string(limits.route.maxScreenNameBytes, route.targetScreen);
}

FileTransferDataCodecError readChunk(Reader &reader, FileTransferDataChunk &chunk, const FileTransferDataLimits &limits)
{
  auto error = readRoute(reader, chunk.route, limits);
  std::uint32_t length = 0;
  if (error != FileTransferDataCodecError::None || !reader.uint32(chunk.itemIndex) || !reader.uint64(chunk.offset) ||
      !reader.uint32(length)) {
    return error == FileTransferDataCodecError::None ? FileTransferDataCodecError::Truncated : error;
  }
  if (length == 0) {
    return FileTransferDataCodecError::EmptyChunk;
  }
  if (length > limits.maxChunkBytes) {
    return FileTransferDataCodecError::ChunkTooLarge;
  }
  if (length > std::numeric_limits<std::uint64_t>::max() - chunk.offset) {
    return FileTransferDataCodecError::InvalidValue;
  }

  std::span<const std::uint8_t> bytes;
  if (!reader.data(length, bytes)) {
    return FileTransferDataCodecError::Truncated;
  }
  chunk.bytes.assign(bytes.begin(), bytes.end());
  return FileTransferDataCodecError::None;
}

FileTransferDataCodecError
readItemEnd(Reader &reader, FileTransferDataItemEnd &itemEnd, const FileTransferDataLimits &limits)
{
  const auto error = readRoute(reader, itemEnd.route, limits);
  if (error != FileTransferDataCodecError::None || !reader.uint32(itemEnd.itemIndex) || !reader.uint64(itemEnd.size)) {
    return error == FileTransferDataCodecError::None ? FileTransferDataCodecError::Truncated : error;
  }

  std::span<const std::uint8_t> digestBytes;
  if (!reader.data(itemEnd.sha256.size(), digestBytes)) {
    return FileTransferDataCodecError::Truncated;
  }
  std::ranges::copy(digestBytes, itemEnd.sha256.begin());
  return FileTransferDataCodecError::None;
}

FileTransferDataCodecError
readFinish(Reader &reader, FileTransferDataFinish &finish, const FileTransferDataLimits &limits)
{
  const auto error = readRoute(reader, finish.route, limits);
  if (error != FileTransferDataCodecError::None || !reader.uint32(finish.itemCount) ||
      !reader.uint64(finish.totalBytes)) {
    return error == FileTransferDataCodecError::None ? FileTransferDataCodecError::Truncated : error;
  }
  return finish.itemCount == 0 ? FileTransferDataCodecError::InvalidValue : FileTransferDataCodecError::None;
}

FileTransferDataCodecError
readMessage(MessageKind kind, Reader &reader, FileTransferDataMessage &message, const FileTransferDataLimits &limits)
{
  switch (kind) {
  case MessageKind::Begin: {
    FileTransferDataBegin begin;
    const auto error = readRoute(reader, begin.route, limits);
    message = std::move(begin);
    return error;
  }
  case MessageKind::Chunk: {
    FileTransferDataChunk chunk;
    const auto error = readChunk(reader, chunk, limits);
    message = std::move(chunk);
    return error;
  }
  case MessageKind::ItemEnd: {
    FileTransferDataItemEnd itemEnd;
    const auto error = readItemEnd(reader, itemEnd, limits);
    message = std::move(itemEnd);
    return error;
  }
  case MessageKind::Finish: {
    FileTransferDataFinish finish;
    const auto error = readFinish(reader, finish, limits);
    message = std::move(finish);
    return error;
  }
  }
  return FileTransferDataCodecError::InvalidKind;
}

std::uint32_t readHeaderLength(std::span<const std::uint8_t> bytes)
{
  return (static_cast<std::uint32_t>(bytes[8]) << 24U) | (static_cast<std::uint32_t>(bytes[9]) << 16U) |
         (static_cast<std::uint32_t>(bytes[10]) << 8U) | bytes[11];
}

} // namespace

FileTransferDataEncodeResult
encodeFileTransferData(const FileTransferDataMessage &message, const FileTransferDataLimits &limits)
{
  if (!hasRepresentableValues(message)) {
    return encodeFailure(FileTransferDataCodecError::ValueTooLarge);
  }

  const auto valueError = validateMessageValues(message, limits);
  if (valueError != FileTransferDataCodecError::None) {
    return encodeFailure(valueError);
  }

  const auto &route = routeOf(message);
  const auto validation = validateTransferRoute(route.id, route.sourceScreen, route.targetScreen, limits.route);
  if (!validation.ok()) {
    return encodeFailure(FileTransferDataCodecError::PolicyRejected, validation);
  }

  Writer body(limits.maxPayloadBytes);
  if (!writeBody(body, message) || body.size() > std::numeric_limits<std::uint32_t>::max()) {
    return encodeFailure(FileTransferDataCodecError::MessageTooLarge, validation);
  }

  Writer output(limits.maxPayloadBytes);
  if (!output.data(kMagic) || !output.byte(kWireVersion) || !output.byte(static_cast<std::uint8_t>(kindOf(message))) ||
      !output.uint16(0) || !output.uint32(static_cast<std::uint32_t>(body.size()))) {
    return encodeFailure(FileTransferDataCodecError::MessageTooLarge, validation);
  }
  auto bodyBytes = body.take();
  if (!output.data(bodyBytes)) {
    return encodeFailure(FileTransferDataCodecError::MessageTooLarge, validation);
  }
  return {.error = FileTransferDataCodecError::None, .bytes = output.take(), .validation = validation};
}

FileTransferDataDecodeResult
decodeFileTransferData(std::span<const std::uint8_t> bytes, const FileTransferDataLimits &limits)
{
  if (bytes.size() > limits.maxPayloadBytes) {
    return decodeFailure(FileTransferDataCodecError::MessageTooLarge);
  }
  if (bytes.size() < kHeaderSize) {
    return decodeFailure(FileTransferDataCodecError::Truncated);
  }
  if (!std::ranges::equal(bytes.first(kMagic.size()), kMagic)) {
    return decodeFailure(FileTransferDataCodecError::InvalidMagic);
  }
  if (bytes[4] != kWireVersion) {
    return decodeFailure(FileTransferDataCodecError::UnsupportedVersion);
  }

  const auto kind = messageKind(bytes[5]);
  if (!kind.has_value()) {
    return decodeFailure(FileTransferDataCodecError::InvalidKind);
  }
  if (bytes[6] != 0 || bytes[7] != 0) {
    return decodeFailure(FileTransferDataCodecError::InvalidFlags);
  }

  const auto bodyLength = readHeaderLength(bytes);
  if (bodyLength != bytes.size() - kHeaderSize) {
    return decodeFailure(FileTransferDataCodecError::LengthMismatch);
  }

  Reader reader(bytes.subspan(kHeaderSize));
  FileTransferDataMessage message = FileTransferDataBegin{};
  const auto readError = readMessage(*kind, reader, message, limits);
  if (readError != FileTransferDataCodecError::None) {
    return decodeFailure(readError);
  }
  if (reader.remaining() != 0) {
    return decodeFailure(FileTransferDataCodecError::TrailingData);
  }

  const auto &route = routeOf(message);
  const auto validation = validateTransferRoute(route.id, route.sourceScreen, route.targetScreen, limits.route);
  if (!validation.ok()) {
    return decodeFailure(FileTransferDataCodecError::PolicyRejected, validation);
  }
  return {.error = FileTransferDataCodecError::None, .message = std::move(message), .validation = validation};
}

} // namespace deskflow::filetransfer
