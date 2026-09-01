# Ieum Product and Commercialization Roadmap

## Purpose

This document fixes the order in which Ieum moves from the current alpha into
a production-ready local KVM and, only after that foundation is trustworthy,
into paid distribution, hosted connectivity, team administration, and
enterprise support. It is an engineering boundary document, not a promise that
an unimplemented paid product already exists.

## Decisions

1. Reliability and release trust come before accounts, billing, relay, or team
   features.
2. The local KVM, input/IME synchronization, clipboard, protocol, and desktop
   application remain in this repository under
   `GPL-2.0-only WITH LicenseRef-OpenSSL-Exception`.
3. The existing repository is not relicensed as part of commercialization.
4. Paid value is built around signed official distribution, stable update
   channels, hosted services, fleet administration, and support.
5. A proprietary hosted service, if selected after legal review, lives in a
   separate repository and process and communicates through a documented
   network API. Proprietary code is not linked into the GPL-covered desktop
   executable.
6. Billing never becomes a prerequisite for using the local-network community
   core.
7. No phase is called complete without its automated gates and the native or
   physical acceptance evidence listed below.
8. Pro Local moves file bytes directly between users and may use a one-time
   official-package price. Pro Cloud pays recurring control-plane and relay
   costs, so hosted relay is subscription and quota based rather than an
   unlimited lifetime entitlement.
9. `alpha.22` uses a manually issued offline Pro Local file for direct
   Windows ↔ macOS transfer. It does not count devices, contact an activation
   service, or support remote revocation. The signing authority stays outside
   Git and is not distributed with the application or supporter license.
10. The limited-window Pro Local Early Access offer is USD 30 once. It grants
    a perpetual local file-transfer entitlement, not every future Pro feature
    and not hosted relay capacity. Payment is verified manually before email
    fulfillment.

## Repository and Process Boundaries

| Boundary | Contents | License and publication rule |
| --- | --- | --- |
| This public repository | Desktop GUI, core process, local KVM protocol, IME/input code, clipboard, public service API types, local service emulator | Keep the current GPL-2.0-only expression and OpenSSL exception |
| Hosted control plane | Account, entitlement, subscription, device directory, organization policy, audit history | Separate process and repository; choose its license only after a service-boundary and contributor-rights review |
| Relay data plane | Connection rendezvous and encrypted packet forwarding | Separate deployable; must not require plaintext clipboard or input payload access |
| Deployment configuration | Production secrets, signing credentials, billing credentials, infrastructure state | Never commit to the public source repository |
| Official artifacts | Signed MSI/DMG/packages, update metadata, checksums, provenance | Publish matching corresponding source and license notices with every GPL binary distribution |

The intended runtime relationship is:

```text
GPL desktop/core
  ├─ local LAN connection ─────────────────────────────> peer desktop/core
  ├─ OIDC/entitlement API ──> hosted control plane
  └─ end-to-end encrypted session ─> relay data plane ─> peer desktop/core

hosted control plane
  ├─ billing-provider adapter
  ├─ device and organization directory
  └─ append-only administrative audit history
```

The hosted components may authorize and route a connection, but they do not
receive the TLS session keys used to protect remote input and clipboard
payloads.

The first file-transfer product keeps the data path local:

```text
Pro Local: sender desktop ───── direct authenticated stream ─────> receiver desktop
           Ieum-operated bandwidth: none

Pro Cloud: sender desktop ── direct when possible ───────────────> receiver desktop
                          └── metered encrypted relay fallback ──> receiver desktop
```

The hosted relay is not bundled as unlimited lifetime traffic. Plans expose a
clear included relay quota and keep direct-transfer bytes outside that quota.

## License Policy

### Current decision

Keep `GPL-2.0-only WITH LicenseRef-OpenSSL-Exception`. Do not remove upstream
notices, do not convert the repository to a source-available license, and do
not add a proprietary module to the same executable.

GPL-covered binaries may be sold. Recipients still retain the license rights
to obtain the corresponding source and redistribute their copies. Official
binary convenience, signing, hosted availability, administration, and support
are therefore the commercial differentiators rather than source restriction.
The public verifier and feature gate can also be modified by someone building
their own GPL copy, so the offline key is an entitlement for official binaries,
not unbreakable DRM or a way to make the desktop source proprietary.

### Conditions before any future license change

A dual-license or different-license proposal requires all of the following:

- a file-by-file copyright and provenance matrix;
- written authority covering every nontrivial contribution that would be
  relicensed, or a clean replacement of code for which that authority is not
  available;
- compatibility review for Deskflow lineage, Qt modules, OpenSSL integration,
  bundled assets, and every third-party dependency;
- review of the exact desktop/service linking and IPC/network boundaries;
- qualified legal review and a release-specific compliance checklist.

Until those conditions are met, a license change is not an implementation
task and does not block the hosted-service model.

Primary references:

- GNU GPL v2 text: <https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html>
- GNU GPL FAQ, including commercial distribution and corresponding source:
  <https://www.gnu.org/licenses/gpl-faq.en.html>
- SPDX `WITH` exception expression model:
  <https://spdx.org/rdf/ontology/spdx-2-0-rev-12/classes/WithExceptionOperator___-1383513126.html>
- Qt open-source obligations:
  <https://www.qt.io/development/open-source-lgpl-obligations>

This policy records an engineering decision and is not legal advice.

## Ordered Delivery Phases

| Phase | Deliverable | Entry gate | Exit gate |
| ---: | --- | --- | --- |
| 1 | v1 reliability and audit-backlog closure | Repository audit and code graph merged | Issues #6–#38 closed with tests; required CI green; withheld security candidates privately revalidated |
| 2 | Official release trust | Phase 1 complete | Windows signing, Apple Developer ID/notarization, signed update metadata, rollback drills, and published physical acceptance matrix |
| 3 | Pro foundation | Phase 2 stable channel operating | OIDC login, device registration, entitlement cache, billing adapter, drag-and-drop file-transfer beta, self-service account lifecycle, and community-offline behavior verified |
| 4 | Hosted discovery and relay | Phase 3 identity and device identity stable | Authenticated rendezvous, end-to-end encrypted relay, abuse controls, regional health evidence, and direct-LAN fallback |
| 5 | Teams administration | Phase 4 service SLOs measured | Organizations, roles, fleet inventory, policy rollout, audit export, retention controls, and tenant-isolation tests |
| 6 | Enterprise readiness | Phase 5 team controls stable | SAML/OIDC federation, SCIM lifecycle, delegated administration, support tooling, disaster recovery evidence, and contractual SLO reporting |

Phases are sequential product gates. Work inside a later phase may be explored
locally, but it is not shipped or sold before the preceding exit gate passes.

## Phase 1: Reliability and Audit Backlog

Phase 1 closes every public audit issue without mixing unrelated platform
changes into one review. The implementation order is:

1. **TLS write correctness:** #6.
2. **CI trust:** #34, #35, #36.
3. **Platform-neutral primitives and protocol:** #7–#11, #17, #18, #21,
   #22.
4. **GUI and persisted state:** #20, #23, #38.
5. **Windows runtime:** #12, #13, #14, #33.
6. **X11:** #15, #16, #28.
7. **Wayland/libei:** #24–#27.
8. **macOS:** #29–#32.
9. **Protocol documentation and translation quality:** #19, #37.

As of 2026-08-30, 16 of these 33 issues are closed and 17 remain. Issue #38 is
complete; the next ordered implementation group is the remaining Windows
runtime work beginning with #12.

Each issue begins with a failing regression test. A public issue is closed only
after its relevant native lane passes. Platform-specific issues may merge with
host-independent tests first, but remain open until the named Windows, Linux,
Wayland/X11, or Apple evidence is attached.

The three withheld candidates remain outside public issue text and public
plans. They require a sealed private scan before advisory or remediation work.

## Phase 2: Official Distribution and Release Trust

Deliverables:

- organization-owned Windows signing and Apple Developer ID credentials stored
  only in protected release environments;
- timestamped Windows signatures, hardened-runtime macOS signatures, Apple
  notarization, stapling, and verification steps that fail closed;
- signed update manifests binding version, platform, architecture, language,
  artifact digest, minimum compatible version, and rollback metadata;
- a stable channel separate from alpha/beta channels, with a staged rollout
  and a tested rollback path;
- a published Windows x64/ARM64 and macOS Intel/Apple Silicon physical input
  matrix, including Korean/CJK IME, clipboard, reconnect, mixed DPI, and
  sustained-use acceptance;
- release provenance, checksums, matching source archives, and license notices.

Signing and notarization cannot be declared complete from unsigned CI
structure tests. They require the real protected credentials and provider
verification receipts.

## Phase 3: Identity, Devices, and Billing

The desktop uses OIDC Authorization Code with PKCE through the system browser.
It stores only refresh/device credentials in the native OS credential store.
Short-lived access tokens remain in memory. A device has a generated key pair
and a revocable server-side registration; it is not identified by hostname or
hardware serial number alone.

Entitlements are capability records with an expiry and signed cache envelope.
Loss of the hosted service must not disable local community KVM. Paid hosted
capabilities may use a bounded offline grace period, while cancellation and
refund handling remain server-authoritative when connectivity returns.

Billing is accessed through a provider adapter owned by the hosted control
plane. The desktop never handles raw card data and never embeds a provider
secret. Required lifecycle cases are checkout, renewal, payment failure,
cancellation, refund, plan change, grace expiry, webhook replay, and duplicate
webhook delivery.

The first Pro product feature is drag-and-drop file transfer between computers.
Its desktop implementation, offline verifier, and protocol remain in the
public GPL repository. Official Pro distribution, hosted relay, device
administration, and support form the paid boundary. Drag semantics,
receiving-user approval, filename conflicts, transfer limits, cancellation,
resumption, and platform integration are governed by a separate approved
design.

That design is now approved in
[`docs/superpowers/specs/2026-08-30-pro-local-edge-file-transfer-design.md`](../superpowers/specs/2026-08-30-pro-local-edge-file-transfer-design.md).
In `alpha.22`, both endpoints require a valid offline Pro Local entitlement.
Windows and macOS can each originate an edge drop while running as either
server or client. Files move in bounded 60 KiB chunks over the existing TLS
connection. The receiver stages bytes in a restricted hidden directory,
verifies each SHA-256 digest, and publishes without overwriting into the
configured directory (default: `Downloads/Ieum`). Direct bytes do not use an
Ieum-operated service.

The app creates a 32-character claim and opens a claim-tagged GitHub Sponsors
page. After a one-time USD 30 payment, the buyer emails the GitHub username and
claim to `easecompany@protonmail.ch`; the owner verifies the transaction and
emails the perpetual file-transfer license. GitHub does not expose a sponsor's
private email to the app. This is manual fulfillment, not an automated store,
account system, or activation service.

Per-transfer approval UI, pause/resume, progress UI, native drop into an
arbitrary remote application, hosted discovery, account-based activation,
billing automation, seats, and relay remain later work. Offline files are not
device-bound or remotely revocable. Shipping this capability in an
experimental GPL alpha does not mean the Phase 1 and Phase 2 production gates
are complete.

## Phase 4: Discovery and Relay

The control plane performs authenticated device discovery and rendezvous. The
desktop attempts direct LAN and direct authenticated paths first, then uses the
relay when direct connectivity is unavailable. Relay frames carry only
end-to-end encrypted session data.

Required controls include per-device authorization, explicit peer approval,
short-lived connection grants, replay protection, rate limits, abuse
revocation, bounded queues, backpressure, regional health checks, and a manual
way to force local-only operation.

Clipboard and input contents are excluded from service logs, metrics, billing
events, and administrative audit records.

## Phase 5: Teams

Teams adds organizations, invitations, owner/admin/member roles, device groups,
policy versions, staged rollout, revocation, and append-only administrative
events. Tenant identity is explicit in every storage key and authorization
decision. Cross-tenant access tests are required at API, background-job,
export, and cache boundaries.

Policy controls may limit hosted connectivity, clipboard direction, file
transfer when available, approved devices, update channels, and diagnostic
collection. Policies cannot silently weaken local transport security.

## Phase 6: Enterprise

Enterprise work begins only after team authorization and audit semantics are
stable. It adds:

- SAML or OIDC federation with domain verification and safe account linking;
- SCIM user/group provisioning with idempotent deprovisioning;
- delegated administration and break-glass recovery with mandatory audit
  events;
- configurable retention and export, data-region controls, and deletion
  evidence;
- support bundles that preserve the existing privacy-redaction rules;
- tested backup restore, regional failover, incident response, status history,
  and SLO/error-budget reporting.

## Cross-Phase Quality Gates

- Tests are written before production changes and must demonstrate the failure
  they prevent.
- `ci-passed` remains the protected merge gate; platform evidence is attached
  to the issue or pull request that needs it.
- Public release artifacts pass the private deny-map tree, history, package,
  and post-publication scans.
- Secrets, signing identities, tokens, billing payloads, clipboard contents,
  input contents, and raw diagnostic logs never enter public artifacts.
- Service APIs use explicit versions and backward-compatible deprecation
  windows before a stable desktop release depends on them.
- Every paid capability has an observable community/offline fallback and a
  documented support boundary.

## Immediate Implementation Slices

Phase 1 continues through the remaining Windows, X11, Wayland, macOS,
protocol-documentation, and translation work. The Pro Local offline-license
and file-transfer path may be exercised in alpha releases, but it is not
described as production-ready before the Phase 1 and Phase 2 exit gates. The
next Pro Local slices are four-route Windows/macOS physical acceptance,
per-transfer approval, progress UI, and resumable sessions. Account activation,
payment automation and seats follow those local foundations; hosted discovery
and relay come afterward.
