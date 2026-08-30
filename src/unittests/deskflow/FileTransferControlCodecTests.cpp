/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferControlCodec.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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

TransferId transferId()
{
  TransferId id{};
  id[0] = 0x42;
  return id;
}

FileTransferOffer offer(std::string source = "s", std::string target = "t")
{
  return {
      .id = transferId(),
      .sourceScreen = std::move(source),
      .targetScreen = std::move(target),
      .items = {{0, "한글.txt", 0}, {1, "photo.png", 17}},
  };
}

template <typename Message> void requireRoundTrip(const Message &message, std::string_view label)
{
  const auto encoded = encodeFileTransferControl(FileTransferControlMessage{message});
  require(encoded.ok(), std::string(label) + " should encode");
  require(encoded.bytes.size() <= 64 * 1024, std::string(label) + " should stay bounded");

  const auto decoded = decodeFileTransferControl(encoded.bytes);
  require(decoded.ok(), std::string(label) + " should decode");
  require(decoded.message == FileTransferControlMessage{message}, std::string(label) + " should round-trip exactly");
}

void roundTripsEveryControlType()
{
  requireRoundTrip(offer("작업실", "거실 PC"), "offer");
  requireRoundTrip(
      FileTransferDecision{transferId(), "작업실", "거실 PC", FileTransferDecisionValue::Accept}, "accept"
  );
  requireRoundTrip(
      FileTransferDecision{transferId(), "작업실", "거실 PC", FileTransferDecisionValue::Reject}, "reject"
  );
  requireRoundTrip(
      FileTransferCancel{transferId(), "작업실", "거실 PC", FileTransferCancelReason::SenderCancelled}, "cancel"
  );
  requireRoundTrip(
      FileTransferResult{
          transferId(), "작업실", "거실 PC", FileTransferResultStatus::Failed, FileTransferResultCode::IntegrityMismatch
      },
      "result"
  );
}

void writesDeterministicNetworkOrder()
{
  const FileTransferDecision decision{transferId(), "s", "t", FileTransferDecisionValue::Accept};
  const auto encoded = encodeFileTransferControl(FileTransferControlMessage{decision});
  require(encoded.ok(), "deterministic fixture should encode");

  const std::vector<std::uint8_t> expected = {
      0x49, 0x46, 0x54, 0x43, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x73, 0x00, 0x01, 0x74, 0x01,
  };
  require(encoded.bytes == expected, "decision bytes should use the published big-endian layout");
}

void rejectsInvalidMessagesBeforeEncoding()
{
  auto invalidOffer = offer();
  invalidOffer.id = {};
  auto encoded = encodeFileTransferControl(FileTransferControlMessage{invalidOffer});
  require(encoded.error == FileTransferCodecError::PolicyRejected, "empty offer ID should fail policy");
  require(encoded.validation.error == OfferError::EmptyTransferId, "offer policy error should be preserved");

  FileTransferDecision invalidRoute{transferId(), "same", "same", FileTransferDecisionValue::Accept};
  encoded = encodeFileTransferControl(FileTransferControlMessage{invalidRoute});
  require(encoded.error == FileTransferCodecError::PolicyRejected, "same-screen decision should fail policy");
  require(encoded.validation.error == OfferError::SameSourceAndTarget, "route policy error should be preserved");

  auto invalidEnum = FileTransferCancel{transferId(), "s", "t", static_cast<FileTransferCancelReason>(255)};
  encoded = encodeFileTransferControl(FileTransferControlMessage{invalidEnum});
  require(encoded.error == FileTransferCodecError::InvalidEnum, "unknown cancel reason should not encode");
}

void enforcesWireAndPayloadLimitsBeforeEncoding()
{
  auto oversizedName = offer();
  oversizedName.items = {{0, std::string(65'536, 'a'), 1}};
  FileTransferControlLimits wideLimits;
  wideLimits.maxPayloadBytes = 100'000;
  wideLimits.offer.maxNameBytes = 70'000;
  auto encoded = encodeFileTransferControl(FileTransferControlMessage{oversizedName}, wideLimits);
  require(encoded.error == FileTransferCodecError::ValueTooLarge, "u16 name overflow should fail before writing");

  FileTransferControlLimits tinyPayload;
  tinyPayload.maxPayloadBytes = 20;
  const FileTransferDecision decision{transferId(), "s", "t", FileTransferDecisionValue::Accept};
  encoded = encodeFileTransferControl(FileTransferControlMessage{decision}, tinyPayload);
  require(encoded.error == FileTransferCodecError::MessageTooLarge, "payload cap should be enforced");
}

void rejectsEveryTruncatedPrefix()
{
  const auto encoded = encodeFileTransferControl(FileTransferControlMessage{offer()});
  require(encoded.ok(), "truncation fixture should encode");

  for (std::size_t length = 0; length < encoded.bytes.size(); ++length) {
    const auto decoded = decodeFileTransferControl(std::span(encoded.bytes).first(length));
    require(!decoded.ok(), "every truncated prefix should fail");
    require(!decoded.message.has_value(), "truncated data should never expose a partial message");
  }
}

void rejectsHeaderAndFramingMutations()
{
  const FileTransferDecision decision{transferId(), "s", "t", FileTransferDecisionValue::Accept};
  const auto encoded = encodeFileTransferControl(FileTransferControlMessage{decision});
  require(encoded.ok(), "mutation fixture should encode");

  auto bytes = encoded.bytes;
  bytes[0] = 'X';
  require(decodeFileTransferControl(bytes).error == FileTransferCodecError::InvalidMagic, "magic mutation should fail");

  bytes = encoded.bytes;
  bytes[4] = 2;
  require(
      decodeFileTransferControl(bytes).error == FileTransferCodecError::UnsupportedVersion,
      "unknown version should fail"
  );

  bytes = encoded.bytes;
  bytes[5] = 255;
  require(decodeFileTransferControl(bytes).error == FileTransferCodecError::InvalidKind, "unknown kind should fail");

  bytes = encoded.bytes;
  bytes[6] = 1;
  require(decodeFileTransferControl(bytes).error == FileTransferCodecError::InvalidFlags, "unknown flags should fail");

  bytes = encoded.bytes;
  ++bytes[11];
  require(
      decodeFileTransferControl(bytes).error == FileTransferCodecError::LengthMismatch,
      "body length mismatch should fail"
  );

  bytes = encoded.bytes;
  bytes.push_back(0);
  ++bytes[11];
  require(
      decodeFileTransferControl(bytes).error == FileTransferCodecError::TrailingData, "inner trailing byte should fail"
  );
}

void rejectsCountsLengthsEnumsAndDecodedPolicy()
{
  const FileTransferDecision decision{transferId(), "s", "t", FileTransferDecisionValue::Accept};
  const auto decisionBytes = encodeFileTransferControl(FileTransferControlMessage{decision}).bytes;

  auto bytes = decisionBytes;
  bytes.back() = 0;
  require(decodeFileTransferControl(bytes).error == FileTransferCodecError::InvalidEnum, "zero decision should fail");

  bytes = decisionBytes;
  bytes[28] = 0x01;
  bytes[29] = 0x00;
  require(decodeFileTransferControl(bytes).error == FileTransferCodecError::StringTooLong, "screen length should cap");

  bytes = decisionBytes;
  bytes[33] = 's';
  auto decoded = decodeFileTransferControl(bytes);
  require(decoded.error == FileTransferCodecError::PolicyRejected, "same decoded route should fail policy");
  require(decoded.validation.error == OfferError::SameSourceAndTarget, "decoded route error should be preserved");

  auto simpleOffer = offer();
  simpleOffer.items = {{0, "a", 1}};
  const auto offerBytes = encodeFileTransferControl(FileTransferControlMessage{simpleOffer}).bytes;

  bytes = offerBytes;
  bytes[34] = 0;
  bytes[35] = 101;
  require(decodeFileTransferControl(bytes).error == FileTransferCodecError::TooManyItems, "item count should cap");

  bytes = offerBytes;
  bytes[50] = '/';
  decoded = decodeFileTransferControl(bytes);
  require(decoded.error == FileTransferCodecError::PolicyRejected, "unsafe decoded name should fail policy");
  require(decoded.validation.error == OfferError::UnsafeItemName, "decoded item error should be preserved");
}

} // namespace

int main()
{
  roundTripsEveryControlType();
  writesDeterministicNetworkOrder();
  rejectsInvalidMessagesBeforeEncoding();
  enforcesWireAndPayloadLimitsBeforeEncoding();
  rejectsEveryTruncatedPrefix();
  rejectsHeaderAndFramingMutations();
  rejectsCountsLengthsEnumsAndDecodedPolicy();
  std::cout << "PASS: FileTransferControlCodecTests\n";
  return 0;
}
