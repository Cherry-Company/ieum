<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Ieum Phase 0 Input Baseline

Date: 2026-07-14

Code baseline: `deskflow/deskflow@39bf4fb`

Status: instrumentation implemented; two-machine hardware matrix pending

## Instrumentation

- `tools/cgtap-logger/main.swift` records the last 40 macOS key-down, key-up,
  and flags-changed events with virtual key, flags, event timestamp, source
  state id, keyboard type, and a wall-clock timestamp reconstructed from
  system uptime. Press F12 to dump the ring buffer as TSV.
- `tools/merge-input-timeline.py` merges server, client, and CGEvent tap logs
  into one time-ordered TSV for comparing send, inject, and observe stages.
- Core `DEBUG2` logs retain the existing key send/injection path diagnostics.

## Baseline hypotheses encoded in the fix

| Mechanism | Pre-fix risk | Implemented control |
|---|---|---|
| M-A | Empty IME groups trigger synthesized `TISSelectInputSource` | IME sources are excluded from key-map groups; group changes are ignored while an IME is active |
| M-B | IOHID/CGEvent path changes disrupt event ordering | IME input uses one persistent CGEvent source with explicit flags, keyboard type, repeat bit, and monotonic timestamp |
| M-C | Language translation rewrites Korean typing keys | Translation is bypassed while the secondary reports an input method; optional Set-1 passthrough is automatic |
| M-D | Down/up keys diverge across state changes | Raw-mode keys are tracked from down through repeat/up |

## Hardware matrix to execute

Run 10 minutes per cell across TextEdit, Chrome or VS Code, iTerm2, Safari,
and a native messenger. Cover the macOS 2-Set Korean IME and one third-party
IME, normal and 150+ CPM typing, Shift double-consonants, mouse movement during
composition, repeated Korean/English switches, and typing after paste.

Acceptance targets are: zero decomposed syllables, 20 consecutive input-source
switches without desynchronization, switch convergence within 500 ms, no
non-CJK regression, and less than 2 ms p95 added key latency. This report does
not claim those hardware results until the matrix is run on a Windows/macOS
pair with the required OS permissions.

## Commands

```sh
swiftc tools/cgtap-logger/main.swift -o build/cgtap-logger
./build/cgtap-logger > cgtap.tsv
python3 tools/merge-input-timeline.py server.log client.log cgtap.tsv -o timeline.tsv
```
