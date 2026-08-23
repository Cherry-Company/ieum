#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

"""Generate Ieum's deterministic JSON, Mermaid, and Markdown code graph."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from collections.abc import Sequence
from pathlib import Path

from codegraph import build_graph, render_mermaid, render_readme


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path.cwd(),
        help="Git repository root (default: current directory)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("docs/codegraph"),
        help="Artifact directory (default: docs/codegraph)",
    )
    return parser.parse_args(argv)


def write_text(path: Path, content: str) -> None:
    """Replace one UTF-8/LF artifact after its complete content is ready."""

    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(content, encoding="utf-8", newline="\n")
    temporary.replace(path)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    root = args.root.resolve()
    output = args.output.resolve()
    try:
        graph = build_graph(root)
        output.mkdir(parents=True, exist_ok=True)
        write_text(
            output / "codegraph.json",
            json.dumps(graph, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        )
        write_text(output / "component-graph.mmd", render_mermaid(graph))
        write_text(output / "README.md", render_readme(graph))
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f"code graph generation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
