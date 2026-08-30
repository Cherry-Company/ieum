/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferControlCodec.h"

#include "deskflow/FileTransferOfferValidator.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

namespace deskflow::filetransfer {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic{'I', 'F', 'T', 'C'};
constexpr std::uint8_t kWireVersion = 1;
constexpr std::size_t kHeaderSize = 12;

enum class MessageKind : std::uint8_t
{
  Offer = 1,
  Decision = 2,
  Cancel = 3,
  Result = 4,
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

  FileTransferCodecError string(std::size_t limit, std::string &value)
  {
    std::uint16_t length = 0;
    if (!uint16(length)) {
      return FileTransferCodecError::Truncated;
    }
    if (length > limit) {
      return FileTransferCodecError::StringTooLong;
    }

    std::span<const std::uint8_t> bytes;
    if (!data(length, bytes)) {
      return FileTransferCodecError::Truncated;
    }
    value.assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    return FileTransferCodecError::None;
  }

  [[nodiscard]] std::size_t remaining() const noexcept
  {
    return m_bytes.size() - m_position;
  }

private:
  std::span<const std::uint8_t> m_bytes;
  std::size_t m_position = 0;
};

FileTransferControlEncodeResult encodeFailure(FileTransferCodecError error, OfferValidation validation = {})
{
  return {.error = error, .bytes = {}, .validation = validation};
}

FileTransferControlDecodeResult decodeFailure(FileTransferCodecError error, OfferValidation validation = {})
{
  return {.error = error, .message = std::nullopt, .validation = validation};
}

bool validDecision(FileTransferDecisionValue value)
{
  return value == FileTransferDecisionValue::Accept || value == FileTransferDecisionValue::Reject;
}

bool validCancelReason(FileTransferCancelReason value)
{
  return value >= FileTransferCancelReason::SenderCancelled && value <= FileTransferCancelReason::Disconnected;
}

bool validResultStatus(FileTransferResultStatus value)
{
  return value >= FileTransferResultStatus::Completed && value <= FileTransferResultStatus::Cancelled;
}

bool validResultCode(FileTransferResultCode value)
{
  return value >= FileTransferResultCode::None && value <= FileTransferResultCode::ProtocolError;
}

MessageKind kindOf(const FileTransferControlMessage &message)
{
  return std::visit(
      [](const auto &value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferOffer>) {
          return MessageKind::Offer;
        } else if constexpr (std::is_same_v<Value, FileTransferDecision>) {
          return MessageKind::Decision;
        } else if constexpr (std::is_same_v<Value, FileTransferCancel>) {
          return MessageKind::Cancel;
        } else {
          return MessageKind::Result;
        }
      },
      message
  );
}

OfferValidation
validateMessagePolicy(const FileTransferControlMessage &message, const FileTransferControlLimits &limits)
{
  return std::visit(
      [&](const auto &value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferOffer>) {
          return validateOffer(value, limits.offer);
        } else {
          return validateTransferRoute(value.id, value.sourceScreen, value.targetScreen, limits.offer);
        }
      },
      message
  );
}

bool hasRepresentableValues(const FileTransferControlMessage &message)
{
  return std::visit(
      [](const auto &value) {
        if (value.sourceScreen.size() > std::numeric_limits<std::uint16_t>::max() ||
            value.targetScreen.size() > std::numeric_limits<std::uint16_t>::max()) {
          return false;
        }

        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferOffer>) {
          if (value.items.size() > std::numeric_limits<std::uint16_t>::max()) {
            return false;
          }
          return std::ranges::all_of(value.items, [](const auto &item) {
            return item.name.size() <= std::numeric_limits<std::uint16_t>::max();
          });
        } else {
          return true;
        }
      },
      message
  );
}

bool hasValidEnums(const FileTransferControlMessage &message)
{
  return std::visit(
      [](const auto &value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferDecision>) {
          return validDecision(value.decision);
        } else if constexpr (std::is_same_v<Value, FileTransferCancel>) {
          return validCancelReason(value.reason);
        } else if constexpr (std::is_same_v<Value, FileTransferResult>) {
          return validResultStatus(value.status) && validResultCode(value.code);
        } else {
          return true;
        }
      },
      message
  );
}

template <typename Message> bool writeRoute(Writer &writer, const Message &message)
{
  return writer.data(message.id) && writer.string(message.sourceScreen) && writer.string(message.targetScreen);
}

bool writeMessageBody(Writer &writer, const FileTransferControlMessage &message)
{
  return std::visit(
      [&](const auto &value) {
        if (!writeRoute(writer, value)) {
          return false;
        }

        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, FileTransferOffer>) {
          if (!writer.uint16(static_cast<std::uint16_t>(value.items.size()))) {
            return false;
          }
          for (const auto &item : value.items) {
            if (!writer.uint32(item.index) || !writer.uint64(item.size) || !writer.string(item.name)) {
              return false;
            }
          }
          return true;
        } else if constexpr (std::is_same_v<Value, FileTransferDecision>) {
          return writer.byte(static_cast<std::uint8_t>(value.decision));
        } else if constexpr (std::is_same_v<Value, FileTransferCancel>) {
          return writer.byte(static_cast<std::uint8_t>(value.reason));
        } else {
          return writer.byte(static_cast<std::uint8_t>(value.status)) &&
                 writer.uint16(static_cast<std::uint16_t>(value.code));
        }
      },
      message
  );
}

FileTransferCodecError readRoute(
    Reader &reader, TransferId &id, std::string &sourceScreen, std::string &targetScreen,
    const FileTransferControlLimits &limits
)
{
  std::span<const std::uint8_t> idBytes;
  if (!reader.data(id.size(), idBytes)) {
    return FileTransferCodecError::Truncated;
  }
  std::ranges::copy(idBytes, id.begin());

  auto error = reader.string(limits.offer.maxScreenNameBytes, sourceScreen);
  if (error != FileTransferCodecError::None) {
    return error;
  }
  return reader.string(limits.offer.maxScreenNameBytes, targetScreen);
}

FileTransferCodecError readOffer(Reader &reader, FileTransferOffer &offer, const FileTransferControlLimits &limits)
{
  auto error = readRoute(reader, offer.id, offer.sourceScreen, offer.targetScreen, limits);
  if (error != FileTransferCodecError::None) {
    return error;
  }

  std::uint16_t itemCount = 0;
  if (!reader.uint16(itemCount)) {
    return FileTransferCodecError::Truncated;
  }
  if (itemCount > limits.offer.maxItems) {
    return FileTransferCodecError::TooManyItems;
  }

  offer.items.reserve(itemCount);
  for (std::uint16_t position = 0; position < itemCount; ++position) {
    FileTransferItem item;
    if (!reader.uint32(item.index) || !reader.uint64(item.size)) {
      return FileTransferCodecError::Truncated;
    }
    error = reader.string(limits.offer.maxNameBytes, item.name);
    if (error != FileTransferCodecError::None) {
      return error;
    }
    offer.items.push_back(std::move(item));
  }
  return FileTransferCodecError::None;
}

template <typename Message>
FileTransferCodecError readRoutedMessage(Reader &reader, Message &message, const FileTransferControlLimits &limits)
{
  return readRoute(reader, message.id, message.sourceScreen, message.targetScreen, limits);
}

std::optional<MessageKind> messageKind(std::uint8_t value)
{
  switch (static_cast<MessageKind>(value)) {
  case MessageKind::Offer:
  case MessageKind::Decision:
  case MessageKind::Cancel:
  case MessageKind::Result:
    return static_cast<MessageKind>(value);
  }
  return std::nullopt;
}

std::uint32_t readHeaderLength(std::span<const std::uint8_t> bytes)
{
  return (static_cast<std::uint32_t>(bytes[8]) << 24U) | (static_cast<std::uint32_t>(bytes[9]) << 16U) |
         (static_cast<std::uint32_t>(bytes[10]) << 8U) | bytes[11];
}

} // namespace

FileTransferControlEncodeResult
encodeFileTransferControl(const FileTransferControlMessage &message, const FileTransferControlLimits &limits)
{
  if (!hasValidEnums(message)) {
    return encodeFailure(FileTransferCodecError::InvalidEnum);
  }

  const auto validation = validateMessagePolicy(message, limits);
  if (!validation.ok()) {
    return encodeFailure(FileTransferCodecError::PolicyRejected, validation);
  }
  if (!hasRepresentableValues(message)) {
    return encodeFailure(FileTransferCodecError::ValueTooLarge, validation);
  }
  if (limits.maxPayloadBytes < kHeaderSize) {
    return encodeFailure(FileTransferCodecError::MessageTooLarge, validation);
  }

  Writer body(limits.maxPayloadBytes - kHeaderSize);
  if (!writeMessageBody(body, message)) {
    return encodeFailure(FileTransferCodecError::MessageTooLarge, validation);
  }
  if (body.bytes().size() > std::numeric_limits<std::uint32_t>::max()) {
    return encodeFailure(FileTransferCodecError::MessageTooLarge, validation);
  }

  Writer output(limits.maxPayloadBytes);
  const auto bodyLength = static_cast<std::uint32_t>(body.bytes().size());
  if (!output.data(kMagic) || !output.byte(kWireVersion) || !output.byte(static_cast<std::uint8_t>(kindOf(message))) ||
      !output.uint16(0) || !output.uint32(bodyLength) || !output.data(body.bytes())) {
    return encodeFailure(FileTransferCodecError::MessageTooLarge, validation);
  }

  return {.error = FileTransferCodecError::None, .bytes = output.take(), .validation = validation};
}

FileTransferControlDecodeResult
decodeFileTransferControl(std::span<const std::uint8_t> bytes, const FileTransferControlLimits &limits)
{
  if (bytes.size() > limits.maxPayloadBytes) {
    return decodeFailure(FileTransferCodecError::MessageTooLarge);
  }
  if (bytes.size() < kHeaderSize) {
    return decodeFailure(FileTransferCodecError::Truncated);
  }
  if (!std::ranges::equal(kMagic, bytes.first(kMagic.size()))) {
    return decodeFailure(FileTransferCodecError::InvalidMagic);
  }
  if (bytes[4] != kWireVersion) {
    return decodeFailure(FileTransferCodecError::UnsupportedVersion);
  }

  const auto kind = messageKind(bytes[5]);
  if (!kind.has_value()) {
    return decodeFailure(FileTransferCodecError::InvalidKind);
  }
  if (bytes[6] != 0 || bytes[7] != 0) {
    return decodeFailure(FileTransferCodecError::InvalidFlags);
  }
  if (readHeaderLength(bytes) != bytes.size() - kHeaderSize) {
    return decodeFailure(FileTransferCodecError::LengthMismatch);
  }

  Reader reader(bytes.subspan(kHeaderSize));
  FileTransferControlMessage message;
  FileTransferCodecError error = FileTransferCodecError::None;

  switch (*kind) {
  case MessageKind::Offer: {
    FileTransferOffer value;
    error = readOffer(reader, value, limits);
    message = std::move(value);
    break;
  }
  case MessageKind::Decision: {
    FileTransferDecision value;
    error = readRoutedMessage(reader, value, limits);
    std::uint8_t rawDecision = 0;
    if (error == FileTransferCodecError::None && !reader.byte(rawDecision)) {
      error = FileTransferCodecError::Truncated;
    }
    value.decision = static_cast<FileTransferDecisionValue>(rawDecision);
    if (error == FileTransferCodecError::None && !validDecision(value.decision)) {
      error = FileTransferCodecError::InvalidEnum;
    }
    message = std::move(value);
    break;
  }
  case MessageKind::Cancel: {
    FileTransferCancel value;
    error = readRoutedMessage(reader, value, limits);
    std::uint8_t rawReason = 0;
    if (error == FileTransferCodecError::None && !reader.byte(rawReason)) {
      error = FileTransferCodecError::Truncated;
    }
    value.reason = static_cast<FileTransferCancelReason>(rawReason);
    if (error == FileTransferCodecError::None && !validCancelReason(value.reason)) {
      error = FileTransferCodecError::InvalidEnum;
    }
    message = std::move(value);
    break;
  }
  case MessageKind::Result: {
    FileTransferResult value;
    error = readRoutedMessage(reader, value, limits);
    std::uint8_t rawStatus = 0;
    std::uint16_t rawCode = 0;
    if (error == FileTransferCodecError::None && (!reader.byte(rawStatus) || !reader.uint16(rawCode))) {
      error = FileTransferCodecError::Truncated;
    }
    value.status = static_cast<FileTransferResultStatus>(rawStatus);
    value.code = static_cast<FileTransferResultCode>(rawCode);
    if (error == FileTransferCodecError::None && (!validResultStatus(value.status) || !validResultCode(value.code))) {
      error = FileTransferCodecError::InvalidEnum;
    }
    message = std::move(value);
    break;
  }
  }

  if (error != FileTransferCodecError::None) {
    return decodeFailure(error);
  }
  if (reader.remaining() != 0) {
    return decodeFailure(FileTransferCodecError::TrailingData);
  }

  const auto validation = validateMessagePolicy(message, limits);
  if (!validation.ok()) {
    return decodeFailure(FileTransferCodecError::PolicyRejected, validation);
  }
  return {.error = FileTransferCodecError::None, .message = std::move(message), .validation = validation};
}

} // namespace deskflow::filetransfer
