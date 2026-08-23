# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

"""Deterministic source and component graph helpers for Ieum."""

from __future__ import annotations

import re
import shlex
import subprocess
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

_QUOTED_INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)
_ANGLE_INCLUDE = re.compile(r"^\s*#\s*include\s*<([^>]+)>", re.MULTILINE)
_MAIN_FUNCTION = re.compile(r"\b(?:int|auto)\s+main\s*\(")
_PYTHON_MAIN = re.compile(
    r"^\s*if\s+__name__\s*==\s*['\"]__main__['\"]\s*:", re.MULTILINE
)
_CMAKE_VARIABLE = re.compile(r"\$\{([^}]+)\}")
_CMAKE_FUNCTION_BLOCK = re.compile(
    r"(?is)\b(?:function|macro)\s*\([^)]*\).*?\bend(?:function|macro)\s*\([^)]*\)"
)

_C_FAMILY_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".m",
    ".mm",
}

_SOURCE_LANGUAGES = {
    "c",
    "c++",
    "c/c++ header",
    "objective-c",
    "objective-c++",
    "python",
    "swift",
}

_LANGUAGE_BY_SUFFIX = {
    ".c": "c",
    ".cc": "c++",
    ".cpp": "c++",
    ".cxx": "c++",
    ".h": "c/c++ header",
    ".hh": "c/c++ header",
    ".hpp": "c/c++ header",
    ".hxx": "c/c++ header",
    ".m": "objective-c",
    ".mm": "objective-c++",
    ".swift": "swift",
    ".py": "python",
    ".cmake": "cmake",
    ".in": "template",
    ".ui": "qt-ui",
    ".qrc": "qt-resource",
    ".ts": "qt-translation",
    ".json": "json",
    ".yml": "yaml",
    ".yaml": "yaml",
    ".xml": "xml",
    ".md": "markdown",
    ".txt": "text",
    ".ps1": "powershell",
    ".sh": "shell",
    ".bat": "batch",
    ".plist": "plist",
    ".desktop": "desktop-entry",
    ".rc": "windows-resource",
    ".toml": "toml",
}

_BINARY_SUFFIXES = {
    ".7z",
    ".bmp",
    ".dmg",
    ".dll",
    ".exe",
    ".ico",
    ".icns",
    ".jpg",
    ".jpeg",
    ".msi",
    ".png",
    ".tiff",
    ".zip",
}

_GRAPH_OUTPUT_PREFIX = "docs/codegraph/"

_COMPONENT_DESCRIPTIONS = {
    "app:deskflow-core": "Server/client KVM runtime process and command-line entry point.",
    "app:deskflow-daemon": "Windows service process that owns core lifecycle and session handoff.",
    "app:deskflow-gui": "Qt desktop entry point for configuration and runtime orchestration.",
    "app:res": "Application icons, manifests, bundle metadata, and packaged resources.",
    "lib:arch": "Operating-system abstraction for daemon, network, logging, and threading primitives.",
    "lib:base": "Events, jobs, queues, logging, strings, Unicode, and timing primitives.",
    "lib:client": "Connection to the server and dispatch of incoming KVM protocol messages.",
    "lib:common": "Settings, network-interface selection, Tailscale integration, and shared product types.",
    "lib:deskflow": "Shared KVM application, protocol, screen, key, clipboard, and IPC behavior.",
    "lib:gui": "Qt windows, dialogs, widgets, validators, startup, diagnostics, updates, and IPC clients.",
    "lib:io": "Stream interfaces, buffers, filters, and framed I/O support.",
    "lib:mt": "Thread, mutex, lock, and condition-variable wrappers.",
    "lib:net": "TCP/TLS sockets, multiplexing, addresses, certificates, and fingerprint persistence.",
    "lib:platform": "Windows, macOS, X11, libei, and portal input/clipboard adapters.",
    "lib:server": "Client proxies, input routing, screen topology, filters, and cursor transforms.",
    "support:github": "GitHub Actions workflows, composite actions, templates, and repository automation.",
    "support:translations": "Qt translation catalogs and translation build rules.",
    "support:docs": "User, developer, security, design, release, audit, and generated documentation.",
    "support:cmake": "Shared dependency, coverage, packaging, and signing CMake modules.",
    "support:artwork": "Brand artwork and documentation screenshots.",
}

_KIND_GROUP_LABELS = {
    "application": "Applications",
    "library": "Libraries",
    "test": "Tests",
    "tool": "Tools",
    "deployment": "Deployment",
    "automation": "Automation",
    "localization": "Localization",
    "documentation": "Documentation",
    "build": "Build support",
    "resource": "Resources",
    "support": "Other support",
}


@dataclass(frozen=True)
class ComponentRef:
    """Stable component identity derived from a repository-relative path."""

    id: str
    kind: str
    path: str


@dataclass(frozen=True)
class TargetDecl:
    """A CMake target declaration."""

    name: str
    kind: str


@dataclass(frozen=True)
class TargetLink:
    """A dependency named by target_link_libraries."""

    source: str
    target: str


def classify_component(path: str) -> ComponentRef:
    """Classify one repository-relative path into a stable component."""

    normalized = PurePosixPath(path.replace("\\", "/"))
    parts = normalized.parts
    if (
        len(parts) == 3
        and parts[0] == "src"
        and parts[1] in {"apps", "lib", "unittests"}
        and parts[2] == "CMakeLists.txt"
    ):
        return ComponentRef(
            id="support:source-build",
            kind="build",
            path="src",
        )
    if parts[:2] == ("src", "lib") and len(parts) >= 4:
        return ComponentRef(
            id=f"lib:{parts[2]}",
            kind="library",
            path=f"src/lib/{parts[2]}",
        )
    if parts[:2] == ("src", "apps") and len(parts) >= 4:
        return ComponentRef(
            id=f"app:{parts[2]}",
            kind="application",
            path=f"src/apps/{parts[2]}",
        )
    if parts[:2] == ("src", "unittests") and len(parts) >= 4:
        return ComponentRef(
            id=f"test:{parts[2]}",
            kind="test",
            path=f"src/unittests/{parts[2]}",
        )
    if parts[:2] == ("tools", "tests"):
        return ComponentRef(
            id="test:codegraph-tools",
            kind="test",
            path="tools/tests",
        )
    if parts and parts[0] == "tools" and len(parts) >= 2:
        if len(parts) >= 3:
            name = parts[1]
            component_path = f"tools/{name}"
        else:
            name = PurePosixPath(parts[1]).stem
            component_path = f"tools/{parts[1]}"
        return ComponentRef(
            id=f"tool:{name}",
            kind="tool",
            path=component_path,
        )
    if parts and parts[0] == "deploy":
        name = parts[1] if len(parts) >= 3 else "root"
        component_path = f"deploy/{name}" if name != "root" else "deploy"
        return ComponentRef(
            id=f"deploy:{name}",
            kind="deployment",
            path=component_path,
        )
    if parts and parts[0] == ".github":
        return ComponentRef(
            id="support:github",
            kind="automation",
            path=".github",
        )
    if parts and parts[0] == "translations":
        return ComponentRef(
            id="support:translations",
            kind="localization",
            path="translations",
        )
    if parts and parts[0] == "docs":
        return ComponentRef(
            id="support:docs",
            kind="documentation",
            path="docs",
        )
    if parts and parts[0] == "cmake":
        return ComponentRef(id="support:cmake", kind="build", path="cmake")
    if parts and parts[0] == "artwork":
        return ComponentRef(
            id="support:artwork",
            kind="resource",
            path="artwork",
        )
    return ComponentRef(id="support:root", kind="support", path=".")


def parse_local_includes(text: str) -> list[str]:
    """Return sorted, unique quoted includes from C-family source text."""

    return sorted(set(_QUOTED_INCLUDE.findall(text)))


def _normalize_relative_path(path: PurePosixPath) -> str | None:
    parts: list[str] = []
    for part in path.parts:
        if part in ("", "."):
            continue
        if part == "..":
            if not parts:
                return None
            parts.pop()
        else:
            parts.append(part)
    return "/".join(parts)


def resolve_local_include(
    source_path: str,
    include: str,
    tracked_paths: set[str],
) -> str | None:
    """Resolve a quoted include to a tracked repository path when possible."""

    normalized_tracked = {path.replace("\\", "/") for path in tracked_paths}
    source = PurePosixPath(source_path.replace("\\", "/"))
    include_path = PurePosixPath(include.replace("\\", "/"))
    component_root = PurePosixPath(classify_component(source_path).path)
    candidate_roots = (
        source.parent,
        component_root,
        PurePosixPath("."),
        PurePosixPath("src/lib"),
        PurePosixPath("src"),
    )
    for root in candidate_roots:
        candidate = _normalize_relative_path(root / include_path)
        if candidate is not None and candidate in normalized_tracked:
            return candidate
        generated_template = f"{candidate}.in" if candidate is not None else None
        if generated_template is not None and generated_template in normalized_tracked:
            return generated_template
    return None


def _strip_cmake_comments(text: str) -> str:
    return "\n".join(line.split("#", 1)[0] for line in text.splitlines())


def _iter_cmake_calls(text: str, names: set[str]):
    pattern = re.compile(r"(?i)\b(" + "|".join(map(re.escape, names)) + r")\s*\(")
    position = 0
    while match := pattern.search(text, position):
        depth = 1
        index = match.end()
        quote: str | None = None
        escaped = False
        while index < len(text) and depth:
            char = text[index]
            if escaped:
                escaped = False
            elif char == "\\" and quote is not None:
                escaped = True
            elif char in ('"', "'"):
                quote = None if quote == char else char if quote is None else quote
            elif quote is None:
                if char == "(":
                    depth += 1
                elif char == ")":
                    depth -= 1
            index += 1
        if depth == 0:
            yield match.group(1).lower(), text[match.end() : index - 1]
            position = index
        else:
            position = match.end()


def _cmake_tokens(body: str) -> list[str]:
    lexer = shlex.shlex(body, posix=True)
    lexer.whitespace_split = True
    lexer.commenters = ""
    return list(lexer)


def _expand_cmake_variables(value: str, variables: dict[str, str]) -> str:
    expanded = value
    for _ in range(20):
        replacement = _CMAKE_VARIABLE.sub(
            lambda match: variables.get(match.group(1), match.group(0)),
            expanded,
        )
        if replacement == expanded:
            break
        expanded = replacement
    return expanded


def _strip_cmake_function_bodies(text: str) -> str:
    return _CMAKE_FUNCTION_BLOCK.sub("", text)


def parse_cmake_variables(
    text: str,
    initial: dict[str, str] | None = None,
) -> dict[str, str]:
    """Resolve simple scalar CMake set() assignments in source order."""

    variables = dict(initial or {})
    cleaned = _strip_cmake_function_bodies(_strip_cmake_comments(text))
    terminators = {"CACHE", "PARENT_SCOPE", "FORCE"}
    for _, body in _iter_cmake_calls(cleaned, {"set"}):
        tokens = _cmake_tokens(body)
        if len(tokens) < 2 or tokens[0].startswith("ENV{"):
            continue
        values: list[str] = []
        for token in tokens[1:]:
            if token.upper() in terminators:
                break
            values.append(token)
        if not values:
            continue
        value = " ".join(values)
        variables[tokens[0]] = _expand_cmake_variables(value, variables)
    return variables


def parse_cmake(
    text: str,
    variables: dict[str, str] | None = None,
) -> tuple[list[TargetDecl], list[TargetLink]]:
    """Extract target declarations and link items from CMake source."""

    targets: set[TargetDecl] = set()
    links: set[TargetLink] = set()
    cleaned = _strip_cmake_function_bodies(_strip_cmake_comments(text))
    local_variables = parse_cmake_variables(cleaned, variables)
    call_names = {
        "add_executable",
        "add_library",
        "create_test",
        "target_link_libraries",
    }
    ignored_link_tokens = {
        "PRIVATE",
        "PUBLIC",
        "INTERFACE",
        "LINK_PRIVATE",
        "LINK_PUBLIC",
        "debug",
        "general",
        "optimized",
    }
    for name, body in _iter_cmake_calls(cleaned, call_names):
        tokens = [
            _expand_cmake_variables(token, local_variables)
            for token in _cmake_tokens(body)
        ]
        if not tokens:
            continue
        if name == "add_executable":
            targets.add(TargetDecl(tokens[0], "executable"))
        elif name == "add_library":
            targets.add(TargetDecl(tokens[0], "library"))
        elif name == "create_test":
            for index, token in enumerate(tokens[:-1]):
                if token.upper() == "NAME":
                    targets.add(TargetDecl(tokens[index + 1], "test"))
                    break
        elif name == "target_link_libraries":
            source = tokens[0]
            for target in tokens[1:]:
                if target.upper() not in {
                    token.upper() for token in ignored_link_tokens
                } and not _CMAKE_VARIABLE.search(target):
                    links.add(TargetLink(source, target))
    return (
        sorted(targets, key=lambda target: (target.name, target.kind)),
        sorted(links, key=lambda link: (link.source, link.target)),
    )


def _run_git(
    root: Path, *args: str, check: bool = True
) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        ["git", *args],
        cwd=root,
        check=check,
        capture_output=True,
    )


def git_tracked_files(root: Path) -> list[str]:
    """Return Git-indexed files in deterministic repository path order."""

    result = _run_git(root, "ls-files", "-z")
    paths = [
        item.decode("utf-8", errors="surrogateescape").replace("\\", "/")
        for item in result.stdout.split(b"\0")
        if item
    ]
    return sorted(paths)


def _git_text(root: Path, *args: str, default: str = "") -> str:
    result = _run_git(root, *args, check=False)
    if result.returncode != 0:
        return default
    return result.stdout.decode("utf-8", errors="replace").strip()


def _revision_metadata(root: Path) -> dict[str, object]:
    status_lines = _git_text(
        root,
        "status",
        "--porcelain=v1",
        "--untracked-files=no",
    ).splitlines()
    relevant_dirty_lines = [
        line
        for line in status_lines
        if _GRAPH_OUTPUT_PREFIX not in line.replace("\\", "/")
    ]
    return {
        "branch": _git_text(root, "branch", "--show-current"),
        "commit": _git_text(root, "rev-parse", "HEAD", default="uncommitted"),
        "dirtyTrackedFiles": bool(relevant_dirty_lines),
    }


def _language_for_path(path: str) -> str:
    pure_path = PurePosixPath(path)
    if pure_path.name == "CMakeLists.txt":
        return "cmake"
    if pure_path.name.lower() == "dockerfile":
        return "dockerfile"
    return _LANGUAGE_BY_SUFFIX.get(pure_path.suffix.lower(), "other")


def _read_file(root: Path, path: str) -> tuple[bytes, str | None]:
    data = (root / Path(*PurePosixPath(path).parts)).read_bytes()
    suffix = PurePosixPath(path).suffix.lower()
    if suffix in _BINARY_SUFFIXES or b"\0" in data:
        return data, None
    return data, data.decode("utf-8-sig", errors="replace")


def _line_count(text: str) -> int:
    if not text:
        return 0
    return text.count("\n") + (0 if text.endswith("\n") else 1)


def _external_dependency_list(
    dependencies: dict[tuple[str, str], set[str]],
) -> list[dict[str, object]]:
    return [
        {
            "kind": kind,
            "name": name,
            "sources": sorted(sources),
        }
        for (kind, name), sources in sorted(dependencies.items())
    ]


def _is_entry_point(component: ComponentRef, path: str, text: str | None) -> bool:
    if text is None or component.kind not in {"application", "tool"}:
        return False
    suffix = PurePosixPath(path).suffix.lower()
    if suffix in {".c", ".cc", ".cpp", ".cxx", ".m", ".mm"}:
        return bool(_MAIN_FUNCTION.search(text))
    if suffix == ".py":
        return bool(_PYTHON_MAIN.search(text))
    return suffix == ".swift"


def build_graph(root: Path) -> dict[str, object]:
    """Build the complete deterministic repository code graph."""

    root = root.resolve()
    if not (root / ".git").exists():
        raise ValueError(f"not a Git repository root: {root}")

    tracked_paths = git_tracked_files(root)
    excluded_paths = [
        path for path in tracked_paths if path.startswith(_GRAPH_OUTPUT_PREFIX)
    ]
    represented_paths = [
        path for path in tracked_paths if not path.startswith(_GRAPH_OUTPUT_PREFIX)
    ]
    tracked_set = set(represented_paths)

    file_nodes: list[dict[str, object]] = []
    text_by_path: dict[str, str] = {}
    component_refs: dict[str, ComponentRef] = {}
    component_files: dict[str, list[dict[str, object]]] = {}
    entry_points: list[dict[str, str]] = []

    for path in represented_paths:
        component = classify_component(path)
        component_refs[component.id] = component
        data, text = _read_file(root, path)
        if text is not None:
            text_by_path[path] = text
        node: dict[str, object] = {
            "binary": text is None,
            "component": component.id,
            "language": _language_for_path(path),
            "lineCount": None if text is None else _line_count(text),
            "path": path,
            "sizeBytes": len(data),
        }
        file_nodes.append(node)
        component_files.setdefault(component.id, []).append(node)
        if _is_entry_point(component, path, text):
            entry_points.append(
                {
                    "component": component.id,
                    "kind": "process" if component.kind == "application" else "tool",
                    "path": path,
                }
            )

    file_edges: set[tuple[str, str, str]] = set()
    component_edge_data: dict[tuple[str, str], dict[str, set[str]]] = {}
    external_dependencies: dict[tuple[str, str], set[str]] = {}
    unresolved_includes: set[tuple[str, str]] = set()

    def add_component_edge(
        source_component: str,
        target_component: str,
        kind: str,
        evidence: str,
    ) -> None:
        if source_component == target_component:
            return
        data = component_edge_data.setdefault(
            (source_component, target_component),
            {"kinds": set(), "evidence": set()},
        )
        data["kinds"].add(kind)
        data["evidence"].add(evidence)

    for source_path in sorted(text_by_path):
        if PurePosixPath(source_path).suffix.lower() not in _C_FAMILY_SUFFIXES:
            continue
        source_text = text_by_path[source_path]
        source_component = classify_component(source_path).id
        for include in parse_local_includes(source_text):
            target_path = resolve_local_include(source_path, include, tracked_set)
            if target_path is None:
                unresolved_includes.add((source_path, include))
                external_dependencies.setdefault(
                    ("unresolved-quoted-include", include), set()
                ).add(source_path)
                continue
            file_edges.add((source_path, target_path, include))
            add_component_edge(
                source_component,
                classify_component(target_path).id,
                "include",
                f'{source_path}: #include "{include}" -> {target_path}',
            )
        for include in sorted(set(_ANGLE_INCLUDE.findall(source_text))):
            external_dependencies.setdefault(("system-include", include), set()).add(
                source_path
            )

    cmake_targets: list[dict[str, str]] = []
    target_components: dict[str, str] = {}
    cmake_links: list[tuple[str, TargetLink]] = []
    root_cmake_variables = parse_cmake_variables(text_by_path.get("CMakeLists.txt", ""))
    for cmake_path in sorted(
        path
        for path in text_by_path
        if PurePosixPath(path).name == "CMakeLists.txt"
        or PurePosixPath(path).suffix.lower() == ".cmake"
    ):
        declarations, links = parse_cmake(
            text_by_path[cmake_path],
            root_cmake_variables,
        )
        owner = classify_component(cmake_path).id
        for declaration in declarations:
            cmake_targets.append(
                {
                    "component": owner,
                    "kind": declaration.kind,
                    "name": declaration.name,
                    "source": cmake_path,
                }
            )
            target_components.setdefault(declaration.name, owner)
        for link in links:
            cmake_links.append((cmake_path, link))

    for cmake_path, link in sorted(
        cmake_links,
        key=lambda item: (item[0], item[1].source, item[1].target),
    ):
        source_component = target_components.get(
            link.source,
            classify_component(cmake_path).id,
        )
        target_component = target_components.get(link.target)
        if target_component is None:
            external_dependencies.setdefault(("cmake-link", link.target), set()).add(
                cmake_path
            )
            continue
        add_component_edge(
            source_component,
            target_component,
            "link",
            f"{cmake_path}: target_link_libraries({link.source} {link.target})",
        )

    targets_by_component: dict[str, list[str]] = {}
    for target in cmake_targets:
        targets_by_component.setdefault(target["component"], []).append(target["name"])

    components: list[dict[str, object]] = []
    for component_id, component in sorted(component_refs.items()):
        files = component_files.get(component_id, [])
        language_counts: dict[str, int] = {}
        line_count = 0
        for file_node in files:
            language = str(file_node["language"])
            language_counts[language] = language_counts.get(language, 0) + 1
            if isinstance(file_node["lineCount"], int):
                line_count += file_node["lineCount"]
        components.append(
            {
                "fileCount": len(files),
                "id": component_id,
                "kind": component.kind,
                "languageCounts": dict(sorted(language_counts.items())),
                "lineCount": line_count,
                "path": component.path,
                "targets": sorted(set(targets_by_component.get(component_id, []))),
            }
        )

    component_edges = [
        {
            "evidence": sorted(data["evidence"]),
            "kinds": sorted(data["kinds"]),
            "source": source,
            "target": target,
        }
        for (source, target), data in sorted(component_edge_data.items())
    ]
    serialized_file_edges = [
        {
            "evidence": f'#include "{include}"',
            "kind": "include",
            "source": source,
            "target": target,
        }
        for source, target, include in sorted(file_edges)
    ]

    source_nodes = [
        node for node in file_nodes if node["language"] in _SOURCE_LANGUAGES
    ]
    source_lines = sum(
        node["lineCount"] for node in source_nodes if isinstance(node["lineCount"], int)
    )

    return {
        "componentEdges": component_edges,
        "components": components,
        "entryPoints": sorted(entry_points, key=lambda item: item["path"]),
        "excludedPaths": excluded_paths,
        "externalDependencies": _external_dependency_list(external_dependencies),
        "fileEdges": serialized_file_edges,
        "files": sorted(file_nodes, key=lambda item: item["path"]),
        "revision": _revision_metadata(root),
        "schemaVersion": 1,
        "summary": {
            "componentEdges": len(component_edges),
            "components": len(components),
            "excludedFiles": len(excluded_paths),
            "fileIncludeEdges": len(serialized_file_edges),
            "representedFiles": len(file_nodes),
            "sourceFiles": len(source_nodes),
            "sourceLines": source_lines,
            "targets": len(cmake_targets),
            "trackedFiles": len(tracked_paths),
        },
        "targets": sorted(
            cmake_targets,
            key=lambda item: (item["name"], item["source"], item["kind"]),
        ),
        "unresolvedIncludes": [
            {"include": include, "source": source}
            for source, include in sorted(unresolved_includes)
        ],
    }


def component_description(component: dict[str, object]) -> str:
    """Return a concise responsibility statement for a component node."""

    component_id = str(component["id"])
    known = _COMPONENT_DESCRIPTIONS.get(component_id)
    if known:
        return known
    kind = str(component["kind"])
    path = str(component["path"])
    if kind == "test":
        return (
            f"Automated tests for the `{component_id.removeprefix('test:')}` surface."
        )
    if kind == "tool":
        return f"Developer or diagnostic tool rooted at `{path}`."
    if kind == "deployment":
        return f"Packaging and deployment logic for `{component_id.removeprefix('deploy:')}`."
    return f"Repository support files rooted at `{path}`."


def _mermaid_label(value: object) -> str:
    return str(value).replace('"', "'").replace("\n", " ")


def render_mermaid(graph: dict[str, object]) -> str:
    """Render a reviewable component-level Mermaid dependency graph."""

    components = list(graph["components"])
    node_ids = {
        str(component["id"]): f"n{index:03d}"
        for index, component in enumerate(components)
    }
    grouped: dict[str, list[dict[str, object]]] = {}
    for component in components:
        grouped.setdefault(str(component["kind"]), []).append(component)

    lines = [
        "flowchart LR",
        "  classDef application fill:#dbeafe,stroke:#2563eb,color:#172554",
        "  classDef library fill:#dcfce7,stroke:#16a34a,color:#052e16",
        "  classDef test fill:#fef3c7,stroke:#d97706,color:#451a03",
        "  classDef support fill:#f3e8ff,stroke:#9333ea,color:#3b0764",
    ]
    ordered_kinds = list(_KIND_GROUP_LABELS)
    ordered_kinds.extend(sorted(set(grouped) - set(ordered_kinds)))
    for group_index, kind in enumerate(ordered_kinds):
        items = grouped.get(kind)
        if not items:
            continue
        group_label = _KIND_GROUP_LABELS.get(kind, kind.title())
        lines.append(f'  subgraph g{group_index:02d}["{_mermaid_label(group_label)}"]')
        for component in sorted(items, key=lambda item: str(item["id"])):
            component_id = str(component["id"])
            targets = ", ".join(str(item) for item in component["targets"])
            detail = f"<br/>{targets}" if targets else ""
            lines.append(
                f'    {node_ids[component_id]}["{_mermaid_label(component_id)}{_mermaid_label(detail)}"]'
            )
        lines.append("  end")

    support_kinds = set(_KIND_GROUP_LABELS) - {"application", "library", "test"}
    for component in components:
        component_id = str(component["id"])
        kind = str(component["kind"])
        css_class = kind if kind in {"application", "library", "test"} else "support"
        if kind in support_kinds or kind not in {"application", "library", "test"}:
            css_class = "support"
        lines.append(f"  class {node_ids[component_id]} {css_class}")

    for edge in graph["componentEdges"]:
        source = str(edge["source"])
        target = str(edge["target"])
        if source not in node_ids or target not in node_ids:
            continue
        kinds = " + ".join(str(kind) for kind in edge["kinds"])
        lines.append(
            f'  {node_ids[source]} -->|"{_mermaid_label(kinds)}"| {node_ids[target]}'
        )
    return "\n".join(lines) + "\n"


def _markdown_cell(value: object) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def render_readme(graph: dict[str, object]) -> str:
    """Render the generated human-readable component inventory."""

    summary = graph["summary"]
    revision = graph["revision"]
    lines = [
        "# Ieum Code Graph",
        "",
        "이 디렉터리는 Git 인덱스, CMake 대상, C/C++ include를 근거로 생성한",
        "Ieum 코드 그래프입니다. 상세 파일 그래프는 `codegraph.json`, 사람이 읽는",
        "컴포넌트 그래프는 `component-graph.mmd`에 있습니다.",
        "",
        "## Snapshot",
        "",
        f"- Revision: `{_markdown_cell(revision['commit'])}`",
        f"- Branch: `{_markdown_cell(revision['branch'])}`",
        f"- Tracked files: {summary['trackedFiles']:,}",
        f"- Represented files: {summary['representedFiles']:,}",
        f"- Source files / lines: {summary['sourceFiles']:,} / {summary['sourceLines']:,}",
        f"- Components / targets: {summary['components']:,} / {summary['targets']:,}",
        f"- File include edges / component edges: {summary['fileIncludeEdges']:,} / {summary['componentEdges']:,}",
        "",
        "`docs/codegraph/`의 생성 산출물은 자기참조로 인한 비결정성을 막기 위해",
        "파일 인벤토리에서 명시적으로 제외됩니다.",
        "",
        "## Runtime flow",
        "",
        "```text",
        "ieum GUI ──local IPC──> ieum-core",
        "                         ├─ server → TLS/KVM protocol → remote client",
        "                         └─ client ← TLS/KVM protocol ← remote server",
        "Windows service ───────> ieum-daemon ──lifecycle──> ieum-core",
        "platform adapters <──> deskflow/client/server <──> net/io/mt/base/arch",
        "```",
        "",
        "## Entry points",
        "",
        "| Component | Kind | Path |",
        "| --- | --- | --- |",
    ]
    for entry in graph["entryPoints"]:
        lines.append(
            f"| `{_markdown_cell(entry['component'])}` | {_markdown_cell(entry['kind'])} | `{_markdown_cell(entry['path'])}` |"
        )
    if not graph["entryPoints"]:
        lines.append("| — | — | No entry point detected |")

    lines.extend(
        [
            "",
            "## Component inventory",
            "",
            "| Component | Kind | Responsibility | Files | Lines | CMake targets |",
            "| --- | --- | --- | ---: | ---: | --- |",
        ]
    )
    for component in graph["components"]:
        targets = ", ".join(f"`{target}`" for target in component["targets"]) or "—"
        lines.append(
            "| "
            f"`{_markdown_cell(component['id'])}` | "
            f"{_markdown_cell(component['kind'])} | "
            f"{_markdown_cell(component_description(component))} | "
            f"{component['fileCount']:,} | {component['lineCount']:,} | {targets} |"
        )

    lines.extend(
        [
            "",
            "## Graph semantics",
            "",
            "- `fileEdges`는 실제 저장소 파일로 해석된 quoted include입니다.",
            "- `componentEdges`는 파일 include와 CMake link를 컴포넌트 단위로 합칩니다.",
            "- `externalDependencies`는 system include, 외부 CMake link, 해석되지 않은",
            "  quoted include를 별도로 보존합니다.",
            "- `targets`는 선언 CMake 파일과 소유 컴포넌트를 함께 기록합니다.",
            "- 모든 배열과 증거 목록은 안정적으로 정렬되며 생성 시각은 저장하지 않습니다.",
            "",
            "## Regeneration",
            "",
            "```powershell",
            "uv run --no-project --python 3.13 python tools/generate-codegraph.py --root . --output docs/codegraph",
            "uv run --no-project --python 3.13 python -m unittest discover -s tools/tests -p test_codegraph.py -v",
            "```",
            "",
            "두 번째 생성에서 `git diff -- docs/codegraph`가 비어 있어야 합니다.",
            "",
            "## Limitations",
            "",
            "- CMake는 타깃 선언과 `target_link_libraries`의 정적 문법을 추출합니다.",
            "  모든 조건식과 generator expression을 평가하는 CMake 인터프리터는 아닙니다.",
            "- 파일 include 해석은 현재 파일, 저장소 루트, `src/lib`, `src` 순서의",
            "  보수적 탐색입니다. 해석 실패는 삭제하지 않고 JSON에 남깁니다.",
            "- Mermaid는 가독성을 위해 컴포넌트 수준입니다. 파일 수준 질의에는 JSON을",
            "  사용해야 합니다.",
            "- 런타임 동적 호출, Qt signal/slot 연결, 이벤트 라우팅, 네트워크 메시지",
            "  순서는 정적 include/link 그래프만으로 완전히 표현되지 않습니다.",
            "",
            "이 파일은 생성 산출물입니다. 컴포넌트 규칙은 `tools/codegraph.py`와",
            "`docs/superpowers/specs/2026-08-19-repository-audit-codegraph-design.md`에서",
            "변경한 뒤 다시 생성하십시오.",
        ]
    )
    return "\n".join(lines) + "\n"
