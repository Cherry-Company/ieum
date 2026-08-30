/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferDataProtocol.h"
#include "deskflow/FileTransferDataRouting.h"
#include "deskflow/ProtocolTypes.h"
#include "io/IStream.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <iterator>
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

class MemoryStream final : public deskflow::IStream
{
public:
  void setReadChunks(std::vector<std::string> chunks)
  {
    m_chunks = std::deque<std::string>(std::make_move_iterator(chunks.begin()), std::make_move_iterator(chunks.end()));
  }

  void close() override
  {
    m_closed = true;
  }

  std::uint32_t read(void *buffer, std::uint32_t size) override
  {
    ++m_readCalls;
    m_largestRead = (std::max)(m_largestRead, size);
    if (m_closed || m_chunks.empty() || size == 0) {
      return 0;
    }

    auto &chunk = m_chunks.front();
    const auto count = (std::min)(static_cast<std::size_t>(size), chunk.size());
    if (buffer != nullptr) {
      std::memcpy(buffer, chunk.data(), count);
    }
    chunk.erase(0, count);
    if (chunk.empty()) {
      m_chunks.pop_front();
    }
    return static_cast<std::uint32_t>(count);
  }

  void write(const void *buffer, std::uint32_t size) override
  {
    m_written.append(static_cast<const char *>(buffer), size);
  }

  void flush() override
  {
  }

  void shutdownInput() override
  {
    m_closed = true;
  }

  void shutdownOutput() override
  {
  }

  void *getEventTarget() const override
  {
    return const_cast<MemoryStream *>(this);
  }

  bool isReady() const override
  {
    return !m_closed && !m_chunks.empty();
  }

  std::uint32_t getSize() const override
  {
    std::size_t size = 0;
    for (const auto &chunk : m_chunks) {
      size += chunk.size();
    }
    return static_cast<std::uint32_t>(size);
  }

  const std::string &written() const
  {
    return m_written;
  }

  std::size_t readCalls() const
  {
    return m_readCalls;
  }

  std::uint32_t largestRead() const
  {
    return m_largestRead;
  }

private:
  std::deque<std::string> m_chunks;
  std::string m_written;
  std::size_t m_readCalls = 0;
  std::uint32_t m_largestRead = 0;
  bool m_closed = false;
};

TransferId transferId()
{
  TransferId id{};
  id[0] = 0x42;
  id[15] = 0x99;
  return id;
}

FileTransferDataRoute route()
{
  return {.id = transferId(), .sourceScreen = "source", .targetScreen = "target"};
}

FileTransferDigest digest()
{
  FileTransferDigest value{};
  for (std::size_t index = 0; index < value.size(); ++index) {
    value[index] = static_cast<std::uint8_t>(index);
  }
  return value;
}

std::string bytes(std::initializer_list<std::uint8_t> values)
{
  return {reinterpret_cast<const char *>(values.begin()), values.size()};
}

template <typename Message> void roundTrips(const Message &message, std::string_view label)
{
  const FileTransferDataMessage original = message;
  const auto encoded = encodeFileTransferData(original);
  require(encoded.ok(), std::string(label) + " should encode");
  const auto decoded = decodeFileTransferData(encoded.bytes);
  require(decoded.ok(), std::string(label) + " should decode");
  require(decoded.message == original, std::string(label) + " should round-trip exactly");
}

void allMessageKindsRoundTrip()
{
  const auto common = route();
  roundTrips(FileTransferDataBegin{common}, "begin");
  roundTrips(
      FileTransferDataChunk{.route = common, .itemIndex = 1, .offset = 65'536, .bytes = {0, 1, 2, 255}}, "chunk"
  );
  roundTrips(FileTransferDataItemEnd{.route = common, .itemIndex = 1, .size = 65'540, .sha256 = digest()}, "item end");
  roundTrips(FileTransferDataFinish{.route = common, .itemCount = 2, .totalBytes = 65'540}, "finish");
}

void frameRoundTripsAcrossFragmentedReads()
{
  MemoryStream output;
  const FileTransferDataMessage original =
      FileTransferDataChunk{.route = route(), .itemIndex = 0, .offset = 7, .bytes = {10, 20, 30, 40}};
  const auto write = writeFileTransferDataFrame(&output, original);
  require(write.ok(), "valid data message should frame");
  require(output.written().starts_with("DFTD"), "frame should use the DFTD message code");

  MemoryStream input;
  const auto &wire = output.written();
  input.setReadChunks({wire.substr(4, 1), wire.substr(5, 2), wire.substr(7, 5), wire.substr(12)});
  const auto read = readFileTransferDataFrame(&input);
  require(read.ok(), "fragmented data frame should decode");
  require(read.message == original, "framing should preserve the data message");
}

void rejectsInvalidRoutesAndChunksBeforeWriting()
{
  auto invalidRoute = route();
  invalidRoute.id = {};
  auto encoded = encodeFileTransferData(FileTransferDataMessage{FileTransferDataBegin{invalidRoute}});
  require(encoded.error == FileTransferDataCodecError::PolicyRejected, "empty transfer ID should fail policy");

  auto emptyChunk = FileTransferDataChunk{.route = route(), .itemIndex = 0, .offset = 0, .bytes = {}};
  encoded = encodeFileTransferData(FileTransferDataMessage{emptyChunk});
  require(encoded.error == FileTransferDataCodecError::EmptyChunk, "empty data chunks should fail");

  FileTransferDataLimits limits;
  limits.maxChunkBytes = 3;
  auto largeChunk = emptyChunk;
  largeChunk.bytes = {1, 2, 3, 4};
  encoded = encodeFileTransferData(FileTransferDataMessage{largeChunk}, limits);
  require(encoded.error == FileTransferDataCodecError::ChunkTooLarge, "oversized data chunks should fail");

  MemoryStream output;
  const auto write = writeFileTransferDataFrame(&output, FileTransferDataMessage{emptyChunk});
  require(!write.ok() && output.written().empty(), "invalid chunks should not write partial frames");
}

void rejectsOversizedFrameBeforePayloadRead()
{
  MemoryStream stream;
  stream.setReadChunks({bytes({0x00, 0x01, 0x00, 0x01}), std::string(65'537, 'x')});
  const auto read = readFileTransferDataFrame(&stream);
  require(read.error == FileTransferDataFrameError::FrameTooLarge, "oversized outer frame should fail");
  require(stream.readCalls() == 1, "oversized data frame should stop after its length");
  require(stream.largestRead() == 4, "oversized data frame should never request its payload");
}

void rejectsTruncatedAndMalformedFrames()
{
  MemoryStream shortLength;
  shortLength.setReadChunks({bytes({0x00, 0x00, 0x00})});
  require(
      readFileTransferDataFrame(&shortLength).error == FileTransferDataFrameError::TruncatedLength,
      "short outer length should fail"
  );

  MemoryStream shortPayload;
  shortPayload.setReadChunks({bytes({0x00, 0x00, 0x00, 0x04}), "bad"});
  require(
      readFileTransferDataFrame(&shortPayload).error == FileTransferDataFrameError::TruncatedPayload,
      "short payload should fail"
  );

  MemoryStream malformed;
  malformed.setReadChunks({bytes({0x00, 0x00, 0x00, 0x04}), "nope"});
  const auto read = readFileTransferDataFrame(&malformed);
  require(read.error == FileTransferDataFrameError::InvalidPayload, "malformed payload should fail");
  require(read.codecError == FileTransferDataCodecError::Truncated, "inner data codec error should be retained");
}

void rejectsMutatedPayloads()
{
  const auto message = FileTransferDataMessage{FileTransferDataChunk{
      .route = route(),
      .itemIndex = 0,
      .offset = 0,
      .bytes = {1, 2, 3},
  }};
  const auto encoded = encodeFileTransferData(message);
  require(encoded.ok(), "test data should encode");

  auto mutated = encoded.bytes;
  mutated[0] = 'X';
  require(
      decodeFileTransferData(mutated).error == FileTransferDataCodecError::InvalidMagic, "mutated magic should fail"
  );

  mutated = encoded.bytes;
  mutated[4] = 2;
  require(
      decodeFileTransferData(mutated).error == FileTransferDataCodecError::UnsupportedVersion,
      "unknown data codec version should fail"
  );

  mutated = encoded.bytes;
  mutated[5] = 255;
  require(
      decodeFileTransferData(mutated).error == FileTransferDataCodecError::InvalidKind,
      "unknown data message kind should fail"
  );

  mutated = encoded.bytes;
  mutated[6] = 1;
  require(
      decodeFileTransferData(mutated).error == FileTransferDataCodecError::InvalidFlags,
      "unknown data flags should fail"
  );

  mutated = encoded.bytes;
  mutated[11] ^= 1;
  require(
      decodeFileTransferData(mutated).error == FileTransferDataCodecError::LengthMismatch,
      "mutated body length should fail"
  );

  for (std::size_t length = 0; length < encoded.bytes.size(); ++length) {
    const auto decoded = decodeFileTransferData(std::span(encoded.bytes).first(length));
    require(!decoded.ok(), "every strict prefix should fail closed");
  }
}

void routesOnlyFromSourceToTarget()
{
  const FileTransferDataMessage message = FileTransferDataBegin{route()};
  auto result = resolveFileTransferDataDestination(message, "source");
  require(result.ok() && result.destinationScreen == "target", "source data should route to target");

  result = resolveFileTransferDataDestination(message, "target");
  require(result.error == FileTransferRouteError::InvalidSenderRole, "target cannot originate transfer data");

  result = resolveFileTransferDataDestination(message, "intruder");
  require(result.error == FileTransferRouteError::SenderNotParticipant, "third party cannot inject transfer data");
}

void exposesProtocolThirteenFeatureGate()
{
  require(kProtocolMinorVersion == 13, "current protocol should negotiate minor 13");
  require(kProtocolFileTransferControlMinorVersion == 12, "file control feature gate should remain minor 12");
  require(kProtocolFileTransferDataMinorVersion == 13, "file data feature gate should be minor 13");
  require(std::string_view(kMsgDFileTransferData, 4) == "DFTD", "file data message code should be DFTD");
}

} // namespace

int main()
{
  allMessageKindsRoundTrip();
  frameRoundTripsAcrossFragmentedReads();
  rejectsInvalidRoutesAndChunksBeforeWriting();
  rejectsOversizedFrameBeforePayloadRead();
  rejectsTruncatedAndMalformedFrames();
  rejectsMutatedPayloads();
  routesOnlyFromSourceToTarget();
  exposesProtocolThirteenFeatureGate();
  std::cout << "PASS: FileTransferDataProtocolTests\n";
  return 0;
}
