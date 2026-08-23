# Repository-Wide Code Audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Inspect every tracked Ieum component, create a reproducible code graph, validate actionable defects, and register non-duplicate findings in GitHub.

**Architecture:** A standard-library Python generator derives a deterministic file/component graph from Git and CMake/source evidence. A separate audit report records compiler, test, analyzer, workflow, and manual review receipts; only independently validated findings are copied into GitHub issues.

**Tech Stack:** Python 3 `unittest`, Git, CMake 3.24+, C++20/MSVC or Clang, Qt 6.7+, GitHub CLI/connector, Mermaid, JSON.

**Spec:** `docs/superpowers/specs/2026-08-19-repository-audit-codegraph-design.md`

## Global Constraints

- Audit the current source revision and preserve unrelated working-tree content.
- Do not modify, stage, or commit `deskflow_kr_ime_spec_v0.1.md`.
- Do not modify product source while performing this diagnostic audit.
- Treat unavailable platform and hardware validation as explicit partial coverage, never as a pass.
- Follow `docs/SECURITY.md`: exploit details and sensitive information do not belong in public issues.
- Deduplicate against open and closed `Cherry-Company/ieum` issues before every issue creation.
- Generated paths use forward slashes and deterministic ordering.

---

### Task 1: Freeze Inventory and Review Boundaries

**Files:**
- Create: `docs/audit/2026-08-19-full-code-audit.md`
- Reference: `.github/workflows/*.yml`
- Reference: `CMakeLists.txt`
- Reference: `src/**/CMakeLists.txt`

**Interfaces:**
- Consumes: `git ls-files`, Git revision, workflow metadata, repository runner metadata.
- Produces: audit scope, component checklist, tool availability table, and baseline working-tree record.

- [ ] **Step 1: Record the immutable audit revision and working tree**

Run:

```powershell
git rev-parse HEAD
git status --short --branch
git ls-files | Measure-Object
```

Expected: one revision SHA, the current branch, and the tracked-file count; the user-owned untracked specification remains visible and untouched.

- [ ] **Step 2: Inventory languages, components, targets, workflows, and live runners**

Run:

```powershell
rg --files --hidden -g '!**/.git/**'
rg -n "add_(library|executable|subdirectory)|target_link_libraries|runs-on:" CMakeLists.txt src .github
gh api repos/Cherry-Company/ieum/actions/runners
```

Expected: every source subtree, executable/library/test target, workflow lane, and registered runner is accounted for.

- [ ] **Step 3: Create the audit report header and coverage matrix**

Write exact fields for revision, branch, dirty-state exclusions, tracked/source file counts, component review receipts, available tools, platform coverage, and GitHub issue publication status.

- [ ] **Step 4: Commit the scope documents**

```powershell
git add docs/superpowers/specs/2026-08-19-repository-audit-codegraph-design.md docs/superpowers/plans/2026-08-19-repository-wide-code-audit.md docs/audit/2026-08-19-full-code-audit.md
git commit -m "docs: define repository audit and code graph"
```

### Task 2: Build the Graph Core with TDD

**Files:**
- Create: `tools/tests/test_codegraph.py`
- Create: `tools/codegraph.py`

**Interfaces:**
- Consumes: repository root `pathlib.Path` and tracked relative paths.
- Produces: `classify_component(path)`, `parse_local_includes(text)`, `parse_cmake(text)`, and `build_graph(root)`.

- [ ] **Step 1: Write failing classification and include-resolution tests**

```python
from pathlib import Path
import unittest

from codegraph import classify_component, parse_local_includes


class CodeGraphTests(unittest.TestCase):
    def test_classifies_runtime_components(self):
        self.assertEqual(classify_component("src/lib/net/SecureSocket.cpp").id, "lib:net")
        self.assertEqual(classify_component("src/apps/deskflow-core/deskflow-core.cpp").id, "app:deskflow-core")

    def test_extracts_only_quoted_repository_includes(self):
        text = '#include "net/SecureSocket.h"\n#include <QCoreApplication>\n'
        self.assertEqual(parse_local_includes(text), ["net/SecureSocket.h"])
```

- [ ] **Step 2: Run the tests and verify RED**

Run:

```powershell
python -m unittest discover -s tools/tests -p 'test_codegraph.py' -v
```

Expected: import failure for missing `codegraph` or missing requested API, proving the new behavior is not already implemented.

- [ ] **Step 3: Implement the minimum classification and include parser**

```python
@dataclass(frozen=True)
class ComponentRef:
    id: str
    kind: str
    path: str


def classify_component(path: str) -> ComponentRef:
    normalized = PurePosixPath(path.replace("\\", "/"))
    parts = normalized.parts
    if parts[:2] == ("src", "lib") and len(parts) >= 3:
        return ComponentRef(f"lib:{parts[2]}", "library", f"src/lib/{parts[2]}")
    if parts[:2] == ("src", "apps") and len(parts) >= 3:
        return ComponentRef(f"app:{parts[2]}", "application", f"src/apps/{parts[2]}")
    return ComponentRef("support:root", "support", ".")


def parse_local_includes(text: str) -> list[str]:
    return sorted(set(re.findall(r'^\s*#\s*include\s*"([^"]+)"', text, re.MULTILINE)))
```

- [ ] **Step 4: Run the tests and verify GREEN**

Run the same `unittest` command. Expected: classification and include tests pass with no warning output.

- [ ] **Step 5: Add failing CMake, stable-order, and fixture graph tests**

The tests create a temporary repository-shaped directory with two libraries and one app, then assert that `target_link_libraries(app PRIVATE alpha)` produces one `link` component edge and that repeated graph builds serialize identically.

- [ ] **Step 6: Implement CMake extraction and deterministic graph assembly**

```python
def parse_cmake(text: str) -> tuple[list[TargetDecl], list[TargetLink]]:
    cleaned = strip_cmake_comments(text)
    targets = extract_target_declarations(cleaned)
    links = extract_target_link_libraries(cleaned)
    return targets, links


def build_graph(root: Path) -> dict[str, object]:
    tracked = git_tracked_files(root)
    files = build_file_nodes(root, tracked)
    return normalize_graph(build_nodes_and_edges(root, files))
```

- [ ] **Step 7: Run the full graph test module**

Run:

```powershell
python -m unittest discover -s tools/tests -p 'test_codegraph.py' -v
```

Expected: all graph-core tests pass and consecutive serialized fixture graphs are byte-identical.

- [ ] **Step 8: Commit the tested graph core**

```powershell
git add tools/codegraph.py tools/tests/test_codegraph.py
git commit -m "feat: add deterministic code graph core"
```

### Task 3: Add the Generator CLI and Artifacts with TDD

**Files:**
- Modify: `tools/tests/test_codegraph.py`
- Create: `tools/generate-codegraph.py`
- Create: `docs/codegraph/codegraph.json`
- Create: `docs/codegraph/component-graph.mmd`
- Create: `docs/codegraph/README.md`

**Interfaces:**
- Consumes: `codegraph.build_graph(root)`.
- Produces: UTF-8/LF JSON, Mermaid, and Markdown at a caller-selected output directory.

- [ ] **Step 1: Write a failing subprocess test for all three outputs**

```python
result = subprocess.run(
    [sys.executable, str(script), "--root", str(repo), "--output", str(output)],
    text=True,
    capture_output=True,
)
self.assertEqual(result.returncode, 0, result.stderr)
self.assertTrue((output / "codegraph.json").is_file())
self.assertTrue((output / "component-graph.mmd").is_file())
self.assertTrue((output / "README.md").is_file())
```

- [ ] **Step 2: Run the CLI test and verify RED**

Expected: failure because `tools/generate-codegraph.py` does not exist.

- [ ] **Step 3: Implement the minimal CLI and renderers**

```python
def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    graph = build_graph(args.root.resolve())
    args.output.mkdir(parents=True, exist_ok=True)
    write_json(args.output / "codegraph.json", graph)
    write_text(args.output / "component-graph.mmd", render_mermaid(graph))
    write_text(args.output / "README.md", render_readme(graph))
    return 0
```

- [ ] **Step 4: Run CLI and graph tests and verify GREEN**

Expected: all tests pass and all three fixture artifacts exist.

- [ ] **Step 5: Generate the real repository graph twice**

```powershell
python tools/generate-codegraph.py --root . --output docs/codegraph
git diff -- docs/codegraph
python tools/generate-codegraph.py --root . --output docs/codegraph
git diff --exit-code -- docs/codegraph
```

Expected: the second run adds no diff.

- [ ] **Step 6: Validate JSON and Mermaid structure**

```powershell
python -m json.tool docs/codegraph/codegraph.json > $null
rg -n "^(flowchart|graph) |subgraph|-->" docs/codegraph/component-graph.mmd
```

Expected: valid JSON and a Mermaid graph containing component groups and dependency edges.

- [ ] **Step 7: Commit the generator and artifacts**

```powershell
git add tools/generate-codegraph.py tools/tests/test_codegraph.py docs/codegraph
git commit -m "docs: generate Ieum component code graph"
```

### Task 4: Run Automated Repository-Wide Checks

**Files:**
- Modify: `docs/audit/2026-08-19-full-code-audit.md`

**Interfaces:**
- Consumes: all tracked source/configuration files and installed analyzers.
- Produces: command receipts, exit codes, findings, and explicit unavailable-tool records.

- [ ] **Step 1: Detect toolchain and analyzers without installing system software**

Run version checks for CMake, Ninja, MSBuild, Visual Studio, Python, Clang,
clang-tidy, cppcheck, reuse, cspell, and GitHub CLI. Record every exact result.

- [ ] **Step 2: Run repository hygiene and configuration checks**

Run `git diff --check`, Python compilation/tests, CMake syntax/configuration,
REUSE when installed, workflow YAML parsing, and spelling when configured.

- [ ] **Step 3: Run source pattern sweeps**

Search all source/configuration for process creation, shell construction,
filesystem mutation, unchecked buffers and lengths, raw allocation, casts,
thread/event lifetime, TLS verification overrides, secret-like material,
diagnostic logging, protocol parsing, `TODO`/`FIXME`, ignored errors, and
platform-specific fallback branches. Record candidates with file and line.

- [ ] **Step 4: Run every available static analyzer**

Use the repository's compile database when available. Analyzer output is saved
outside tracked source or summarized in the audit report; warnings are grouped
by root cause rather than counted as independent defects.

- [ ] **Step 5: Update the audit matrix**

For each tool, record `pass`, `failure`, `unavailable`, or `blocked`, including
the exact command and relevant output excerpt.

### Task 5: Configure, Build, and Test the Native Windows Lane

**Files:**
- Modify: `docs/audit/2026-08-19-full-code-audit.md`
- Reference: `.github/workflows/continuous-integration.yml`
- Reference: `.github/actions/run-tests/action.yml`

**Interfaces:**
- Consumes: installed Windows compiler, Qt, OpenSSL, CMake presets/options.
- Produces: configuration, compilation, CTest, legacy test, and packaging-test receipts.

- [ ] **Step 1: Derive the exact local configuration from workflow and installed dependencies**

Record compiler architecture, generator, Qt/OpenSSL roots, installer setting,
test setting, and any environment-only limitation.

- [ ] **Step 2: Configure a dedicated audit build directory**

Use a new explicit directory such as `build-audit` and do not delete or reuse
an unrelated user build tree.

- [ ] **Step 3: Build all configured targets**

Expected: application, core, daemon, libraries, translations, and test targets
compile; any failure is preserved with its first causal diagnostic.

- [ ] **Step 4: Run CTest with failure output and enumerate skipped tests**

Record test count, pass/fail/skip count, duration, and environment-gated tests.

- [ ] **Step 5: Run repository-specific legacy and packaging validations that are safe locally**

Do not install, upgrade, uninstall, sign, publish, or release packages during
the audit. Packaging scripts may be inspected or invoked in validation-only
mode when they do not mutate system installation state.

### Task 6: Perform Component-by-Component Manual Review

**Files:**
- Modify: `docs/audit/2026-08-19-full-code-audit.md`
- Reference: `docs/codegraph/codegraph.json`

**Interfaces:**
- Consumes: generated graph, source, tests, docs, and automated candidates.
- Produces: a review receipt for every component and evidence-backed candidate findings.

- [ ] **Step 1: Review process entry points and lifecycle ownership**

Trace GUI, core, and daemon startup, argument parsing, singleton/IPC behavior,
service handoff, shutdown, update preparation, and crash recovery.

- [ ] **Step 2: Review network and protocol boundaries**

Trace listen/connect, TLS setup and certificate decisions, framing/size limits,
version negotiation, clipboard chunks, input-language messages, reconnect,
timeouts, and teardown.

- [ ] **Step 3: Review server, client, and shared KVM state machines**

Trace screen entry/leave, key down/repeat/up ownership, modifiers, cursor
transforms, clipboard ownership, options, timers, and event-handler lifetime.

- [ ] **Step 4: Review every platform adapter**

Cover Windows hook/service/IME/clipboard/DPI/session code, macOS TIS/CGEvent/
clipboard/TCC/startup code, Linux X11/libei/portal/clipboard/power code, and
compile-time fallback behavior.

- [ ] **Step 5: Review GUI, configuration, update, diagnostics, and privacy paths**

Trace settings persistence, validators, process argument construction, update
asset selection, release/version parsing, diagnostics redaction, logs, and
external URL handling.

- [ ] **Step 6: Review tests, deployment, workflows, translations, and documentation claims**

Match behavior claims to automated coverage; identify dead tests, silently
disabled lanes, packaging inconsistencies, stale protocol tables, and missing
negative cases.

- [ ] **Step 7: Mark every graph component reviewed**

Expected: no application, library, test, platform, tool, deployment, workflow,
or behavior-defining documentation component remains without a receipt or an
explicit environment limitation.

### Task 7: Validate and Register Findings

**Files:**
- Modify: `docs/audit/2026-08-19-full-code-audit.md`

**Interfaces:**
- Consumes: candidate list, reproduction results, repository issue history, security policy.
- Produces: validated findings and normalized GitHub issues.

- [ ] **Step 1: Attempt to disprove each candidate**

Read callers, guards, tests, platform documentation, and version history. Drop
candidates whose impact or reachability cannot be demonstrated.

- [ ] **Step 2: Add the smallest safe reproduction evidence**

Prefer an existing failing test, isolated command, or source-level proof. Do
not fix product code in this audit.

- [ ] **Step 3: Search open and closed issues by component, symbol, symptom, and error text**

Expected: each remaining finding has a recorded duplicate-search query and no
matching issue.

- [ ] **Step 4: Classify public issue versus private advisory**

Follow `docs/SECURITY.md`. Public issues must not contain useful exploit
instructions or sensitive data.

- [ ] **Step 5: Restate the exact repository and issue payload, then create the issue**

Target: `Cherry-Company/ieum`. Use the existing matching label, a concise
title, revision-pinned source links, impact, reproduction, acceptance criteria,
and regression-test requirements.

- [ ] **Step 6: Fetch each created issue and verify title, body, labels, and URL**

Correct malformed content immediately; never create a second issue merely to
repair formatting.

### Task 8: Final Verification and Handoff

**Files:**
- Modify: `docs/audit/2026-08-19-full-code-audit.md`
- Verify: `docs/codegraph/codegraph.json`
- Verify: `docs/codegraph/component-graph.mmd`
- Verify: `docs/codegraph/README.md`

**Interfaces:**
- Consumes: all audit artifacts, test output, generated issues, and Git state.
- Produces: reproducible final audit record and concise handoff.

- [ ] **Step 1: Regenerate the graph and require a clean artifact diff**

```powershell
python tools/generate-codegraph.py --root . --output docs/codegraph
git diff --exit-code -- docs/codegraph
```

- [ ] **Step 2: Run generator tests and artifact validators**

```powershell
python -m unittest discover -s tools/tests -p 'test_codegraph.py' -v
python -m json.tool docs/codegraph/codegraph.json > $null
git diff --check
```

- [ ] **Step 3: Reconcile report counts with graph and GitHub**

Tracked files, components, review receipts, findings, created issue numbers,
and URLs must agree across JSON, report, and remote issue tracker.

- [ ] **Step 4: Record final Git status and commit only audit-owned files**

```powershell
git status --short
git add docs/audit/2026-08-19-full-code-audit.md docs/codegraph tools/codegraph.py tools/generate-codegraph.py tools/tests/test_codegraph.py
git commit -m "docs: complete full repository audit"
```

- [ ] **Step 5: Report outcome and limitations**

State reviewed scope, automated results, GitHub issue links, code graph paths,
unavailable platform/hardware checks, commits created, and the untouched
user-owned untracked file.
