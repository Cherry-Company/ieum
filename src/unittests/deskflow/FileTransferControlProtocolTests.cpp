/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferControlProtocol.h"
#include "deskflow/FileTransferControlRouting.h"
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
  return id;
}

FileTransferOffer offer()
{
  return {
      .id = transferId(),
      .sourceScreen = "source",
      .targetScreen = "target",
      .items = {{0, "한글.txt", 9}},
  };
}

std::string bytes(std::initializer_list<std::uint8_t> values)
{
  return {reinterpret_cast<const char *>(values.begin()), values.size()};
}

void frameRoundTripsAcrossFragmentedReads()
{
  MemoryStream output;
  const FileTransferControlMessage original = offer();
  const auto write = writeFileTransferControlFrame(&output, original);
  require(write.ok(), "valid control message should frame");
  require(output.written().starts_with("DFTC"), "frame should use the DFTC message code");
  require(output.written().size() > 8, "frame should contain an outer length and payload");

  MemoryStream input;
  const auto &wire = output.written();
  input.setReadChunks({wire.substr(4, 1), wire.substr(5, 2), wire.substr(7, 4), wire.substr(11)});
  const auto read = readFileTransferControlFrame(&input);
  require(read.ok(), "fragmented frame should decode");
  require(read.message == original, "framing should preserve the control message");
}

void invalidEncodeDoesNotWrite()
{
  MemoryStream stream;
  auto invalid = offer();
  invalid.id = {};
  const auto write = writeFileTransferControlFrame(&stream, FileTransferControlMessage{invalid});
  require(write.error == FileTransferCodecError::PolicyRejected, "invalid offer should fail before framing");
  require(stream.written().empty(), "invalid offer should not write partial bytes");
}

void rejectsOversizedFrameBeforePayloadRead()
{
  MemoryStream stream;
  stream.setReadChunks({bytes({0x00, 0x01, 0x00, 0x01}), std::string(65'537, 'x')});
  const auto read = readFileTransferControlFrame(&stream);
  require(read.error == FileTransferFrameError::FrameTooLarge, "oversized outer frame should fail");
  require(stream.readCalls() == 1, "oversized frame should stop after its length");
  require(stream.largestRead() == 4, "oversized frame should never request its payload");
}

void rejectsTruncatedAndMalformedFrames()
{
  MemoryStream shortLength;
  shortLength.setReadChunks({bytes({0x00, 0x00, 0x00})});
  require(
      readFileTransferControlFrame(&shortLength).error == FileTransferFrameError::TruncatedLength,
      "short outer length should fail"
  );

  MemoryStream shortPayload;
  shortPayload.setReadChunks({bytes({0x00, 0x00, 0x00, 0x04}), "bad"});
  require(
      readFileTransferControlFrame(&shortPayload).error == FileTransferFrameError::TruncatedPayload,
      "short payload should fail"
  );

  MemoryStream malformed;
  malformed.setReadChunks({bytes({0x00, 0x00, 0x00, 0x04}), "nope"});
  const auto read = readFileTransferControlFrame(&malformed);
  require(read.error == FileTransferFrameError::InvalidPayload, "malformed codec payload should fail");
  require(read.codecError == FileTransferCodecError::Truncated, "inner codec error should be retained");
}

void resolvesDirectionByMessageRole()
{
  auto route = resolveFileTransferControlDestination(FileTransferControlMessage{offer()}, "source");
  require(route.ok() && route.destinationScreen == "target", "offer should travel source to target");

  const FileTransferDecision decision{transferId(), "source", "target", FileTransferDecisionValue::Accept};
  route = resolveFileTransferControlDestination(FileTransferControlMessage{decision}, "target");
  require(route.ok() && route.destinationScreen == "source", "decision should travel target to source");

  const FileTransferCancel cancel{transferId(), "source", "target", FileTransferCancelReason::SenderCancelled};
  route = resolveFileTransferControlDestination(FileTransferControlMessage{cancel}, "source");
  require(route.ok() && route.destinationScreen == "target", "source cancellation should reach target");
  route = resolveFileTransferControlDestination(FileTransferControlMessage{cancel}, "target");
  require(route.ok() && route.destinationScreen == "source", "target cancellation should reach source");

  const FileTransferResult result{
      transferId(), "source", "target", FileTransferResultStatus::Completed, FileTransferResultCode::None
  };
  route = resolveFileTransferControlDestination(FileTransferControlMessage{result}, "target");
  require(route.ok() && route.destinationScreen == "source", "receiver result should reach source");
}

void rejectsSpoofedMessageRoles()
{
  auto route = resolveFileTransferControlDestination(FileTransferControlMessage{offer()}, "target");
  require(route.error == FileTransferRouteError::InvalidSenderRole, "target cannot originate an offer");

  const FileTransferDecision decision{transferId(), "source", "target", FileTransferDecisionValue::Accept};
  route = resolveFileTransferControlDestination(FileTransferControlMessage{decision}, "source");
  require(route.error == FileTransferRouteError::InvalidSenderRole, "source cannot decide its own offer");

  const FileTransferCancel cancel{transferId(), "source", "target", FileTransferCancelReason::SenderCancelled};
  route = resolveFileTransferControlDestination(FileTransferControlMessage{cancel}, "intruder");
  require(route.error == FileTransferRouteError::SenderNotParticipant, "third party cannot inject control messages");
}

void exposesProtocolTwelveFeatureGate()
{
  require(kProtocolMinorVersion == 14, "current protocol should negotiate minor 14");
  require(kProtocolFileTransferControlMinorVersion == 12, "file control feature gate should be minor 12");
  require(std::string_view(kMsgDFileTransferControl, 4) == "DFTC", "file control message code should be DFTC");
}

} // namespace

int main()
{
  frameRoundTripsAcrossFragmentedReads();
  invalidEncodeDoesNotWrite();
  rejectsOversizedFrameBeforePayloadRead();
  rejectsTruncatedAndMalformedFrames();
  resolvesDirectionByMessageRole();
  rejectsSpoofedMessageRoles();
  exposesProtocolTwelveFeatureGate();
  std::cout << "PASS: FileTransferControlProtocolTests\n";
  return 0;
}
