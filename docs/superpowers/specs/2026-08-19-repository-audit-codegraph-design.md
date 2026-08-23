# Repository-Wide Audit and Code Graph Design

## Purpose

This work inventories every tracked source and configuration file in Ieum,
builds a reproducible code graph, validates defects with more than one form of
evidence where practical, and records actionable non-duplicate findings in the
`Cherry-Company/ieum` GitHub issue tracker.

The audit is diagnostic. Product source is not modified as part of the audit.
Only audit documentation, the graph generator, its tests, and generated graph
artifacts are added to the repository.

## Scope

The authoritative inventory is `git ls-files` at the audited revision. It
includes:

- C, C++, Objective-C, Objective-C++, Swift, and Python source;
- CMake, GitHub Actions, deployment, packaging, translation, and resource
  configuration;
- GUI, core, daemon, client, server, protocol, TLS, IPC, platform, clipboard,
  input, update, and diagnostics paths;
- unit, legacy, packaging, workflow, and static-analysis tests;
- developer, user, release, and security documentation where it defines
  behavior or operational guarantees.

Generated build trees, Git internals, caches, release binaries, and the user's
untracked `deskflow_kr_ime_spec_v0.1.md` are excluded from generated graph
inputs. The untracked specification may be read as product context but is not
modified, staged, or committed.

## Deliverables

1. `tools/codegraph.py` contains deterministic inventory and graph-building
   functions with no third-party Python dependency.
2. `tools/generate-codegraph.py` is a command-line entry point that writes all
   graph artifacts atomically enough for a failed run not to be mistaken for a
   successful refresh.
3. `tools/tests/test_codegraph.py` verifies component classification, include
   resolution, CMake dependency extraction, stable ordering, and CLI output.
4. `docs/codegraph/codegraph.json` is the machine-readable graph.
5. `docs/codegraph/component-graph.mmd` is a reviewable Mermaid component
   graph.
6. `docs/codegraph/README.md` explains component responsibilities, entry
   points, boundaries, graph schema, regeneration, and known limitations.
7. `docs/audit/2026-08-19-full-code-audit.md` records the audited revision,
   inventory coverage, commands, results, validated findings, GitHub issue
   links, limitations, and deferred platform-only checks.

## Graph Model

`codegraph.json` uses schema version `1` and contains these top-level fields:

- `schemaVersion`: integer schema discriminator;
- `revision`: audited Git commit and dirty-state metadata;
- `summary`: counts for tracked files, source files, source lines, components,
  targets, file include edges, and component edges;
- `components`: stable nodes for applications, libraries, tests, tools,
  deployment, translations, and documentation;
- `targets`: CMake targets and the component that owns each target;
- `files`: tracked implementation/configuration files with component, language,
  and line count;
- `fileEdges`: resolved repository-local include edges;
- `componentEdges`: deduplicated include and CMake-link relationships, each
  carrying source evidence;
- `externalDependencies`: unresolved system, Qt, OpenSSL, and platform includes
  kept separate from repository-local edges;
- `entryPoints`: process and tool entry files.

All arrays are sorted by stable identifiers. Paths use forward slashes even on
Windows. Timestamps are not embedded, so two runs at the same revision produce
byte-identical graph files.

## Component Boundaries

Classification is path-driven and intentionally conservative:

- `src/apps/<name>` becomes an application component;
- `src/lib/<name>` becomes a library component;
- `src/unittests/<name>` becomes a test component;
- `tools/<name-or-file>` becomes a tool component;
- `deploy/<platform>` becomes a deployment component;
- `translations`, `docs`, `.github`, and root build configuration become
  support components.

Cross-component C/C++ includes create `include` edges. `target_link_libraries`
entries create `link` edges when both targets can be mapped to repository
components. Multiple file-level relationships collapse into one component edge
with a sorted evidence list. Unknown includes remain external and never become
invented internal dependencies.

## Human-Readable Architecture View

The Mermaid graph is component-level because a file-level diagram with several
hundred nodes is not reviewable. It groups applications, libraries, platform
adapters, tests, and support tooling. The JSON retains the complete file-level
inventory and resolved local include graph for programmatic queries.

The generated README supplements the graph with manually curated descriptions
for the major runtime paths:

- `ieum` GUI orchestrates settings, discovery, updates, diagnostics, and local
  IPC;
- `ieum-core` runs server or client input transport;
- `ieum-daemon` owns the Windows service lifecycle;
- `server` captures and routes local input to clients;
- `client` consumes server protocol messages;
- `deskflow` contains shared KVM protocol and application behavior;
- `net`, `io`, and `mt` provide transport, framing, and concurrency;
- `platform` and `arch` isolate operating-system integration;
- `gui` contains Qt UI, configuration models, validators, IPC clients, and
  update/diagnostic surfaces.

## Audit Method

The audit combines independent evidence classes:

1. inventory and dependency mapping;
2. compiler configuration, native build, and CTest execution;
3. repository-provided CodeQL, SonarCloud, Valgrind, lint, and packaging
   configuration review;
4. available local analyzers such as compiler warnings, clang-tidy, cppcheck,
   CMake validation, Python compilation, and repository hygiene tools;
5. targeted source-pattern searches for memory ownership, integer/length
   handling, process execution, filesystem boundaries, TLS verification,
   logging of sensitive material, concurrency, event lifetime, unchecked return
   values, platform API misuse, and stale protocol/version assumptions;
6. manual trace review of each runtime component and all platform branches;
7. focused reproduction or unit-level proof for each candidate finding;
8. comparison with open and closed GitHub issues before publication.

A finding is published only when it has a concrete affected path, an observable
impact, a reproducible or source-provable failure mechanism, and a bounded
remediation direction. Style preferences and unproven suspicions stay in the
audit report as observations or are discarded.

## Public Repository Security Policy

The repository is public. `docs/SECURITY.md` requires exploit details or
sensitive information to use GitHub Security Advisories. Therefore:

- ordinary correctness, reliability, maintainability, test, documentation, and
  non-sensitive hardening defects are filed as public issues;
- a vulnerability whose reproduction would materially help exploitation is
  not disclosed in a public issue; it is summarized in the local audit report
  with sensitive mechanics omitted and reported through a Security Advisory
  only if the available GitHub interface safely supports that action;
- secrets, tokens, private addresses, user paths, certificate material, and
  captured input/clipboard contents are never included in issues or artifacts.

## GitHub Issue Format

Each issue contains:

- impact and affected component;
- exact source locations at the audited revision;
- minimal reproduction or deterministic reasoning;
- expected and actual behavior;
- proposed acceptance criteria;
- tests that should prevent recurrence;
- audit revision and a note that no product fix was included in this audit.

The existing `bug`, `documentation`, and `enhancement` labels are used only
when their meaning matches. No new labels, assignees, milestones, or project
membership are created without a repository convention that justifies them.

## Error Handling and Reproducibility

The generator exits non-zero for a missing repository root, malformed output
path, or failed Git inventory. It reports unresolved local-looking includes in
the JSON rather than silently dropping them. Generated files are UTF-8 with LF
line endings and a final newline.

The audit report distinguishes:

- verified pass;
- verified failure;
- unavailable tool or platform;
- test blocked by environment;
- behavior requiring physical multi-machine validation.

An unavailable macOS, Linux, or hardware-only check never becomes a passing
claim. Existing failures are separated from failures introduced by audit
artifacts.

## Acceptance Criteria

- Every tracked file is represented in the inventory or an explicit support
  exclusion.
- Every `src/apps`, `src/lib`, and `src/unittests` subtree has a component node.
- All resolved repository-local includes and mapped CMake links have evidence.
- Re-running the generator without source changes produces no diff.
- Generator tests pass from a clean Python standard-library environment.
- Native build/test and every available static check have recorded commands and
  outcomes.
- Every source component receives a recorded manual review receipt.
- Every published GitHub issue is independently rechecked and is not a known
  duplicate.
- The final report links every created issue and states all untested platform or
  hardware surfaces.

## Non-Goals

- Refactoring or fixing product code during the audit;
- claiming macOS, Linux, multi-machine, signing, notarization, or hardware
  behavior without executing the corresponding environment;
- publishing sensitive exploit instructions in a public issue;
- replacing compiler/linker truth with a heuristic graph;
- staging or modifying user-owned untracked files.
