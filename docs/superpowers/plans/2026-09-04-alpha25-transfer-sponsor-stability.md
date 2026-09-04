<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Alpha.25 Transfer, Sponsorship, and Stability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:executing-plans` to implement this plan task-by-task. Every
> behavioral change starts with a failing test and ends with focused and
> integration verification.

**Goal:** Release `v0.1.0-alpha.25` with working Windows-origin edge-drag file
transfer, a live GitHub repository Sponsor button, explicit desktop URL failure
handling, and hardened Windows daemon restart/shutdown behavior.

**Architecture:** Keep the elevated core and transfer protocol intact. Own OLE
drop windows in the normal-integrity Windows GUI, exchange active-side state and
a bounded redacted drop payload over authenticated local IPC, then enter the
existing core `FileTransferService`. Add bounded exponential watchdog backoff
and guaranteed thread joins.

**Tech stack:** C++20, Qt 6 Core/Widgets/Network/Test, Win32 OLE/named pipes and
token APIs, CMake/Ninja/CTest, GitHub Actions/Releases.

**Spec:**
`docs/superpowers/specs/2026-09-04-alpha25-transfer-sponsor-stability-design.md`

## Constraints

- Preserve ordinary-files-only, TLS, send/receive opt-in, entitlement,
  checksum, staging, collision, and source-preservation boundaries.
- Do not disable core elevation, request UIAccess, or weaken UIPI.
- Do not log transferred paths or private license material.
- Preserve protocol 1.14 and alpha.24 peer compatibility.
- Preserve the user's unrelated original-worktree file.
- Use the MacBook Pro for required Apple acceptance; do not use the Mac mini
  without explicit direction.
- Do not tag or publish while any required acceptance cell is pending.

## Task 1: Add the bounded and redacted edge-drop IPC value

**Files:**

- Create: `src/lib/common/FileTransferEdgeDropIpc.h`
- Create: `src/lib/common/FileTransferEdgeDropIpc.cpp`
- Modify: `src/lib/common/IpcMessage.h`
- Modify: `src/lib/common/CMakeLists.txt`
- Create: `src/unittests/common/FileTransferEdgeDropIpcTests.cpp`
- Modify: `src/unittests/common/CMakeLists.txt`

- [ ] Write deterministic tests for round trips with Korean/space-containing
  absolute paths and each direction.
- [ ] Add rejection tests for malformed base64/JSON, version, enum, numeric
  range, missing/extra fields, empty/relative/non-string paths, 101 paths, and
  oversized payloads.
- [ ] Add a log-sanitization test requiring
  `fileTransferEdgeDrop=<redacted>` and unchanged non-sensitive messages.
- [ ] Build and run the new target; record the expected compile/test failure.
- [ ] Implement compact unpadded-base64url JSON and strict limits.
- [ ] Run the focused target green and `git diff --check`.

## Task 2: Authenticate the privileged core IPC command

**Files:**

- Modify: `src/lib/deskflow/ipc/IpcServer.h`
- Modify: `src/lib/deskflow/ipc/IpcServer.cpp`
- Modify: `src/lib/deskflow/ipc/CoreIpcServer.h`
- Modify: `src/lib/deskflow/ipc/CoreIpcServer.cpp`
- Create: `src/lib/deskflow/win32/CoreIpcClientValidator.h`
- Create: `src/lib/deskflow/win32/CoreIpcClientValidator.cpp`
- Modify: `src/lib/deskflow/CMakeLists.txt`
- Modify: `src/unittests/deskflow/IpcServerTests.h`
- Modify: `src/unittests/deskflow/IpcServerTests.cpp`

- [ ] Add tests proving exact-version clients are tracked and removed on
  disconnect while legacy mismatch clients retain legacy command behavior.
- [ ] Add Core IPC tests with an injected validator: authorized current client
  emits one decoded drop; denied, legacy, malformed, and missing-argument
  requests emit none and receive an error.
- [ ] Build/run and observe the red failure.
- [ ] Implement handshake tracking and sensitive-command authorization.
- [ ] Implement the Windows validator for named-pipe PID, session, normalized
  adjacent GUI path, token user/integrity, and AppContainer rejection.
- [ ] Apply IPC log redaction to both client and server before any drop command
  can be emitted.
- [ ] Run `IpcServerTests` green under Windows.

## Task 3: Move Windows drop ownership to the desktop GUI

**Files:**

- Create: `src/lib/gui/win32/WindowsFileTransferDropBroker.h`
- Create: `src/lib/gui/win32/WindowsFileTransferDropBroker.cpp`
- Modify: `src/lib/gui/CMakeLists.txt`
- Modify: `src/lib/gui/ipc/CoreIpcClient.h`
- Modify: `src/lib/gui/ipc/CoreIpcClient.cpp`
- Modify: `src/lib/gui/core/CoreProcess.h`
- Modify: `src/lib/gui/core/CoreProcess.cpp`
- Modify: `src/lib/gui/MainWindow.h`
- Modify: `src/lib/gui/MainWindow.cpp`
- Create: `src/unittests/gui/WindowsFileTransferDropBrokerTests.cpp`
- Modify: `src/unittests/gui/CMakeLists.txt`

- [ ] Add tests for side-mask validation, zero-mask cleanup, coalesced geometry
  refresh, encoded callback delivery, and broker failure when OLE is
  unavailable.
- [ ] Add CoreProcess/CoreIpcClient tests for status request, active-side update,
  malformed mask rejection, disconnect-to-zero, and bounded drop sending.
- [ ] Build/run and observe the red failure.
- [ ] Link the existing Windows host/target/decision implementation into the
  GUI and add the RAII OLE broker on the main thread.
- [ ] Rebuild on screen add/remove and geometry changes; clear on zero or
  shutdown.
- [ ] Wire `MainWindow` to core side changes and broker drop callbacks.
- [ ] Run GUI, host, and target focused tests green.

## Task 4: Publish core side state and dispatch authorized drops

**Files:**

- Modify: `src/lib/deskflow/ipc/CoreIpc.h`
- Modify: `src/lib/deskflow/ipc/CoreIpc.cpp`
- Modify: `src/lib/deskflow/ServerApp.h`
- Modify: `src/lib/deskflow/ServerApp.cpp`
- Modify: `src/lib/deskflow/ClientApp.h`
- Modify: `src/lib/deskflow/ClientApp.cpp`
- Modify: `src/lib/server/Server.h`
- Modify: `src/lib/server/Server.cpp`
- Modify: `src/apps/deskflow-core/deskflow-core.cpp`
- Modify: corresponding `src/unittests/deskflow` and
  `src/unittests/server` tests.

- [ ] Add tests for current-state response, non-queued/coalesced state updates,
  server primary side changes, client capability forwarding, and service-null
  rejection.
- [ ] Build/run and observe the red failure.
- [ ] Store/broadcast side state in `CoreIpcServer` and answer the GUI status
  request after an exact handshake.
- [ ] On Windows, skip elevated core OLE host installation and publish masks;
  keep the direct macOS host path unchanged.
- [ ] Report server mask changes for config, connection, lock, and removal.
- [ ] Queue decoded drops onto the core app thread and enter the existing
  service for either role.
- [ ] Publish zero during stop/disconnect and run focused service/routing tests.

## Task 5: Harden watchdog lifetime and retry behavior

**Files:**

- Create: `src/lib/platform/MSWindowsWatchdogRetryPolicy.h`
- Modify: `src/lib/platform/MSWindowsWatchdog.h`
- Modify: `src/lib/platform/MSWindowsWatchdog.cpp`
- Modify: `src/lib/deskflow/win32/AppUtilWindows.h`
- Modify: `src/lib/deskflow/win32/AppUtilWindows.cpp`
- Create: `src/unittests/platform/MSWindowsWatchdogRetryPolicyTests.cpp`
- Modify: `src/unittests/platform/CMakeLists.txt`

- [ ] Add exact delay-table and saturation tests, then observe the expected
  compile failure.
- [ ] Implement immediate/1/2/4/8/16/30-second bounded backoff and reset after
  success.
- [ ] Add the explicit watchdog destructor, atomic stop flag, start/stop
  lifecycle guard, null-safe idempotent stop, blocking final joins, and locked
  SAS process-state read.
- [ ] Make the AppUtil event-loop run flag atomic and its join defensive.
- [ ] Run the policy test, daemon build, and available shutdown smoke tests.

## Task 6: Make desktop Sponsor actions fail explicitly

**Files:**

- Create: `src/lib/gui/ExternalUrlLauncher.h`
- Create: `src/lib/gui/ExternalUrlLauncher.cpp`
- Modify: `src/lib/gui/MainWindow.cpp`
- Modify: `src/lib/gui/dialogs/SettingsDialog.cpp`
- Create: `src/unittests/gui/ExternalUrlLauncherTests.cpp`
- Modify: `src/unittests/gui/CMakeLists.txt`

- [ ] Add injected-launcher tests for success and failure fallback state.
- [ ] Build/run and observe the red failure.
- [ ] Route repository and Pro Early Access links through the helper.
- [ ] On launch failure, show the canonical copyable URL without claiming the
  browser opened; preserve the full claim query.
- [ ] Run sponsor URL, claim, and GUI policy tests green.
- [ ] Reconfirm the live repository top and sidebar Sponsor buttons.

## Task 7: Run focused and repository-wide verification

- [ ] Configure/build Windows Release with English tool output and warnings as
  errors where supported.
- [ ] Run the new IPC, broker, host/target, file-transfer, watchdog, Sponsor,
  daemon, GUI, and routing test targets with `--output-on-failure`.
- [ ] Run the full Windows CTest suite and package build.
- [ ] Run static checks, `git diff --check`, privacy scans, and deterministic
  code-graph generation twice.
- [ ] Build macOS ARM64 and x86_64 on the MacBook lane and run focused transfer
  plus regression suites.
- [ ] Inspect the final diff for path leakage, privilege-boundary regressions,
  stale windows, lifecycle races, and unrelated changes.

## Task 8: Record physical cross-device acceptance

**Files:**

- Create: `docs/releases/v0.1.0-alpha.25-acceptance.md`

- [ ] Install the candidate on Windows x64 and the MacBook Pro.
- [ ] Test Windows primary → macOS secondary and macOS secondary → Windows
  primary.
- [ ] Swap roles and test macOS primary → Windows secondary and Windows
  secondary → macOS primary.
- [ ] For each arrangement record Korean/spaced name, duplicate publication,
  bounded larger-file SHA-256, source preservation, TLS, and entitlement-off
  rejection.
- [ ] Attach sanitized logs/evidence; absolute local source paths must not be in
  public artifacts.
- [ ] Leave every unexecuted item explicitly pending and do not release while a
  required item remains pending.

## Task 9: Prepare, review, push, and release alpha.25

**Files:**

- Modify: `VERSION`
- Modify: `README.md`, `README.en.md`, `README.zh-CN.md` as applicable
- Modify: transfer/sponsorship documentation
- Modify: `src/gui/res/lang/*.ts` only for changed strings
- Regenerate: `docs/codegraph/*`

- [ ] Update current-version and troubleshooting text with the normal-integrity
  broker architecture and explicit Sponsor fallback.
- [ ] Reconcile the repository's live Windows/MacBook runner registrations and
  services with documented lane policy; do not expose registration tokens.
- [ ] Request a final code review and apply only verified feedback.
- [ ] Run verification again from the final diff and record exact commands and
  results.
- [ ] Commit coherent slices, push the branch, open a PR to `ieum/main`, and
  wait for all required checks.
- [ ] Merge only after automated and physical acceptance are complete.
- [ ] Create and push annotated tag `v0.1.0-alpha.25` at the merged release
  commit.
- [ ] Wait for release/package/privacy workflows, then independently download
  and verify checksums, versions, required Windows/macOS assets, and absence of
  private material.
- [ ] Confirm `origin/ieum/main`, the tag, and the public release resolve to the
  same commit; confirm the Sponsor button remains live.

## Execution checkpoints

Tasks 1–6 are strict red/green slices. Task 7 is the local and platform gate.
Task 8 is a release gate, not documentation theatre. Task 9 performs external
state changes only after the candidate is proven. Any newly discovered defect
gets the smallest reproducing test before its implementation change.
