#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT

from __future__ import annotations

import base64
import hashlib
import importlib.util
import io
import json
import os
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import unicodedata
import unittest
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SCANNER = ROOT / "tools/privacy/public_privacy_guard.py"
HOOK = ROOT / ".githooks/pre-push"
INSTALLER = ROOT / "tools/privacy/install_hook.sh"
LINK_CHECKER = ROOT / "tools/privacy/check_public_links.py"
MANIFEST_REPORTER = ROOT / "tools/privacy/report_public_manifest.py"
WORKFLOW = ROOT / ".github/workflows/continuous-integration.yml"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, cwd: Path, env=None, check=True):
    return subprocess.run(
        [str(item) for item in command],
        cwd=str(cwd),
        env=env,
        text=True,
        capture_output=True,
        check=check,
    )


def write_map(root: Path, values=None):
    values = values or [("private-owner", "owner identity", "SecretOwner")]
    path = root / "private-deny-map.json"
    payload = {
        "schema": "ieum.private-deny-map.v1",
        "version": "synthetic-v1",
        "entries": [
            {
                "id": rule_id,
                "class": value_class,
                "value_b64": base64.b64encode(value.encode("utf-8")).decode("ascii"),
            }
            for rule_id, value_class, value in values
        ],
    }
    path.write_text(json.dumps(payload, sort_keys=True) + "\n", encoding="utf-8")
    path.chmod(0o600)
    return path, sha256(path)


def init_repo(root: Path) -> Path:
    repo = root / "repo"
    repo.mkdir()
    run(["git", "init", "-b", "main"], repo)
    run(["git", "config", "user.name", "Synthetic Tester"], repo)
    run(["git", "config", "user.email", "synthetic@example.invalid"], repo)
    return repo


def commit_all(repo: Path, message: str) -> str:
    run(["git", "add", "-A"], repo)
    run(["git", "commit", "-m", message], repo)
    return run(["git", "rev-parse", "HEAD"], repo).stdout.strip()


def guard_command(mode: str, deny_map: Path, deny_sha: str, manifest: Path, *extra):
    return [
        "python3",
        SCANNER,
        mode,
        "--deny-map",
        deny_map,
        "--deny-map-sha256",
        deny_sha,
        "--manifest-out",
        manifest,
        *extra,
    ]


def add_zip(path: Path, members):
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in members:
            archive.writestr(name, data)


def tar_gz_bytes(members):
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w:gz") as archive:
        for name, data in members:
            info = tarfile.TarInfo(name)
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))
    return buffer.getvalue()


def load_scanner_module():
    spec = importlib.util.spec_from_file_location("ieum_public_privacy_guard", SCANNER)
    if spec is None or spec.loader is None:
        raise AssertionError("privacy guard module could not be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class PublicPrivacyGuardTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="ieum-privacy-test-")
        self.root = Path(self.temporary.name)
        self.deny_map, self.deny_sha = write_map(self.root)

    def tearDown(self):
        self.temporary.cleanup()

    def test_release_package_formats_dispatch_to_bounded_extractors(self):
        scanner = load_scanner_module()
        cases = (
            ("package.pkg.tar.zst", b"\x28\xb5\x2f\xfdpayload", "zstd"),
            ("package.deb", b"!<arch>\npayload", "sevenzip"),
            ("package.rpm", b"\xed\xab\xee\xdbpayload", "sevenzip"),
            ("package.7z", b"7z\xbc\xaf'\x1cpayload", "sevenzip"),
            ("package.msi", b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1payload", "sevenzip"),
            ("package.dmg", b"opaque disk image", "sevenzip"),
            ("package.flatpak", b"flatpak\x00payload", "flatpak"),
        )

        for name, data, expected in cases:
            with self.subTest(name=name):
                self.assertEqual(
                    scanner.external_package_kind(Path(name), data), expected
                )

    def test_external_extraction_tree_is_scanned_under_the_package_identity(self):
        scanner = load_scanner_module()
        extracted = self.root / "extracted"
        (extracted / "nested").mkdir(parents=True)
        (extracted / "nested" / "payload.txt").write_text(
            "SecretOwner", encoding="utf-8"
        )
        state = scanner.ScanState(
            [scanner.Rule("private-owner", "SecretOwner", "owner identity")],
            scanner.Limits(1024 * 1024, 8 * 1024 * 1024, 4),
        )

        scanner.scan_extracted_tree(state, extracted, "release.pkg", depth=0)

        self.assertGreaterEqual(len(state.findings), 1)
        self.assertEqual(
            {finding.target for finding in state.findings},
            {"release.pkg!/nested/payload.txt"},
        )

    def test_flatpak_export_allows_only_resolved_internal_tar_links(self):
        scanner = load_scanner_module()
        archive_bytes = io.BytesIO()
        with tarfile.open(fileobj=archive_bytes, mode="w") as archive:
            payload = b"clean"
            regular = tarfile.TarInfo("files/lib/libexample.so.1")
            regular.size = len(payload)
            archive.addfile(regular, io.BytesIO(payload))
            link = tarfile.TarInfo("files/lib/libexample.so")
            link.type = tarfile.SYMTYPE
            link.linkname = "libexample.so.1"
            archive.addfile(link)

        state = scanner.ScanState(
            [], scanner.Limits(1024 * 1024, 8 * 1024 * 1024, 4)
        )
        self.assertTrue(
            scanner.scan_archive(
                state,
                archive_bytes.getvalue(),
                "bundle.flatpak!/ostree-export.tar",
                allow_safe_links=True,
            )
        )

        source_zip = io.BytesIO()
        with zipfile.ZipFile(source_zip, "w") as archive:
            archive.writestr("source/target.txt", "clean")
            link = zipfile.ZipInfo("source/link.txt")
            link.create_system = 3
            link.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive.writestr(link, "target.txt")
        self.assertTrue(
            scanner.scan_archive(
                state,
                source_zip.getvalue(),
                "source.zip",
                allow_safe_links=True,
            )
        )

        escaping = io.BytesIO()
        with tarfile.open(fileobj=escaping, mode="w") as archive:
            link = tarfile.TarInfo("files/lib/escape")
            link.type = tarfile.SYMTYPE
            link.linkname = "../../../outside"
            archive.addfile(link)
        with self.assertRaises(scanner.CoverageError):
            scanner.scan_archive(
                state,
                escaping.getvalue(),
                "bundle.flatpak!/escaping.tar",
                allow_safe_links=True,
            )

    @unittest.skipIf(os.name == "nt", "Windows symlink creation requires elevation")
    def test_dmg_extraction_allows_only_resolved_internal_filesystem_links(self):
        scanner = load_scanner_module()
        extracted = self.root / "dmg"
        extracted.mkdir()
        (extracted / "target.txt").write_text("clean", encoding="utf-8")
        (extracted / "safe-link").symlink_to("target.txt")
        state = scanner.ScanState(
            [], scanner.Limits(1024 * 1024, 8 * 1024 * 1024, 4)
        )

        scanner.scan_extracted_tree(
            state,
            extracted,
            "release.dmg",
            depth=0,
            allow_safe_links=True,
        )

        outside = self.root / "outside.txt"
        outside.write_text("clean", encoding="utf-8")
        (extracted / "escaping-link").symlink_to(outside)
        with self.assertRaises(scanner.CoverageError):
            scanner.scan_extracted_tree(
                state,
                extracted,
                "release.dmg",
                depth=0,
                allow_safe_links=True,
            )

    def test_manifest_reporter_exposes_actionable_ids_without_private_targets(self):
        private_value = "OwnerPrivateValue"
        blob_oid = "a" * 40
        manifest = self.root / "affected.json"
        manifest.write_text(
            json.dumps(
                {
                    "schema": "ieum.public-surface-manifest.v1",
                    "result": "affected",
                    "findings": [
                        {
                            "rule_id": "private-owner",
                            "target": f"git-blob:{blob_oid}:docs/{private_value}.md",
                            "target_kind": "blob",
                            "detector": "utf-8",
                            "offset": 42,
                        }
                    ],
                    "details": {"private_canary": private_value},
                    "coverage": {
                        "records": [{"target": private_value}],
                    },
                }
            ),
            encoding="utf-8",
        )

        result = run(["python3", MANIFEST_REPORTER, manifest], ROOT, check=False)

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("result=affected findings=1", result.stdout)
        self.assertIn("rule_id=private-owner", result.stdout)
        self.assertIn(f"object={blob_oid}", result.stdout)
        self.assertIn("target_kind=blob", result.stdout)
        self.assertIn("detector=utf-8", result.stdout)
        self.assertIn("offset=42", result.stdout)
        self.assertNotIn(private_value, result.stdout)
        self.assertNotIn(private_value, result.stderr)

    def test_tree_allows_upstream_provenance_and_hosted_ci_counterexamples(self):
        repo = init_repo(self.root)
        (repo / "allowed.txt").write_text(
            "SPDX-FileCopyrightText: 2025 Upstream Person <upstream@gmail.com>\n"
            "/Users/runner/work/example runs-on: macos-15-intel\n",
            encoding="utf-8",
        )
        head = commit_all(repo, "allowed provenance")
        manifest = self.root / "tree.json"
        result = run(
            guard_command(
                "tree",
                self.deny_map,
                self.deny_sha,
                manifest,
                "--repo",
                repo,
                "--treeish",
                head,
            ),
            repo,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(manifest.read_text())["result"], "verified-clean")

    def test_tree_detects_paths_unicode_case_slash_and_encoded_binary_variants(self):
        deny_map, deny_sha = write_map(
            self.root,
            [("private-path", "owner path", "Café/PrivateOwner")],
        )
        repo = init_repo(self.root)
        nested = repo / unicodedata.normalize("NFD", "CAFÉ")
        nested.mkdir()
        (nested / "privateowner.txt").write_text("clean body\n", encoding="utf-8")
        (repo / "utf16le.bin").write_bytes("Café/PrivateOwner".encode("utf-16le"))
        (repo / "utf16be.bin").write_bytes("Café/PrivateOwner".encode("utf-16be"))
        head = commit_all(repo, "encoded private variants")
        manifest = self.root / "affected-tree.json"
        result = run(
            guard_command(
                "tree",
                deny_map,
                deny_sha,
                manifest,
                "--repo",
                repo,
                "--treeish",
                head,
            ),
            repo,
            check=False,
        )
        self.assertEqual(result.returncode, 2, result.stderr)
        payload = json.loads(manifest.read_text())
        self.assertEqual(payload["result"], "affected")
        detectors = {item["detector"] for item in payload["findings"]}
        self.assertIn("utf-16le", detectors)
        self.assertIn("utf-16be", detectors)
        self.assertIn("unicode-slash-casefold", detectors)

    def test_range_ignores_prebaseline_blob_but_catches_new_blob_and_metadata(self):
        repo = init_repo(self.root)
        (repo / "legacy.txt").write_text("SecretOwner\n", encoding="utf-8")
        baseline = commit_all(repo, "prebaseline contamination")
        (repo / "new.txt").write_text("clean\n", encoding="utf-8")
        clean_head = commit_all(repo, "clean change")
        clean_manifest = self.root / "clean-range.json"
        clean = run(
            guard_command(
                "range",
                self.deny_map,
                self.deny_sha,
                clean_manifest,
                "--repo",
                repo,
                "--baseline",
                baseline,
                "--head",
                clean_head,
            ),
            repo,
            check=False,
        )
        self.assertEqual(clean.returncode, 0, clean.stderr)

        (repo / "new.txt").write_text("SecretOwner\n", encoding="utf-8")
        affected_head = commit_all(repo, "new private value")
        affected_manifest = self.root / "affected-range.json"
        affected = run(
            guard_command(
                "range",
                self.deny_map,
                self.deny_sha,
                affected_manifest,
                "--repo",
                repo,
                "--baseline",
                clean_head,
                "--head",
                affected_head,
            ),
            repo,
            check=False,
        )
        self.assertEqual(affected.returncode, 2, affected.stderr)
        kinds = {item["target_kind"] for item in json.loads(affected_manifest.read_text())["findings"]}
        self.assertIn("blob", kinds)

        env = os.environ.copy()
        env.update(
            {
                "GIT_AUTHOR_NAME": "SecretOwner",
                "GIT_AUTHOR_EMAIL": "synthetic@example.invalid",
                "GIT_COMMITTER_NAME": "Synthetic Tester",
                "GIT_COMMITTER_EMAIL": "synthetic@example.invalid",
            }
        )
        (repo / "metadata.txt").write_text("clean\n", encoding="utf-8")
        run(["git", "add", "metadata.txt"], repo)
        run(["git", "commit", "-m", "SecretOwner metadata case"], repo, env=env)
        metadata_head = run(["git", "rev-parse", "HEAD"], repo).stdout.strip()
        metadata_manifest = self.root / "metadata-range.json"
        metadata = run(
            guard_command(
                "range",
                self.deny_map,
                self.deny_sha,
                metadata_manifest,
                "--repo",
                repo,
                "--baseline",
                affected_head,
                "--head",
                metadata_head,
            ),
            repo,
            check=False,
        )
        self.assertEqual(metadata.returncode, 2, metadata.stderr)
        self.assertIn(
            "commit",
            {item["target_kind"] for item in json.loads(metadata_manifest.read_text())["findings"]},
        )

    def test_rewrite_scans_supplied_reachable_history(self):
        repo = init_repo(self.root)
        (repo / "legacy.txt").write_text("SecretOwner\n", encoding="utf-8")
        commit_all(repo, "legacy")
        (repo / "legacy.txt").write_text("clean\n", encoding="utf-8")
        commit_all(repo, "forward clean")
        manifest = self.root / "rewrite.json"
        result = run(
            guard_command(
                "rewrite",
                self.deny_map,
                self.deny_sha,
                manifest,
                "--repo",
                repo,
                "--ref",
                "refs/heads/main",
            ),
            repo,
            check=False,
        )
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(json.loads(manifest.read_text())["result"], "affected")

    def make_release_inputs(self, denied_nested=False):
        body = self.root / "body.md"
        notes = self.root / "notes.md"
        package = self.root / "package.zip"
        checksum = self.root / "SHA256SUMS.txt"
        source_zip = self.root / "source.zip"
        source_tar = self.root / "source.tar.gz"
        body.write_text("Release body\n", encoding="utf-8")
        notes.write_text("Generated notes\n", encoding="utf-8")
        if denied_nested:
            inner = io.BytesIO()
            with zipfile.ZipFile(inner, "w") as archive:
                archive.writestr("deep.txt", "SecretOwner")
            add_zip(package, [("inner.zip", inner.getvalue())])
        else:
            add_zip(package, [("payload.txt", b"clean package")])
        checksum.write_text(f"{sha256(package)}  {package.name}\n", encoding="utf-8")
        with zipfile.ZipFile(
            source_zip, "w", compression=zipfile.ZIP_DEFLATED
        ) as archive:
            archive.writestr("source/file.txt", b"clean source")
            link = zipfile.ZipInfo("source/file-link.txt")
            link.create_system = 3
            link.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive.writestr(link, "file.txt")
        source_buffer = io.BytesIO()
        with tarfile.open(fileobj=source_buffer, mode="w:gz") as archive:
            payload = b"clean source"
            regular = tarfile.TarInfo("source/file.txt")
            regular.size = len(payload)
            archive.addfile(regular, io.BytesIO(payload))
            link = tarfile.TarInfo("source/file-link.txt")
            link.type = tarfile.SYMTYPE
            link.linkname = "file.txt"
            archive.addfile(link)
        source_tar.write_bytes(source_buffer.getvalue())
        return body, notes, package, checksum, source_zip, source_tar

    def release_command(self, manifest, inputs, *extra):
        body, notes, package, checksum, source_zip, source_tar = inputs
        return guard_command(
            "release",
            self.deny_map,
            self.deny_sha,
            manifest,
            "--body",
            body,
            "--generated-notes",
            notes,
            "--package",
            package,
            "--checksum",
            checksum,
            "--source-archive",
            source_zip,
            "--source-archive",
            source_tar,
            *extra,
        )

    def test_release_scans_nested_packages_checksums_and_two_source_archives(self):
        inputs = self.make_release_inputs()
        manifest = self.root / "release.json"
        result = run(self.release_command(manifest, inputs), self.root, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads(manifest.read_text())
        self.assertEqual(payload["result"], "verified-clean")
        self.assertGreaterEqual(payload["coverage"]["archive_member_count"], 3)

        affected_inputs = self.make_release_inputs(denied_nested=True)
        affected_manifest = self.root / "release-affected.json"
        affected = run(self.release_command(affected_manifest, affected_inputs), self.root, check=False)
        self.assertEqual(affected.returncode, 2, affected.stderr)
        self.assertEqual(json.loads(affected_manifest.read_text())["result"], "affected")

    def test_release_fails_closed_on_traversal_symlink_corruption_size_and_checksum(self):
        cases = []

        traversal = self.make_release_inputs()
        with zipfile.ZipFile(traversal[2], "w") as archive:
            archive.writestr("../escape.txt", "clean")
        traversal[3].write_text(f"{sha256(traversal[2])}  {traversal[2].name}\n", encoding="utf-8")
        cases.append(("traversal", traversal, []))

        symlink = self.make_release_inputs()
        with zipfile.ZipFile(symlink[2], "w") as archive:
            info = zipfile.ZipInfo("link")
            info.create_system = 3
            info.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive.writestr(info, "target")
        symlink[3].write_text(f"{sha256(symlink[2])}  {symlink[2].name}\n", encoding="utf-8")
        cases.append(("symlink", symlink, []))

        corrupt = self.make_release_inputs()
        corrupt[2].write_bytes(b"PK\x03\x04broken")
        corrupt[3].write_text(f"{sha256(corrupt[2])}  {corrupt[2].name}\n", encoding="utf-8")
        cases.append(("corrupt", corrupt, []))

        oversized = self.make_release_inputs()
        add_zip(oversized[2], [("large.txt", b"x" * 4096)])
        oversized[3].write_text(f"{sha256(oversized[2])}  {oversized[2].name}\n", encoding="utf-8")
        cases.append(("oversized", oversized, ["--max-member-bytes", "1024", "--max-total-bytes", "8192"]))

        checksum = self.make_release_inputs()
        checksum[3].write_text(f"{'0' * 64}  {checksum[2].name}\n", encoding="utf-8")
        cases.append(("checksum", checksum, []))

        for name, inputs, extra in cases:
            with self.subTest(name=name):
                manifest = self.root / f"{name}.json"
                result = run(self.release_command(manifest, inputs, *extra), self.root, check=False)
                self.assertEqual(result.returncode, 3, result.stderr)
                self.assertEqual(json.loads(manifest.read_text())["result"], "unverified")

    def test_missing_or_tampered_private_map_fails_without_manifest(self):
        repo = init_repo(self.root)
        (repo / "clean.txt").write_text("clean\n", encoding="utf-8")
        head = commit_all(repo, "clean")
        missing_manifest = self.root / "missing.json"
        missing = run(
            guard_command(
                "tree",
                self.root / "absent.json",
                self.deny_sha,
                missing_manifest,
                "--repo",
                repo,
                "--treeish",
                head,
            ),
            repo,
            check=False,
        )
        self.assertEqual(missing.returncode, 4)
        self.assertFalse(missing_manifest.exists())

        self.deny_map.write_text(self.deny_map.read_text() + " ", encoding="utf-8")
        self.deny_map.chmod(0o600)
        tampered_manifest = self.root / "tampered.json"
        tampered = run(
            guard_command(
                "tree",
                self.deny_map,
                self.deny_sha,
                tampered_manifest,
                "--repo",
                repo,
                "--treeish",
                head,
            ),
            repo,
            check=False,
        )
        self.assertEqual(tampered.returncode, 4)
        self.assertFalse(tampered_manifest.exists())

    def test_post_fetch_uses_disposable_mirror_and_exact_stable_ref_map(self):
        repo = init_repo(self.root)
        (repo / "base.txt").write_text("clean base\n", encoding="utf-8")
        baseline = commit_all(repo, "clean baseline")
        (repo / "next.txt").write_text("clean next\n", encoding="utf-8")
        commit_all(repo, "clean next")
        remote = self.root / "remote.git"
        run(["git", "clone", "--bare", repo, remote], self.root)
        refs = run(["git", "ls-remote", "--refs", remote], self.root).stdout.splitlines()
        ref_map = self.root / "expected-refs.txt"
        ref_map.write_text("\n".join(sorted(set(refs))) + "\n", encoding="utf-8")
        inputs = self.make_release_inputs()
        body, notes, package, checksum, source_zip, source_tar = inputs
        manifest = self.root / "post-fetch.json"
        command = guard_command(
            "post-fetch",
            self.deny_map,
            self.deny_sha,
            manifest,
            "--remote",
            remote,
            "--allow-local-test-remote",
            "--expected-ref-map",
            ref_map,
            "--ref",
            "refs/heads/main",
            "--baseline",
            baseline,
            "--body",
            body,
            "--generated-notes",
            notes,
            "--package",
            package,
            "--checksum",
            checksum,
            "--source-archive",
            source_zip,
            "--source-archive",
            source_tar,
        )
        result = run(command, self.root, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads(manifest.read_text())
        self.assertEqual(payload["result"], "verified-clean")
        self.assertEqual(payload["details"]["advertised_ref_map_sha256"], sha256(ref_map))

        wrong_map = self.root / "wrong-refs.txt"
        wrong_map.write_text(("0" * 40) + "\trefs/heads/main\n", encoding="utf-8")
        wrong_manifest = self.root / "post-fetch-drift.json"
        wrong_command = list(command)
        wrong_command[wrong_command.index(manifest)] = wrong_manifest
        wrong_command[wrong_command.index(ref_map)] = wrong_map
        drift = run(wrong_command, self.root, check=False)
        self.assertEqual(drift.returncode, 3, drift.stderr)
        self.assertEqual(json.loads(wrong_manifest.read_text())["result"], "unverified")

    def test_pre_push_create_update_delete_and_force_update_lines(self):
        repo = init_repo(self.root)
        (repo / "tools/privacy").mkdir(parents=True)
        (repo / ".githooks").mkdir()
        shutil.copy2(SCANNER, repo / "tools/privacy/public_privacy_guard.py")
        shutil.copy2(HOOK, repo / ".githooks/pre-push")
        (repo / ".githooks/pre-push").chmod(0o755)
        (repo / "clean.txt").write_text("base\n", encoding="utf-8")
        baseline = commit_all(repo, "guard baseline")
        (repo / "clean.txt").write_text("update\n", encoding="utf-8")
        head = commit_all(repo, "update")

        env = os.environ.copy()
        env.update(
            {
                "IEUM_PRIVATE_DENY_MAP": str(self.deny_map),
                "IEUM_PRIVATE_DENY_MAP_SHA256": self.deny_sha,
                "IEUM_PRIVACY_BASELINE": baseline,
                "GIT_DIR": ".git",
                "GIT_CONFIG_COUNT": "1",
                "GIT_CONFIG_KEY_0": "http.https://github.com/.extraHeader",
                "GIT_CONFIG_VALUE_0": "AUTHORIZATION: basic test-only",
            }
        )
        zero = "0" * 40
        lines = {
            "create": f"refs/heads/topic {head} refs/heads/topic {zero}\n",
            "update": f"refs/heads/topic {head} refs/heads/topic {baseline}\n",
            "delete": f"(delete) {zero} refs/heads/topic {head}\n",
        }
        for name, line in lines.items():
            with self.subTest(name=name):
                result = subprocess.run(
                    [str(repo / ".githooks/pre-push"), "origin", "https://example.invalid/repo.git"],
                    cwd=str(repo),
                    env=env,
                    input=line,
                    text=True,
                    capture_output=True,
                )
                self.assertEqual(result.returncode, 0, result.stderr)

        run(["git", "checkout", "-b", "alternate", baseline], repo)
        (repo / "alternate.txt").write_text("clean alternative\n", encoding="utf-8")
        alternate = commit_all(repo, "alternate clean")
        forced = subprocess.run(
            [str(repo / ".githooks/pre-push"), "origin", "https://example.invalid/repo.git"],
            cwd=str(repo),
            env=env,
            input=f"refs/heads/topic {alternate} refs/heads/topic {head}\n",
            text=True,
            capture_output=True,
        )
        self.assertEqual(forced.returncode, 0, forced.stderr)

    def test_hook_install_and_active_path_verification(self):
        repo = init_repo(self.root)
        (repo / "tools/privacy").mkdir(parents=True)
        (repo / ".githooks").mkdir()
        shutil.copy2(INSTALLER, repo / "tools/privacy/install_hook.sh")
        shutil.copy2(HOOK, repo / ".githooks/pre-push")
        (repo / "tools/privacy/install_hook.sh").chmod(0o755)
        (repo / ".githooks/pre-push").chmod(0o755)
        commit_all(repo, "tracked hook")
        install = run([repo / "tools/privacy/install_hook.sh", "--install"], repo, check=False)
        self.assertEqual(install.returncode, 0, install.stderr)
        check = run([repo / "tools/privacy/install_hook.sh", "--check"], repo, check=False)
        self.assertEqual(check.returncode, 0, check.stderr)
        self.assertEqual(run(["git", "config", "--local", "--get", "core.hooksPath"], repo).stdout.strip(), ".githooks")

    def test_ci_release_and_link_regression_contracts_are_tracked(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        for required in (
            "privacy-guard:",
            "public_privacy_guard.py tree",
            "public_privacy_guard.py range",
            "            release\n            --deny-map",
            "privacy-post-fetch:",
            "            post-fetch\n            --deny-map",
            "generate_release_notes: false",
            "check_public_links.py --online",
        ):
            self.assertIn(required, workflow)
        link = run(["python3", LINK_CHECKER], ROOT, check=False)
        self.assertEqual(link.returncode, 0, link.stderr)


if __name__ == "__main__":
    unittest.main()
