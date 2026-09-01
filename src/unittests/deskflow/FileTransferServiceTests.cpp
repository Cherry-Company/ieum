/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferPlatform.h"
#include "deskflow/FileTransferService.h"

#include "base/Log.h"

#ifdef _WIN32
#include "platform/MSWindowsFileTransferPlatform.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
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
    std::vector<FileTransferSourceCandidate> sources;
    sources.reserve(paths.size());
    for (const auto &path : paths) {
      const auto name = path.filename().string();
      const auto found = m_state->sourceBytes.find(name);
      if (found == m_state->sourceBytes.end()) {
        return {.error = FileTransferIoError::OpenFailed};
      }
      auto source = candidate(name, {});
      source.path = path;
      source.size = found->second.size();
      sources.push_back(std::move(source));
    }
    return {.sources = std::move(sources)};
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

FileTransferService makeEdgeSender(
    const std::shared_ptr<FakePlatformState> &state, bool &authorized,
    std::vector<FileTransferEdgeMessage> &edgeMessages, std::vector<FileTransferControlMessage> &controls
)
{
  return FileTransferService(
      FileTransferServiceOptions{
          .localScreen = "windows-client",
          .authorizeFileTransfer = [&] { return authorized; },
          .sendControl =
              [&](const FileTransferControlMessage &message) {
                controls.push_back(message);
                return true;
              },
          .sendData = [](const FileTransferDataMessage &) { return true; },
          .sendEdge =
              [&](const FileTransferEdgeMessage &message) {
                edgeMessages.push_back(message);
                return true;
              },
          .platform = std::make_unique<FakeFileTransferPlatform>(state),
      }
  );
}

void resolvesSecondaryEdgeDropBeforeOffering()
{
  auto state = std::make_shared<FakePlatformState>();
  state->sourceBytes.emplace("edge file.txt", bytes("edge payload"));
  bool authorized = true;
  std::vector<FileTransferEdgeMessage> edges;
  std::vector<FileTransferControlMessage> controls;
  auto service = makeEdgeSender(state, authorized, edges, controls);

  const auto active =
      static_cast<std::uint32_t>(DirectionMask::LeftMask) | static_cast<std::uint32_t>(DirectionMask::BottomMask);
  require(
      service.handleEdgeMessage(FileTransferEdgeMessage{FileTransferEdgeCapabilities{.activeSides = active}}),
      "capabilities should apply"
  );
  require(service.activeSides() == active, "service should expose active edge sides");
  require(
      service.beginEdgeDrop(
          FileTransferEdgeDrop{
              .direction = Direction::Left,
              .x = 0,
              .y = 400,
              .paths = {std::filesystem::current_path() / "edge file.txt"},
          }
      ),
      "secondary edge drop should request a target"
  );
  require(edges.size() == 1 && service.pendingEdgeRequestCount() == 1, "one target request should be pending");
  require(controls.empty() && !state->readerOpened, "source contents must stay closed before target acceptance");
  require(
      !service.beginEdgeDrop(
          FileTransferEdgeDrop{
              .direction = Direction::Left,
              .paths = {std::filesystem::current_path() / "edge file.txt"},
          }
      ),
      "a second drop must not replace a pending request"
  );

  const auto request = std::get<FileTransferEdgeTargetRequest>(edges.front());
  auto mismatchedId = request.requestId;
  ++mismatchedId[0];
  require(
      !service.handleEdgeMessage(
          FileTransferEdgeMessage{FileTransferEdgeTargetResponse{
              .requestId = mismatchedId,
              .sourceScreen = request.sourceScreen,
              .status = FileTransferEdgeStatus::NoTarget,
          }}
      ),
      "stale response should be ignored"
  );
  require(service.pendingEdgeRequestCount() == 1, "stale response must not cancel the current request");

  require(
      service.handleEdgeMessage(
          FileTransferEdgeMessage{FileTransferEdgeTargetResponse{
              .requestId = request.requestId,
              .sourceScreen = request.sourceScreen,
              .status = FileTransferEdgeStatus::Resolved,
              .targetScreen = "mac-server",
          }}
      ),
      "matching resolved response should create an offer"
  );
  require(service.pendingEdgeRequestCount() == 0, "resolved request should leave no pending state");
  require(controls.size() == 1 && std::holds_alternative<FileTransferOffer>(controls.front()), "offer should follow");
  require(!state->readerOpened, "source contents must remain closed until the peer accepts the offer");
}

void clearsPendingEdgeDropOnTerminalPaths()
{
  auto beginPending = [](FileTransferService &service) {
    require(
        service.beginEdgeDrop(
            FileTransferEdgeDrop{
                .direction = Direction::Right,
                .x = 1919,
                .y = 500,
                .paths = {std::filesystem::current_path() / "terminal.txt"},
            }
        ),
        "terminal-path fixture should become pending"
    );
  };

  {
    auto state = std::make_shared<FakePlatformState>();
    state->sourceBytes.emplace("terminal.txt", bytes("payload"));
    bool authorized = true;
    std::vector<FileTransferEdgeMessage> edges;
    std::vector<FileTransferControlMessage> controls;
    auto service = makeEdgeSender(state, authorized, edges, controls);
    beginPending(service);
    const auto request = std::get<FileTransferEdgeTargetRequest>(edges.front());
    require(
        service.handleEdgeMessage(
            FileTransferEdgeMessage{FileTransferEdgeTargetResponse{
                .requestId = request.requestId,
                .sourceScreen = request.sourceScreen,
                .status = FileTransferEdgeStatus::NoTarget,
            }}
        ),
        "no-target should be handled"
    );
    require(service.pendingEdgeRequestCount() == 0 && controls.empty(), "no-target should clear without an offer");
  }

  {
    auto state = std::make_shared<FakePlatformState>();
    state->sourceBytes.emplace("terminal.txt", bytes("payload"));
    bool authorized = true;
    std::vector<FileTransferEdgeMessage> edges;
    std::vector<FileTransferControlMessage> controls;
    auto service = makeEdgeSender(state, authorized, edges, controls);
    beginPending(service);
    const auto request = std::get<FileTransferEdgeTargetRequest>(edges.front());
    service.expirePendingEdgeRequest(request.requestId);
    require(service.pendingEdgeRequestCount() == 0, "five-second expiry callback should clear matching state");
    require(
        !service.handleEdgeMessage(
            FileTransferEdgeMessage{FileTransferEdgeTargetResponse{
                .requestId = request.requestId,
                .sourceScreen = request.sourceScreen,
                .status = FileTransferEdgeStatus::Resolved,
                .targetScreen = "late-target",
            }}
        ),
        "late response should be ignored"
    );
  }

  {
    auto state = std::make_shared<FakePlatformState>();
    state->sourceBytes.emplace("terminal.txt", bytes("payload"));
    bool authorized = true;
    std::vector<FileTransferEdgeMessage> edges;
    std::vector<FileTransferControlMessage> controls;
    auto service = makeEdgeSender(state, authorized, edges, controls);
    beginPending(service);
    const auto request = std::get<FileTransferEdgeTargetRequest>(edges.front());
    authorized = false;
    require(
        !service.handleEdgeMessage(
            FileTransferEdgeMessage{FileTransferEdgeTargetResponse{
                .requestId = request.requestId,
                .sourceScreen = request.sourceScreen,
                .status = FileTransferEdgeStatus::Resolved,
                .targetScreen = "mac-server",
            }}
        ),
        "authorization withdrawal should reject resolution"
    );
    require(service.pendingEdgeRequestCount() == 0 && controls.empty(), "withdrawal should clear pending state");
  }
}

void resolvesPrimaryEdgeDropSynchronously()
{
  auto state = std::make_shared<FakePlatformState>();
  state->sourceBytes.emplace("primary.txt", bytes("payload"));
  bool authorized = true;
  std::vector<FileTransferControlMessage> controls;
  FileTransferService service(
      FileTransferServiceOptions{
          .localScreen = "windows-primary",
          .authorizeFileTransfer = [&] { return authorized; },
          .sendControl =
              [&](const FileTransferControlMessage &message) {
                controls.push_back(message);
                return true;
              },
          .sendData = [](const FileTransferDataMessage &) { return true; },
          .resolveLocalTarget = [](Direction direction, std::int32_t, std::int32_t) -> std::optional<EdgeTarget> {
            return EdgeTarget{.direction = direction, .screen = "mac-client"};
          },
          .platform = std::make_unique<FakeFileTransferPlatform>(state),
      }
  );

  require(
      service.beginEdgeDrop(
          FileTransferEdgeDrop{
              .direction = Direction::Right,
              .paths = {std::filesystem::current_path() / "primary.txt"},
          }
      ),
      "primary should resolve its edge locally"
  );
  require(service.pendingEdgeRequestCount() == 0, "primary resolution should not create network pending state");
  require(controls.size() == 1 && std::holds_alternative<FileTransferOffer>(controls.front()), "primary should offer");
}

#ifdef _WIN32
class DiskFixture final
{
public:
  DiskFixture()
  {
    const auto base = std::filesystem::temp_directory_path();
    for (std::uint32_t attempt = 0; attempt < 100; ++attempt) {
      m_path = base / ("ieum-portable-service-" + std::to_string(GetCurrentProcessId()) + "-" +
                       std::to_string(GetTickCount64()) + "-" + std::to_string(attempt));
      std::error_code error;
      if (std::filesystem::create_directory(m_path, error)) {
        return;
      }
    }
    throw std::runtime_error("could not create a unique disk fixture");
  }

  ~DiskFixture()
  {
    std::error_code error;
    std::filesystem::remove_all(m_path, error);
  }

  [[nodiscard]] const std::filesystem::path &path() const noexcept
  {
    return m_path;
  }

private:
  std::filesystem::path m_path;
};

void writeDiskFile(const std::filesystem::path &path, std::string_view contents)
{
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  if (!stream) {
    throw std::runtime_error("could not write disk fixture");
  }
}

std::string readDiskFile(const std::filesystem::path &path)
{
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void transfersWindowsDiskWithoutDeletingTheSource()
{
  constexpr std::string_view payload = "Windows adapter round trip";
  DiskFixture fixture;
  const auto sourcePath = fixture.path() / L"한글 source.bin";
  const auto destination = fixture.path() / L"received";
  writeDiskFile(sourcePath, payload);

  auto senderPlatform = std::make_unique<MSWindowsFileTransferPlatform>();
  const auto inspected = senderPlatform->inspectSources({sourcePath}, 100);
  require(inspected.ok() && inspected.sources.size() == 1, "Windows disk source should inspect");

  bool authorized = true;
  std::vector<FileTransferControlMessage> senderControl;
  std::vector<FileTransferControlMessage> receiverControl;
  std::vector<FileTransferDataMessage> senderData;
  FileTransferService sender(
      FileTransferServiceOptions{
          .localScreen = "windows",
          .authorizeFileTransfer = [&] { return authorized; },
          .sendControl =
              [&](const FileTransferControlMessage &message) {
                senderControl.push_back(message);
                return true;
              },
          .sendData =
              [&](const FileTransferDataMessage &message) {
                senderData.push_back(message);
                return true;
              },
          .platform = std::move(senderPlatform),
      }
  );
  FileTransferService receiver(
      FileTransferServiceOptions{
          .localScreen = "mac",
          .destinationDirectory = destination,
          .receiveEnabled = true,
          .authorizeFileTransfer = [&] { return authorized; },
          .sendControl =
              [&](const FileTransferControlMessage &message) {
                receiverControl.push_back(message);
                return true;
              },
          .sendData = [](const FileTransferDataMessage &) { return false; },
          .platform = std::make_unique<MSWindowsFileTransferPlatform>(),
      }
  );

  require(sender.offerLocalFiles("mac", inspected.sources), "Windows disk sender should offer");
  require(receiver.handleControl(senderControl.front()), "Windows disk receiver should accept");
  require(sender.handleControl(receiverControl.front()), "Windows disk sender should begin");
  std::size_t delivered = 0;
  while (delivered < senderData.size()) {
    require(receiver.handleData(senderData[delivered++]), "Windows disk receiver should apply begin");
  }
  while (sender.processNextOutgoingChunk()) {
    while (delivered < senderData.size()) {
      require(receiver.handleData(senderData[delivered++]), "Windows disk receiver should apply data");
    }
  }
  while (delivered < senderData.size()) {
    require(receiver.handleData(senderData[delivered++]), "Windows disk receiver should apply final data");
  }

  require(receiverControl.size() == 2, "Windows disk receiver should report completion");
  require(sender.handleControl(receiverControl.back()), "Windows disk sender should apply completion");
  require(readDiskFile(sourcePath) == payload, "file transfer must preserve source bytes");
  require(readDiskFile(destination / sourcePath.filename()) == payload, "Windows disk destination should match");
}
#endif

} // namespace

int main()
{
  Log log;
  transfersThroughInjectedPlatforms();
  rejectsWhenReceiveOptInIsDisabled();
  rejectsMissingAuthorization();
  stopsBeforeFirstChunkWhenAuthorizationIsWithdrawn();
  stopsReceiveWhenAuthorizationIsWithdrawn();
  resolvesSecondaryEdgeDropBeforeOffering();
  clearsPendingEdgeDropOnTerminalPaths();
  resolvesPrimaryEdgeDropSynchronously();
#ifdef _WIN32
  transfersWindowsDiskWithoutDeletingTheSource();
#endif
  std::cout << "PASS: FileTransferServiceTests\n";
  return 0;
}
