#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 Ieum Developers
# SPDX-License-Identifier: MIT

"""Merge server, client, and CGEvent logs by wall-clock timestamp."""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import re


ISO_TIME = re.compile(r"(\d{4}-\d\d-\d\d[T ][0-9:.]+(?:Z|[+-]\d\d:?\d\d)?)")
FLOAT_TIME = re.compile(r"^\s*(\d{9,}(?:\.\d+)?)\b")


def timestamp(line: str) -> float | None:
    if match := FLOAT_TIME.search(line):
        return float(match.group(1))
    if match := ISO_TIME.search(line):
        value = match.group(1).replace(" ", "T")
        if value.endswith("Z"):
            value = value[:-1] + "+00:00"
        try:
            parsed = dt.datetime.fromisoformat(value)
        except ValueError:
            return None
        if parsed.tzinfo is None:
            parsed = parsed.astimezone()
        return parsed.timestamp()
    return None


def read_events(path: pathlib.Path, source: str) -> list[tuple[float, str, str]]:
    events = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("wall_epoch\t"):
            continue
        if (value := timestamp(line)) is not None:
            events.append((value, source, line))
    return events


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("server", type=pathlib.Path)
    parser.add_argument("client", type=pathlib.Path)
    parser.add_argument("cgtap", type=pathlib.Path)
    parser.add_argument("-o", "--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    events = []
    for path, source in ((args.server, "server"), (args.client, "client"), (args.cgtap, "cgtap")):
        events.extend(read_events(path, source))
    events.sort(key=lambda item: item[0])

    with args.output.open("w", encoding="utf-8", newline="\n") as output:
        output.write("wall_epoch\tsource\tevent\n")
        for value, source, line in events:
            output.write(f"{value:.6f}\t{source}\t{line}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
