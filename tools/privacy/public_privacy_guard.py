#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT

"""Fail-closed privacy guard for public Git trees, ranges, and release bytes.

The detector intentionally contains no private identity values. Callers provide
an owner-private deny map and its independently frozen SHA-256 digest.
"""

from __future__ import annotations

import argparse
import base64
import binascii
import dataclasses
import gzip
import hashlib
import io
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import unicodedata
import zipfile
from pathlib import Path, PurePosixPath
from typing import Any, Dict, Iterable, List, Optional, Sequence, Set, Tuple


SCANNER_VERSION = "1.0.0"
DENY_MAP_SCHEMA = "ieum.private-deny-map.v1"
MANIFEST_SCHEMA = "ieum.public-surface-manifest.v1"
ZERO_OIDS = {"0" * 40, "0" * 64}
HEX_OID = re.compile(r"^[0-9a-f]{40}(?:[0-9a-f]{24})?$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
REF_LINE = re.compile(rb"^([0-9a-f]{40}|[0-9a-f]{64})\t(refs/[^\x00\r\n]+)$")
CHECKSUM_LINE = re.compile(r"^([0-9a-fA-F]{64})(?:  | \*)(.+)$")


class GuardError(RuntimeError):
    """Base fail-closed error."""


class SecurityError(GuardError):
    """A caller, repository, or private-map precondition is unsafe."""


class CoverageError(GuardError):
    """Input bytes could not be scanned completely."""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def strict_object(pairs: Sequence[Tuple[str, Any]]) -> Dict[str, Any]:
    result: Dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise SecurityError("duplicate JSON key in private deny map")
        result[key] = value
    return result


@dataclasses.dataclass(frozen=True)
class Rule:
    rule_id: str
    value: str
    value_class: str


@dataclasses.dataclass(frozen=True)
class Finding:
    rule_id: str
    target: str
    target_kind: str
    detector: str
    offset: int

    def as_json(self) -> Dict[str, Any]:
        return dataclasses.asdict(self)


@dataclasses.dataclass
class Limits:
    max_member_bytes: int
    max_total_bytes: int
    max_archive_depth: int


class ScanState:
    def __init__(self, rules: Sequence[Rule], limits: Limits) -> None:
        self.rules = list(rules)
        self.limits = limits
        self.findings: List[Finding] = []
        self._finding_keys: Set[Tuple[str, str, str, str]] = set()
        self.records: List[Dict[str, Any]] = []
        self.bytes_read = 0
        self.archive_members = 0

    def add_finding(
        self,
        rule_id: str,
        target: str,
        target_kind: str,
        detector: str,
        offset: int,
    ) -> None:
        key = (rule_id, target, target_kind, detector)
        if key in self._finding_keys:
            return
        self._finding_keys.add(key)
        self.findings.append(Finding(rule_id, target, target_kind, detector, offset))

    def record(self, target: str, kind: str, data: bytes) -> None:
        self.records.append(
            {
                "target": target,
                "kind": kind,
                "size": len(data),
                "sha256": sha256_bytes(data),
            }
        )


def reject_dangerous_environment() -> None:
    exact = {
        "GIT_DIR",
        "GIT_WORK_TREE",
        "GIT_INDEX_FILE",
        "GIT_OBJECT_DIRECTORY",
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
        "GIT_COMMON_DIR",
        "GIT_PROXY_COMMAND",
        "GIT_ASKPASS",
        "SSH_ASKPASS",
    }
    prefixed = ("GIT_CONFIG_", "GIT_CONFIG_KEY_", "GIT_CONFIG_VALUE_")
    rejected = sorted(
        key for key in os.environ if key in exact or any(key.startswith(prefix) for prefix in prefixed)
    )
    if rejected:
        raise SecurityError("dangerous caller Git environment is set: " + ",".join(rejected))


def load_deny_map(path: Path, expected_sha256: str) -> Tuple[List[Rule], str, str]:
    if not SHA256.fullmatch(expected_sha256):
        raise SecurityError("expected deny-map SHA-256 is not a lowercase digest")
    try:
        metadata = path.lstat()
    except FileNotFoundError as exc:
        raise SecurityError("private deny map is missing") from exc
    if stat.S_ISLNK(metadata.st_mode) or not stat.S_ISREG(metadata.st_mode):
        raise SecurityError("private deny map must be a regular non-symlink file")
    if metadata.st_mode & 0o077:
        raise SecurityError("private deny map permissions must not grant group or other access")
    data = path.read_bytes()
    actual_sha256 = sha256_bytes(data)
    if actual_sha256 != expected_sha256:
        raise SecurityError("private deny map SHA-256 mismatch")
    try:
        payload = json.loads(data.decode("utf-8"), object_pairs_hook=strict_object)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise SecurityError("private deny map is not strict UTF-8 JSON") from exc
    if not isinstance(payload, dict) or payload.get("schema") != DENY_MAP_SCHEMA:
        raise SecurityError("private deny map schema mismatch")
    version = payload.get("version")
    entries = payload.get("entries")
    if not isinstance(version, str) or not version.strip():
        raise SecurityError("private deny map version is missing")
    if not isinstance(entries, list) or not entries:
        raise SecurityError("private deny map entries are missing")

    rules: List[Rule] = []
    ids: Set[str] = set()
    values: Set[str] = set()
    for entry in entries:
        if not isinstance(entry, dict) or set(entry) != {"id", "class", "value_b64"}:
            raise SecurityError("private deny-map entry has an unexpected shape")
        rule_id = entry["id"]
        value_class = entry["class"]
        encoded = entry["value_b64"]
        if not all(isinstance(item, str) and item for item in (rule_id, value_class, encoded)):
            raise SecurityError("private deny-map entry has an empty field")
        if rule_id in ids:
            raise SecurityError("private deny-map rule ids are not unique")
        try:
            value = base64.b64decode(encoded, validate=True).decode("utf-8")
        except (binascii.Error, UnicodeDecodeError) as exc:
            raise SecurityError("private deny-map value is not canonical base64 UTF-8") from exc
        if not value or "\x00" in value:
            raise SecurityError("private deny-map value is empty or contains NUL")
        canonical = unicodedata.normalize("NFKC", value).replace("\\", "/").casefold()
        if canonical in values:
            raise SecurityError("private deny-map values are normalization duplicates")
        ids.add(rule_id)
        values.add(canonical)
        rules.append(Rule(rule_id, value, value_class))
    return rules, version, actual_sha256


def text_variants(value: str) -> Set[str]:
    variants: Set[str] = set()
    for form in ("NFC", "NFD", "NFKC", "NFKD"):
        normalized = unicodedata.normalize(form, value)
        variants.add(normalized)
        variants.add(normalized.replace("\\", "/"))
        variants.add(normalized.replace("/", "\\"))
    return {item for item in variants if item}


def normalized_text(value: str) -> str:
    return unicodedata.normalize("NFKC", value).replace("\\", "/").casefold()


def scan_bytes(state: ScanState, data: bytes, target: str, target_kind: str) -> None:
    if len(data) > state.limits.max_member_bytes:
        raise CoverageError(f"member exceeds configured byte limit: {target}")
    state.bytes_read += len(data)
    if state.bytes_read > state.limits.max_total_bytes:
        raise CoverageError("aggregate scan byte limit exceeded")
    state.record(target, target_kind, data)
    try:
        decoded = data.decode("utf-8")
    except UnicodeDecodeError:
        decoded = None
    normalized = normalized_text(decoded) if decoded is not None else None
    for rule in state.rules:
        for variant in text_variants(rule.value):
            for encoding, pattern in (
                ("utf-8", variant.encode("utf-8")),
                ("utf-16le", variant.encode("utf-16le")),
                ("utf-16be", variant.encode("utf-16be")),
            ):
                offset = data.find(pattern)
                if offset >= 0:
                    state.add_finding(rule.rule_id, target, target_kind, encoding, offset)
        if normalized is not None:
            offset = normalized.find(normalized_text(rule.value))
            if offset >= 0:
                state.add_finding(rule.rule_id, target, target_kind, "unicode-slash-casefold", offset)


def safe_member_name(name: str) -> str:
    normalized = name.replace("\\", "/")
    candidate = PurePosixPath(normalized)
    if not normalized or normalized.startswith("/") or any(part in {"", ".", ".."} for part in candidate.parts):
        raise CoverageError("archive contains an unsafe member path")
    return str(candidate)


def archive_kind(data: bytes) -> Optional[str]:
    if data.startswith((b"PK\x03\x04", b"PK\x05\x06", b"PK\x07\x08")):
        return "zip"
    if data.startswith(b"\x1f\x8b"):
        return "gzip"
    if data.startswith((b"BZh", b"\xfd7zXZ\x00")) or (len(data) >= 262 and data[257:262] == b"ustar"):
        return "tar"
    return None


def external_package_kind(path: Path, data: bytes) -> Optional[str]:
    """Return the bounded external decoder required by a release package."""
    name = path.name.casefold()
    if data.startswith(b"flatpak\x00"):
        return "flatpak"
    if data.startswith(b"\x28\xb5\x2f\xfd"):
        return "zstd"
    sevenzip_magics = (
        b"!<arch>\n",  # Debian ar container
        b"\xed\xab\xee\xdb",  # RPM
        b"7z\xbc\xaf'\x1c",
        b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1",  # MSI compound file
        b"MSCF",  # Cabinet
        b"070701",  # new ASCII cpio
        b"070702",  # CRC ASCII cpio
        b"070707",  # old ASCII cpio
    )
    if data.startswith(sevenzip_magics) or name.endswith(".dmg"):
        return "sevenzip"
    return None


def required_tool(*names: str) -> str:
    for name in names:
        candidate = shutil.which(name)
        if candidate is None:
            continue
        resolved = Path(candidate).resolve()
        if resolved.is_file():
            return str(resolved)
    raise CoverageError("required package inspection tool is unavailable")


def tool_environment(home: Path) -> Dict[str, str]:
    return {
        "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
        "HOME": str(home),
        "LC_ALL": "C",
        "LANG": "C",
        "XDG_CACHE_HOME": str(home / ".cache"),
        "XDG_CONFIG_HOME": str(home / ".config"),
        "XDG_DATA_HOME": str(home / ".local" / "share"),
    }


def process_limits(max_file_bytes: int, timeout_seconds: int):
    if os.name != "posix":
        return None

    def apply_limits() -> None:
        import resource

        os.umask(0o077)
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_FSIZE, (max_file_bytes, max_file_bytes))
        resource.setrlimit(
            resource.RLIMIT_CPU,
            (timeout_seconds, timeout_seconds + 1),
        )

    return apply_limits


def run_bounded_tool(
    command: Sequence[str],
    cwd: Path,
    env: Dict[str, str],
    max_file_bytes: int,
    capture_limit: int = 1024 * 1024,
    timeout_seconds: int = 120,
    allowed_return_codes: Sequence[int] = (0,),
) -> Tuple[int, bytes, bytes]:
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        process = subprocess.Popen(
            list(command),
            cwd=str(cwd),
            env=env,
            stdin=subprocess.DEVNULL,
            stdout=stdout,
            stderr=stderr,
            preexec_fn=process_limits(max_file_bytes, timeout_seconds),
        )
        try:
            return_code = process.wait(timeout=timeout_seconds)
        except subprocess.TimeoutExpired as exc:
            process.kill()
            process.wait()
            raise CoverageError("package inspection tool timed out") from exc

        stdout.flush()
        stderr.flush()
        stdout_size = os.fstat(stdout.fileno()).st_size
        stderr_size = os.fstat(stderr.fileno()).st_size
        if stdout_size > capture_limit or stderr_size > capture_limit:
            raise CoverageError("package inspection tool output exceeds configured byte limit")
        if return_code not in allowed_return_codes:
            raise CoverageError("package inspection tool rejected the archive")
        stdout.seek(0)
        stderr.seek(0)
        return return_code, stdout.read(), stderr.read()


def scan_extracted_tree(
    state: ScanState,
    root: Path,
    target: str,
    depth: int,
    allow_safe_links: bool = False,
) -> None:
    if depth >= state.limits.max_archive_depth:
        raise CoverageError("archive nesting depth limit exceeded")
    metadata = root.lstat()
    if stat.S_ISLNK(metadata.st_mode) or not stat.S_ISDIR(metadata.st_mode):
        raise CoverageError("package extractor did not produce a regular directory")

    entries = sorted(root.rglob("*"), key=lambda item: item.relative_to(root).as_posix())
    files = 0
    seen: Set[str] = set()
    for path in entries:
        relative = safe_member_name(path.relative_to(root).as_posix())
        if relative in seen:
            raise CoverageError("archive contains duplicate member paths")
        seen.add(relative)
        entry = path.lstat()
        if stat.S_ISLNK(entry.st_mode):
            if not allow_safe_links:
                raise CoverageError("archive contains a symlink member")
            try:
                link_name = os.readlink(path)
                link_data = link_name.encode("utf-8")
                resolved = (path.parent / link_name).resolve(strict=True)
                resolved.relative_to(root.resolve())
            except (OSError, RuntimeError, UnicodeEncodeError, ValueError) as exc:
                raise CoverageError("archive contains an unsafe symlink member") from exc
            resolved_entry = resolved.lstat()
            if not (
                stat.S_ISREG(resolved_entry.st_mode)
                or stat.S_ISDIR(resolved_entry.st_mode)
            ):
                raise CoverageError("archive symlink resolves to an unsupported member")
            state.archive_members += 1
            scan_bytes(
                state,
                link_data,
                target + "!/" + relative + "!/link-target",
                "archive-link",
            )
            continue
        if stat.S_ISDIR(entry.st_mode):
            continue
        if not stat.S_ISREG(entry.st_mode):
            raise CoverageError("archive contains a link or special member")
        files += 1
        state.archive_members += 1
        scan_file(
            state,
            path,
            "archive-member",
            depth=depth + 1,
            target=target + "!/" + relative,
        )
    if files == 0:
        raise CoverageError("package extractor produced no regular files")


def scan_sevenzip_package(
    state: ScanState,
    path: Path,
    target: str,
    depth: int,
    original_sha256: str,
) -> None:
    sevenzip = required_tool("7zz", "7z")
    with tempfile.TemporaryDirectory(prefix="ieum-privacy-7z-") as temporary:
        root = Path(temporary)
        output = root / "output"
        home = root / "home"
        output.mkdir()
        home.mkdir()
        return_code, stdout, stderr = run_bounded_tool(
            (
                sevenzip,
                "x",
                "-y",
                "-bd",
                "-bb0",
                "-aoa",
                f"-o{output}",
                "--",
                str(path.resolve()),
            ),
            root,
            tool_environment(home),
            state.limits.max_member_bytes,
            allowed_return_codes=(0, 1, 2)
            if path.name.casefold().endswith(".dmg")
            else (0,),
        )
        if return_code == 1:
            combined = stdout + b"\n" + stderr
            errors = [
                line.strip()
                for line in combined.splitlines()
                if line.strip().startswith(b"ERROR:")
            ]
            prefix = b"ERROR: Dangerous link path was ignored : "
            if not errors or any(not line.startswith(prefix) for line in errors):
                raise CoverageError("package inspection tool reported an unsafe warning")
            for index, line in enumerate(errors):
                state.archive_members += 1
                scan_bytes(
                    state,
                    line,
                    f"{target}!/ignored-external-link-{index}",
                    "archive-link",
                )
        if sha256_file(path) != original_sha256:
            raise SecurityError("package inspection tool changed its input")
        scan_extracted_tree(
            state,
            output,
            target,
            depth,
            allow_safe_links=path.name.casefold().endswith(".dmg"),
        )


def scan_zstd_package(
    state: ScanState,
    path: Path,
    target: str,
    depth: int,
    original_sha256: str,
) -> None:
    zstd = required_tool("zstd")
    with tempfile.TemporaryDirectory(prefix="ieum-privacy-zstd-") as temporary:
        root = Path(temporary)
        home = root / "home"
        home.mkdir()
        _, expanded, _ = run_bounded_tool(
            (
                zstd,
                "--decompress",
                "--stdout",
                "--quiet",
                "--no-progress",
                str(path.resolve()),
            ),
            root,
            tool_environment(home),
            state.limits.max_member_bytes,
            capture_limit=state.limits.max_member_bytes,
        )
        if sha256_file(path) != original_sha256:
            raise SecurityError("package inspection tool changed its input")
        expanded_target = target + "!/zstd-payload"
        state.archive_members += 1
        scan_bytes(state, expanded, expanded_target, "archive-member")
        if scan_archive(state, expanded, expanded_target, depth + 1):
            return

        payload = root / "zstd-payload"
        payload.write_bytes(expanded)
        payload.chmod(0o600)
        nested_kind = external_package_kind(payload, expanded)
        if nested_kind is None:
            raise CoverageError("Zstandard payload is not a readable archive")
        scan_external_package(
            state,
            payload,
            expanded_target,
            depth + 1,
            nested_kind,
            sha256_bytes(expanded),
        )


def scan_flatpak_package(
    state: ScanState,
    path: Path,
    target: str,
    depth: int,
    original_sha256: str,
) -> None:
    flatpak = required_tool("flatpak")
    ostree = required_tool("ostree")
    with tempfile.TemporaryDirectory(prefix="ieum-privacy-flatpak-") as temporary:
        root = Path(temporary)
        repository = root / "repository"
        home = root / "home"
        home.mkdir()
        env = tool_environment(home)
        run_bounded_tool(
            (ostree, f"--repo={repository}", "init", "--mode=archive-z2"),
            root,
            env,
            state.limits.max_member_bytes,
        )
        run_bounded_tool(
            (flatpak, "build-import-bundle", str(repository), str(path.resolve())),
            root,
            env,
            state.limits.max_member_bytes,
        )
        _, refs_raw, _ = run_bounded_tool(
            (ostree, f"--repo={repository}", "refs"),
            root,
            env,
            state.limits.max_member_bytes,
        )
        try:
            refs = [item for item in refs_raw.decode("utf-8").splitlines() if item]
        except UnicodeDecodeError as exc:
            raise CoverageError("Flatpak repository returned an undecodable ref") from exc
        app_refs = sorted(item for item in refs if item.startswith("app/"))
        if len(app_refs) != 1 or any(safe_member_name(item) != item for item in app_refs):
            raise CoverageError("Flatpak bundle did not import exactly one safe app ref")

        _, commit_raw, _ = run_bounded_tool(
            (ostree, f"--repo={repository}", "rev-parse", app_refs[0]),
            root,
            env,
            state.limits.max_member_bytes,
        )
        try:
            commit = commit_raw.decode("ascii").strip()
        except UnicodeDecodeError as exc:
            raise CoverageError("Flatpak repository returned an invalid commit") from exc
        if HEX_OID.fullmatch(commit) is None:
            raise CoverageError("Flatpak repository returned an invalid commit")

        _, exported, _ = run_bounded_tool(
            (ostree, f"--repo={repository}", "export", commit),
            root,
            env,
            state.limits.max_member_bytes,
            capture_limit=state.limits.max_member_bytes,
        )
        if sha256_file(path) != original_sha256:
            raise SecurityError("package inspection tool changed its input")
        exported_target = target + "!/ostree-export.tar"
        state.archive_members += 1
        scan_bytes(state, exported, exported_target, "archive-member")
        if not scan_archive(
            state,
            exported,
            exported_target,
            depth + 1,
            allow_safe_links=True,
        ):
            raise CoverageError("Flatpak export is not a readable tar archive")


def scan_external_package(
    state: ScanState,
    path: Path,
    target: str,
    depth: int,
    kind: str,
    original_sha256: str,
) -> None:
    if depth >= state.limits.max_archive_depth:
        raise CoverageError("archive nesting depth limit exceeded")
    if kind == "sevenzip":
        scan_sevenzip_package(state, path, target, depth, original_sha256)
    elif kind == "zstd":
        scan_zstd_package(state, path, target, depth, original_sha256)
    elif kind == "flatpak":
        scan_flatpak_package(state, path, target, depth, original_sha256)
    else:
        raise CoverageError("unsupported package inspection tool")


def resolve_archive_link(
    member_name: str,
    link_name: str,
    hardlink: bool,
    members: Dict[str, tarfile.TarInfo],
) -> str:
    def resolve_once(name: str, target: str, is_hardlink: bool) -> str:
        normalized = target.replace("\\", "/")
        if (
            not normalized
            or "\x00" in normalized
            or normalized.startswith("/")
            or re.match(r"^[A-Za-z]:/", normalized)
        ):
            raise CoverageError("archive contains an unsafe link target")
        parts = [] if is_hardlink else list(PurePosixPath(name).parent.parts)
        for part in PurePosixPath(normalized).parts:
            if part in {"", "."}:
                continue
            if part == "..":
                if not parts:
                    raise CoverageError("archive link target escapes the archive root")
                parts.pop()
            else:
                parts.append(part)
        resolved_target = "/".join(parts)
        if not resolved_target or resolved_target not in members:
            raise CoverageError("archive link target is missing")
        return resolved_target

    visited = {member_name}
    first = resolve_once(member_name, link_name, hardlink)
    current = first
    while True:
        if current in visited:
            raise CoverageError("archive contains a link cycle")
        visited.add(current)
        info = members[current]
        if info.isfile() or info.isdir():
            return first
        if not (info.issym() or info.islnk()):
            raise CoverageError("archive link resolves to an unsupported member")
        current = resolve_once(
            current,
            info.linkname,
            info.islnk(),
        )


def scan_archive(
    state: ScanState,
    data: bytes,
    target: str,
    depth: int = 0,
    allow_safe_links: bool = False,
) -> bool:
    kind = archive_kind(data)
    if kind is None:
        return False
    if depth >= state.limits.max_archive_depth:
        raise CoverageError("archive nesting depth limit exceeded")
    if kind == "gzip":
        try:
            with gzip.GzipFile(fileobj=io.BytesIO(data)) as source:
                member = source.read(state.limits.max_member_bytes + 1)
        except (OSError, EOFError) as exc:
            raise CoverageError("gzip member is unreadable") from exc
        if len(member) > state.limits.max_member_bytes:
            raise CoverageError("gzip expansion exceeds configured byte limit")
        child = target + "!/payload"
        state.archive_members += 1
        scan_bytes(state, member, child, "archive-member")
        scan_archive(state, member, child, depth + 1)
        return True
    if kind == "zip":
        try:
            with zipfile.ZipFile(io.BytesIO(data)) as archive:
                infos = sorted(archive.infolist(), key=lambda item: item.filename)
                seen: Set[str] = set()
                for info in infos:
                    name = safe_member_name(info.filename)
                    if name in seen:
                        raise CoverageError("archive contains duplicate member paths")
                    seen.add(name)
                    mode = (info.external_attr >> 16) & 0xFFFF
                    if stat.S_ISLNK(mode):
                        raise CoverageError("archive contains a symlink member")
                    if info.flag_bits & 0x1:
                        raise CoverageError("archive contains an encrypted member")
                    if info.is_dir():
                        continue
                    if info.file_size > state.limits.max_member_bytes:
                        raise CoverageError("archive member exceeds configured byte limit")
                    try:
                        member = archive.read(info)
                    except (OSError, RuntimeError, zipfile.BadZipFile) as exc:
                        raise CoverageError("archive member is unreadable") from exc
                    child = target + "!/" + name
                    state.archive_members += 1
                    scan_bytes(state, member, child, "archive-member")
                    scan_archive(state, member, child, depth + 1)
        except zipfile.BadZipFile as exc:
            raise CoverageError("zip archive is unreadable") from exc
        return True
    try:
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:*") as archive:
            members = sorted(archive.getmembers(), key=lambda item: item.name)
            seen: Set[str] = set()
            named_members: List[Tuple[tarfile.TarInfo, str]] = []
            for info in members:
                name = safe_member_name(info.name)
                if name in seen:
                    raise CoverageError("archive contains duplicate member paths")
                seen.add(name)
                named_members.append((info, name))
            member_map = {name: info for info, name in named_members}
            for info, name in named_members:
                if info.issym() or info.islnk():
                    if not allow_safe_links:
                        raise CoverageError("archive contains a link or special member")
                    resolve_archive_link(name, info.linkname, info.islnk(), member_map)
                    try:
                        link_data = info.linkname.encode("utf-8")
                    except UnicodeEncodeError as exc:
                        raise CoverageError("archive link target is undecodable") from exc
                    child = target + "!/" + name + "!/link-target"
                    state.archive_members += 1
                    scan_bytes(state, link_data, child, "archive-link")
                    continue
                if info.isdev() or info.isfifo():
                    raise CoverageError("archive contains a link or special member")
                if info.isdir():
                    continue
                if not info.isfile():
                    raise CoverageError("archive contains an unsupported member type")
                if info.size > state.limits.max_member_bytes:
                    raise CoverageError("archive member exceeds configured byte limit")
                source = archive.extractfile(info)
                if source is None:
                    raise CoverageError("archive member is unreadable")
                member = source.read(state.limits.max_member_bytes + 1)
                if len(member) > state.limits.max_member_bytes:
                    raise CoverageError("archive member exceeds configured byte limit")
                child = target + "!/" + name
                state.archive_members += 1
                scan_bytes(state, member, child, "archive-member")
                scan_archive(state, member, child, depth + 1)
    except (tarfile.TarError, EOFError, OSError) as exc:
        raise CoverageError("tar archive is unreadable") from exc
    return True


def scan_file(
    state: ScanState,
    path: Path,
    target_kind: str,
    must_expand: bool = False,
    *,
    depth: int = 0,
    target: Optional[str] = None,
) -> None:
    try:
        metadata = path.lstat()
    except FileNotFoundError as exc:
        raise CoverageError(f"required input is missing: {path}") from exc
    if stat.S_ISLNK(metadata.st_mode) or not stat.S_ISREG(metadata.st_mode):
        raise CoverageError(f"required input is not a regular file: {path}")
    if metadata.st_size > state.limits.max_member_bytes:
        raise CoverageError(f"required input exceeds configured byte limit: {path}")
    data = path.read_bytes()
    scan_target = target if target is not None else str(path.resolve())
    scan_bytes(state, data, scan_target, target_kind)
    expanded = scan_archive(state, data, scan_target, depth)
    if not expanded:
        external_kind = external_package_kind(path, data)
        if external_kind is not None:
            scan_external_package(
                state,
                path,
                scan_target,
                depth,
                external_kind,
                sha256_bytes(data),
            )
            expanded = True
    if must_expand and not expanded:
        raise CoverageError(f"required package/archive format is not safely expandable: {path.name}")


class GitRepository:
    def __init__(self, path: Path) -> None:
        reject_dangerous_environment()
        self.path = path.resolve()
        self._home = tempfile.TemporaryDirectory(prefix="ieum-privacy-git-home-")
        self.env = {
            "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
            "HOME": self._home.name,
            "LC_ALL": "C",
            "LANG": "C",
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": "/dev/null",
            "GIT_TERMINAL_PROMPT": "0",
            "GIT_OPTIONAL_LOCKS": "0",
            "GIT_NO_REPLACE_OBJECTS": "1",
        }
        if not self.path.is_dir():
            raise SecurityError("repository path is not a directory")
        self.bare = self.text("rev-parse", "--is-bare-repository").strip() == "true"
        if self.bare:
            absolute_git_dir = self.text("rev-parse", "--path-format=absolute", "--absolute-git-dir").strip()
            if Path(absolute_git_dir).resolve() != self.path:
                raise SecurityError("bare repository path is not its exact Git directory")
        else:
            top = self.text("rev-parse", "--show-toplevel").strip()
            if Path(top).resolve() != self.path:
                raise SecurityError("repository path is not its exact worktree root")
        git_dir_raw = self.text("rev-parse", "--path-format=absolute", "--git-dir").strip()
        common_raw = self.text("rev-parse", "--path-format=absolute", "--git-common-dir").strip()
        git_dir_path = Path(git_dir_raw)
        common_path = Path(common_raw)
        self.git_dir = (git_dir_path if git_dir_path.is_absolute() else self.path / git_dir_path).resolve()
        self.common_dir = (common_path if common_path.is_absolute() else self.path / common_path).resolve()
        self._validate_controls()

    def close(self) -> None:
        self._home.cleanup()

    def command(self, *args: str) -> List[str]:
        return [
            "git",
            "-C",
            str(self.path),
            "-c",
            "core.fsmonitor=false",
            "-c",
            "core.hooksPath=/dev/null",
            "-c",
            "fetch.writeCommitGraph=false",
            *args,
        ]

    def bytes(self, *args: str) -> bytes:
        completed = subprocess.run(self.command(*args), env=self.env, check=True, capture_output=True)
        return completed.stdout

    def text(self, *args: str) -> str:
        return self.bytes(*args).decode("utf-8")

    def _validate_controls(self) -> None:
        config = self.bytes("config", "--local", "--null", "--list", "--no-includes")
        forbidden_fragments = (
            b"include.",
            b"includeif.",
            b"url.",
            b"http.proxy",
            b"https.proxy",
            b"core.fsmonitor",
            b"core.gitproxy",
            b"remote.origin.promisor",
            b"extensions.partialclone",
        )
        lowered = config.lower()
        if any(fragment in lowered for fragment in forbidden_fragments):
            raise SecurityError("repository-local Git config contains a forbidden control")
        alternates = self.common_dir / "objects" / "info" / "alternates"
        if os.path.lexists(str(alternates)):
            raise SecurityError("external or symlinked Git alternates are forbidden")
        if os.path.lexists(str(self.common_dir / "shallow")):
            raise SecurityError("shallow repositories are forbidden")
        if self.text("for-each-ref", "--format=%(refname)", "refs/replace").strip():
            raise SecurityError("Git replacement refs are forbidden")

    def snapshot(self) -> Dict[str, Any]:
        paths: Set[Path] = set()
        for root in {self.git_dir, self.common_dir}:
            for relative in ("HEAD", "index", "config", "config.worktree", "packed-refs", "commondir", "gitdir"):
                candidate = root / relative
                if os.path.lexists(str(candidate)):
                    paths.add(candidate)
            refs = root / "refs"
            if refs.exists():
                for candidate in refs.rglob("*"):
                    if candidate.is_file() or candidate.is_symlink():
                        paths.add(candidate)
        if not self.bare:
            pointer = self.path / ".git"
            if os.path.lexists(str(pointer)) and not pointer.is_dir():
                paths.add(pointer)
        result: Dict[str, Any] = {}
        for candidate in sorted(paths, key=lambda item: str(item)):
            metadata = candidate.lstat()
            if stat.S_ISLNK(metadata.st_mode) or not stat.S_ISREG(metadata.st_mode):
                raise SecurityError("Git control subset contains a non-regular entry")
            data = candidate.read_bytes()
            result[str(candidate)] = {"size": len(data), "sha256": sha256_bytes(data)}
        objects = self.common_dir / "objects"
        object_stat = objects.stat()
        result["objects-directory-identity"] = {
            "device": object_stat.st_dev,
            "inode": object_stat.st_ino,
        }
        return result

    def resolve(self, expression: str) -> str:
        oid = self.text("rev-parse", "--verify", expression).strip()
        if not HEX_OID.fullmatch(oid):
            raise CoverageError("Git object resolution returned an invalid object id")
        return oid


def parse_ls_tree(raw: bytes) -> List[Tuple[str, str, str, str]]:
    rows: List[Tuple[str, str, str, str]] = []
    for record in raw.split(b"\0"):
        if not record:
            continue
        try:
            header, path_raw = record.split(b"\t", 1)
            mode, kind, oid = header.decode("ascii").split(" ")
            path = path_raw.decode("utf-8")
        except (ValueError, UnicodeDecodeError) as exc:
            raise CoverageError("Git tree contains an undecodable entry") from exc
        rows.append((mode, kind, oid, path))
    return rows


def scan_tree(repo: GitRepository, state: ScanState, treeish: str, seen_blobs: Optional[Set[str]] = None) -> str:
    object_oid = repo.resolve(treeish)
    object_type = repo.text("cat-file", "-t", object_oid).strip()
    if object_type == "tag":
        scan_bytes(state, repo.bytes("cat-file", "tag", object_oid), f"git-object:{object_oid}", "tag-object")
    tree_oid = repo.resolve(treeish + "^{tree}")
    rows = parse_ls_tree(repo.bytes("ls-tree", "-rz", "--full-tree", "-r", tree_oid))
    seen = seen_blobs if seen_blobs is not None else set()
    for mode, kind, oid, path in rows:
        scan_bytes(state, path.encode("utf-8"), f"git-path:{path}", "path")
        if kind != "blob":
            raise CoverageError(f"unsupported non-blob tree entry: {path} ({kind})")
        if oid in seen:
            continue
        seen.add(oid)
        scan_bytes(state, repo.bytes("cat-file", "blob", oid), f"git-blob:{oid}:{path}", "blob")
    return tree_oid


def range_commits(repo: GitRepository, baseline: str, head: str) -> List[str]:
    if baseline in ZERO_OIDS:
        raise CoverageError("range baseline may not be an all-zero object id")
    base_oid = repo.resolve(baseline + "^{commit}")
    head_oid = repo.resolve(head + "^{commit}")
    lines = repo.text("rev-list", "--reverse", f"{base_oid}..{head_oid}").splitlines()
    if any(not HEX_OID.fullmatch(item) for item in lines):
        raise CoverageError("Git range returned an invalid commit id")
    return lines


def scan_range(repo: GitRepository, state: ScanState, baseline: str, head: str) -> List[str]:
    commits = range_commits(repo, baseline, head)
    seen_blobs: Set[str] = set()
    for commit in commits:
        scan_bytes(state, repo.bytes("cat-file", "commit", commit), f"git-commit:{commit}", "commit")
        changed = repo.bytes(
            "diff-tree", "--root", "--no-commit-id", "--name-only", "-r", "-z", commit
        )
        for path_raw in changed.split(b"\0"):
            if not path_raw:
                continue
            try:
                path = path_raw.decode("utf-8")
            except UnicodeDecodeError as exc:
                raise CoverageError("Git range contains an undecodable path") from exc
            scan_bytes(state, path_raw, f"git-path:{commit}:{path}", "path")
            entry_raw = repo.bytes("ls-tree", "-z", commit, "--", path)
            entries = parse_ls_tree(entry_raw)
            if not entries:
                continue
            if len(entries) != 1:
                raise CoverageError("Git range path resolved to multiple tree entries")
            _, kind, oid, resolved_path = entries[0]
            if resolved_path != path or kind != "blob":
                raise CoverageError("Git range path did not resolve to one resulting blob")
            if oid in seen_blobs:
                continue
            seen_blobs.add(oid)
            scan_bytes(
                state,
                repo.bytes("cat-file", "blob", oid),
                f"git-blob:{commit}:{oid}:{path}",
                "blob",
            )
    return commits


def parse_ref_map(data: bytes) -> bytes:
    rows = [row for row in data.splitlines() if row]
    if not rows or any(REF_LINE.fullmatch(row) is None for row in rows):
        raise CoverageError("advertised ref map is empty or malformed")
    if rows != sorted(set(rows)):
        raise CoverageError("advertised ref map must be unique and byte-sorted")
    return b"\n".join(rows) + b"\n"


def remote_ref_map(remote: str, env: Dict[str, str]) -> bytes:
    completed = subprocess.run(
        ["git", "ls-remote", "--refs", remote],
        env=env,
        check=True,
        capture_output=True,
    )
    rows = sorted(set(row for row in completed.stdout.splitlines() if row))
    return parse_ref_map(b"\n".join(rows) + b"\n")


def verify_checksums(checksum_paths: Sequence[Path], package_paths: Sequence[Path]) -> None:
    packages = {path.name: sha256_file(path) for path in package_paths}
    if len(packages) != len(package_paths):
        raise CoverageError("release package basenames are not unique")
    if not checksum_paths:
        raise CoverageError("release checksum manifest is missing")
    described: Dict[str, str] = {}
    for manifest in checksum_paths:
        try:
            lines = manifest.read_text(encoding="utf-8").splitlines()
        except (OSError, UnicodeDecodeError) as exc:
            raise CoverageError("checksum manifest is unreadable") from exc
        for line in lines:
            match = CHECKSUM_LINE.fullmatch(line)
            if match is None:
                raise CoverageError("checksum manifest contains a malformed line")
            digest, name = match.groups()
            if PurePosixPath(name).name != name or name in described:
                raise CoverageError("checksum manifest path is unsafe or duplicated")
            described[name] = digest.lower()
    if described != packages:
        raise CoverageError("checksum manifest does not exactly describe the release package set")


def scan_release_inputs(args: argparse.Namespace, state: ScanState) -> None:
    bodies = [Path(args.body), Path(args.generated_notes)]
    packages = [Path(item) for item in args.package]
    checksums = [Path(item) for item in args.checksum]
    sources = [Path(item) for item in args.source_archive]
    responses = [Path(item) for item in args.response_record]
    if len(sources) != 2:
        raise CoverageError("exactly two generated source archives are required")
    verify_checksums(checksums, packages)
    for path in bodies:
        scan_file(state, path, "release-body")
    for path in packages:
        scan_file(state, path, "release-package", must_expand=True)
    for path in checksums:
        scan_file(state, path, "checksum-manifest")
    for path in sources:
        scan_file(state, path, "generated-source-archive", must_expand=True)
    for path in responses:
        scan_file(state, path, "response-identity")


def manifest_base(
    mode: str,
    deny_version: str,
    deny_sha256: str,
    state: ScanState,
    result: str,
) -> Dict[str, Any]:
    return {
        "schema": MANIFEST_SCHEMA,
        "scanner_version": SCANNER_VERSION,
        "mode": mode,
        "private_deny_map": {"version": deny_version, "sha256": deny_sha256},
        "result": result,
        "findings": [item.as_json() for item in state.findings],
        "coverage": {
            "records": sorted(state.records, key=lambda item: (item["target"], item["kind"])),
            "record_count": len(state.records),
            "archive_member_count": state.archive_members,
            "bytes_read": state.bytes_read,
        },
    }


def write_manifest(path: Path, payload: Dict[str, Any]) -> None:
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    if os.path.lexists(str(path)) and path.is_symlink():
        raise SecurityError("manifest output path may not be a symlink")
    data = (json.dumps(payload, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")
    fd, temporary = tempfile.mkstemp(prefix=path.name + ".", dir=str(path.parent))
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        os.chmod(temporary, 0o600)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="mode", required=True)

    def common(subparser: argparse.ArgumentParser, manifest: bool = True) -> None:
        subparser.add_argument("--deny-map", type=Path, required=True)
        subparser.add_argument("--deny-map-sha256", required=True)
        if manifest:
            subparser.add_argument("--manifest-out", type=Path, required=True)
        subparser.add_argument("--max-member-bytes", type=int, default=512 * 1024 * 1024)
        subparser.add_argument("--max-total-bytes", type=int, default=8 * 1024 * 1024 * 1024)
        subparser.add_argument("--max-archive-depth", type=int, default=4)

    validate = subparsers.add_parser("validate-map")
    common(validate, manifest=False)

    tree = subparsers.add_parser("tree")
    common(tree)
    tree.add_argument("--repo", type=Path, required=True)
    tree.add_argument("--treeish", required=True)

    range_parser = subparsers.add_parser("range")
    common(range_parser)
    range_parser.add_argument("--repo", type=Path, required=True)
    range_parser.add_argument("--baseline", required=True)
    range_parser.add_argument("--head", required=True)

    rewrite = subparsers.add_parser("rewrite")
    common(rewrite)
    rewrite.add_argument("--repo", type=Path, required=True)
    rewrite.add_argument("--ref", action="append", required=True)

    def release_arguments(subparser: argparse.ArgumentParser) -> None:
        common(subparser)
        subparser.add_argument("--body", required=True)
        subparser.add_argument("--generated-notes", required=True)
        subparser.add_argument("--package", action="append", default=[])
        subparser.add_argument("--checksum", action="append", default=[])
        subparser.add_argument("--source-archive", action="append", default=[])
        subparser.add_argument("--response-record", action="append", default=[])

    release = subparsers.add_parser("release")
    release_arguments(release)

    post = subparsers.add_parser("post-fetch")
    release_arguments(post)
    post.add_argument("--remote", required=True)
    post.add_argument("--expected-ref-map", type=Path, required=True)
    post.add_argument("--ref", required=True)
    post.add_argument("--baseline", required=True)
    post.add_argument("--allow-local-test-remote", action="store_true", help=argparse.SUPPRESS)
    return parser


def validate_limits(args: argparse.Namespace) -> Limits:
    values = (args.max_member_bytes, args.max_total_bytes, args.max_archive_depth)
    if any(value <= 0 for value in values) or args.max_total_bytes < args.max_member_bytes:
        raise SecurityError("scan limits are invalid")
    return Limits(*values)


def execute(args: argparse.Namespace) -> Tuple[int, Optional[Dict[str, Any]]]:
    limits = validate_limits(args)
    rules, deny_version, deny_sha256 = load_deny_map(args.deny_map.resolve(), args.deny_map_sha256)
    if args.mode == "validate-map":
        return 0, None
    state = ScanState(rules, limits)
    repository: Optional[GitRepository] = None
    before: Optional[Dict[str, Any]] = None
    details: Dict[str, Any] = {}
    try:
        if args.mode in {"tree", "range", "rewrite"}:
            repository = GitRepository(args.repo)
            before = repository.snapshot()
            if args.mode == "tree":
                details["tree_oid"] = scan_tree(repository, state, args.treeish)
            elif args.mode == "range":
                details["commits"] = scan_range(repository, state, args.baseline, args.head)
            else:
                seen_commits: Set[str] = set()
                seen_blobs: Set[str] = set()
                for ref in sorted(set(args.ref)):
                    scan_bytes(state, ref.encode("utf-8"), f"git-ref:{ref}", "ref")
                    tip_type = repository.text("cat-file", "-t", ref).strip()
                    if tip_type == "tag":
                        oid = repository.resolve(ref)
                        scan_bytes(state, repository.bytes("cat-file", "tag", oid), f"git-tag:{oid}", "tag-object")
                    commits = repository.text("rev-list", ref).splitlines()
                    for commit in commits:
                        if commit in seen_commits:
                            continue
                        seen_commits.add(commit)
                        scan_bytes(
                            state,
                            repository.bytes("cat-file", "commit", commit),
                            f"git-commit:{commit}",
                            "commit",
                        )
                        scan_tree(repository, state, commit, seen_blobs)
                details["refs"] = sorted(set(args.ref))
                details["commit_count"] = len(seen_commits)
        elif args.mode == "release":
            scan_release_inputs(args, state)
        elif args.mode == "post-fetch":
            reject_dangerous_environment()
            if not args.remote.startswith("https://github.com/") and not args.allow_local_test_remote:
                raise SecurityError("post-fetch remote must use the canonical GitHub HTTPS boundary")
            expected = parse_ref_map(args.expected_ref_map.read_bytes())
            with tempfile.TemporaryDirectory(prefix="ieum-privacy-post-fetch-") as temporary:
                home = Path(temporary) / "home"
                home.mkdir()
                env = {
                    "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
                    "HOME": str(home),
                    "LC_ALL": "C",
                    "LANG": "C",
                    "GIT_CONFIG_NOSYSTEM": "1",
                    "GIT_CONFIG_GLOBAL": "/dev/null",
                    "GIT_TERMINAL_PROMPT": "0",
                    "GIT_OPTIONAL_LOCKS": "0",
                    "GIT_NO_REPLACE_OBJECTS": "1",
                }
                observed_before = remote_ref_map(args.remote, env)
                if observed_before != expected:
                    raise CoverageError("remote ref map differs from the frozen pre-publication map")
                mirror = Path(temporary) / "mirror"
                subprocess.run(
                    ["git", "clone", "--mirror", "--no-local", args.remote, str(mirror)],
                    env=env,
                    check=True,
                    capture_output=True,
                )
                repository = GitRepository(mirror)
                before = repository.snapshot()
                details["tree_oid"] = scan_tree(repository, state, args.ref)
                details["commits"] = scan_range(repository, state, args.baseline, args.ref)
                scan_release_inputs(args, state)
                observed_after = remote_ref_map(args.remote, env)
                if observed_after != expected:
                    raise CoverageError("remote ref map drifted during post-fetch verification")
                details["advertised_ref_map_sha256"] = sha256_bytes(expected)
                after = repository.snapshot()
                if before != after:
                    raise SecurityError("repository control subset drifted during the scan")
                repository.close()
                repository = None
                before = None
        else:
            raise SecurityError("unsupported guard mode")

        if repository is not None and before is not None:
            after = repository.snapshot()
            if before != after:
                raise SecurityError("repository control subset drifted during the scan")
        result = "affected" if state.findings else "verified-clean"
        payload = manifest_base(args.mode, deny_version, deny_sha256, state, result)
        payload["details"] = details
        return (2 if state.findings else 0), payload
    except CoverageError as exc:
        payload = manifest_base(args.mode, deny_version, deny_sha256, state, "unverified")
        payload["details"] = details
        payload["coverage_error"] = str(exc)
        return 3, payload
    finally:
        if repository is not None:
            repository.close()


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        exit_code, payload = execute(args)
        if payload is not None:
            write_manifest(args.manifest_out, payload)
        return exit_code
    except (SecurityError, subprocess.CalledProcessError, OSError) as exc:
        print(f"public privacy guard failed closed: {exc}", file=sys.stderr)
        return 4


if __name__ == "__main__":
    raise SystemExit(main())
