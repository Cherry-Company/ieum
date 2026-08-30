# Pro Local Edge File Transfer Design

**Status:** Approved direction, implementation design

**Date:** 2026-08-30

**Product decision:** Public GPL desktop code, paid official distribution and hosted services

**Selected gesture:** Drag files through the configured screen edge toward the receiving computer

## 1. Goal

Build the first Pro implementation track without requiring an Ieum-operated
file server. A user drags one or more local files toward a configured adjacent
computer, confirms the handoff at the screen edge, and the receiving user
accepts the transfer. The bytes move over the users' existing local connection
and are written into a dedicated receive directory.

This design deliberately separates two products:

- **Pro Local** provides direct local-network transfer and can be sold as an
  official one-time-purchase package without recurring relay cost.
- **Pro Cloud** may later add account-backed discovery and metered relay as a
  subscription because Ieum pays recurring network costs when file bytes pass
  through an Ieum-operated relay.

The desktop implementation and protocol remain in this GPL repository. No
client-side feature flag is treated as a durable copy-protection boundary.

## 2. User Experience

### 2.1 First beta

1. The sender selects one or more regular files in the native file manager.
2. The sender drags them to the screen edge that is configured next to another
   Ieum computer.
3. Ieum shows an edge overlay containing the target computer name, item count,
   and total size.
4. Holding in the edge zone for 250 ms arms the handoff. Moving away disarms it
   without changing the native drag.
5. Releasing while armed sends a transfer offer. The original local drag ends
   as a copy operation; source files are never moved or deleted.
6. The receiver sees the sender name, item count, total size, and destination
   directory, then chooses Accept or Reject.
7. Accepted files are streamed into `Ieum Transfers` under the receiving
   user's Downloads directory. Progress, cancellation, completion, and the
   final directory are visible on both computers.

Reject, timeout, disconnect, or failure leaves the source files untouched. The
first beta does not write into an arbitrary application underneath the remote
cursor. Native remote drop projection is a later platform adapter built on the
same offer and transport layers.

### 2.2 Later native remote drop

After the safe receive-folder flow is stable, each platform may project a
virtual native drag on the receiving computer. That work must preserve the
same approval, path, transfer, and integrity rules. Windows OLE virtual files,
macOS pasteboard promises, Xdnd, and Wayland portal behavior are separate
platform deliverables; none may weaken the beta's safe fallback.

## 3. Scope

### Included in Pro Local beta

- Windows-to-Windows source and receiver support first;
- a platform-neutral transfer offer, policy, and state machine;
- multiple regular files from one drag;
- source-to-receiver control messages over the existing authenticated Ieum
  connection;
- file bytes over a separate, authenticated transfer stream so a large file
  cannot stall keyboard or mouse traffic;
- explicit receiver approval;
- per-file and aggregate progress, cancellation, timeouts, and error results;
- collision-safe file creation with no silent overwrite;
- streaming SHA-256 verification and atomic finalization;
- direct local transfer without an Ieum-operated service;
- an experimental opt-in until the release gates in the commercialization
  roadmap are complete.

### Deferred

- folders, symlinks, reparse points, devices, sparse-file preservation, and
  alternate data streams;
- resume across process restart;
- client-to-client transfer through a third desktop;
- native drop into an arbitrary remote application;
- macOS, X11, and Wayland drag capture and projection;
- account login, billing, device directory, internet rendezvous, or hosted
  relay;
- browser or mobile senders;
- an unlimited lifetime relay entitlement.

## 4. Architecture

```text
native file manager
        │ native drag metadata only
        ▼
platform edge adapter
        │ candidate files + configured edge
        ▼
FileTransferOfferBuilder
        │ validated names, sizes, opaque transfer ID
        ▼
FileTransferCoordinator
        │ offer / accept / reject / cancel control messages
        ├──────────────── existing authenticated KVM channel ───────────────┐
        │                                                                  │
        │ accepted transfer                                                ▼
        └──────────── dedicated authenticated data stream ───────> ReceiveWriter
                                                                       │
                                                                       ▼
                                                           Downloads/Ieum Transfers
```

The GUI IPC remains a status and user-decision boundary. It never carries file
bytes or unbounded arbitrary-path payloads. The existing deprecated `DDRG` and
`DFTR` messages are not restored.

### 4.1 Source-side components

- `FileTransferItem` contains only the receiver-safe relative display name,
  byte size, and a stable item index. It never serializes the absolute source
  path.
- `FileTransferOffer` contains the opaque transfer ID, source and target screen
  identities, item list, aggregate size, and creation time.
- `FileTransferOfferValidator` enforces all manifest and policy limits before
  any network message is sent.
- `EdgeHandoffDecision` is a platform-neutral state machine that arms only
  after a configured edge and dwell interval match. It does not inspect the
  filesystem or send network data.
- A Windows OLE adapter extracts `CF_HDROP` regular-file candidates from the
  active drag and feeds the neutral builder. COM objects remain on their owning
  apartment and are not retained as the transfer's source of truth.
- Accepted source files are reopened for streaming. If identity, size, or
  modification time differs from the accepted offer, that item fails rather
  than silently sending different content.

### 4.2 Control plane

Protocol 1.12 introduces bounded control messages for offer, decision,
endpoint authorization, cancellation, and terminal result. The control plane
contains metadata only. Each parser rejects oversized counts, lengths, totals,
unknown enum values, duplicate terminal decisions, and messages received in an
invalid state.

Peers below protocol 1.12 never see the edge overlay as an available target.
The server routes control messages only between the named, currently connected
source and target. A target cannot accept a transfer addressed to another
screen.

### 4.3 Data plane

The beta uses a separate TLS-protected stream bound to the already trusted
Ieum peer relationship. The accepted offer produces a random 256-bit,
single-use connection token with a 60-second connection deadline. The token is
sent only through the authenticated control channel and is consumed before any
file bytes are accepted.

For the first Windows-to-Windows beta, one endpoint is the primary Ieum
computer and the other is one connected secondary. Direct-LAN failure ends the
transfer with an actionable error. It does not silently use a vendor relay.

File frames contain the transfer ID, item index, offset, bounded chunk length,
and bytes. A per-stream queue and backpressure cap memory use. Keyboard, mouse,
IME, clipboard, GUI IPC, and keepalive traffic never share the file-byte queue.

### 4.4 Receive writer

The receiver resolves its own destination root. A sender cannot provide an
absolute destination path. Each item name is normalized and checked again on
the receiver before opening a file.

Data is written to a newly created temporary file in the destination directory.
The writer rejects traversal, reserved names, device paths, symlinks or reparse
points, and any component that escapes the chosen root. It never follows an
existing link. Existing destination files are preserved by choosing a visible
`name (N).ext` conflict suffix.

After the declared byte count and SHA-256 digest match, the temporary file is
flushed and atomically renamed. Rejection, cancellation, digest mismatch,
disconnect, disk-full, or permission failure removes only the temporary file
owned by that transfer.

## 5. Limits

The first beta uses explicit defaults that can later become receiver policy:

| Limit | Beta default |
| --- | ---: |
| Items per offer | 100 |
| UTF-8 bytes per relative item name | 1,024 |
| UTF-8 bytes per screen identity | 255 |
| Serialized control manifest | 64 KiB |
| Size of one file | 20 GiB |
| Aggregate offer size | 50 GiB |
| Edge dwell before arming | 250 ms |
| Offer decision timeout | 60 seconds |
| Data-channel connection deadline | 60 seconds |
| File chunk payload | 256 KiB |
| In-memory queued file data per transfer | 8 MiB |
| Concurrent transfers per peer | 1 |

Zero-byte regular files are allowed. Empty offers, duplicate item indices,
overflowed totals, and names containing NUL or traversal components are
rejected.

## 6. Transfer State Machine

```text
Candidate
   │ edge dwell satisfied
   ▼
Armed ── moved away ──> Cancelled
   │ released
   ▼
Offered ── reject/timeout ──> Rejected
   │ accept
   ▼
Connecting ── auth/connect failure ──> Failed
   │ authenticated
   ▼
Transferring ── cancel/error ──> Cancelled/Failed
   │ every file finalized
   ▼
Completed
```

Only one terminal state is valid. Replayed decisions, progress after a terminal
state, and a second use of a data token are protocol errors for the transfer
but do not crash or corrupt the KVM session.

## 7. Security and Privacy

- Source absolute paths, destination absolute paths, filenames, and file
  contents are excluded from normal logs, metrics, diagnostics, and billing
  events.
- User-facing progress may display local filenames in memory, but diagnostic
  export redacts them.
- The receiver approves every offer in the beta. Persistent trust rules are a
  later, separately reviewed feature.
- Source files are opened read-only and never deleted or moved.
- A sender cannot select the receiver's destination path.
- Manifest integers use checked arithmetic. Allocation never uses an untrusted
  total without a smaller configured bound.
- A transfer token is random, short-lived, scoped to one source, target, and
  transfer ID, and consumed once.
- The data channel authenticates before creating a destination temporary file.
- Partial files are not exposed under their final name.
- Deprecated `DDRG` and `DFTR` inputs remain nonfunctional and are not treated
  as a compatibility route around the new validation.

## 8. Failure Semantics

| Failure | Sender result | Receiver result |
| --- | --- | --- |
| Target disconnects before decision | Failed | No file created |
| Receiver rejects or times out | Rejected | No file created |
| Direct connection unavailable | Failed with local-network guidance | No file created |
| Source file changed after offer | Failed for that item | Owned temporary file removed |
| Destination collision | Continues with conflict suffix | Existing file untouched |
| Disk full or permission denied | Failed with receiver-side reason | Owned temporary file removed |
| Digest or size mismatch | Failed integrity check | Owned temporary file removed |
| User cancels | Cancelled | Owned temporary file removed |
| KVM session ends | Cancelled unless already finalized | Finalized files remain; partial file removed |

## 9. Product and Cost Boundary

```text
Pro Local (one-time official package)
  └─ direct LAN file bytes: sender ───────────────> receiver
     Ieum-operated bandwidth cost: none

Pro Cloud (future subscription)
  ├─ account, device discovery, and entitlement metadata
  └─ relay fallback: sender ──> Ieum relay ──> receiver
     Ieum-operated bandwidth cost: metered and quota-controlled
```

Pro Cloud must prefer direct transfer and use relay only after direct paths
fail or policy requires it. Relay bytes are metered; an unlimited lifetime
relay promise is explicitly outside this design. Files are streamed
end-to-end encrypted and are not stored by default.

## 10. Platform Sequence

1. Platform-neutral offer validation and edge-handoff state machine.
2. Windows `CF_HDROP` edge adapter and Windows receive-folder beta.
3. Protocol 1.12 control routing and dedicated local data stream.
4. Windows physical two-machine acceptance.
5. macOS source and receive adapter.
6. X11 adapter.
7. Wayland portal adapter where compositor APIs permit it.
8. Native remote drop projection per platform.
9. Hosted discovery and metered relay in the separate Pro Cloud track.

Unsupported platform pairs expose no active edge-drop target. Ordinary KVM
screen switching continues unchanged.

## 11. Verification

### Automated

- manifest accepts valid zero-byte and multi-file offers;
- validation rejects empty, oversized, overflowing, absolute, traversal, NUL,
  reserved, and duplicate entries;
- edge state arms only for a configured side after 250 ms and disarms on exit;
- every invalid state transition is rejected;
- parser tests truncate every field and mutate all count and length fields;
- a loopback transfer proves byte-for-byte output and digest verification;
- cancellation, disconnect, disk-full simulation, and collision behavior leave
  no unintended final file;
- file traffic under backpressure does not block a synthetic control-message
  stream;
- Windows adapter tests cover multi-file Unicode `CF_HDROP` extraction and COM
  resource release.

### Physical Windows acceptance

- Explorer-to-Explorer edge handoff in both directions;
- filenames containing Korean, CJK, emoji, spaces, and long-but-valid names;
- zero-byte, 1-byte, 256-KiB boundary, 1-GiB, and aggregate multi-file cases;
- reject, cancel, disconnect, source modification, destination collision, disk
  full, and permission failure;
- mouse, keyboard, IME, and clipboard remain responsive during a large transfer;
- source files remain unchanged after every success and failure case;
- no source or destination absolute path appears in the exported diagnostic.

## 12. Initial Implementation Slice

The first merge is intentionally smaller than the whole beta. It adds the
platform-neutral `FileTransferItem`, `FileTransferOffer`,
`FileTransferOfferValidator`, and `EdgeHandoffDecision` components with tests,
plus this approved product boundary in the public documentation. It sends no
network messages and moves no bytes.

That slice fixes the data contract and edge gesture before platform COM code or
protocol parsers depend on them. The next merge adds the Windows edge adapter;
the following merge adds protocol 1.12 control routing and the dedicated data
stream. Each merge remains inert or explicitly experimental until its complete
vertical behavior and platform acceptance gate exist.
