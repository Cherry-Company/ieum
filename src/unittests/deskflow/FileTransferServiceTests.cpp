/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferPlatform.h"
#include "deskflow/FileTransferService.h"

#include "base/Log.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
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

TransferId fixedId()
{
  TransferId id{};
  for (std::size_t index = 0; index < id.size(); ++index) {
    id[index] = static_cast<std::uint8_t>(index + 1);
  }
  return id;
}

std::vector<std::uint8_t> bytes(std::string_view value)
{
  return {value.begin(), value.end()};
}

FileTransferSourceCandidate candidate(std::string name, std::string_view contents)
{
  FileTransferSourceCandidate source;
  source.path = std::filesystem::current_path() / "virtual-source.bin";
  source.identity.bytes[0] = 0x42;
  source.identity.length = 1;
  source.size = contents.size();
  source.lastWriteTime = 1234;
  source.name = std::move(name);
  return source;
}

struct FakePlatformState
{
  std::map<std::string, std::vector<std::uint8_t>> sourceBytes;
  std::vector<std::uint8_t> publishedBytes;
  bool readerOpened = false;
  bool writerCreated = false;
  bool writerBegan = false;
  bool writerFinished = false;
};

class FakeReader final : public IFileTransferReader
{
public:
  explicit FakeReader(std::vector<std::uint8_t> source) : m_source(std::move(source))
  {
  }

  FileTransferReadResult readNext() override
  {
    if (m_complete) {
      return {.error = FileTransferIoError::WrongPhase};
    }
    m_complete = true;
    FileTransferDigest digest{};
    digest.fill(0x5a);
    return {.offset = 0, .bytes = std::move(m_source), .complete = true, .sha256 = digest};
  }

private:
  std::vector<std::uint8_t> m_source;
  bool m_complete = false;
};

class FakeWriter final : public IFileTransferWriter
{
public:
  explicit FakeWriter(std::shared_ptr<FakePlatformState> state) : m_state(std::move(state))
  {
  }

  FileTransferWriterMutation begin(const FileTransferDataBegin &) override
  {
    if (m_state->writerBegan) {
      return {.error = FileTransferIoError::WrongPhase};
    }
    m_state->writerBegan = true;
    return {};
  }

  FileTransferWriterMutation writeChunk(const FileTransferDataChunk &message) override
  {
    if (!m_state->writerBegan || m_state->writerFinished) {
      return {.error = FileTransferIoError::WrongPhase};
    }
    m_state->publishedBytes.insert(m_state->publishedBytes.end(), message.bytes.begin(), message.bytes.end());
    return {};
  }

  FileTransferWriterMutation endItem(const FileTransferDataItemEnd &message) override
  {
    if (!m_state->writerBegan || message.size != m_state->publishedBytes.size()) {
      return {.error = FileTransferIoError::IntegrityMismatch};
    }
    return {};
  }

  FileTransferWriterMutation finish(const FileTransferDataFinish &) override
  {
    if (!m_state->writerBegan || m_state->writerFinished) {
      return {.error = FileTransferIoError::WrongPhase};
    }
    m_state->writerFinished = true;
    return {.publishedPaths = {std::filesystem::current_path() / "received.bin"}};
  }

private:
  std::shared_ptr<FakePlatformState> m_state;
};

class FakeFileTransferPlatform final : public IFileTransferPlatform
{
public:
  explicit FakeFileTransferPlatform(std::shared_ptr<FakePlatformState> state) : m_state(std::move(state))
  {
  }

  FileTransferSourceInspection
  inspectSources(const std::vector<std::filesystem::path> &paths, std::size_t maxItems) override
  {
    if (paths.empty()) {
      return {.error = FileTransferIoError::EmptyPaths};
    }
    if (paths.size() > maxItems) {
      return {.error = FileTransferIoError::TooManyPaths};
    }
    return {.error = FileTransferIoError::OpenFailed};
  }

  FileTransferReaderOpenResult openReader(FileTransferSourceCandidate source, std::size_t chunkBytes) override
  {
    const auto found = m_state->sourceBytes.find(source.name);
    if (chunkBytes == 0 || found == m_state->sourceBytes.end()) {
      return {.error = FileTransferIoError::OpenFailed};
    }
    m_state->readerOpened = true;
    return {.reader = std::make_unique<FakeReader>(found->second)};
  }

  FileTransferWriterCreateResult createWriter(std::filesystem::path destination, FileTransferOffer) override
  {
    if (destination.empty() || !destination.is_absolute()) {
      return {.error = FileTransferIoError::InvalidDestination};
    }
    m_state->writerCreated = true;
    return {.writer = std::make_unique<FakeWriter>(m_state)};
  }

  std::optional<TransferId> randomId() override
  {
    return fixedId();
  }

private:
  std::shared_ptr<FakePlatformState> m_state;
};

FileTransferService makeSender(
    const std::shared_ptr<FakePlatformState> &state, bool &authorized,
    std::vector<FileTransferControlMessage> &controls, std::vector<FileTransferDataMessage> &data
)
{
  return FileTransferService(
      FileTransferServiceOptions{
          .localScreen = "windows",
          .authorizeFileTransfer = [&] { return authorized; },
          .sendControl =
              [&](const FileTransferControlMessage &message) {
                controls.push_back(message);
                return true;
              },
          .sendData =
              [&](const FileTransferDataMessage &message) {
                data.push_back(message);
                return true;
              },
          .platform = std::make_unique<FakeFileTransferPlatform>(state),
      }
  );
}

FileTransferService makeReceiver(
    const std::shared_ptr<FakePlatformState> &state, bool &authorized, bool receiveEnabled,
    std::vector<FileTransferControlMessage> &controls
)
{
  return FileTransferService(
      FileTransferServiceOptions{
          .localScreen = "mac",
          .destinationDirectory = std::filesystem::current_path() / "received",
          .receiveEnabled = receiveEnabled,
          .authorizeFileTransfer = [&] { return authorized; },
          .sendControl =
              [&](const FileTransferControlMessage &message) {
                controls.push_back(message);
                return true;
              },
          .sendData = [](const FileTransferDataMessage &) { return false; },
          .platform = std::make_unique<FakeFileTransferPlatform>(state),
      }
  );
}

void transfersThroughInjectedPlatforms()
{
  constexpr std::string_view payload = "portable transfer payload";
  auto senderState = std::make_shared<FakePlatformState>();
  auto receiverState = std::make_shared<FakePlatformState>();
  senderState->sourceBytes.emplace("한글 file.bin", bytes(payload));

  bool senderAuthorized = true;
  bool receiverAuthorized = true;
  std::vector<FileTransferControlMessage> senderControl;
  std::vector<FileTransferControlMessage> receiverControl;
  std::vector<FileTransferDataMessage> senderData;
  auto sender = makeSender(senderState, senderAuthorized, senderControl, senderData);
  auto receiver = makeReceiver(receiverState, receiverAuthorized, true, receiverControl);

  require(sender.offerLocalFiles("mac", {candidate("한글 file.bin", payload)}), "portable sender should offer");
  require(receiver.handleControl(senderControl.front()), "portable receiver should accept");
  require(sender.handleControl(receiverControl.front()), "portable sender should apply acceptance");
  require(senderState->readerOpened, "sender platform should open a reader");
  require(receiverState->writerCreated, "receiver platform should create a writer");

  std::size_t delivered = 0;
  while (delivered < senderData.size()) {
    require(receiver.handleData(senderData[delivered++]), "receiver should apply begin");
  }
  require(sender.processNextOutgoingChunk(), "sender should emit source data");
  while (delivered < senderData.size()) {
    require(receiver.handleData(senderData[delivered++]), "receiver should apply ordered data");
  }

  require(receiverState->writerFinished, "receiver should publish after finish");
  require(receiverState->publishedBytes == bytes(payload), "published bytes should match source bytes");
  require(receiverControl.size() == 2, "receiver should report completion");
  require(sender.handleControl(receiverControl.back()), "sender should apply completion");
  require(sender.activeSessionCount() == 0 && receiver.activeSessionCount() == 0, "sessions should be released");
}

void rejectsWhenReceiveOptInIsDisabled()
{
  auto state = std::make_shared<FakePlatformState>();
  bool authorized = true;
  std::vector<FileTransferControlMessage> controls;
  auto receiver = makeReceiver(state, authorized, false, controls);
  const FileTransferOffer offer{
      .id = fixedId(),
      .sourceScreen = "windows",
      .targetScreen = "mac",
      .items = {{.index = 0, .name = "private.txt", .size = 7}},
  };

  require(receiver.handleControl(FileTransferControlMessage{offer}), "disabled receiver should reject cleanly");
  require(!state->writerCreated, "disabled receiver must not create a writer");
  const auto *decision = std::get_if<FileTransferDecision>(&controls.front());
  require(
      decision != nullptr && decision->decision == FileTransferDecisionValue::Reject, "receiving must remain opt-in"
  );
}

void rejectsMissingAuthorization()
{
  auto state = std::make_shared<FakePlatformState>();
  constexpr std::string_view payload = "locked";
  state->sourceBytes.emplace("locked.txt", bytes(payload));
  std::vector<FileTransferControlMessage> controls;
  std::vector<FileTransferDataMessage> data;
  FileTransferService sender(
      FileTransferServiceOptions{
          .localScreen = "windows",
          .sendControl =
              [&](const FileTransferControlMessage &message) {
                controls.push_back(message);
                return true;
              },
          .sendData =
              [&](const FileTransferDataMessage &message) {
                data.push_back(message);
                return true;
              },
          .platform = std::make_unique<FakeFileTransferPlatform>(state),
      }
  );

  require(!sender.offerLocalFiles("mac", {candidate("locked.txt", payload)}), "missing authorization must fail");
  require(controls.empty() && data.empty(), "unauthorized sender must emit no frames");
  require(sender.activeSessionCount() == 0, "unauthorized sender must create no session");
}

void stopsBeforeFirstChunkWhenAuthorizationIsWithdrawn()
{
  auto state = std::make_shared<FakePlatformState>();
  constexpr std::string_view payload = "revoked sender";
  state->sourceBytes.emplace("sender.bin", bytes(payload));
  bool authorized = true;
  std::vector<FileTransferControlMessage> controls;
  std::vector<FileTransferDataMessage> data;
  auto sender = makeSender(state, authorized, controls, data);

  require(sender.offerLocalFiles("mac", {candidate("sender.bin", payload)}), "authorized offer should succeed");
  const auto *offer = std::get_if<FileTransferOffer>(&controls.front());
  require(offer != nullptr, "first sender control should be an offer");
  require(
      sender.handleControl(
          FileTransferControlMessage{FileTransferDecision{
              .id = offer->id,
              .sourceScreen = offer->sourceScreen,
              .targetScreen = offer->targetScreen,
              .decision = FileTransferDecisionValue::Accept,
          }}
      ),
      "accepted sender should start"
  );
  require(data.size() == 1 && std::holds_alternative<FileTransferDataBegin>(data.front()), "only begin precedes data");

  authorized = false;
  require(!sender.processNextOutgoingChunk(), "withdrawn authorization should stop the chunk");
  require(data.size() == 1 && sender.activeSessionCount() == 0, "withdrawal should emit no bytes and erase state");
  const auto *result = std::get_if<FileTransferResult>(&controls.back());
  require(
      result != nullptr && result->code == FileTransferResultCode::AuthorizationFailed,
      "sender should report authorization failure"
  );
}

void stopsReceiveWhenAuthorizationIsWithdrawn()
{
  auto state = std::make_shared<FakePlatformState>();
  bool authorized = true;
  std::vector<FileTransferControlMessage> controls;
  auto receiver = makeReceiver(state, authorized, true, controls);
  const FileTransferOffer offer{
      .id = fixedId(),
      .sourceScreen = "windows",
      .targetScreen = "mac",
      .items = {{.index = 0, .name = "receiver.bin", .size = 8}},
  };
  require(receiver.handleControl(FileTransferControlMessage{offer}), "authorized receiver should accept");

  authorized = false;
  const FileTransferDataRoute route{
      .id = offer.id,
      .sourceScreen = offer.sourceScreen,
      .targetScreen = offer.targetScreen,
  };
  require(
      !receiver.handleData(FileTransferDataMessage{FileTransferDataBegin{.route = route}}),
      "withdrawn authorization should reject incoming bytes"
  );
  require(receiver.activeSessionCount() == 0, "withdrawal should erase receiver state");
  const auto *result = std::get_if<FileTransferResult>(&controls.back());
  require(
      result != nullptr && result->code == FileTransferResultCode::AuthorizationFailed,
      "receiver should report authorization failure"
  );
}

} // namespace

int main()
{
  Log log;
  transfersThroughInjectedPlatforms();
  rejectsWhenReceiveOptInIsDisabled();
  rejectsMissingAuthorization();
  stopsBeforeFirstChunkWhenAuthorizationIsWithdrawn();
  stopsReceiveWhenAuthorizationIsWithdrawn();
  std::cout << "PASS: FileTransferServiceTests\n";
  return 0;
}
