<!--
SPDX-FileCopyrightText: (C) 2026 Ieum contributors
SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
-->

# Safe update delivery

Ieum cannot replace a running GUI, core, service, or input hook with literally zero interruption. The product
target is a verified, rollback-capable update with a short controlled reconnect, not an unsafe in-process binary
replacement.

## Release prerequisites

Automatic installation stays disabled until every production package has a stable publisher identity:

- Windows MSI and update metadata are Authenticode-signed.
- The macOS app and updater use a stable Developer ID identity and Apple notarization.
- A release manifest names the exact platform and architecture asset, its size, SHA-256 digest, minimum
  operating-system version, and release channel.
- The manifest is signed separately from the transport channel. HTTPS and a GitHub release checksum alone are
  not sufficient authorization to install code.
- The updater rejects downgrades, architecture mismatches, expired metadata, and packages whose publisher or
  manifest signature does not match the installed trust root.

## Update sequence

1. The GUI checks signed metadata and offers an update without interrupting the running core.
2. The package downloads to an application-owned staging directory and is verified before any process stops.
3. Ieum records the current package version, configuration, core mode, connection target, and rollback location.
4. A separately signed updater helper takes ownership of the operation.
5. The helper pauses new input, releases held keys and buttons, then stops the core and GUI in that order.
6. Windows performs a transactional MSI major upgrade while preserving the service configuration. macOS
   replaces the application bundle atomically only after validating its code signature and notarization ticket.
7. The service or login item starts the new build, which reconnects using the saved configuration.
8. A health check verifies the GUI/core IPC version and core process. Failure restores the previous package and
   configuration, then reports the rollback.

The expected interruption is the process replacement and TLS reconnect window. Clipboard content already
committed to either operating system remains there, but clipboard or input events in flight during the handoff
cannot be promised and must not be replayed blindly.

## Acceptance criteria

- A forged, truncated, wrong-architecture, or wrong-publisher package is rejected before shutdown.
- Canceling before installation leaves the running version untouched.
- Power loss or installer failure leaves either the previous version or the new version recoverable.
- Windows upgrades do not create a second service core or discard the saved pre-login configuration.
- macOS updates retain Accessibility and Input Monitoring approval when the stable signed code requirement is
  unchanged.
- A successful update restores login startup, reconnects the core, and exposes the installed version in both
  GUI and diagnostics.

## Current alpha behavior

Until these prerequisites are met, Ieum deliberately stops before silent installation:

- Update checks also run after a delayed background/login start when the user enabled update checks.
- The update action selects the matching Windows or macOS architecture and opens its published installer directly.
- A Windows MSI asks the running GUI to exit cleanly, stops the service-owned core, performs the major upgrade, and
  starts the service again. The user no longer has to find and close those processes first.
- macOS opens the disk image for a user-approved replacement. The app does not claim that an ad-hoc alpha identity
  can preserve TCC permissions across every replacement.

This is a deliberate security boundary rather than a missing silent-update switch. Downloading in the background
would be safe once signed metadata exists; executing an unsigned package automatically would not be.
