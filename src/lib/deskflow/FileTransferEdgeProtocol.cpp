/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferEdgeProtocol.h"

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
    const auto received = stream->read(position, static_cast<std::uint32_t>(size));
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

FileTransferEdgeFrameReadResult frameFailure(FileTransferEdgeFrameError error)
{
  return {.error = error};
}

} // namespace

FileTransferEdgeEncodeResult writeFileTransferEdgeFrame(
    IStream *stream, const FileTransferEdgeMessage &message, const FileTransferEdgeLimits &limits
)
{
  auto encoded = encodeFileTransferEdge(message, limits);
  if (!encoded.ok()) {
    return encoded;
  }
  if (encoded.bytes.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    encoded.error = FileTransferEdgeCodecError::MessageTooLarge;
    encoded.bytes.clear();
    return encoded;
  }

  const auto payloadLength = encodeLength(static_cast<std::uint32_t>(encoded.bytes.size()));
  stream->write(kMsgDFileTransferEdge, 4);
  stream->write(payloadLength.data(), static_cast<std::uint32_t>(payloadLength.size()));
  stream->write(encoded.bytes.data(), static_cast<std::uint32_t>(encoded.bytes.size()));
  return encoded;
}

FileTransferEdgeFrameReadResult readFileTransferEdgeFrame(IStream *stream, const FileTransferEdgeLimits &limits)
{
  std::array<std::uint8_t, 4> lengthBytes{};
  if (!readExact(stream, lengthBytes.data(), lengthBytes.size())) {
    return frameFailure(FileTransferEdgeFrameError::TruncatedLength);
  }

  const auto payloadLength = decodeLength(lengthBytes);
  if (payloadLength > limits.maxPayloadBytes) {
    return frameFailure(FileTransferEdgeFrameError::FrameTooLarge);
  }

  std::vector<std::uint8_t> payload(payloadLength);
  if (!payload.empty() && !readExact(stream, payload.data(), payload.size())) {
    return frameFailure(FileTransferEdgeFrameError::TruncatedPayload);
  }

  auto decoded = decodeFileTransferEdge(payload, limits);
  if (!decoded.ok()) {
    return {
        .error = FileTransferEdgeFrameError::InvalidPayload,
        .codecError = decoded.error,
        .message = std::nullopt,
    };
  }
  return {.message = std::move(decoded.message)};
}

} // namespace deskflow::filetransfer
