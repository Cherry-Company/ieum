<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Windows–macOS Pro Local File Transfer and Early-Access Sponsorship Design

## Status and product decisions

This design targets `0.1.0-alpha.22`. It replaces the Windows-only execution
boundary of the alpha.21 Pro Local preview with native Windows and macOS file
transfer in both directions. A user can drag a local file across a configured
screen edge regardless of whether the sending computer is the primary/server
or a secondary/client.

The public offer is a limited-time USD 30 one-time GitHub Sponsors tier. Each
verified sponsorship receives one perpetual `file-transfer` entitlement for
compatible official Ieum builds. The offer may be retired before or when the
full Pro product launches, but an already-issued key does not expire. The key
does not grant future Pro features other than those explicitly listed in its
signed feature set.

License delivery remains manual and serverless. The public contact address is
`easecompany@protonmail.ch`. The private signing key, issuer, receipts, and
ledger remain in `C:\Users\LYR\Ieum-License-Authority-PRIVATE\` and must never
enter Git, CI, a package, or a GitHub release.

## Success criteria

- Windows can send ordinary files to macOS and macOS can send ordinary files
  to Windows without changing server/client roles.
- The existing Windows-to-Windows path continues to work.
- A secondary/client can originate an edge drop; the server resolves the
  actual adjacent target and relays the existing bounded control and data
  messages.
- Both endpoints require TLS, an explicit send or receive opt-in, and a valid
  Pro Local `file-transfer` entitlement.
- A transfer copies files. It never deletes or mutates the source.
- macOS receives into a private staging area, verifies size and SHA-256, then
  publishes without overwriting an existing destination.
- The Files settings page on Windows and macOS exposes the license controls,
  the USD 30 one-time early-access call to action, and an email request flow.
- The owner can match an emailed request to the real GitHub Sponsors
  transaction without operating an Ieum server.
- The macOS ARM64 and x64 packages compile and contain the feature, and focused
  Windows–macOS hardware acceptance covers both directions and both roles.

## Explicit non-goals

- Linux edge-drop support in alpha.22.
- Direct dropping into a remote application; received files are published to
  the configured directory.
- Directories, symbolic links, reparse points, aliases, packages, cloud-only
  stub files, devices, sockets, or other special files.
- Per-transfer approval dialogs, progress UI, pause/resume, or resumable data.
- Accounts, device activation, remote revocation, payment processing, or
  automatic license issuance.
- Hiding the GPL-covered desktop source or claiming that the offline gate is
  unbreakable DRM.

## Current boundary and required refactor

The alpha.21 codecs, route validation, session state, manifests, checksums, and
server relay are already portable. The restriction comes from four concrete
boundaries:

1. `MSWindowsFileTransferService` directly owns Windows readers and writers and
   uses BCrypt for transfer IDs.
2. source inspection, secure reading, staging, and publishing are implemented
   only by `MSWindowsFileTransferSource` and `MSWindowsFileTransferIo`.
3. edge drops are discovered only through Windows OLE drop windows.
4. `ClientApp`, `ServerApp`, and the Files settings policy compile or enable
   the service only on Windows; clients can receive but cannot originate a
   drop.

The protocol is not replaced. The implementation extracts its portable
orchestration and injects platform operations behind narrow interfaces.

## Architecture and component boundaries

### Portable transfer service

`FileTransferService` moves to the portable deskflow layer and owns only:

- entitlement checks;
- offer, decision, cancel, result, and data-message handling;
- `FileTransferSessionRegistry` state;
- one bounded pending edge-target request;
- outgoing scheduling and incoming session lifetime;
- conversion of platform errors to protocol result codes.

It depends on an `IFileTransferPlatform` factory rather than concrete OS
types. The factory inspects selected paths, creates a reader for an immutable
source candidate, creates a destination writer, and produces a cryptographic
transfer/request ID. Reader and writer interfaces expose the existing bounded
chunk and publish operations. Their API contains no HWND, POINTL, Objective-C
object, Win32 error, or AppKit type.

The service remains fail-closed. A missing factory, authorization callback,
event target, route sender, or destination prevents the operation.

### Platform file adapters

The Windows adapter preserves the current reparse-point checks, handle-based
metadata revalidation, private staging directory, SHA-256 verification, and
collision-safe publication. Existing public behavior and tests remain intact
while names are adapted to the common interfaces.

The macOS adapter is implemented in focused C++/Objective-C++ files. It:

- accepts absolute local file URLs only;
- opens source files without following symbolic links and verifies with
  `fstat` that they are regular files;
- records stable device/inode, size, and modification metadata and rechecks it
  while opening and completing a read;
- transmits a separator-free UTF-8 NFC filename;
- creates a mode-0700 hidden staging directory under the selected destination;
- creates new staging files exclusively, writes in order, verifies the final
  size and SHA-256, and renames to a non-conflicting final name;
- removes owned incomplete staging state on failure, cancellation, shutdown,
  or authorization loss.

Neither adapter follows a link or overwrites an existing file. Ordinary files
only is a deliberate alpha.22 safety boundary.

### Native edge-drop hosts

The common callback uses `Direction`, a signed 32-bit local point, generic
source candidates, and an asynchronous target-resolution result.

Windows keeps the existing OLE `IDropTarget` implementation behind that
callback. macOS adds a borderless transparent AppKit edge host with small
windows only on configured active sides. Its view registers for local file
URLs, converts AppKit bottom-left coordinates to Ieum top-left coordinates,
reuses the existing dwell/handoff decision, reports copy semantics, and never
activates Ieum or steals keyboard focus.

Display or topology changes rebuild only the affected edge windows. Disabling
the screen, send preference, entitlement, or service removes them immediately.

### Secondary-originated target resolution

Protocol minor 1.14 adds bounded edge-target capability, request, and response
messages. Existing alpha.21 offer/data frames and protocol minors 1.12/1.13
remain unchanged.

- On connection and topology change, the server sends each compatible client
  a four-side availability mask. The client creates edge windows only for
  sides with a configured neighbor.
- On a completed client edge drop, the client retains at most one bounded set
  of inspected source candidates and sends a request containing a random
  request ID, its canonical screen name, direction, and local point.
- The server validates the claimed source against the authenticated connection
  and resolves the exact neighbor through its existing screen topology.
- The response identifies the canonical destination or a rejection reason.
  A successful response starts the normal offer. A rejection or five-second
  timeout discards the pending candidates without opening file contents.
- Primary/server drops call the same generalized topology resolver locally and
  do not take a network round trip.

Once the offer exists, existing routing already supports client-to-primary and
client-to-client control/data relay. A peer older than protocol 1.14 may still
receive a compatible primary-originated transfer, but it cannot originate a
secondary edge drop.

## End-to-end data flow

### Primary/server source

1. The native host accepts an ordinary-file drop on an active edge.
2. The platform adapter inspects paths without reading contents.
3. The server resolves the adjacent screen and the service emits an offer.
4. The receiver rechecks entitlement and receive opt-in, creates staging, and
   accepts or rejects.
5. The sender opens and revalidates each source, then sends bounded chunks over
   the existing TLS connection.
6. The receiver verifies size and digest, publishes collision-safely, sends the
   final result, and removes staging.

### Secondary/client source

The first two steps are identical. The client then requests target resolution,
waits up to five seconds, and starts the same offer/data flow after a successful
response. The bytes still travel peer-to-peer through the existing Ieum server
relay; they never pass through an Ieum-operated Internet service.

## Pro Local settings and purchase flow

The Files tab supports license import, activation, removal, send, receive, and
destination controls on Windows and macOS. Linux shows the feature as
unsupported and cannot activate transfer controls in alpha.22.

When no active entitlement exists, the license group shows:

> Pro Local Early Access — USD 30 one-time
>
> Sponsor once to receive a perpetual Pro Local file-transfer license.
>
> Licenses are issued manually after payment verification.

The primary button reads `Get Pro Local Early Access — $30`. Each press creates
a fresh 128-bit lowercase hexadecimal claim code using the platform crypto
source and opens this documented GitHub Sponsors checkout shape:

`https://github.com/sponsors/victoriousian/sponsorships?metadata_campaign=ieum_pro_local_ea&metadata_source=desktop_app&metadata_claim=<claim>`

Metadata contains no email address, computer name, path, license, or other
personal data. The app displays the claim code with Copy and `Email license
request` actions. The email action opens a URL-encoded `mailto:` draft to
`easecompany@protonmail.ch` containing the claim code and an empty field for the
sponsor's GitHub username. A plain-text fallback explains how to send the same
two values when no desktop mail client exists.

The CTA never says payment was detected or that activation is automatic. The
app cannot observe GitHub, and generating a claim code grants no entitlement.

## GitHub Sponsors tier and manual fulfillment

A new published one-time USD 30 tier is separate from the existing monthly USD
30 tier. Its description states:

- limited-time Ieum Pro Local early access;
- one perpetual `file-transfer` entitlement, not all future Pro features;
- use on the sponsor's own Windows and macOS computers without current device
  counting;
- prerelease status and manual delivery after verification;
- the request address `easecompany@protonmail.ch`;
- no instant activation or Ieum cloud account.

The post-payment welcome message instructs the sponsor to email their GitHub
username and app-generated claim code. It must not ask them to publish an email
address or license key in a GitHub issue. Because the published tier is also
visible outside the app, the message includes a fallback for a valid purchaser
who has no app-generated claim: email the GitHub username and forward the
GitHub Sponsors receipt. If the sender cannot otherwise be linked to that
GitHub account, the owner supplies a one-time random challenge that the sponsor
temporarily publishes from the claimed GitHub account before delivery.

For each request, the owner:

1. exports the current GitHub Sponsors transaction data;
2. matches the exact one-time tier/amount, GitHub login, campaign/source, and
   claim code; for a direct tier purchase without a claim, matches the login,
   tier, amount, and transaction date and completes the GitHub-account
   challenge when needed;
3. rejects unmatched, reused, monthly-tier, or already-fulfilled transactions;
4. generates one perpetual license whose recipient label is the GitHub login
   or a non-personal order reference;
5. emails only the `.ieum-license.txt` file;
6. records the claim hash, tier/transaction reference, license ID, status, and
   issuance time in the private ledger.

The complete claim or fallback verification reference may be retained in the
private transaction record but is never committed or logged by the app. A
refunded or charged-back offline key cannot be remotely revoked; the private
ledger records that state and future online entitlement policy may handle it,
but alpha.22 makes no revocation claim.

## Failure handling

- Unsupported OS or peer: keep controls disabled and show the minimum required
  version/platform.
- Missing TLS, license, send/receive opt-in, or target: reject before opening a
  source or creating staging.
- Target request timeout/topology change: discard the pending drop and report a
  concise local error; do not guess a destination.
- Source changes, link substitution, read failure, or authorization loss: stop,
  notify the peer with the narrowest result code, and close the reader.
- Destination denial, out-of-order data, size overflow, digest mismatch, or
  publish failure: remove owned staging, preserve existing destination files,
  and fail the session.
- Disconnect: both endpoints terminate the session and remove only state they
  own. A failed alpha.22 session does not resume automatically.
- Sponsor checkout cancellation or wrong tier: no key is issued. The email
  response explains the mismatch without exposing another sponsor's data.
- Duplicate claim or transaction: return the already-recorded fulfillment
  status privately; never create a second entitlement accidentally.

## Tests and acceptance

Focused automated tests cover:

- common service behavior with fake platform factories on every OS;
- target capability/request/response codec limits, authenticated source
  validation, topology routing, timeout, disconnect, and stale response;
- Windows adapter regression, including reparse and source-change rejection;
- macOS URL extraction, UTF-8/NFC names, symlink/special-file rejection,
  source revalidation, staging permissions, digest failure, collision naming,
  and cleanup;
- settings policy enabling Windows/macOS but not Linux;
- claim-code generation, documented metadata constraints, URL encoding,
  mailto fallback, and absence of license material from logs;
- both-end entitlement withdrawal during an active transfer;
- compilation and focused unit suites on macOS ARM64/x64 and Windows x64/ARM64.

The focused hardware acceptance matrix is deliberately small:

| Primary/server | Secondary/client | Required transfers |
| --- | --- | --- |
| Windows x64 | macOS ARM64 | Windows→macOS and macOS→Windows |
| macOS ARM64 | Windows x64 | macOS→Windows and Windows→macOS |

Each row uses a small Korean/space-containing filename, a duplicate destination
name, and one bounded larger file. It verifies bytes and SHA-256, source
preservation, destination collision behavior, TLS, and entitlement rejection.
This is the only new physical acceptance required for alpha.22; unrelated broad
validation is not added.

## Documentation, code graph, and release

- Update the README roadmap diagram to show Community Core, Pro Local
  early-access purchase/activation, Windows↔macOS transfer, and later hosted
  Pro services.
- Update `docs/dev/pro_local_file_transfer.md` with both roles, both OSes,
  purchase/request instructions, claim matching, and the ordinary-files-only
  boundary.
- Regenerate the checked-in code graph after all component names and edges are
  final.
- Add Korean and existing supported GUI translations for every new user-facing
  string.
- Treat `easecompany@protonmail.ch` as an intentional public contact in privacy
  validation while continuing to reject private issuer material and historical
  private identities.
- Publish and verify the exact USD 30 one-time tier before tagging the app so
  the released CTA never points to an absent offer.
- Merge through the protected `ieum/main` branch, tag `v0.1.0-alpha.22`, wait
  for package/release/post-fetch privacy jobs, and verify checksums and the two
  macOS DMGs plus Windows packages from the public release.

## Commercial and GPL boundary

The desktop application and platform adapters remain
`GPL-2.0-only WITH LicenseRef-OpenSSL-Exception`. The embedded verification key,
license format, sponsor URL, and offline gate are public. The Ed25519 signing
key and issuer remain private.

The USD 30 sponsorship buys a perpetual entitlement for the official Pro Local
file-transfer feature and supporter fulfillment; it does not make the public
desktop source proprietary. Stronger account, seat, relay, or revocation
enforcement belongs to a later hosted service and is not implied by alpha.22.

## Delivery order

1. Extract and test the portable service and platform interfaces without
   changing Windows behavior.
2. Add bounded secondary target discovery and client-originated offers.
3. Add secure macOS source/reader/writer adapters.
4. Add the macOS edge-drop host and connect both app roles on Windows/macOS.
5. Enable cross-platform Files settings and add the claim-based Sponsors/email
   flow.
6. Publish and independently verify the one-time tier and welcome text.
7. Run focused automated and two-row hardware acceptance, update docs,
   translations, roadmap, and code graph.
8. Merge, tag alpha.22, publish, and perform post-fetch release verification.
