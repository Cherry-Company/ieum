# Ieum Code Graph

이 디렉터리는 Git 인덱스, CMake 대상, C/C++ include를 근거로 생성한
Ieum 코드 그래프입니다. 상세 파일 그래프는 `codegraph.json`, 사람이 읽는
컴포넌트 그래프는 `component-graph.mmd`에 있습니다.

## Snapshot

- Revision: `54830a475e08eb2daafae96e5a58a908457db49a`
- Branch: `fix/tailscale-async-query`
- Tracked files: 960
- Represented files: 957
- Source files / lines: 598 / 97,501
- Components / targets: 42 / 66
- File include edges / component edges: 1,416 / 97

`docs/codegraph/`의 생성 산출물은 자기참조로 인한 비결정성을 막기 위해
파일 인벤토리에서 명시적으로 제외됩니다.

## Runtime flow

```text
ieum GUI ──local IPC──> ieum-core
                         ├─ server → TLS/KVM protocol → remote client
                         └─ client ← TLS/KVM protocol ← remote server
Windows service ───────> ieum-daemon ──lifecycle──> ieum-core
platform adapters <──> deskflow/client/server <──> net/io/mt/base/arch
```

## Entry points

| Component | Kind | Path |
| --- | --- | --- |
| `app:deskflow-core` | process | `src/apps/deskflow-core/deskflow-core.cpp` |
| `app:deskflow-daemon` | process | `src/apps/deskflow-daemon/deskflow-daemon.cpp` |
| `app:deskflow-gui` | process | `src/apps/deskflow-gui/deskflow-gui.cpp` |
| `tool:cgtap-logger` | tool | `tools/cgtap-logger/main.swift` |
| `tool:ci` | tool | `tools/ci/fixtures/valgrind-invalid-access.c` |
| `tool:ci` | tool | `tools/ci/tests/test_analysis_workflows.py` |
| `tool:generate-codegraph` | tool | `tools/generate-codegraph.py` |
| `tool:merge-input-timeline` | tool | `tools/merge-input-timeline.py` |
| `tool:privacy` | tool | `tools/privacy/check_public_links.py` |
| `tool:privacy` | tool | `tools/privacy/public_privacy_guard.py` |
| `tool:privacy` | tool | `tools/privacy/report_public_manifest.py` |
| `tool:privacy` | tool | `tools/privacy/tests/test_public_privacy_guard.py` |

## Component inventory

| Component | Kind | Responsibility | Files | Lines | CMake targets |
| --- | --- | --- | ---: | ---: | --- |
| `app:deskflow-core` | application | Server/client KVM runtime process and command-line entry point. | 6 | 409 | `ieum-core` |
| `app:deskflow-daemon` | application | Windows service process that owns core lifecycle and session handoff. | 4 | 593 | `ieum-daemon` |
| `app:deskflow-gui` | application | Qt desktop entry point for configuration and runtime orchestration. | 2 | 507 | `ieum` |
| `app:res` | application | Application icons, manifests, bundle metadata, and packaged resources. | 209 | 2,921 | — |
| `deploy:linux` | deployment | Packaging and deployment logic for `linux`. | 7 | 359 | — |
| `deploy:mac` | deployment | Packaging and deployment logic for `mac`. | 5 | 203 | — |
| `deploy:root` | deployment | Packaging and deployment logic for `root`. | 1 | 39 | — |
| `deploy:windows` | deployment | Packaging and deployment logic for `windows`. | 10 | 504 | `wix-custom` |
| `lib:arch` | library | Operating-system abstraction for daemon, network, logging, and threading primitives. | 28 | 6,167 | `arch` |
| `lib:base` | library | Events, jobs, queues, logging, strings, Unicode, and timing primitives. | 32 | 3,642 | `base` |
| `lib:client` | library | Connection to the server and dispatch of incoming KVM protocol messages. | 5 | 2,295 | `client` |
| `lib:common` | library | Settings, network-interface selection, Tailscale integration, and shared product types. | 22 | 2,683 | `common` |
| `lib:deskflow` | library | Shared KVM application, protocol, screen, key, clipboard, and IPC behavior. | 78 | 13,500 | `app` |
| `lib:gui` | library | Qt windows, dialogs, widgets, validators, startup, diagnostics, updates, and IPC clients. | 111 | 17,771 | `gui` |
| `lib:io` | library | Stream interfaces, buffers, filters, and framed I/O support. | 8 | 570 | `io` |
| `lib:mt` | library | Thread, mutex, lock, and condition-variable wrappers. | 12 | 892 | `mt` |
| `lib:net` | library | TCP/TLS sockets, multiplexing, addresses, certificates, and fingerprint persistence. | 35 | 4,180 | `net` |
| `lib:platform` | library | Windows, macOS, X11, libei, and portal input/clipboard adapters. | 126 | 28,290 | `platform` |
| `lib:server` | library | Client proxies, input routing, screen topology, filters, and cursor transforms. | 42 | 9,555 | `server` |
| `support:artwork` | resource | Brand artwork and documentation screenshots. | 5 | 253 | — |
| `support:cmake` | build | Shared dependency, coverage, packaging, and signing CMake modules. | 4 | 746 | — |
| `support:docs` | documentation | User, developer, security, design, release, audit, and generated documentation. | 22 | 3,767 | — |
| `support:github` | automation | GitHub Actions workflows, composite actions, templates, and repository automation. | 26 | 3,594 | — |
| `support:root` | support | Repository support files rooted at `.`. | 20 | 2,892 | — |
| `support:source-build` | build | Repository support files rooted at `src`. | 3 | 170 | — |
| `support:translations` | localization | Qt translation catalogs and translation build rules. | 8 | 11,440 | — |
| `test:base` | test | Automated tests for the `base` surface. | 12 | 649 | `BaseExceptionTests`, `EventQueueTests`, `LogOutputtersTests`, `LogTests`, `StringTests`, `UnicodeTests` |
| `test:client` | test | Automated tests for the `client` surface. | 3 | 670 | `ServerProxyTests` |
| `test:codegraph-tools` | test | Automated tests for the `codegraph-tools` surface. | 1 | 461 | — |
| `test:common` | test | Automated tests for the `common` surface. | 11 | 942 | `I18NTests`, `LogLevelTests`, `NetworkInterfacesTests`, `SettingsTests`, `TailscaleIntegrationTests` |
| `test:deskflow` | test | Automated tests for the `deskflow` surface. | 26 | 2,335 | `CanonicalScancodeTests`, `ClientReconnectPolicyTests`, `ClipboardChunksTests`, `ClipboardTests`, `IKeyStateTests`, `IpcServerTests`, `KeyMapTests`, `KeyStateTests`, `KeyboardLayoutManagerTests`, `Protocol19Tests`, `X11LayoutParserTests` |
| `test:gui` | test | Automated tests for the `gui` surface. | 26 | 1,696 | `DiagnosticTests`, `FileTailTests`, `IpcClientTests`, `KeySequenceTests`, `LogWidgetTests`, `LoggerTests`, `NetworkMonitorTests`, `ScreenSetupModelTests`, `ScreenTests`, `ServerConfigDialogTests`, `ServiceStartCoordinatorTests`, `StatusBarTests`, `StyleUtilsTests`, `VersionCheckerTests` |
| `test:legacytests` | test | Automated tests for the `legacytests` surface. | 10 | 887 | `legacytests` |
| `test:net` | test | Automated tests for the `net` surface. | 9 | 595 | `FingerprintDatabaseTests`, `FingerprintTests`, `NetworkAddressTests`, `SecureSocketWriteBufferTests`, `SecureUtilsTests` |
| `test:platform` | test | Automated tests for the `platform` surface. | 11 | 692 | `ArchNetworkWinsockTests`, `MSWindowsClipboardTests`, `MSWindowsMouseInputTests`, `OSXClipboardTests`, `OSXKeyStateTests`, `XWindowsClipboardTests` |
| `test:server` | test | Automated tests for the `server` surface. | 5 | 437 | `ServerConfigTests`, `ServerTests` |
| `tool:cgtap-logger` | tool | Developer or diagnostic tool rooted at `tools/cgtap-logger`. | 1 | 108 | — |
| `tool:ci` | tool | Developer or diagnostic tool rooted at `tools/ci`. | 2 | 326 | — |
| `tool:codegraph` | tool | Developer or diagnostic tool rooted at `tools/codegraph.py`. | 1 | 945 | — |
| `tool:generate-codegraph` | tool | Developer or diagnostic tool rooted at `tools/generate-codegraph.py`. | 1 | 64 | — |
| `tool:merge-input-timeline` | tool | Developer or diagnostic tool rooted at `tools/merge-input-timeline.py`. | 1 | 67 | — |
| `tool:privacy` | tool | Developer or diagnostic tool rooted at `tools/privacy`. | 6 | 1,737 | — |

## Graph semantics

- `fileEdges`는 실제 저장소 파일로 해석된 quoted include입니다.
- `componentEdges`는 파일 include와 CMake link를 컴포넌트 단위로 합칩니다.
- `externalDependencies`는 system include, 외부 CMake link, 해석되지 않은
  quoted include를 별도로 보존합니다.
- `targets`는 선언 CMake 파일과 소유 컴포넌트를 함께 기록합니다.
- 모든 배열과 증거 목록은 안정적으로 정렬되며 생성 시각은 저장하지 않습니다.

## Regeneration

```powershell
uv run --no-project --python 3.13 python tools/generate-codegraph.py --root . --output docs/codegraph
uv run --no-project --python 3.13 python -m unittest discover -s tools/tests -p test_codegraph.py -v
```

두 번째 생성에서 `git diff -- docs/codegraph`가 비어 있어야 합니다.

## Limitations

- CMake는 타깃 선언과 `target_link_libraries`의 정적 문법을 추출합니다.
  모든 조건식과 generator expression을 평가하는 CMake 인터프리터는 아닙니다.
- 파일 include 해석은 현재 파일, 저장소 루트, `src/lib`, `src` 순서의
  보수적 탐색입니다. 해석 실패는 삭제하지 않고 JSON에 남깁니다.
- Mermaid는 가독성을 위해 컴포넌트 수준입니다. 파일 수준 질의에는 JSON을
  사용해야 합니다.
- 런타임 동적 호출, Qt signal/slot 연결, 이벤트 라우팅, 네트워크 메시지
  순서는 정적 include/link 그래프만으로 완전히 표현되지 않습니다.

이 파일은 생성 산출물입니다. 컴포넌트 규칙은 `tools/codegraph.py`와
`docs/superpowers/specs/2026-08-19-repository-audit-codegraph-design.md`에서
변경한 뒤 다시 생성하십시오.
