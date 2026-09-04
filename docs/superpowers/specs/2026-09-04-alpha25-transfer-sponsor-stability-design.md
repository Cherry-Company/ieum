<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Alpha.25 Windows-Origin Transfer, Sponsorship, and Runtime Stability Design

## Status and release decision

This design targets `v0.1.0-alpha.25`. It repairs the shipped Windows-to-macOS
edge-drag path without weakening the elevated core that Ieum uses for secure
desktop input, restores the repository GitHub Sponsors entry point, and fixes
two Windows service lifetime failures found while reproducing the transfer
problem.

The release is a corrective prerelease. It does not change the wire protocol,
license format, price, transfer data format, or macOS implementation.

## Evidence and root causes

### Windows-origin file transfer

The installed alpha.24 process tree provides the decisive evidence:

- Explorer and `Ieum.exe` run at normal interactive-user integrity.
- The Windows service launches `ieum-core.exe` from a duplicated
  `winlogon.exe` token when the default `elevate=true` setting is active.
- Alpha.24 creates and registers the OLE edge-drop windows inside that elevated
  core process.
- A failed Windows-origin drag produces no edge-drop or file-offer log entry,
  while the reverse macOS-origin path reaches the transfer service.

Windows User Interface Privilege Isolation prevents a normal-integrity drag
source from reliably driving a higher-integrity drop target. The existing
attempt to set `TokenUIAccess` does not establish a supported bridge: UIAccess
requires a trusted signature and secure installation, while the alpha package
is explicitly unsigned. Broadly admitting window messages would neither fix
the OLE trust boundary nor be safe.

The defect therefore occurs before file inspection, routing, TLS, or transfer
streaming. The file protocol itself is not replaced.

### GitHub Sponsors button

The repository already contains `.github/FUNDING.yml` pointing to
`victoriousian`, and the sponsorable profile is live. The repository-level
Sponsorships feature was disabled, so GitHub did not render the repository
Sponsor button. The setting is now enabled and the public repository page
shows both the top Sponsor action and the sidebar sponsor card.

The desktop application still treats an external-browser launch failure as a
silent success. Alpha.25 adds a visible, copyable fallback so the in-app action
also fails explicitly instead of appearing inert.

### Runtime stability

The installed daemon log captured approximately sixty immediate core respawns
in ninety seconds after a transient bind failure (`10049`). The watchdog gives
every failure after the first the same one-second delay, amplifying persistent
configuration or network faults.

Repository issue 13 identifies a second concrete failure class:

- watchdog worker threads capture `this`, but the destructor is defaulted;
- `stop()` returns after bounded waits even if workers still access members;
- the stop flag and SAS process-state reads are unsynchronized;
- `AppUtilWindows` reads and writes its event-loop flag from different threads
  without synchronization.

These are data-race and use-after-free risks during shutdown, service changes,
and fast failure paths.

## Success criteria

- A normal Windows Explorer drag can be captured while the core remains
  elevated, and it enters the existing `FileTransferService` path.
- Windows-primary to macOS-secondary and Windows-secondary to macOS-primary
  use the same bounded, TLS-protected, licensed transfer flow as alpha.24.
- macOS-origin and Windows-to-Windows behavior remain unchanged.
- No untrusted local process can use the new SYSTEM-core IPC command as an
  arbitrary file-read primitive.
- File paths never appear in routine IPC logs.
- Edge windows exist only while send is enabled, licensed, connected to the
  current core, and backed by a configured neighbor.
- Reconfiguration, disconnect, core stop, GUI shutdown, and display changes
  remove or rebuild the Windows edge windows deterministically.
- Repeated core start failures use bounded exponential backoff rather than a
  one-second restart storm.
- Every watchdog worker has exited before its owner is destroyed; shared stop
  and event-loop state is synchronized.
- GitHub visibly renders the repository Sponsor button, and an in-app browser
  failure gives the user a usable fallback URL.

## Explicit non-goals

- Changing protocol minor 1.14 or the transfer control/data/edge wire frames.
- Sending directories, links, reparse points, cloud placeholders, or special
  files.
- Removing elevation or secure-desktop input support.
- Enabling UIAccess for unsigned builds or bypassing UIPI with broad message
  filters.
- Adding Linux edge-drop support.
- Adding progress UI, resumable transfers, account activation, or automatic
  sponsor fulfillment.
- Treating an unexecuted automated test as physical cross-device acceptance.

## Architecture

### Normal-integrity Windows drop broker

On Windows, `Ieum.exe` owns a `WindowsFileTransferDropBroker` on the Qt GUI
thread. The broker initializes an OLE apartment for its lifetime and reuses the
existing `MSWindowsEdgeDropHost` and `MSWindowsEdgeDropTarget` implementation.
Those sources are linked into the GUI in addition to the core library; no
behavior is forked or reimplemented.

The broker receives only a four-side mask from the core. It creates the same
three-pixel, transparent, non-activating windows used by alpha.24 on the
current virtual desktop. Keeping the existing thickness avoids turning a
repair into a new click-interception surface. Qt screen add/remove and geometry
changes schedule a coalesced rebuild. A zero mask destroys every window.

The broker converts a completed OLE drop into a compact IPC payload and hands
it to `CoreProcess`. It does not open, hash, or retain file contents. The core's
existing platform adapter remains responsible for ordinary-file checks,
reparse rejection, stable metadata, and secure reads.

macOS continues to host edge windows directly in the core process because its
source and core do not cross the Windows integrity boundary.

### Core-to-GUI edge state

`CoreIpcServer` stores the most recent Windows transfer-side mask. Server and
client runtime code publish zero when transfer sending is unavailable and the
actual active mask when it is available.

The GUI explicitly requests current state after the version handshake. This
avoids relying on an unbounded backlog while the GUI is absent. Subsequent
changes are broadcast without queueing. The GUI clears its broker immediately
when the socket disconnects, the core stops, or the app shuts down.

For the server role, a narrow callback from `Server` reports changes whenever
the primary screen's active sides change because of configuration, client
connect/disconnect, cursor locking, or screen removal. For the client role,
the existing protocol-1.14 capability callback publishes the server-provided
mask. Existing peer capability messages remain unchanged.

### Bounded edge-drop IPC format

The payload is compact JSON encoded with unpadded base64url so it is safe in
the existing newline-delimited `command=value` channel. It contains exactly:

~~~text
version, direction, signed x, signed y, paths[]
~~~

Encoding and decoding reject:

- unsupported versions or directions outside Left/Right/Top/Bottom;
- missing, empty, relative, or non-string paths;
- more than 100 paths;
- coordinates outside signed 32-bit range;
- malformed JSON/base64 or unexpected fields/types;
- a payload too large to leave room inside the 64 KiB IPC frame.

The command name may be logged, but its argument is always rendered as
`<redacted>`. This applies on both client and server logging paths.

### Privileged IPC authorization

The core IPC endpoint must retain world access because the service-launched
core and desktop GUI use different accounts. Therefore the new path-bearing
command has stronger checks than legacy stop/reload commands.

`IpcServer` records whether each socket completed an exact current-version
hello. `fileTransferEdgeDrop` is accepted only when that state is true and a
Windows peer validator confirms all of the following:

1. the named-pipe client PID can be obtained from the server-side handle;
2. the process is in the same Windows session as the core;
3. its executable path is the expected GUI binary next to the running core;
4. its token is an interactive, non-AppContainer token with at least medium
   integrity and not the SYSTEM account.

Path comparison uses normalized absolute Windows paths and is case-insensitive.
Failure is closed, logged without the payload, and acknowledged as an error.
The validator is injectable so authorization and rejection are deterministic
in unit tests.

This is not intended to defend a user from code already running as that same
user and able to control their files. It prevents unrelated sessions and
low-integrity/sandboxed processes from turning the SYSTEM core into a confused
deputy.

### Core dispatch

After IPC validation and decoding, the Qt main thread emits only direction,
coordinates, and a string list. A queued connection reconstructs
`FileTransferEdgeDrop` on the core application thread and calls a public,
role-neutral entry point on `ServerApp` or `ClientApp`.

That entry point succeeds only when the existing service is alive. The service
then repeats entitlement and platform source validation before it resolves a
neighbor or sends an offer. IPC acceptance alone never grants a transfer.

## Watchdog and event-loop hardening

`MSWindowsWatchdog` uses an atomic run flag and an explicit destructor that
calls an idempotent `stop()`. A lifecycle mutex prevents duplicate starts from
orphaning worker handles. The five-second wait remains a diagnostic threshold;
after a warning, destruction blocks until each existing worker has actually
exited. Calling stop before start or more than once is safe.

The SAS loop reads `m_processState` while holding its existing mutex. The
`AppUtilWindows` event-loop run flag becomes atomic and its destructor joins
only a joinable thread.

Consecutive watchdog start failures use this deterministic policy:

| Consecutive failure | Delay |
| ---: | ---: |
| 1 | immediate |
| 2 | 1 second |
| 3 | 2 seconds |
| 4 | 4 seconds |
| 5 | 8 seconds |
| 6 | 16 seconds |
| 7 and later | 30 seconds |

A successful start resets the counter. The cap keeps automatic recovery while
preventing sustained CPU/log/process churn.

## Sponsor failure handling

Repository Sponsorships remains enabled as external GitHub state, with
`.github/FUNDING.yml` as the checked-in source of the sponsorable login.

Desktop sponsor actions use a small injectable external-URL launcher. On
success, behavior is unchanged. On failure, Ieum displays the canonical URL in
a dialog, copies it only when the user selects Copy, and never claims that a
checkout opened. The Pro Early Access claim URL, including its non-secret
claim token, remains intact in the fallback.

## Test strategy

Automated tests cover:

- exact edge-drop IPC round trips, Unicode/space paths, every invalid field,
  path-count and frame-size boundaries, and fail-closed decoding;
- redaction of the path-bearing IPC command on both send and receive logs;
- exact-version handshake state, disconnect cleanup, legacy-version rejection
  for the sensitive command, and injected authorization allow/deny behavior;
- GUI/core active-side request, update, disconnect-to-zero, and malformed-mask
  handling;
- Windows broker zero-mask cleanup and existing OLE host/target regressions;
- server primary-side callback changes and client capability forwarding;
- watchdog delay boundaries, atomic/lifetime build coverage, start-before-stop,
  duplicate stop, and destructor shutdown where practical;
- external URL success and failure policy without launching a real browser.

The focused Windows integration gate launches the built GUI and core at normal
and service-like privilege boundaries where possible and verifies that a
synthetic Explorer-compatible `IDataObject` reaches the broker and causes one
authorized core dispatch. It does not substitute for physical drag acceptance.

Physical acceptance uses the Windows x64 host and the MacBook Pro only. Both
server/client role arrangements must cover Windows-to-macOS and the existing
macOS-to-Windows route with a Korean/space filename, duplicate destination,
SHA-256 comparison, source preservation, and disabled-entitlement rejection.
The Mac mini remains an explicit fallback and is not used without direction.

## Release and rollback

Alpha.25 updates the version, release notes, acceptance record, translations
only if user-facing strings change, and generated code graph. It is pushed
through `ieum/main`, tagged `v0.1.0-alpha.25`, and published only after focused
Windows tests, macOS builds, package jobs, privacy checks, and recorded physical
acceptance pass.

The repository has no registered self-hosted runners at design time. Before
release, runner services and GitHub registrations must be reconciled with the
repository's documented Windows and MacBook lanes; hosted jobs may remain only
where the repository already documents a deliberate packaging exception.

Rollback is a new forward tag or release withdrawal, not history rewriting.
The GitHub Sponsorships switch remains enabled because it repairs repository
funding independently of the binary release.
