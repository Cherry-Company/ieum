#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT

"""Regression check for the live upstream reference used by configuration docs."""

from __future__ import annotations

import argparse
import sys
import urllib.error
import urllib.request
from pathlib import Path


LEGACY_FAQ = "https://github.com/deskflow/deskflow/wiki/Legacy-FAQ"
REMOVED_DEAD_LINKS = {
    "https://github.com/deskflow/deskflow-core/issues/4411",
    "https://github.com/symless/synergy/issues/4411",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--document", type=Path, default=Path("docs/user/configuration.md"))
    parser.add_argument("--online", action="store_true")
    args = parser.parse_args()

    text = args.document.read_text(encoding="utf-8")
    if text.count(LEGACY_FAQ) != 1:
        print("configuration documentation must contain the exact Legacy FAQ URL once", file=sys.stderr)
        return 1
    if any(dead in text for dead in REMOVED_DEAD_LINKS):
        print("configuration documentation still contains a retired issue URL", file=sys.stderr)
        return 1
    if not args.online:
        return 0

    request = urllib.request.Request(
        LEGACY_FAQ,
        method="HEAD",
        headers={"User-Agent": "Ieum-public-link-check/1.0"},
    )
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            if response.status != 200 or response.geturl() != LEGACY_FAQ:
                print(
                    f"Legacy FAQ response mismatch: status={response.status} url={response.geturl()}",
                    file=sys.stderr,
                )
                return 1
    except (urllib.error.URLError, TimeoutError) as exc:
        print(f"Legacy FAQ request failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
