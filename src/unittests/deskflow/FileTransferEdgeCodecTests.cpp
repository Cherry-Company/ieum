/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferEdgeCodec.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

using namespace deskflow::filetransfer;

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

TransferId transferId(std::uint8_t first = 0x21)
{
  TransferId id{};
  id[0] = first;
  return id;
}

std::uint32_t allSides()
{
  return static_cast<std::uint32_t>(DirectionMask::LeftMask) | static_cast<std::uint32_t>(DirectionMask::RightMask) |
         static_cast<std::uint32_t>(DirectionMask::TopMask) | static_cast<std::uint32_t>(DirectionMask::BottomMask);
}

template <typename Message> void requireRoundTrip(const Message &message, std::string_view label)
{
  const auto encoded = encodeFileTransferEdge(FileTransferEdgeMessage{message});
  require(encoded.ok(), std::string(label) + " should encode");
  require(encoded.bytes.size() <= 4096, std::string(label) + " should stay within the edge frame bound");
  const auto decoded = decodeFileTransferEdge(encoded.bytes);
  require(decoded.ok(), std::string(label) + " should decode");
  require(decoded.message == FileTransferEdgeMessage{message}, std::string(label) + " should round-trip exactly");
}

void roundTripsEveryMessage()
{
  requireRoundTrip(FileTransferEdgeCapabilities{.activeSides = allSides()}, "capabilities");
  requireRoundTrip(
      FileTransferEdgeTargetRequest{
          .requestId = transferId(),
          .sourceScreen = "맥북",
          .direction = Direction::Left,
          .x = -120,
          .y = 720,
      },
      "target request"
  );
  requireRoundTrip(
      FileTransferEdgeTargetResponse{
          .requestId = transferId(),
          .sourceScreen = "맥북",
          .status = FileTransferEdgeStatus::Resolved,
          .targetScreen = "Windows PC",
      },
      "resolved response"
  );
  requireRoundTrip(
      FileTransferEdgeTargetResponse{
          .requestId = transferId(0x22),
          .sourceScreen = "macbook",
          .status = FileTransferEdgeStatus::NoTarget,
          .targetScreen = {},
      },
      "no-target response"
  );
}

void writesDeterministicRequestBytes()
{
  const FileTransferEdgeTargetRequest request{
      .requestId = transferId(),
      .sourceScreen = "s",
      .direction = Direction::Left,
      .x = -1,
      .y = 720,
  };
  const auto encoded = encodeFileTransferEdge(FileTransferEdgeMessage{request});
  require(encoded.ok(), "deterministic request should encode");

  const std::vector<std::uint8_t> expected = {
      0x49, 0x46, 0x45, 0x31, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x21, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x01, 0x73, 0x01, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x02, 0xd0,
  };
  require(encoded.bytes == expected, "request bytes should use the documented network order");
}

void rejectsInvalidValuesBeforeEncoding()
{
  auto encoded =
      encodeFileTransferEdge(FileTransferEdgeMessage{FileTransferEdgeCapabilities{.activeSides = allSides() | 1U}});
  require(encoded.error == FileTransferEdgeCodecError::InvalidSides, "unknown capability bits should fail");

  auto request = FileTransferEdgeTargetRequest{
      .requestId = {},
      .sourceScreen = "macbook",
      .direction = Direction::Left,
      .x = 0,
      .y = 0,
  };
  encoded = encodeFileTransferEdge(FileTransferEdgeMessage{request});
  require(encoded.error == FileTransferEdgeCodecError::EmptyRequestId, "all-zero request ID should fail");

  request.requestId = transferId();
  request.sourceScreen.clear();
  encoded = encodeFileTransferEdge(FileTransferEdgeMessage{request});
  require(encoded.error == FileTransferEdgeCodecError::InvalidScreen, "empty source screen should fail");

  request.sourceScreen = "macbook";
  request.direction = Direction::NoDirection;
  encoded = encodeFileTransferEdge(FileTransferEdgeMessage{request});
  require(encoded.error == FileTransferEdgeCodecError::InvalidEnum, "NoDirection should fail");

  request.direction = static_cast<Direction>(255);
  encoded = encodeFileTransferEdge(FileTransferEdgeMessage{request});
  require(encoded.error == FileTransferEdgeCodecError::InvalidEnum, "unknown direction should fail");

  request.direction = Direction::Top;
  request.sourceScreen = std::string(256, 's');
  encoded = encodeFileTransferEdge(FileTransferEdgeMessage{request});
  require(encoded.error == FileTransferEdgeCodecError::StringTooLong, "oversized source screen should fail");

  FileTransferEdgeTargetResponse response{
      .requestId = transferId(),
      .sourceScreen = "macbook",
      .status = FileTransferEdgeStatus::Resolved,
      .targetScreen = {},
  };
  encoded = encodeFileTransferEdge(FileTransferEdgeMessage{response});
  require(encoded.error == FileTransferEdgeCodecError::InvalidTarget, "resolved response needs a target");

  response.status = FileTransferEdgeStatus::Rejected;
  response.targetScreen = "unexpected";
  encoded = encodeFileTransferEdge(FileTransferEdgeMessage{response});
  require(encoded.error == FileTransferEdgeCodecError::InvalidTarget, "rejected response must not name a target");

  response.status = static_cast<FileTransferEdgeStatus>(0);
  response.targetScreen.clear();
  encoded = encodeFileTransferEdge(FileTransferEdgeMessage{response});
  require(encoded.error == FileTransferEdgeCodecError::InvalidEnum, "unknown response status should fail");
}

void enforcesThe4096ByteLimit()
{
  FileTransferEdgeLimits limits;
  limits.maxPayloadBytes = 4096;
  limits.maxScreenNameBytes = 4096;
  const FileTransferEdgeTargetRequest request{
      .requestId = transferId(),
      .sourceScreen = std::string(4070, 's'),
      .direction = Direction::Right,
      .x = 0,
      .y = 0,
  };
  const auto encoded = encodeFileTransferEdge(FileTransferEdgeMessage{request}, limits);
  require(encoded.error == FileTransferEdgeCodecError::MessageTooLarge, "oversized edge payload should not encode");

  std::vector<std::uint8_t> oversized(4097, 0);
  require(
      decodeFileTransferEdge(oversized).error == FileTransferEdgeCodecError::MessageTooLarge,
      "oversized edge payload should fail before parsing"
  );
}

void rejectsEveryTruncatedPrefix()
{
  const FileTransferEdgeTargetRequest request{
      .requestId = transferId(),
      .sourceScreen = "macbook",
      .direction = Direction::Bottom,
      .x = 50,
      .y = 900,
  };
  const auto encoded = encodeFileTransferEdge(FileTransferEdgeMessage{request});
  require(encoded.ok(), "truncation fixture should encode");

  for (std::size_t length = 0; length < encoded.bytes.size(); ++length) {
    const auto decoded = decodeFileTransferEdge(std::span(encoded.bytes).first(length));
    require(!decoded.ok(), "every truncated edge prefix should fail");
    require(!decoded.message.has_value(), "truncated edge data should expose no message");
  }
}

void rejectsHeaderAndBodyMutations()
{
  const FileTransferEdgeTargetRequest request{
      .requestId = transferId(),
      .sourceScreen = "s",
      .direction = Direction::Left,
      .x = 0,
      .y = 0,
  };
  const auto encoded = encodeFileTransferEdge(FileTransferEdgeMessage{request});
  require(encoded.ok(), "mutation fixture should encode");

  auto data = encoded.bytes;
  data[0] = 'X';
  require(decodeFileTransferEdge(data).error == FileTransferEdgeCodecError::InvalidMagic, "bad magic should fail");

  data = encoded.bytes;
  data[4] = 2;
  require(
      decodeFileTransferEdge(data).error == FileTransferEdgeCodecError::UnsupportedVersion, "bad version should fail"
  );

  data = encoded.bytes;
  data[5] = 255;
  require(decodeFileTransferEdge(data).error == FileTransferEdgeCodecError::InvalidKind, "bad kind should fail");

  data = encoded.bytes;
  data[6] = 1;
  require(decodeFileTransferEdge(data).error == FileTransferEdgeCodecError::InvalidFlags, "bad flags should fail");

  data = encoded.bytes;
  ++data[11];
  require(
      decodeFileTransferEdge(data).error == FileTransferEdgeCodecError::LengthMismatch, "bad body length should fail"
  );

  data = encoded.bytes;
  data.push_back(0);
  ++data[11];
  require(
      decodeFileTransferEdge(data).error == FileTransferEdgeCodecError::TrailingData, "trailing body byte should fail"
  );

  data = encoded.bytes;
  data[31] = 0;
  require(decodeFileTransferEdge(data).error == FileTransferEdgeCodecError::InvalidEnum, "bad direction should fail");

  data = encoded.bytes;
  std::fill(data.begin() + 12, data.begin() + 28, 0);
  require(
      decodeFileTransferEdge(data).error == FileTransferEdgeCodecError::EmptyRequestId,
      "decoded all-zero request ID should fail"
  );

  data = encoded.bytes;
  data[28] = 0x01;
  data[29] = 0x00;
  require(
      decodeFileTransferEdge(data).error == FileTransferEdgeCodecError::StringTooLong,
      "decoded screen length should be bounded"
  );

  auto capabilities =
      encodeFileTransferEdge(FileTransferEdgeMessage{FileTransferEdgeCapabilities{.activeSides = allSides()}});
  require(capabilities.ok(), "capabilities mutation fixture should encode");
  capabilities.bytes[15] |= 1;
  require(
      decodeFileTransferEdge(capabilities.bytes).error == FileTransferEdgeCodecError::InvalidSides,
      "decoded unknown side bit should fail"
  );

  FileTransferEdgeTargetResponse response{
      .requestId = transferId(),
      .sourceScreen = "s",
      .status = FileTransferEdgeStatus::Rejected,
      .targetScreen = {},
  };
  auto rejected = encodeFileTransferEdge(FileTransferEdgeMessage{response});
  require(rejected.ok(), "response mutation fixture should encode");
  rejected.bytes[31] = static_cast<std::uint8_t>(FileTransferEdgeStatus::Resolved);
  require(
      decodeFileTransferEdge(rejected.bytes).error == FileTransferEdgeCodecError::InvalidTarget,
      "decoded resolved response should require a target"
  );
}

} // namespace

int main()
{
  roundTripsEveryMessage();
  writesDeterministicRequestBytes();
  rejectsInvalidValuesBeforeEncoding();
  enforcesThe4096ByteLimit();
  rejectsEveryTruncatedPrefix();
  rejectsHeaderAndBodyMutations();
  std::cout << "PASS: FileTransferEdgeCodecTests\n";
  return 0;
}
