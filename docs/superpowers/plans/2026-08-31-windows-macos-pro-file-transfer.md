<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Windows–macOS Pro Local File Transfer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Ship v0.1.0-alpha.22 with native, licensed Windows↔macOS edge-drag file transfer in both directions and a serverless USD 30 one-time GitHub Sponsors fulfillment flow.

**Architecture:** Keep the bounded alpha.21 offer/data protocol, extract its orchestration into a platform-neutral FileTransferService, and inject secure Windows or macOS filesystem adapters. Protocol 1.14 adds a small edge-capability/request/response channel so secondary clients can ask the authenticated server which configured neighbor owns a dropped edge. The app generates a random claim code for GitHub Sponsors transaction metadata and email fulfillment; payment confirmation and Ed25519 issuance remain manual and private.

**Tech Stack:** C++20, Qt 6 Core/Widgets/Network/Test, Windows OLE and BCrypt, macOS AppKit/POSIX, OpenSSL EVP/RAND, CMake/Ninja/CTest, Python 3 private issuer, GitHub GraphQL/Actions/Releases.

**Spec:** docs/superpowers/specs/2026-08-31-windows-macos-pro-file-transfer-design.md

## Global Constraints

- Release target is exactly 0.1.0-alpha.22 and protocol minor is exactly 1.14.
- Windows→macOS, macOS→Windows, and existing Windows→Windows must work without swapping server/client roles.
- Transfers copy ordinary regular files only; no source deletion, directories, links, aliases, reparse points, packages, cloud stubs, or special files.
- Both endpoints require TLS, explicit local send/receive opt-in, and a currently valid file-transfer entitlement.
- One session and one pending edge-target request are allowed per endpoint; a pending target request expires after five seconds.
- Incoming bytes stage privately, verify size and SHA-256, and publish with collision-safe names without overwriting.
- Linux remains unsupported for Pro Local edge transfer in alpha.22.
- The public offer is USD 30 one-time, limited in availability, and grants a perpetual file-transfer entitlement only.
- The public fulfillment address is easecompany@protonmail.ch.
- The Ed25519 private key, keygen source/binary, receipts, issued licenses, and ledger never enter Git, CI, packages, logs, or release assets.
- New committed source files use the Ieum contributor SPDX header from the design spec, never the historical private identity.
- Use build-local for focused Windows builds. Run only the affected targets after each task, then the focused cross-feature set before the release gate.

---

## File and responsibility map

### Portable core

- src/lib/deskflow/FileTransferPlatform.h — OS-neutral source inspection, reader, writer, error, and random-ID contracts.
- src/lib/deskflow/FileTransferService.h/.cpp — authorization, session orchestration, pending target resolution, and control/data flow.
- src/lib/deskflow/FileTransferEdgeCodec.h/.cpp — bounded protocol-1.14 edge capability/request/response payload.
- src/lib/deskflow/FileTransferEdgeProtocol.h/.cpp — DFTE stream framing.
- src/lib/deskflow/FileTransferEdgeEvent.h — event-queue ownership wrapper.
- src/lib/deskflow/FileTransferEdgeDrop.h — platform-neutral edge drop point/path callback.

### Platform implementations

- src/lib/platform/MSWindowsFileTransferPlatform.h/.cpp — adapters over existing Windows source/reader/writer implementation and BCrypt IDs.
- src/lib/platform/OSXFileTransferPlatform.h/.cpp — secure POSIX/macOS source inspection, reader, writer, SHA-256, staging, and random IDs.
- src/lib/deskflow/win32/MSWindowsEdgeDropTarget.* and MSWindowsEdgeDropHost.* — OLE drop windows producing generic path drops on primary or secondary screens.
- src/lib/platform/OSXFileTransferEdgeDropHost.h/.mm — AppKit edge windows and Finder local-file URL extraction.
- src/lib/platform/FileTransferPlatformFactory.h/.cpp — selected-OS platform factory and QString-to-filesystem path conversion.

### Connection and app integration

- src/lib/server/ClientProxy1_14.h/.cpp — server-side DFTE wire endpoint.
- src/lib/client/ServerProxy.* and src/lib/client/Client.* — client-side DFTE endpoint and events.
- src/lib/server/Server.* — authenticated target resolution and edge availability.
- src/lib/deskflow/ClientApp.* and ServerApp.* — portable service lifecycle and both-role edge hosts.
- src/lib/deskflow/IPlatformScreen.h and PlatformScreen.* — optional generic install/configure/uninstall edge-host hooks.
- src/lib/platform/MSWindowsScreen.* and OSXScreen.* — concrete host ownership.

### Purchase and fulfillment

- src/lib/licensing/ProEarlyAccessClaim.h/.cpp — 128-bit claim generation, validation, Sponsors checkout URL, and mailto URL.
- src/lib/gui/dialogs/SettingsDialog.* and SettingsDialog.ui — CTA, claim display/copy/email, and macOS Files controls.
- src/lib/gui/dialogs/ProLicenseUiPolicy.* — Windows/macOS-supported policy and CTA visibility.
- C:\Users\LYR\Ieum-License-Authority-PRIVATE\keygen.py and test_keygen.py — private transaction/claim-hash ledger support.
- GitHub Sponsors account — published non-recurring USD 30 tier with fulfillment copy.

### Documentation and release

- README.md, README.en.md, README.zh-CN.md — roadmap/table/diagram and public offer.
- docs/dev/pro_local_file_transfer.md — cross-platform use and fulfillment guide.
- docs/release/sponsorship-impact.md — exact alpha.22 transparency note.
- translations/deskflow_*.ts — generated source strings with complete Korean translations.
- docs/codegraph/* — regenerated graph.
- VERSION — 0.1.0-alpha.22.

---

### Task 1: Add portable file-transfer contracts and service

**Files:**

- Create: src/lib/deskflow/FileTransferPlatform.h
- Create: src/lib/deskflow/FileTransferService.h
- Create: src/lib/deskflow/FileTransferService.cpp
- Create: src/unittests/deskflow/FileTransferServiceTests.cpp
- Modify: src/lib/deskflow/CMakeLists.txt
- Modify: src/unittests/deskflow/CMakeLists.txt

**Interfaces:**

- Produces FileTransferIoError, FileTransferSourceInspection, IFileTransferReader, IFileTransferWriter, IFileTransferPlatform, FileTransferServiceOptions, and FileTransferService.
- FileTransferService initially exposes offerLocalFiles, handleControl, handleData, processNextOutgoingChunk, and activeSessionCount. Task 5 adds edge-target methods without changing these signatures.

- [ ] **Step 1: Write the failing portable service test**

Create a fake platform that returns an in-memory source reader and collecting writer, then port the alpha.21 happy path and authorization-withdrawal cases.

~~~cpp
class FakeFileTransferPlatform final : public IFileTransferPlatform
{
public:
  FileTransferSourceInspection inspectSources(
      const std::vector<std::filesystem::path> &paths, std::size_t maxItems
  ) override;
  FileTransferReaderOpenResult openReader(
      FileTransferSourceCandidate source, std::size_t chunkBytes
  ) override;
  FileTransferWriterCreateResult createWriter(
      std::filesystem::path destination, FileTransferOffer offer
  ) override;
  std::optional<TransferId> randomId() override;
};

FileTransferService sender(FileTransferServiceOptions{
    .localScreen = "windows",
    .destinationDirectory = {},
    .receiveEnabled = false,
    .authorizeFileTransfer = [&] { return authorized; },
    .sendControl = [&](const FileTransferControlMessage &message) {
      controls.push_back(message);
      return true;
    },
    .sendData = [&](const FileTransferDataMessage &message) {
      data.push_back(message);
      return true;
    },
    .platform = std::make_unique<FakeFileTransferPlatform>(),
});
~~~

- [ ] **Step 2: Register and run the new test to prove it fails**

Run:

~~~powershell
cmake --build build-local --target FileTransferServiceTests
~~~

Expected: build fails because FileTransferPlatform.h and FileTransferService.h do not exist.

- [ ] **Step 3: Define the portable contracts**

Use these exact public shapes; implementations may add private helpers but not OS types.

~~~cpp
enum class FileTransferIoError {
  None, EmptyPaths, TooManyPaths, EmptyPath, RelativePath, OpenFailed,
  MetadataReadFailed, LinkOrSpecialFile, FilenameConversionFailed,
  SourceChanged, ReadFailed, InvalidOffer, InvalidDestination,
  DestinationDenied, WrongPhase, RouteMismatch, WrongItem, WrongOffset,
  SizeExceeded, WriteFailed, HashFailed, IntegrityMismatch, PublishFailed,
  RandomUnavailable,
};

struct FileTransferSourceInspection {
  FileTransferIoError error = FileTransferIoError::None;
  std::optional<std::size_t> itemPosition;
  std::vector<FileTransferSourceCandidate> sources;
  bool ok() const noexcept { return error == FileTransferIoError::None; }
};

class IFileTransferReader {
public:
  virtual ~IFileTransferReader() = default;
  virtual FileTransferReadResult readNext() = 0;
};

class IFileTransferWriter {
public:
  virtual ~IFileTransferWriter() = default;
  virtual FileTransferWriterMutation begin(const FileTransferDataBegin &) = 0;
  virtual FileTransferWriterMutation writeChunk(const FileTransferDataChunk &) = 0;
  virtual FileTransferWriterMutation endItem(const FileTransferDataItemEnd &) = 0;
  virtual FileTransferWriterMutation finish(const FileTransferDataFinish &) = 0;
};

class IFileTransferPlatform {
public:
  virtual ~IFileTransferPlatform() = default;
  virtual FileTransferSourceInspection inspectSources(
      const std::vector<std::filesystem::path> &, std::size_t maxItems
  ) = 0;
  virtual FileTransferReaderOpenResult openReader(
      FileTransferSourceCandidate, std::size_t chunkBytes
  ) = 0;
  virtual FileTransferWriterCreateResult createWriter(
      std::filesystem::path, FileTransferOffer
  ) = 0;
  virtual std::optional<TransferId> randomId() = 0;
};
~~~

- [ ] **Step 4: Port the Windows service orchestration without Windows dependencies**

Copy session/control/data behavior from MSWindowsFileTransferService, replace concrete readers/writers and BCrypt generation with IFileTransferPlatform, preserve per-operation authorization checks, and keep the production chunk size at 60 KiB.

~~~cpp
struct FileTransferServiceOptions {
  using AuthorizationCheck = std::function<bool()>;
  using ControlSender = std::function<bool(const FileTransferControlMessage &)>;
  using DataSender = std::function<bool(const FileTransferDataMessage &)>;

  std::string localScreen;
  std::filesystem::path destinationDirectory;
  bool receiveEnabled = false;
  AuthorizationCheck authorizeFileTransfer;
  ControlSender sendControl;
  DataSender sendData;
  std::unique_ptr<IFileTransferPlatform> platform;
  IEventQueue *events = nullptr;
  void *eventTarget = nullptr;
};
~~~

- [ ] **Step 5: Build and run the portable service tests**

Run:

~~~powershell
cmake --build build-local --target FileTransferServiceTests
ctest --test-dir build-local/src/unittests -R "^FileTransferServiceTests$" --output-on-failure
~~~

Expected: PASS for complete transfer, receive opt-in rejection, missing authorization, authorization withdrawal before first chunk, and authorization withdrawal during receive.

- [ ] **Step 6: Commit the portable service**

~~~powershell
git add src/lib/deskflow/FileTransferPlatform.* src/lib/deskflow/FileTransferService.* src/lib/deskflow/CMakeLists.txt src/unittests/deskflow/FileTransferServiceTests.cpp src/unittests/deskflow/CMakeLists.txt
git commit -m "refactor(file-transfer): extract portable service"
~~~

---

### Task 2: Adapt and preserve the Windows filesystem implementation

**Files:**

- Create: src/lib/platform/MSWindowsFileTransferPlatform.h
- Create: src/lib/platform/MSWindowsFileTransferPlatform.cpp
- Modify: src/lib/platform/MSWindowsFileTransferSource.h/.cpp
- Modify: src/lib/platform/MSWindowsFileTransferIo.h/.cpp
- Modify: src/lib/platform/CMakeLists.txt
- Modify: src/unittests/platform/MSWindowsFileTransferSourceTests.cpp
- Modify: src/unittests/platform/MSWindowsFileTransferIoTests.cpp
- Modify: src/unittests/platform/CMakeLists.txt
- Modify: src/unittests/deskflow/FileTransferServiceTests.cpp

**Interfaces:**

- Consumes IFileTransferPlatform from Task 1.
- Produces MSWindowsFileTransferPlatform final, with no changes to the Windows safety semantics.

- [ ] **Step 1: Write failing adapter and regression tests**

Add tests that call the common interface rather than concrete Windows result types and preserve all existing cases.

~~~cpp
MSWindowsFileTransferPlatform platform;
auto inspected = platform.inspectSources({sourcePath}, 100);
require(inspected.ok(), "ordinary file should inspect");

auto opened = platform.openReader(inspected.sources.front(), 2);
require(opened.ok(), "stable source should open");
require(opened.reader->readNext().bytes == std::vector<std::uint8_t>({'a', 'b'}), "first chunk");
~~~

Add a service-level Windows disk round trip using one platform instance per endpoint and verify source bytes remain unchanged.

- [ ] **Step 2: Run the focused Windows tests to verify the adapter is missing**

Run:

~~~powershell
cmake --build build-local --target MSWindowsFileTransferSourceTests MSWindowsFileTransferIoTests FileTransferServiceTests
~~~

Expected: build fails because MSWindowsFileTransferPlatform is undefined.

- [ ] **Step 3: Implement thin Windows adapters**

Map every existing Windows source/I/O error to FileTransferIoError. Wrap MSWindowsFileTransferReader and MSWindowsFileTransferWriter behind the portable virtual interfaces. Generate non-zero IDs with BCryptGenRandom and reject an all-zero result.

~~~cpp
class MSWindowsFileTransferPlatform final : public IFileTransferPlatform
{
public:
  FileTransferSourceInspection inspectSources(
      const std::vector<std::filesystem::path> &paths, std::size_t maxItems
  ) override;
  FileTransferReaderOpenResult openReader(
      FileTransferSourceCandidate source, std::size_t chunkBytes
  ) override;
  FileTransferWriterCreateResult createWriter(
      std::filesystem::path destination, FileTransferOffer offer
  ) override;
  std::optional<TransferId> randomId() override;
};
~~~

- [ ] **Step 4: Run Windows regression and service tests**

Run:

~~~powershell
cmake --build build-local --target MSWindowsFileTransferSourceTests MSWindowsFileTransferIoTests FileTransferServiceTests
ctest --test-dir build-local/src/unittests -R "^(MSWindowsFileTransferSourceTests|MSWindowsFileTransferIoTests|FileTransferServiceTests)$" --output-on-failure
~~~

Expected: all three targets PASS, including reparse rejection, source revalidation, staging cleanup, digest mismatch, and collision-safe publication.

- [ ] **Step 5: Commit the Windows adapter**

~~~powershell
git add src/lib/platform/MSWindowsFileTransferPlatform.* src/lib/platform/MSWindowsFileTransferSource.* src/lib/platform/MSWindowsFileTransferIo.* src/lib/platform/CMakeLists.txt src/unittests/platform src/unittests/deskflow/FileTransferServiceTests.cpp
git commit -m "refactor(file-transfer): adapt Windows storage"
~~~

---

### Task 3: Add bounded protocol-1.14 edge discovery frames

**Files:**

- Create: src/lib/deskflow/FileTransferEdgeCodec.h
- Create: src/lib/deskflow/FileTransferEdgeCodec.cpp
- Create: src/lib/deskflow/FileTransferEdgeProtocol.h
- Create: src/lib/deskflow/FileTransferEdgeProtocol.cpp
- Create: src/lib/deskflow/FileTransferEdgeEvent.h
- Create: src/unittests/deskflow/FileTransferEdgeCodecTests.cpp
- Create: src/unittests/deskflow/FileTransferEdgeProtocolTests.cpp
- Modify: src/lib/deskflow/ProtocolTypes.h/.cpp
- Modify: src/lib/deskflow/CMakeLists.txt
- Modify: src/unittests/deskflow/CMakeLists.txt

**Interfaces:**

- Produces FileTransferEdgeCapabilities, FileTransferEdgeTargetRequest, FileTransferEdgeTargetResponse, FileTransferEdgeMessage, codec/frame results, kMsgDFileTransferEdge, and kProtocolFileTransferEdgeMinorVersion.

- [ ] **Step 1: Write failing codec tests**

Use exact round trips and rejection cases for magic, version, kind, flags, enum, screen length, direction, active-side bits, empty/all-zero IDs, trailing data, truncation, and the 4096-byte frame limit.

~~~cpp
const FileTransferEdgeTargetRequest request{
    .requestId = transferId(0x21),
    .sourceScreen = "macbook",
    .direction = Direction::Left,
    .x = 0,
    .y = 720,
};
const auto encoded = encodeFileTransferEdge(FileTransferEdgeMessage{request});
QVERIFY(encoded.ok());
QCOMPARE(*decodeFileTransferEdge(encoded.bytes).message, FileTransferEdgeMessage{request});
~~~

- [ ] **Step 2: Run the new targets and verify they fail**

~~~powershell
cmake --build build-local --target FileTransferEdgeCodecTests FileTransferEdgeProtocolTests
~~~

Expected: build fails because the edge codec/protocol files do not exist.

- [ ] **Step 3: Implement the exact bounded message model**

~~~cpp
enum class FileTransferEdgeStatus : std::uint8_t {
  Resolved = 1,
  NoTarget = 2,
  Rejected = 3,
};

struct FileTransferEdgeCapabilities {
  std::uint32_t activeSides = 0;
  bool operator==(const FileTransferEdgeCapabilities &) const = default;
};

struct FileTransferEdgeTargetRequest {
  TransferId requestId{};
  std::string sourceScreen;
  Direction direction = Direction::NoDirection;
  std::int32_t x = 0;
  std::int32_t y = 0;
  bool operator==(const FileTransferEdgeTargetRequest &) const = default;
};

struct FileTransferEdgeTargetResponse {
  TransferId requestId{};
  std::string sourceScreen;
  FileTransferEdgeStatus status = FileTransferEdgeStatus::Rejected;
  std::string targetScreen;
  bool operator==(const FileTransferEdgeTargetResponse &) const = default;
};

using FileTransferEdgeMessage = std::variant<
    FileTransferEdgeCapabilities,
    FileTransferEdgeTargetRequest,
    FileTransferEdgeTargetResponse>;
~~~

Encode internal magic IFE1, codec version 1, one-byte kind, one-byte zero flags, and big-endian bounded fields. Permit an empty target only when status is not Resolved. Permit only LeftMask, RightMask, TopMask, and BottomMask bits.

- [ ] **Step 4: Add the DFTE stream frame and protocol version**

Set kProtocolMinorVersion to 14, add kProtocolFileTransferEdgeMinorVersion = 14, and define kMsgDFileTransferEdge = "DFTE%S". Follow the existing DFTC/DFTD four-byte code plus uint32 length framing and reject payloads above 4096 bytes before allocation.

- [ ] **Step 5: Run codec and framing tests**

~~~powershell
cmake --build build-local --target FileTransferEdgeCodecTests FileTransferEdgeProtocolTests
ctest --test-dir build-local/src/unittests -R "^FileTransferEdge(Codec|Protocol)Tests$" --output-on-failure
~~~

Expected: both targets PASS.

- [ ] **Step 6: Commit protocol 1.14**

~~~powershell
git add src/lib/deskflow/FileTransferEdge* src/lib/deskflow/ProtocolTypes.* src/lib/deskflow/CMakeLists.txt src/unittests/deskflow/FileTransferEdge* src/unittests/deskflow/CMakeLists.txt
git commit -m "feat(protocol): add edge target discovery"
~~~

---

### Task 4: Wire authenticated edge discovery through client and server

**Files:**

- Create: src/lib/server/ClientProxy1_14.h
- Create: src/lib/server/ClientProxy1_14.cpp
- Create: src/unittests/server/FileTransferEdgeRoutingTests.cpp
- Modify: src/lib/server/BaseClientProxy.h
- Modify: src/lib/server/ClientProxyUnknown.cpp
- Modify: src/lib/server/Server.h/.cpp
- Modify: src/lib/server/CMakeLists.txt
- Modify: src/lib/client/ServerProxy.h/.cpp
- Modify: src/lib/client/Client.h/.cpp
- Modify: src/lib/base/EventTypes.h
- Modify: src/unittests/server/CMakeLists.txt

**Interfaces:**

- Consumes FileTransferEdgeMessage and DFTE framing from Task 3.
- Produces BaseClientProxy::sendFileTransferEdge, Client::sendFileTransferEdge, Server::handleFileTransferEdge, Server::fileTransferActiveSidesFor, and Server::resolveFileTransferEdgeTarget(source, direction, x, y).

- [ ] **Step 1: Write failing authenticated routing tests**

Exercise a client whose canonical name differs from its claimed source and verify rejection. Exercise a real source with left neighbor and verify the response target. Exercise an unconfigured edge and verify NoTarget. Exercise capability masks and a protocol-1.13 client receiving no DFTE frame.

~~~cpp
const FileTransferEdgeTargetRequest spoofed{
    .requestId = transferId(0x31),
    .sourceScreen = "other-client",
    .direction = Direction::Left,
    .x = 0,
    .y = 200,
};
QVERIFY(!server.handleFileTransferEdge(sender, FileTransferEdgeMessage{spoofed}));
~~~

- [ ] **Step 2: Run the server target to verify the new proxy is absent**

~~~powershell
cmake --build build-local --target FileTransferEdgeRoutingTests
~~~

Expected: build fails on missing ClientProxy1_14 and server edge methods.

- [ ] **Step 3: Add ClientProxy1_14 and client ServerProxy support**

ClientProxy1_14 returns protocol minor 14, parses DFTE, validates frames, and calls Server::handleFileTransferEdge. BaseClientProxy gets a default false send method. ServerProxy parses DFTE into EventTypes::FileTransferEdgeReceived and exposes sendFileTransferEdge with a protocol-minor check. Client forwards the send method.

~~~cpp
virtual bool sendFileTransferEdge(
    const deskflow::filetransfer::FileTransferEdgeMessage &
) { return false; }
~~~

- [ ] **Step 4: Generalize topology resolution and capabilities**

Replace the primary-only resolver with:

~~~cpp
std::optional<deskflow::filetransfer::EdgeTarget> resolveFileTransferEdgeTarget(
    BaseClientProxy *source, Direction direction, std::int32_t x, std::int32_t y
) const;

std::uint32_t fileTransferActiveSidesFor(BaseClientProxy *source) const;
bool handleFileTransferEdge(
    BaseClientProxy *sender,
    const deskflow::filetransfer::FileTransferEdgeMessage &message
);
~~~

Validate sourceScreen against getName(sender), resolve with getNeighbor(source, direction, x, y), require target protocol 1.13 for data, and send the response directly to sender. Send capabilities after protocol-1.14 handshake and whenever configuration changes.

- [ ] **Step 5: Run routing plus existing protocol tests**

~~~powershell
cmake --build build-local --target FileTransferEdgeRoutingTests FileTransferControlProtocolTests FileTransferDataProtocolTests
ctest --test-dir build-local/src/unittests -R "^(FileTransferEdgeRoutingTests|FileTransferControlProtocolTests|FileTransferDataProtocolTests)$" --output-on-failure
~~~

Expected: all targets PASS and existing 1.12/1.13 frames remain byte-compatible.

- [ ] **Step 6: Commit the connection wiring**

~~~powershell
git add src/lib/server src/lib/client src/lib/base/EventTypes.h src/unittests/server
git commit -m "feat(file-transfer): resolve client edge targets"
~~~

---

### Task 5: Add pending target resolution and generic edge-drop input

**Files:**

- Create: src/lib/deskflow/FileTransferEdgeDrop.h
- Modify: src/lib/deskflow/FileTransferService.h/.cpp
- Modify: src/unittests/deskflow/FileTransferServiceTests.cpp
- Modify: src/lib/deskflow/CMakeLists.txt

**Interfaces:**

- Consumes edge messages from Task 3 and the portable platform from Task 1.
- Produces FileTransferEdgeDrop, FileTransferService::beginEdgeDrop, handleEdgeMessage, expirePendingEdgeRequest, and activeSides.

- [ ] **Step 1: Write failing pending-request tests**

Cover a primary synchronous resolve, a secondary async resolve, a second drop while pending, stale/mismatched response, NoTarget, five-second expiry, authorization withdrawal, and disconnect cleanup.

~~~cpp
QVERIFY(service.beginEdgeDrop(FileTransferEdgeDrop{
    .direction = Direction::Left,
    .x = 0,
    .y = 400,
    .paths = {std::filesystem::path("C:/files/한글.txt")},
}));
QCOMPARE(edgeRequests.size(), 1U);
QCOMPARE(service.pendingEdgeRequestCount(), 1U);

const auto request = std::get<FileTransferEdgeTargetRequest>(edgeRequests.front());
QVERIFY(service.handleEdgeMessage(FileTransferEdgeMessage{
    FileTransferEdgeTargetResponse{
        .requestId = request.requestId,
        .sourceScreen = "windows-client",
        .status = FileTransferEdgeStatus::Resolved,
        .targetScreen = "mac-server",
    },
}));
QCOMPARE(service.pendingEdgeRequestCount(), 0U);
QVERIFY(std::holds_alternative<FileTransferOffer>(controls.front()));
~~~

- [ ] **Step 2: Run the test and verify the API is missing**

~~~powershell
cmake --build build-local --target FileTransferServiceTests
~~~

Expected: build fails on beginEdgeDrop and handleEdgeMessage.

- [ ] **Step 3: Add generic drop and service options**

~~~cpp
struct FileTransferEdgeDrop {
  Direction direction = Direction::NoDirection;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::vector<std::filesystem::path> paths;
};

struct FileTransferServiceOptions {
  using LocalTargetResolver =
      std::function<std::optional<EdgeTarget>(Direction, std::int32_t, std::int32_t)>;
  using EdgeSender = std::function<bool(const FileTransferEdgeMessage &)>;
  // Task 1 fields remain unchanged.
  LocalTargetResolver resolveLocalTarget;
  EdgeSender sendEdge;
};
~~~

The implementation inspects paths first, retains at most one candidate vector, uses platform.randomId for the request, and never opens file contents until a peer accepts the resulting offer.

- [ ] **Step 4: Implement timeout and edge capabilities**

Store the current active-side mask from FileTransferEdgeCapabilities. Use an event-queue one-shot timer for five seconds and retain the request ID in the callback so an expired callback cannot cancel a newer request. expose:

~~~cpp
bool beginEdgeDrop(FileTransferEdgeDrop drop);
bool handleEdgeMessage(const FileTransferEdgeMessage &message);
void expirePendingEdgeRequest(const TransferId &requestId);
std::size_t pendingEdgeRequestCount() const noexcept;
std::uint32_t activeSides() const noexcept;
~~~

- [ ] **Step 5: Run portable service and codec tests**

~~~powershell
cmake --build build-local --target FileTransferServiceTests FileTransferEdgeCodecTests
ctest --test-dir build-local/src/unittests -R "^(FileTransferServiceTests|FileTransferEdgeCodecTests)$" --output-on-failure
~~~

Expected: PASS for every pending-request terminal path and the existing transfer path.

- [ ] **Step 6: Commit pending resolution**

~~~powershell
git add src/lib/deskflow/FileTransferEdgeDrop.h src/lib/deskflow/FileTransferService.* src/lib/deskflow/CMakeLists.txt src/unittests/deskflow/FileTransferServiceTests.cpp
git commit -m "feat(file-transfer): originate secondary edge drops"
~~~

---

### Task 6: Convert Windows edge drops and both app roles to the portable service

**Files:**

- Modify: src/lib/deskflow/win32/MSWindowsEdgeDropTarget.h/.cpp
- Modify: src/lib/deskflow/win32/MSWindowsEdgeDropHost.h/.cpp
- Modify: src/lib/platform/MSWindowsScreen.h/.cpp
- Modify: src/lib/deskflow/ClientApp.h/.cpp
- Modify: src/lib/deskflow/ServerApp.h/.cpp
- Modify: src/lib/deskflow/IPlatformScreen.h
- Modify: src/lib/deskflow/PlatformScreen.h/.cpp
- Create: src/lib/platform/FileTransferPlatformFactory.h
- Create: src/lib/platform/FileTransferPlatformFactory.cpp
- Modify: src/lib/deskflow/CMakeLists.txt
- Modify: src/lib/platform/CMakeLists.txt
- Modify: src/unittests/deskflow/MSWindowsEdgeDropTargetTests.cpp
- Modify: src/unittests/deskflow/MSWindowsEdgeDropHostTests.cpp
- Modify: src/unittests/deskflow/CMakeLists.txt
- Delete: src/lib/deskflow/win32/MSWindowsFileTransferService.h/.cpp
- Delete: src/unittests/deskflow/MSWindowsFileTransferServiceTests.cpp

**Interfaces:**

- Consumes FileTransferEdgeDrop and FileTransferService.
- Produces optional platform-screen hooks and createFileTransferPlatform.

- [ ] **Step 1: Rewrite Windows edge tests to expect paths and generic points**

The OLE target must load paths, enforce the item/path bounds, dwell on its hosted active side, and call one handler with Direction, x/y, and paths. It must always advertise copy, never move.

~~~cpp
std::optional<FileTransferEdgeDrop> handed;
callbacks.handoff = [&](FileTransferEdgeDrop drop) {
  handed = std::move(drop);
};
QVERIFY(SUCCEEDED(target->Drop(dataObject, 0, POINTL{0, 240}, &effect)));
QCOMPARE(effect, static_cast<DWORD>(DROPEFFECT_COPY));
QCOMPARE(handed->paths.size(), 1U);
QCOMPARE(handed->direction, Direction::Left);
~~~

- [ ] **Step 2: Run Windows edge and app builds to verify the old types still block**

~~~powershell
cmake --build build-local --target MSWindowsEdgeDropTargetTests MSWindowsEdgeDropHostTests deskflow-core
~~~

Expected: tests fail to compile until callbacks and app ownership are converted.

- [ ] **Step 3: Add default platform-screen hooks**

Define non-pure defaults so Linux implementations remain untouched:

~~~cpp
virtual bool installFileTransferEdgeDrop(
    deskflow::filetransfer::FileTransferEdgeDropHandler
) { return false; }
virtual bool configureFileTransferEdgeDrop(std::uint32_t) { return false; }
virtual void uninstallFileTransferEdgeDrop() noexcept {}
~~~

MSWindowsScreen overrides them, allows both primary and secondary screens, configures windows from the supplied active-side mask, and continues to require successful OleInitialize.

- [ ] **Step 4: Convert the OLE host to generic path handoff**

Remove EdgeTarget and FileTransferSourceCandidate from the Windows host callback. Keep source inspection in MSWindowsFileTransferPlatform so the drop host never opens files. Preserve three-pixel windows, 250 ms dwell, display-change rebuild, and cleanup.

- [ ] **Step 5: Add the selected-platform factory**

~~~cpp
std::unique_ptr<deskflow::filetransfer::IFileTransferPlatform>
createFileTransferPlatform();

std::filesystem::path fileTransferPathFromQString(const QString &value);

bool supportsFileTransferPlatform() noexcept;
~~~

On Windows return MSWindowsFileTransferPlatform and construct paths from toStdWString. Task 7 adds the macOS branch. Other platforms return null/false.

- [ ] **Step 6: Convert ServerApp and ClientApp**

Remove the Q_OS_WIN-only service lifecycle and concrete type. Both roles create FileTransferService when TLS, entitlement, and either send or receive opt-in are true. ServerApp supplies resolveLocalTarget using Server::resolveFileTransferEdgeTarget(m_primaryClient, ...). ClientApp supplies sendEdge through Client::sendFileTransferEdge and handles FileTransferEdgeReceived events. Both install the generic edge host when send is enabled and configure it from server/local active sides.

~~~cpp
std::unique_ptr<deskflow::filetransfer::FileTransferService> m_fileTransferService;
~~~

ClientApp must start the service when send is enabled even if receive is disabled. Stop paths uninstall edge windows before destroying the service.

- [ ] **Step 7: Delete the duplicate Windows service and run focused Windows tests**

~~~powershell
cmake --build build-local --target FileTransferServiceTests MSWindowsEdgeDropTargetTests MSWindowsEdgeDropHostTests deskflow-core deskflow
ctest --test-dir build-local/src/unittests -R "^(FileTransferServiceTests|MSWindowsEdgeDrop(Target|Host)Tests|MSWindowsFileTransfer(Source|Io)Tests)$" --output-on-failure
~~~

Expected: PASS and no MSWindowsFileTransferService reference remains under src.

- [ ] **Step 8: Commit Windows and app conversion**

~~~powershell
git add -A src/lib/deskflow src/lib/platform src/unittests/deskflow
git commit -m "feat(file-transfer): enable Windows client-originated drops"
~~~

---

### Task 7: Implement secure macOS file inspection, reading, and publication

**Files:**

- Create: src/lib/platform/OSXFileTransferPlatform.h
- Create: src/lib/platform/OSXFileTransferPlatform.cpp
- Create: src/unittests/platform/OSXFileTransferPlatformTests.cpp
- Modify: src/lib/platform/FileTransferPlatformFactory.cpp
- Modify: src/lib/platform/CMakeLists.txt
- Modify: src/unittests/platform/CMakeLists.txt

**Interfaces:**

- Consumes IFileTransferPlatform.
- Produces OSXFileTransferPlatform with identical portable behavior and macOS-specific link/staging safety.

- [ ] **Step 1: Write macOS-only failing tests**

Register the target only under APPLE. Cover ordinary and zero-byte files, Korean/spaced NFC names, decomposed input normalization, directory and symlink rejection, source replacement/change, wrong offsets, digest mismatch, mode-0700 staging, duplicate names, existing destination preservation, and destructor cleanup.

~~~cpp
OSXFileTransferPlatform platform;
auto inspected = platform.inspectSources({source}, 100);
QVERIFY(inspected.ok());
QCOMPARE(
    QString::fromUtf8(inspected.sources.front().name).normalized(QString::NormalizationForm_C),
    QString::fromUtf8(inspected.sources.front().name)
);
~~~

- [ ] **Step 2: Push the branch and run a macOS compile probe**

Run:

~~~powershell
git push -u origin feat/pro-cross-platform-transfer-alpha22
gh workflow run continuous-integration.yml --ref feat/pro-cross-platform-transfer-alpha22
~~~

Expected: the new OSXFileTransferPlatformTests target fails until the implementation exists. Record the run URL in the task notes, not source.

- [ ] **Step 3: Implement secure source inspection**

Use lstat/open with O_RDONLY | O_CLOEXEC | O_NOFOLLOW, fstat, S_ISREG, and stable device/inode/size/mtime fields stored in FileTransferSourceCandidate.identity. Reject relative/empty paths and more than 100 items. Normalize the basename to UTF-8 NFC through QString and reject slash, backslash, NUL, dot, dot-dot, or empty output.

- [ ] **Step 4: Implement the reader**

Open without following links, compare stable identity and metadata, read at most 60 KiB per call, update an EVP SHA-256 context, re-fstat at EOF, and return SourceChanged if device/inode/size/mtime differ. Never read before the remote decision is accepted.

- [ ] **Step 5: Implement the writer and random IDs**

Validate the offer with the existing policy. Open/create the destination directory, create .ieum-transfer-REQUESTID with mode 0700 and O_EXCL staging files with mode 0600, require sequential chunks, verify item size/digest, fsync/close, and rename to filename, filename (1), filename (2), and so on without overwriting. Use SecRandomCopyBytes for non-zero IDs. The writer destructor removes only its owned staging tree.

- [ ] **Step 6: Run macOS focused tests on both architectures**

Push the implementation and wait for macos-arm64 and macos-x64 jobs. Expected: OSXFileTransferPlatformTests and FileTransferServiceTests PASS and both application packages compile. Also run the Windows portable regression locally:

~~~powershell
cmake --build build-local --target FileTransferServiceTests
ctest --test-dir build-local/src/unittests -R "^FileTransferServiceTests$" --output-on-failure
~~~

- [ ] **Step 7: Commit the macOS storage adapter**

~~~powershell
git add src/lib/platform/OSXFileTransferPlatform.* src/lib/platform/FileTransferPlatformFactory.cpp src/lib/platform/CMakeLists.txt src/unittests/platform
git commit -m "feat(file-transfer): add secure macOS storage"
~~~

---

### Task 8: Add the native macOS edge-drop host

**Files:**

- Create: src/lib/platform/OSXFileTransferEdgeDropHost.h
- Create: src/lib/platform/OSXFileTransferEdgeDropHost.mm
- Create: src/unittests/platform/OSXFileTransferEdgeDropHostTests.mm
- Modify: src/lib/platform/OSXScreen.h
- Modify: src/lib/platform/OSXScreen.mm
- Modify: src/lib/platform/CMakeLists.txt
- Modify: src/unittests/platform/CMakeLists.txt

**Interfaces:**

- Consumes FileTransferEdgeDropHandler and active-side masks.
- Produces OSXFileTransferEdgeDropHost::configure, clear, and windowCount and OSXScreen platform-hook overrides.

- [ ] **Step 1: Write failing layout and pasteboard extraction tests**

Test one window per active side, top/bottom corner exclusion, multi-display bounding coordinates, AppKit-to-Ieum Y conversion, local file URL acceptance, remote URL/directory/package rejection, 100-item limit, copy semantics, and clear.

~~~objective-c++
NSArray<NSURL *> *urls = @[
  [NSURL fileURLWithPath:@"/tmp/한글 file.txt" isDirectory:NO]
];
const auto paths = extractOSXFileDropURLs(urls, 100);
QVERIFY(paths.ok());
QCOMPARE(paths.paths.size(), 1U);
~~~

- [ ] **Step 2: Run macOS CI to confirm the host is missing**

Push the failing test commit to the feature branch and dispatch CI. Expected: macOS compilation fails on the absent host, while Windows targets skip this test.

- [ ] **Step 3: Implement the AppKit host**

Create borderless, transparent, non-activating NSPanel instances at level NSStatusWindowLevel for active sides. Each content view registers NSPasteboardTypeFileURL, returns NSDragOperationCopy after the 250 ms dwell, extracts local file URLs on performDragOperation, converts the drop point to top-left desktop coordinates, and invokes exactly one FileTransferEdgeDropHandler. Do not make the app key, order front regardless, or intercept inactive sides.

- [ ] **Step 4: Integrate OSXScreen lifecycle**

OSXScreen owns the host, permits install only when enabled, rebuilds it after display reconfiguration, clears it on disable, and destroys it before AppKit/event teardown. The same hooks work for primary and secondary instances.

- [ ] **Step 5: Run macOS host, storage, service, and package jobs**

Expected on both macOS architectures:

~~~bash
cmake --build build --target OSXFileTransferEdgeDropHostTests OSXFileTransferPlatformTests FileTransferServiceTests
ctest --test-dir build/src/unittests -R '^(OSXFileTransfer|FileTransferService)' --output-on-failure
cmake --build build --target package
~~~

- [ ] **Step 6: Commit the native macOS host**

~~~powershell
git add src/lib/platform/OSXFileTransferEdgeDropHost.* src/lib/platform/OSXScreen.* src/lib/platform/CMakeLists.txt src/unittests/platform
git commit -m "feat(file-transfer): add macOS edge drops"
~~~

---

### Task 9: Enable Files settings and the Pro Early Access claim flow

**Files:**

- Create: src/lib/licensing/ProEarlyAccessClaim.h
- Create: src/lib/licensing/ProEarlyAccessClaim.cpp
- Create: src/unittests/licensing/ProEarlyAccessClaimTests.cpp
- Modify: src/lib/licensing/CMakeLists.txt
- Modify: src/unittests/licensing/CMakeLists.txt
- Modify: src/lib/common/UrlConstants.h
- Modify: src/gui/src/ProLicenseUiPolicy.h
- Modify: src/gui/src/ProLicenseUiPolicy.cpp
- Modify: src/gui/src/SettingsDialog.h
- Modify: src/gui/src/SettingsDialog.cpp
- Modify: src/gui/src/SettingsDialog.ui
- Modify: src/unittests/gui/ProLicenseUiPolicyTests.cpp
- Modify: src/unittests/gui/CMakeLists.txt

**Interfaces:**

- Add createProEarlyAccessClaim(entropy), isValidProEarlyAccessClaim(claim), proEarlyAccessSponsorUrl(claim), and proEarlyAccessMailtoUrl(claim).
- Extend ProFileTransferUiState with supportedPlatform and purchaseActionsVisible.
- Add enum class FileTransferPlatformSupport { Unsupported, Windows, MacOS }.

- [ ] **Step 1: Write failing deterministic claim-link tests**

Inject the 16 bytes 00 through 0f and require the exact lowercase claim
000102030405060708090a0b0c0d0e0f. Verify malformed or differently sized claims are rejected. Parse the sponsor URL and require exactly these query values:

~~~text
metadata_campaign=ieum_pro_local_ea
metadata_source=desktop_app
metadata_claim=000102030405060708090a0b0c0d0e0f
~~~

Also assert that the mailto link targets easecompany@protonmail.ch, identifies Ieum Pro Local Early Access in its subject, and includes the same claim without including any license key.

- [ ] **Step 2: Write failing Windows/macOS policy tests**

Cover unsupported platforms, Windows and macOS without a license, and Windows and macOS with a valid perpetual file-transfer entitlement. Require the Files tab on both supported platforms, locked controls plus purchase actions without a valid license, enabled file-transfer controls without purchase actions with a valid license, and no purchase actions on unsupported platforms.

- [ ] **Step 3: Build the focused tests and observe the expected failure**

~~~powershell
cmake --build build-local --target ProEarlyAccessClaimTests ProLicenseUiPolicyTests
~~~

Expected: compilation fails because the new claim helper and extended UI state do not exist.

- [ ] **Step 4: Implement claim generation and canonical URLs**

Use OpenSSL RAND_bytes for production entropy and lowercase hex encoding. Put the public constants in UrlConstants.h:

~~~text
https://github.com/sponsors/victoriousian/sponsorships
easecompany@protonmail.ch
~~~

Generate a fresh claim when Settings opens or when a malformed/empty claim must be replaced. Preserve one claim for the lifetime of the dialog. Build URLs through QUrl/QUrlQuery so every value is encoded once.

- [ ] **Step 5: Wire the settings controls**

Add these stable object names:

~~~text
btnGetProEarlyAccess
lineProEarlyAccessClaim
btnCopyProEarlyAccessClaim
btnEmailProLicenseRequest
~~~

Make the claim field read-only. The sponsor button opens the claim-tagged GitHub Sponsors URL, copy puts only the claim code on the clipboard, and email opens the prepared request. Display this locked-state copy:

~~~text
Pro Local Early Access — USD 30 one-time
Sponsor once to receive a perpetual Pro Local file-transfer license.
Licenses are issued manually after payment verification.
~~~

Hide all purchase widgets as soon as a valid file-transfer entitlement is loaded. Show the Files tab on Windows and macOS, and keep the actual file-transfer toggle gated by the entitlement.

- [ ] **Step 6: Run focused tests and build the GUI**

~~~powershell
cmake --build build-local --target ProEarlyAccessClaimTests ProLicenseUiPolicyTests ieum
ctest --test-dir build-local/src/unittests -R "^(ProEarlyAccessClaimTests|ProLicenseUiPolicyTests)$" --output-on-failure
~~~

- [ ] **Step 7: Commit the claim flow**

~~~powershell
git add src/lib/licensing src/lib/common/UrlConstants.h src/gui/src/ProLicenseUiPolicy.* src/gui/src/SettingsDialog.* src/unittests/licensing src/unittests/gui
git commit -m "feat(pro): add early-access sponsor flow"
~~~

---

### Task 10: Upgrade the private issuer and publish the one-time tier

**Private files outside Git:**

- Modify: C:\Users\LYR\Ieum-License-Authority-PRIVATE\keygen.py
- Modify: C:\Users\LYR\Ieum-License-Authority-PRIVATE\test_keygen.py
- Modify: C:\Users\LYR\Ieum-License-Authority-PRIVATE\README-PRIVATE.txt
- Rebuild: C:\Users\LYR\Ieum-License-Authority-PRIVATE\IeumLicenseKeygen.exe

**External state:**

- Create one published, non-recurring USD 30 GitHub Sponsors tier only if no matching one-time tier exists.

**Interfaces:**

- Extend issue_license with sponsor_tier and claim_code.
- Store sponsor tier, claim SHA-256, order reference, and issuance status in the private ledger.
- Never store the raw claim, license payload, signature, or complete license document in the ledger.

- [ ] **Step 1: Write failing private issuer tests**

Use the sample claim 000102030405060708090a0b0c0d0e0f. Assert that its SHA-256 digest is stored, the raw claim and complete license are absent from receipts and ledger entries, an empty claim is accepted for a verified direct purchase, malformed claims are rejected, and a second issuance using the same non-empty claim is rejected.

- [ ] **Step 2: Run the private tests and observe the expected failure**

~~~powershell
& 'C:\Users\LYR\Ieum-License-Authority-PRIVATE\.venv\Scripts\python.exe' -m unittest -v 'C:\Users\LYR\Ieum-License-Authority-PRIVATE\test_keygen.py'
~~~

Expected: new claim and sponsor-tier assertions fail before the issuer is changed.

- [ ] **Step 3: Implement the private issuer fields and GUI**

Add optional Sponsor tier and Claim code inputs. Normalize the claim to lowercase, validate exactly 32 hex characters when non-empty, reject reuse by comparing the digest against prior ledger entries, and write only the digest. Keep the existing recipient, order reference, expiry, signing, file-output, and private-key boundaries intact.

- [ ] **Step 4: Rebuild and smoke-test the owner-only executable**

~~~powershell
& 'C:\Users\LYR\Ieum-License-Authority-PRIVATE\.venv\Scripts\python.exe' -m unittest -v 'C:\Users\LYR\Ieum-License-Authority-PRIVATE\test_keygen.py'
& 'C:\Users\LYR\Ieum-License-Authority-PRIVATE\.venv\Scripts\pyinstaller.exe' --clean --noconfirm 'C:\Users\LYR\Ieum-License-Authority-PRIVATE\IeumLicenseKeygen.spec'
Copy-Item -LiteralPath 'C:\Users\LYR\Ieum-License-Authority-PRIVATE\dist\IeumLicenseKeygen.exe' -Destination 'C:\Users\LYR\Ieum-License-Authority-PRIVATE\IeumLicenseKeygen.exe' -Force
& 'C:\Users\LYR\Ieum-License-Authority-PRIVATE\IeumLicenseKeygen.exe' --self-test
~~~

Do not print or inspect the PEM, generated signatures, or issued license contents. Do not add any private-authority path to the public repository.

- [ ] **Step 5: Check whether the one-time tier already exists**

~~~powershell
gh api graphql -f query='query { user(login: "victoriousian") { sponsorsListing { tiers(first: 100) { nodes { id monthlyPriceInDollars isOneTime description } } } } }' --jq '.data.user.sponsorsListing.tiers.nodes[] | select(.isOneTime == true and .monthlyPriceInDollars == 30)'
~~~

If a matching Ieum Pro Local one-time tier exists, reuse it. Do not create a duplicate.

- [ ] **Step 6: Publish the tier only when absent**

Use the createSponsorsTier mutation with sponsorableLogin victoriousian, amount 30, isRecurring false, and publish true. Use exactly this description:

~~~markdown
**Ieum Pro Local Early Access — Perpetual File Transfer**

One USD 30 payment includes one perpetual Pro Local file-transfer entitlement for your own Windows and macOS computers. This is limited-time prerelease access, not all future Pro features. Delivery is manual after payment verification; no Ieum cloud account or instant activation.
~~~

Use exactly this welcome message:

~~~text
Thank you for supporting Ieum Pro Local Early Access.

Open Ieum → Preferences → Files and use “Get Pro Local Early Access” first whenever possible. Email your GitHub username and the app-generated claim code to easecompany@protonmail.ch. If you purchased directly and have no claim code, forward your GitHub Sponsors receipt instead. Your perpetual file-transfer license file will be issued manually after verification. Never post the license in a public issue.
~~~

Run the mutation as one PowerShell command with variables rather than interpolating GraphQL text:

~~~powershell
$tierDescription = @'
**Ieum Pro Local Early Access — Perpetual File Transfer**

One USD 30 payment includes one perpetual Pro Local file-transfer entitlement for your own Windows and macOS computers. This is limited-time prerelease access, not all future Pro features. Delivery is manual after payment verification; no Ieum cloud account or instant activation.
'@
$tierWelcome = @'
Thank you for supporting Ieum Pro Local Early Access.

Open Ieum → Preferences → Files and use “Get Pro Local Early Access” first whenever possible. Email your GitHub username and the app-generated claim code to easecompany@protonmail.ch. If you purchased directly and have no claim code, forward your GitHub Sponsors receipt instead. Your perpetual file-transfer license file will be issued manually after verification. Never post the license in a public issue.
'@
gh api graphql -f query='mutation($login:String!,$amount:Int!,$description:String!,$welcome:String!){createSponsorsTier(input:{sponsorableLogin:$login,amount:$amount,isRecurring:false,description:$description,welcomeMessage:$welcome,publish:true}){sponsorsTier{id monthlyPriceInDollars isOneTime description}}}' -F login='victoriousian' -F amount=30 -F description="$tierDescription" -F welcome="$tierWelcome"
~~~

- [ ] **Step 7: Verify public/private separation**

Confirm the live Sponsors page exposes the one-time tier and welcome path. Run git status and git grep in the public worktree to ensure no private key, private directory, ledger, generated license, or owner-only executable was added. The private issuer changes remain uncommitted outside the public Git repository.

---

### Task 11: Update release docs, translations, version, and code graph

**Files:**

- Modify: VERSION
- Modify: README.md
- Modify: README.en.md
- Modify: README.zh-CN.md
- Modify: docs/pro-file-transfer-license.md
- Modify: docs/sponsorship-impact.md
- Create: docs/releases/v0.1.0-alpha.22-acceptance.md
- Modify: src/gui/res/lang/*.ts
- Regenerate: docs/codegraph/*

- [ ] **Step 1: Set the alpha.22 version and release line**

Set VERSION to 0.1.0-alpha.22 and update every current-release reference to v0.1.0-alpha.22. Keep roadmap amounts based only on actual funds received; do not count the new offer as revenue before payment.

- [ ] **Step 2: Document the offer and architecture in all READMEs**

Explain that USD 30 is a one-time, limited-window Early Access purchase for a perpetual file-transfer entitlement, not a promise of all future Pro features. Explain the manual flow: use the in-app button, pay on GitHub Sponsors, email GitHub username plus claim code to easecompany@protonmail.ch, owner verifies the transaction, then emails the license file.

Use this Mermaid overview:

~~~mermaid
flowchart LR
  C["Community Core<br/>KVM · Clipboard · IME"] --> L["Pro Local alpha.22<br/>Offline perpetual file-transfer key"]
  S["USD 30 one-time<br/>GitHub Sponsors early access"] -->|"verified email fulfillment"| L
  L --> W["Windows"]
  L --> M["macOS"]
  W <-->|"TLS · direct edge drag"| M
  L -.-> H["Future Hosted Pro<br/>accounts · seats · relay · revocation"]
~~~

- [ ] **Step 3: Rewrite the public license/fulfillment guide**

Document installation, Files settings, claim generation, purchase, request email, manual payment verification, license-file import, Windows↔macOS copy semantics, duplicate naming, source preservation, local-only transfer, and troubleshooting. State that the app does not reveal a sponsor's private email and that no Ieum account/server is required for this Pro Local entitlement.

- [ ] **Step 4: Add the release acceptance evidence template**

Create rows for Windows-primary→macOS-secondary, macOS-secondary→Windows-primary, macOS-primary→Windows-secondary, and Windows-secondary→macOS-primary. Each row records source/destination versions, Korean/spaced filename, duplicate destination naming, bounded larger-file SHA-256, source preservation, entitlement rejection, operator, date, and evidence link.

- [ ] **Step 5: Update translations**

Add or refresh strings for the Files tab, Pro locked state, price, perpetual scope, manual verification, sponsor action, claim copy, email action, transfer progress, and error states in every shipped TS catalog. Run lrelease or the repository translation target and resolve missing/obsolete strings introduced by this feature.

- [ ] **Step 6: Regenerate and verify the code graph**

~~~powershell
uv run --no-project --python 3.13 python tools/generate-codegraph.py --root . --output docs/codegraph
uv run --no-project --python 3.13 python -m unittest discover -s tools/tests -p 'test_generate_codegraph.py' -v
uv run --no-project --python 3.13 python tools/generate-codegraph.py --root . --output docs/codegraph
git diff --exit-code -- docs/codegraph
~~~

Expected: generator tests pass and the second generation creates no additional code-graph diff.

- [ ] **Step 7: Run documentation and privacy checks**

~~~powershell
rg -n "alpha\.21|Windows-only|monthly.*30|all future Pro" README* docs src
rg -n "PRIVATE|BEGIN .*PRIVATE KEY|IeumLicenseKeygen|license-ledger|issued/" .
git diff --check
~~~

Review every match. Historical release notes may retain old version references; no current copy may misstate platform, price, scope, or fulfillment. No owner-only artifact may be tracked.

- [ ] **Step 8: Commit the release material**

~~~powershell
git add VERSION README.md README.en.md README.zh-CN.md docs src/gui/res/lang
git commit -m "docs: prepare Pro Local alpha22"
~~~

---

### Task 12: Prove both physical routes, merge, tag, and release

- [ ] **Step 1: Run the focused Windows gate**

~~~powershell
cmake --build build-local --target FileTransferServiceTests FileTransferEdgeCodecTests ProEarlyAccessClaimTests ProLicenseUiPolicyTests ieum
ctest --test-dir build-local/src/unittests -R "^(FileTransfer|ProEarlyAccessClaim|ProLicenseUiPolicy)" --output-on-failure
git diff --check
~~~

- [ ] **Step 2: Push the branch and open the pull request**

~~~powershell
git push -u origin feat/pro-cross-platform-transfer-alpha22
gh pr create --base ieum/main --head feat/pro-cross-platform-transfer-alpha22 --title "feat: ship cross-platform Pro Local file transfer" --body-file docs/releases/v0.1.0-alpha.22-acceptance.md
gh pr checks --watch
~~~

The PR body points reviewers to the acceptance matrix and describes the explicit hosted-macOS release-packaging exception already documented by the repository. Do not substitute a new runner topology.

- [ ] **Step 3: Record physical Windows/macOS acceptance**

On the MacBook Pro primary Apple lane, exercise both application roles and both directions:

1. Windows primary with macOS secondary: drag Windows→macOS and macOS→Windows.
2. macOS primary with Windows secondary: drag macOS→Windows and Windows→macOS.

For each direction record a Korean filename containing a space, an existing-destination duplicate, a bounded larger file with matching SHA-256, source-file preservation, and a rejected transfer after disabling/removing the file-transfer entitlement. Attach logs/screenshots or artifact links to the acceptance document and PR. Do not use the Mac mini unless the user explicitly authorizes the fallback.

- [ ] **Step 4: Merge only after evidence and required checks pass**

~~~powershell
gh pr checks --watch
gh pr merge --squash --delete-branch
git fetch origin ieum/main --tags
git switch --detach origin/ieum/main
~~~

Confirm the merged commit contains the acceptance evidence and VERSION 0.1.0-alpha.22.

- [ ] **Step 5: Create and push the annotated release tag**

~~~powershell
git tag -a v0.1.0-alpha.22 -m "Ieum v0.1.0-alpha.22"
git push origin v0.1.0-alpha.22
gh run list --workflow release.yml --limit 5
~~~

Wait for the tagged release workflow and GitHub release to complete. A source push or passing CI alone is not a release.

- [ ] **Step 6: Independently verify the published artifacts**

Create an explicit temporary directory named ieum-alpha22-public-verify, download the release assets there, verify the published checksums, inspect package/version names, and confirm the Windows and macOS artifacts contain no private issuer, private key, ledger, or generated license. Remove only that explicit temporary directory after verification; do not delete unrelated temporary files.

- [ ] **Step 7: Verify final state**

Confirm:

- origin/ieum/main contains the merged release commit.
- v0.1.0-alpha.22 resolves to that commit.
- the release is public and has verified Windows and macOS assets.
- the one-time USD 30 tier is live exactly once.
- the original worktree still contains its untouched untracked deskflow_kr_ime_spec_v0.1.md.
- the owner-only issuer remains outside Git.

---

## Execution order and checkpoints

Execute Tasks 1–6 locally with focused red/green tests and commit after each coherent slice. Tasks 7–8 require both macOS architectures before the platform code is considered green. Execute Task 9 locally, Task 10 privately and idempotently, then Task 11. Task 12 is the release gate: do not merge, tag, or publish before CI and all four physical direction rows are complete.

If a checkpoint exposes a defect, fix that defect with its nearest focused regression test and rerun only the affected gate plus its immediate integration gate. Keep unrelated validation out of the critical path.
