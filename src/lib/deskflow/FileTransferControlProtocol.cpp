/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferControlProtocol.h"

#include "deskflow/ProtocolTypes.h"
#include "io/IStream.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace deskflow::filetransfer {
namespace {

bool readExact(IStream *stream, void *buffer, std::size_t size)
{
  auto *position = static_cast<std::uint8_t *>(buffer);
  while (size > 0) {
    const auto request = static_cast<std::uint32_t>(size);
    const auto received = stream->read(position, request);
    if (received == 0) {
      return false;
    }
    position += received;
    size -= received;
  }
  return true;
}

std::uint32_t decodeLength(const std::array<std::uint8_t, 4> &bytes)
{
  return (static_cast<std::uint32_t>(bytes[0]) << 24U) | (static_cast<std::uint32_t>(bytes[1]) << 16U) |
         (static_cast<std::uint32_t>(bytes[2]) << 8U) | bytes[3];
}

std::array<std::uint8_t, 4> encodeLength(std::uint32_t value)
{
  return {
      static_cast<std::uint8_t>(value >> 24U),
      static_cast<std::uint8_t>(value >> 16U),
      static_cast<std::uint8_t>(value >> 8U),
      static_cast<std::uint8_t>(value),
  };
}

FileTransferFrameReadResult frameFailure(FileTransferFrameError error)
{
  return {
      .error = error,
      .codecError = FileTransferCodecError::None,
      .message = std::nullopt,
      .validation = {},
  };
}

} // namespace

FileTransferControlEncodeResult writeFileTransferControlFrame(
    IStream *stream, const FileTransferControlMessage &message, const FileTransferControlLimits &limits
)
{
  auto encoded = encodeFileTransferControl(message, limits);
  if (!encoded.ok()) {
    return encoded;
  }
  if (encoded.bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
    encoded.error = FileTransferCodecError::MessageTooLarge;
    encoded.bytes.clear();
    return encoded;
  }

  const auto payloadLength = encodeLength(static_cast<std::uint32_t>(encoded.bytes.size()));
  stream->write(kMsgDFileTransferControl, 4);
  stream->write(payloadLength.data(), static_cast<std::uint32_t>(payloadLength.size()));
  stream->write(encoded.bytes.data(), static_cast<std::uint32_t>(encoded.bytes.size()));
  return encoded;
}

FileTransferFrameReadResult readFileTransferControlFrame(IStream *stream, const FileTransferControlLimits &limits)
{
  std::array<std::uint8_t, 4> lengthBytes{};
  if (!readExact(stream, lengthBytes.data(), lengthBytes.size())) {
    return frameFailure(FileTransferFrameError::TruncatedLength);
  }

  const auto payloadLength = decodeLength(lengthBytes);
  if (payloadLength > limits.maxPayloadBytes) {
    return frameFailure(FileTransferFrameError::FrameTooLarge);
  }

  std::vector<std::uint8_t> payload(payloadLength);
  if (!payload.empty() && !readExact(stream, payload.data(), payload.size())) {
    return frameFailure(FileTransferFrameError::TruncatedPayload);
  }

  auto decoded = decodeFileTransferControl(payload, limits);
  if (!decoded.ok()) {
    return {
        .error = FileTransferFrameError::InvalidPayload,
        .codecError = decoded.error,
        .message = std::nullopt,
        .validation = decoded.validation,
    };
  }

  return {
      .error = FileTransferFrameError::None,
      .codecError = FileTransferCodecError::None,
      .message = std::move(decoded.message),
      .validation = decoded.validation,
  };
}

} // namespace deskflow::filetransfer
