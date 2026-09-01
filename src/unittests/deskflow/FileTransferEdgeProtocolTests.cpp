/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferEdgeProtocol.h"

#include "deskflow/ProtocolTypes.h"
#include "io/IStream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
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
    std::memcpy(buffer, chunk.data(), count);
    chunk.erase(0, count);
    if (chunk.empty()) {
      m_chunks.pop_front();
    }
    return static_cast<std::uint32_t>(count);
  }

  void write(const void *buffer, std::uint32_t size) override
  {
    ++m_writeCalls;
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

  [[nodiscard]] const std::string &written() const noexcept
  {
    return m_written;
  }

  [[nodiscard]] std::size_t writeCalls() const noexcept
  {
    return m_writeCalls;
  }

  [[nodiscard]] std::size_t readCalls() const noexcept
  {
    return m_readCalls;
  }

  [[nodiscard]] std::uint32_t largestRead() const noexcept
  {
    return m_largestRead;
  }

private:
  std::deque<std::string> m_chunks;
  std::string m_written;
  std::size_t m_writeCalls = 0;
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

FileTransferEdgeTargetRequest request()
{
  return {
      .requestId = transferId(),
      .sourceScreen = "macbook",
      .direction = Direction::Right,
      .x = 1440,
      .y = 450,
  };
}

std::string raw(std::initializer_list<std::uint8_t> values)
{
  return {reinterpret_cast<const char *>(values.begin()), values.size()};
}

void frameRoundTripsAcrossFragmentedReads()
{
  MemoryStream output;
  const FileTransferEdgeMessage original = request();
  const auto write = writeFileTransferEdgeFrame(&output, original);
  require(write.ok(), "valid edge message should frame");
  require(output.writeCalls() == 1, "an edge frame must be emitted as one transport packet");
  require(output.written().starts_with("DFTE"), "edge frame should use the DFTE message code");

  MemoryStream input;
  const auto &wire = output.written();
  input.setReadChunks({wire.substr(4, 1), wire.substr(5, 2), wire.substr(7, 3), wire.substr(10)});
  const auto read = readFileTransferEdgeFrame(&input);
  require(read.ok(), "fragmented edge frame should decode");
  require(read.message == original, "edge framing should preserve the message");
}

void invalidEncodeWritesNothing()
{
  auto invalid = request();
  invalid.requestId = {};
  MemoryStream stream;
  const auto write = writeFileTransferEdgeFrame(&stream, FileTransferEdgeMessage{invalid});
  require(write.error == FileTransferEdgeCodecError::EmptyRequestId, "invalid edge request should fail");
  require(stream.written().empty(), "invalid edge request must not write a partial frame");
}

void rejectsOversizedFrameBeforePayloadRead()
{
  MemoryStream stream;
  stream.setReadChunks({raw({0x00, 0x00, 0x10, 0x01}), std::string(4097, 'x')});
  const auto read = readFileTransferEdgeFrame(&stream);
  require(read.error == FileTransferEdgeFrameError::FrameTooLarge, "4097-byte edge frame should fail");
  require(stream.readCalls() == 1 && stream.largestRead() == 4, "oversized payload must not be requested");
}

void rejectsTruncatedAndMalformedFrames()
{
  MemoryStream shortLength;
  shortLength.setReadChunks({raw({0x00, 0x00, 0x00})});
  require(
      readFileTransferEdgeFrame(&shortLength).error == FileTransferEdgeFrameError::TruncatedLength,
      "short edge length should fail"
  );

  MemoryStream shortPayload;
  shortPayload.setReadChunks({raw({0x00, 0x00, 0x00, 0x04}), "bad"});
  require(
      readFileTransferEdgeFrame(&shortPayload).error == FileTransferEdgeFrameError::TruncatedPayload,
      "short edge payload should fail"
  );

  MemoryStream malformed;
  malformed.setReadChunks({raw({0x00, 0x00, 0x00, 0x04}), "nope"});
  const auto read = readFileTransferEdgeFrame(&malformed);
  require(read.error == FileTransferEdgeFrameError::InvalidPayload, "malformed edge payload should fail");
  require(read.codecError == FileTransferEdgeCodecError::Truncated, "inner edge error should be retained");
}

void exposesProtocolFourteenFeatureGate()
{
  require(kProtocolMinorVersion == 14, "current protocol should negotiate minor 14");
  require(kProtocolFileTransferEdgeMinorVersion == 14, "edge discovery gate should be minor 14");
  require(std::string_view(kMsgDFileTransferEdge, 4) == "DFTE", "edge message code should be DFTE");
}

} // namespace

int main()
{
  frameRoundTripsAcrossFragmentedReads();
  invalidEncodeWritesNothing();
  rejectsOversizedFrameBeforePayloadRead();
  rejectsTruncatedAndMalformedFrames();
  exposesProtocolFourteenFeatureGate();
  std::cout << "PASS: FileTransferEdgeProtocolTests\n";
  return 0;
}
