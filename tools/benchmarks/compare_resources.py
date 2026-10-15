#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT
"""Compare Linux resource probes and optional idle X11 GUIs from two builds."""
import argparse
import json
import os
from pathlib import Path
import select
import shutil
import statistics
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--before", type=Path, required=True)
parser.add_argument("--after", type=Path, required=True)
parser.add_argument("--before-gui", type=Path)
parser.add_argument("--after-gui", type=Path)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--repeats", type=int, default=3)
parser.add_argument("--idle-seconds", type=int, default=30)
parser.add_argument("--gui-only", action="store_true", help="Skip burst probes and file-tail sampling")
parser.add_argument("--gui-repeats", type=int, default=1)
args = parser.parse_args()
if args.repeats < 1 or args.gui_repeats < 1 or args.idle_seconds < 1:
    parser.error("Repeat counts and duration must be positive")
if args.gui_only and (args.before_gui is None or args.after_gui is None):
    parser.error("--gui-only requires --before-gui and --after-gui")
root = Path(tempfile.mkdtemp(prefix="ieum-resource-"))
results = []

def environment(name):
    result = os.environ.copy()
    for variable, folder in [("XDG_CONFIG_HOME", "config"), ("XDG_DATA_HOME", "data"),
                             ("XDG_STATE_HOME", "state"), ("XDG_CACHE_HOME", "cache"),
                             ("TMPDIR", "tmp"), ("XDG_RUNTIME_DIR", "runtime")]:
        path = root / name / folder
        path.mkdir(parents=True, mode=0o700, exist_ok=True)
        result[variable] = str(path)
    result["QT_QPA_PLATFORM"] = "offscreen"
    return result

def save():
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({"platform": "Linux", "results": results}, indent=2) + "\n")

def process_stats(pid):
    status = Path(f"/proc/{pid}/status").read_text()
    resident = next(int(line.split()[1]) for line in status.splitlines() if line.startswith("VmRSS:"))
    raw = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
    cpu_seconds = (int(raw[11]) + int(raw[12])) / os.sysconf("SC_CLK_TCK")
    return resident, cpu_seconds

def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)

def gui_idle(binary, name):
    display = subprocess.Popen(["Xvfb", "-displayfd", "1", "-screen", "0", "1280x800x24", "-nolisten", "tcp"],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    gui = None
    try:
        if not select.select([display.stdout], [], [], 10)[0]:
            raise RuntimeError("Owned display did not become ready")
        number = display.stdout.readline().strip()
        if not number.isdigit():
            raise RuntimeError("Owned display returned no number")
        env = environment(name)
        env.update({"DISPLAY": ":" + number, "QT_QPA_PLATFORM": "xcb"})
        config = Path(env["XDG_CONFIG_HOME"]) / "Ieum/Ieum.conf"
        config.parent.mkdir()
        config.write_text("[gui]\nstartCoreWithGui=false\nenableUpdateCheck=false\nstartAtLogin=false\nlogExpanded=false\n"
                          "[core]\ncoreMode=Client\nprocessMode=Desktop\nlanguage=en\n[security]\ntlsEnabled=false\n")
        with open(root / f"{name}.log", "w") as log:
            gui = subprocess.Popen([str(binary.resolve()), "--show"], env=env, stdout=log, stderr=subprocess.STDOUT)
            time.sleep(3)
            if gui.poll() is not None:
                raise RuntimeError(f"{name} GUI exited during warmup: {gui.returncode}")
            rss_before, cpu_before = process_stats(gui.pid)
            start = time.monotonic()
            time.sleep(args.idle_seconds)
            elapsed = time.monotonic() - start
            rss_after, cpu_after = process_stats(gui.pid)
            return {"scenario": "gui-idle-core-stopped", "elapsedMilliseconds": elapsed * 1000,
                    "cpuMilliseconds": (cpu_after - cpu_before) * 1000,
                    "cpuPercentOfOneCore": (cpu_after - cpu_before) / elapsed * 100,
                    "rssBeforeKiB": rss_before, "rssAfterKiB": rss_after}
    finally:
        if gui is not None:
            stop(gui)
        stop(display)

try:
    scenarios = [("ipc-retry", 100000, 0), ("ipc-events", 10000, 4096), ("logs-hidden", 2000, 16384)]
    if args.gui_only:
        scenarios = []
    for scenario, count, payload in scenarios:
        for repeat in range(args.repeats):
            # Alternate ordering to reduce cold-cache and allocator bias.
            versions = [("before", args.before), ("after", args.after)]
            if repeat % 2:
                versions.reverse()
            for version, binary in versions:
                completed = subprocess.run([str(binary.resolve()), scenario, str(count), str(payload)],
                                           env=environment(f"probe-{version}"), capture_output=True, text=True,
                                           check=True, timeout=30)
                record = json.loads(completed.stdout.splitlines()[-1])
                record.update({"version": version, "repeat": repeat})
                results.append(record)
                save()
    for version, binary in ([] if args.gui_only else [("before", args.before), ("after", args.after)]):
        completed = subprocess.run([str(binary.resolve()), "tail-idle", str(args.idle_seconds * 1000), "0"],
                                   env=environment(f"tail-{version}"), capture_output=True, text=True,
                                   check=True, timeout=args.idle_seconds + 10)
        record = json.loads(completed.stdout.splitlines()[-1])
        record["version"] = version
        results.append(record)
        save()
    for repeat in range(args.gui_repeats):
        versions = [("before", args.before_gui), ("after", args.after_gui)]
        if repeat % 2:
            versions.reverse()
        for version, binary in versions:
            if binary is not None:
                record = gui_idle(binary, f"gui-{version}-{repeat}")
                record.update({"version": version, "repeat": repeat})
                results.append(record)
                save()
    for scenario in dict.fromkeys(row["scenario"] for row in results):
        summary = {"scenario": scenario}
        for version in ["before", "after"]:
            selected = [row for row in results if row["scenario"] == scenario and row["version"] == version]
            if selected:
                summary[version] = {key: statistics.median(row[key] for row in selected)
                                    for key in ["cpuMilliseconds", "rssAfterKiB"]}
                if "pollTimeouts" in selected[0]:
                    summary[version]["pollTimeouts"] = selected[0]["pollTimeouts"]
        print(json.dumps(summary), flush=True)
finally:
    shutil.rmtree(root)
