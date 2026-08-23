#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT

"""Print actionable privacy-scan metadata without reproducing scanned values."""

from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any

MANIFEST_SCHEMA = "ieum.public-surface-manifest.v1"
SAFE_LABEL = re.compile(r"^[A-Za-z0-9_.+-]{1,128}$")
OBJECT_ID = re.compile(r"^[0-9a-f]{40}(?:[0-9a-f]{24})?$")


def safe_label(value: Any) -> str:
    if isinstance(value, str) and SAFE_LABEL.fullmatch(value):
        return value
    encoded = str(value).encode("utf-8", errors="replace")
    return f"sha256:{hashlib.sha256(encoded).hexdigest()}"


def target_object(target: Any) -> str:
    if not isinstance(target, str):
        return safe_label(target)
    object_ids = [part for part in target.split(":")[1:] if OBJECT_ID.fullmatch(part)]
    if object_ids:
        return object_ids[-1]
    return f"sha256:{hashlib.sha256(target.encode('utf-8')).hexdigest()}"


def report(path: Path) -> int:
    try:
        payload = json.loads(path.read_bytes().decode("utf-8", errors="strict"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError):
        print("privacy manifest report failed: unreadable manifest", file=sys.stderr)
        return 4

    if not isinstance(payload, dict) or payload.get("schema") != MANIFEST_SCHEMA:
        print("privacy manifest report failed: schema mismatch", file=sys.stderr)
        return 4
    findings = payload.get("findings")
    if not isinstance(findings, list):
        print("privacy manifest report failed: findings are missing", file=sys.stderr)
        return 4

    print(f"result={safe_label(payload.get('result'))} findings={len(findings)}")
    for finding in findings:
        if not isinstance(finding, dict):
            print("privacy manifest report failed: malformed finding", file=sys.stderr)
            return 4
        offset = finding.get("offset")
        if not isinstance(offset, int) or offset < 0:
            print("privacy manifest report failed: malformed offset", file=sys.stderr)
            return 4
        print(
            " ".join(
                (
                    f"rule_id={safe_label(finding.get('rule_id'))}",
                    f"object={target_object(finding.get('target'))}",
                    f"target_kind={safe_label(finding.get('target_kind'))}",
                    f"detector={safe_label(finding.get('detector'))}",
                    f"offset={offset}",
                )
            )
        )
    return 0


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: report_public_manifest.py MANIFEST", file=sys.stderr)
        return 4
    return report(Path(sys.argv[1]))


if __name__ == "__main__":
    raise SystemExit(main())
