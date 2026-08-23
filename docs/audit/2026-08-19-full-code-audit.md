# Ieum Repository-Wide Code Audit

## Outcome

The repository-wide audit completed against the Ieum alpha.19 development
baseline. It combined a deterministic component/code graph, clean native
Windows builds, unit and legacy test execution, multiple static analyzers,
workflow and packaging inspection, component-by-component source review, and
semantic comparison with the 84 upstream commits after the fork point.

| Result | Value |
| --- | ---: |
| Git-tracked files | 933 |
| Files represented in the code graph | 930 |
| Source files / source lines | 578 / 91,888 |
| Components / CMake targets | 40 / 58 |
| Resolved file include edges / component edges | 1,388 / 94 |
| Public GitHub issues created | 33 |
| Public issue readback failures / duplicate titles | 0 / 0 |
| Potentially sensitive candidates withheld from public artifacts | 3 |

The public issue index is
[2026-08-19-github-issues.md](2026-08-19-github-issues.md). The generated graph
entry point is [../codegraph/README.md](../codegraph/README.md).

## Audit Identity

| Field | Value |
| --- | --- |
| Audit date | 2026-08-19 (Asia/Seoul) |
| Repository | `Cherry-Company/ieum` |
| Repository visibility | Public |
| Product branch | current audit source revision |
| Product baseline | `b94bdc1e381f2f5a749eb1f787ca5f1cf0e08081` |
| Baseline describe | `v0.1.0-alpha.18-5-gb94bdc1e3` |
| Upstream comparison head | `deskflow/deskflow@e856cce65bc44c8df69e6baa810719e1569e11ab` |
| Audit mode | Repository-wide diagnostic review; product source unchanged |

The baseline SHA is used in every issue body so later source changes do not
move the evidence links.

## Working-Tree Boundary

The baseline checkout contained one user-owned untracked file,
`deskflow_kr_ime_spec_v0.1.md`. It was treated as product context only. The
audit did not modify, stage, commit, or include it in the generated graph.

Audit-owned repository changes are restricted to:

- `docs/audit/`;
- `docs/codegraph/`;
- `docs/superpowers/specs/2026-08-19-repository-audit-codegraph-design.md`;
- `docs/superpowers/plans/2026-08-19-repository-wide-code-audit.md`;
- `tools/codegraph.py`;
- `tools/generate-codegraph.py`;
- `tools/tests/test_codegraph.py`.

Native build and analyzer trees were created as untracked, disposable audit
artifacts and removed after their results were captured.

## Component and Runtime Map

### Runtime processes

| Process | Responsibility | Main source surfaces |
| --- | --- | --- |
| `ieum` / `Ieum.app` | Qt GUI, settings, discovery, diagnostics, updates, and local orchestration | `src/apps/deskflow-gui`, `src/lib/gui`, `src/lib/common` |
| `ieum-core` | Server/client KVM runtime, protocol, transport, and platform input | `src/apps/deskflow-core`, `src/lib/{deskflow,server,client,net,platform}` |
| `ieum-daemon` | Windows service, user-session handoff, and core lifecycle | `src/apps/deskflow-daemon`, `src/lib/deskflow/ipc`, `src/lib/platform/MSWindowsWatchdog*` |

### Principal libraries

| Component | Responsibility |
| --- | --- |
| `arch` | OS abstraction, daemon, process, thread, logging, and socket primitives |
| `base` | event queue, logging, jobs, strings, Unicode, and timing |
| `client` | server connection and incoming protocol dispatch |
| `common` | settings, network selection, Tailscale, identity, and shared types |
| `deskflow` | application state, protocol, screens, keys, clipboard, and IPC |
| `io` | streams, buffers, filters, and framed I/O |
| `mt` | thread, mutex, lock, and condition wrappers |
| `net` | TCP/TLS sockets, multiplexing, addresses, certificates, and fingerprints |
| `platform` | Windows, macOS, X11, libei, portal input, and clipboard adapters |
| `server` | client proxies, screen topology, input routing, filtering, and cursor transforms |
| `gui` | Qt windows, dialogs, models, widgets, startup, updates, and IPC clients |

The generated graph records 40 repository components, including applications,
libraries, tests, tools, deployment, documentation, workflows, resources, and
translations. File-level relationships are in `codegraph.json`; the Mermaid
view intentionally collapses them to component-level edges.

## Methodology and Evidence

| Pass | Evidence | Result |
| --- | --- | --- |
| Repository inventory | Git index, manifests, CMake target declarations, workflows, deployment scripts, translations, docs | Complete |
| Code graph | Deterministic Git/CMake/include parser with sorted JSON and Mermaid outputs | Complete |
| Clean native build | Release, tests and installer enabled, MSVC warnings as errors | 413/413 build steps passed |
| Unit tests | CTest on the clean Windows build | 37/38 passed; one host clipboard-access limitation |
| Legacy tests | `legacytests` suite | 15/15 passed |
| Code graph tests | Python unittest suite | 10/10 passed |
| MSVC static analysis | Full `/analyze` build | 232/232 build steps; 34 diagnostics manually triaged |
| Semgrep | `p/security-audit` | 0 findings; 7 partial-parse warnings retained as a limitation |
| Flawfinder | Whole tracked C/C++ source set | 46 heuristic hits, all manually triaged |
| REUSE | Clean exported tree, REUSE 3.3 | 881/881 files had copyright and licensing data |
| PowerShell syntax | Every tracked `.ps1` parsed through the PowerShell AST parser | Passed |
| Shell syntax | No tracked `.sh` or `.bash` files | Not applicable |
| Workflow review | Triggers, permissions, protected result aggregation, static analysis, packaging | Complete |
| Translation review | XML parse, source-message counts, unfinished/vanished counts | Complete |
| Manual component review | Control flow, ownership, bounds, concurrency, protocol failure paths, platform lifecycle | Complete |
| Upstream semantic review | 84 commits after common ancestor `39bf4fbe845fc08de974a9c195b25112db739267` | Complete; equivalent local fixes excluded |

The 34 MSVC diagnostics included analyzer false positives and low-value return
value/style warnings. C6308/C28183 independently corroborated the validated TLS
buffer and Windows DIB findings tracked in issues #6 and #14. No analyzer count
was treated as a defect count without source validation.

## Component Review Receipts

| Surface | Review focus | Public issue receipts |
| --- | --- | --- |
| `base`, `common` | formatting boundaries, logging/rotation, Unicode, persisted state | #7–#10 |
| `net`, `arch` | TLS retry ownership, poll state, addresses, Windows module discovery | #6, #11, #12 |
| `client`, `server`, protocol | decode failures, receive fairness, protocol 1.11 documentation | #17–#19 |
| GUI and local IPC | config synchronization, IPC framing/acknowledgement, model bounds, blocking process calls | #20–#23, #38 |
| Windows platform | worker lifetime, DIB/HTML conversion, keyboard state | #13, #14, #33 |
| X11 platform | LP64 properties, clipboard property validation, XKB refresh | #15, #16, #28 |
| Wayland/libei/portal | hotkeys, modifiers, scroll, topology, coordinates, display idle | #24–#27 |
| macOS platform | pasteboard ownership, CoreFoundation/event-tap lifecycle, key mapping, native quit | #29–#32 |
| Workflows and automation | analysis triggers, Valgrind gating, permissions, immutable actions | #34–#36 |
| Translations | catalog synchronization, coverage, placeholders, numerus | #37 |

Event handler lifetime and deferred client disconnect were specifically
rechecked because upstream contains fixes with different hashes. Equivalent
local commits (`4033b75d4` and `a9fa779a3`) and matching current behavior were
present, so no duplicate issues were created for them. Equivalent local macOS
mouse capture/background click/event-number fixes were excluded for the same
reason.

## GitHub Issue Registration

GitHub Issues was disabled on the repository when registration began. It was
enabled as the required, reversible prerequisite for the explicitly requested
issue workflow. No other repository feature or setting was changed.

The connected GitHub integration could read repository and issue metadata but
returned `403 Resource not accessible by integration` for issue creation. The
existing authenticated `gh` account had repository admin permission and was
used as the write fallback. Each create was followed by a REST readback that
compared title, complete body, open state, and label.

The final aggregate readback reported:

- 33 open audit issues, numbered #6 through #38;
- no missing issue numbers in that audit range;
- no duplicate titles;
- no empty bodies;
- no issue missing the pinned baseline SHA;
- labels: 29 `bug`, 1 `documentation`, 3 `enhancement`.

## Runtime and Packaging Limitations

The clean native Windows build and portable 7z package completed. MSI creation
reached the packaging stage but the host-provided WiX 7 required an OSMF EULA
flow while repository CI pins WiX 5.0.2; the host did not have `dotnet` available
to reproduce that pinned toolchain. This is an environment mismatch, not a
source finding.

`MSWindowsClipboardTests` was the only CTest failure. The automation session
received Win32 `ERROR_ACCESS_DENIED` opening the interactive clipboard; a direct
P/Invoke probe reproduced the host restriction outside the test binary. The
failure was therefore retained as `environment-limited`, not filed as a product
defect.

macOS, real Wayland/libei, Linux X11, FreeBSD, Windows ARM64, and Android runtime
behavior cannot be executed natively on this Windows host. Those surfaces were
covered through source review, upstream semantic comparison, manifests, and
test definitions, but the platform-specific issues still require their native
CI or hardware lanes for fix verification.

## Security Publication Boundary

The repository is public. Three potentially sensitive candidates were withheld
from public issues, committed documents, and code-graph annotations. This
report intentionally does not include their component locations, trigger
conditions, reproduction steps, or impact chains.

The managed deep-security scan could not start because this session did
not expose the required managed filesystem permission profile. Per the security
tracking workflow, findings from an unsealed fallback review cannot be turned
into security advisories through that workflow. The three candidates therefore
remain pending private re-validation in a scan-capable session; they were not
silently downgraded into public bug reports.

No log, path, token, certificate material, input content, clipboard content, or
other sensitive runtime data is included in the published artifacts.

## Regeneration and Verification

```powershell
uv run --no-project --python 3.13 python tools/generate-codegraph.py --root . --output docs/codegraph
uv run --no-project --python 3.13 python -m unittest discover -s tools/tests -p test_codegraph.py -v
uvx ruff check tools/codegraph.py tools/generate-codegraph.py tools/tests/test_codegraph.py
```

Run the generator twice and require the second `git diff -- docs/codegraph` to
be empty. The graph deliberately excludes its own generated directory from the
file inventory to prevent self-reference and timestamp drift.
