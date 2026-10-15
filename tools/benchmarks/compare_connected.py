#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT
"""Compare two Linux cores in an authenticated X11 session, including input delivery."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import socket
import ssl
import subprocess
import tempfile
import time


def statistics(pid):
    status = Path(f"/proc/{pid}/status").read_text()
    rss = next(int(line.split()[1]) for line in status.splitlines() if line.startswith("VmRSS:"))
    fields = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
    cpu = (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")
    return {"rssKiB": rss, "cpuSeconds": cpu}


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def measure(binary, seconds, output, name):
    root = Path(tempfile.mkdtemp(prefix="ieum-connected-"))
    processes, logs = [], []
    try:
        displays = []
        for _ in range(2):
            display = subprocess.Popen(["Xvfb", "-displayfd", "1", "-screen", "0", "1280x800x24", "-nolisten", "tcp"],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            processes.append(display)
            if not select.select([display.stdout], [], [], 10)[0]:
                raise RuntimeError("Owned display did not start")
            number = display.stdout.readline().strip()
            if not number.isdigit():
                raise RuntimeError("Owned display returned no number")
            displays.append(":" + number)
        cert, key = root / "cert.pem", root / "key.pem"
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-sha256", "-nodes", "-days", "1",
                        "-subj", "/CN=ieum-resource-test", "-keyout", str(key), "-out", str(cert)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        key.chmod(0o600)
        fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(cert.read_text())).hexdigest()
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        cores, paths = [], []
        for index, role in enumerate(["server", "client"]):
            directory = root / role
            env = os.environ.copy()
            for variable, folder in [("TMPDIR", "tmp"), ("XDG_CONFIG_HOME", "config"),
                                     ("XDG_STATE_HOME", "state"), ("XDG_RUNTIME_DIR", "runtime"),
                                     ("XDG_DATA_HOME", "data"), ("XDG_CACHE_HOME", "cache")]:
                path = directory / folder
                path.mkdir(parents=True, mode=0o700)
                env[variable] = str(path)
            env.update({"DISPLAY": displays[index], "QT_QPA_PLATFORM": "xcb"})
            config = directory / "config/Ieum/Ieum.conf"
            tls = config.parent / "tls"
            tls.mkdir(parents=True, mode=0o700)
            combined = tls / "ieum.pem"
            combined.write_bytes(cert.read_bytes() + key.read_bytes())
            combined.chmod(0o600)
            for trusted in ["trusted-clients", "trusted-servers"]:
                (tls / trusted).write_text(f"v2:sha256:{fingerprint}\n")
            content = (f"[core]\ncomputerName=resource-{role}\nport={port}\ndisplay={displays[index]}\n"
                       "processMode=Desktop\ninterface=127.0.0.1\n"
                       f"[security]\ntlsEnabled=true\ncheckPeerFingerprints=true\ncertificate={combined}\n"
                       "[log]\nlevel=DEBUG\n")
            if role == "server":
                layout = directory / "layout.conf"
                layout.write_text("section: screens\n resource-server:\n resource-client:\nend\n"
                                  "section: links\n resource-server:\n  right = resource-client\n"
                                  " resource-client:\n  left = resource-server\nend\n")
                content += f"[server]\nexternalConfig=true\nexternalConfigFile={layout}\n"
            else:
                content += "[client]\nremoteHost=127.0.0.1\n"
            config.write_text(content)
            path = output / f"{name}-{role}.log"
            stream = path.open("w")
            logs.append(stream)
            child = subprocess.Popen([str(binary.resolve()), role, "--new-instance", "--settings", str(config)],
                                     env=env, stdout=stream, stderr=subprocess.STDOUT)
            processes.append(child)
            cores.append(child)
            paths.append(path)
            if role == "server":
                deadline = time.monotonic() + 10
                while True:
                    try:
                        with socket.create_connection(("127.0.0.1", port), timeout=0.5):
                            break
                    except ConnectionRefusedError:
                        if child.poll() is not None or time.monotonic() > deadline:
                            raise RuntimeError("Host core did not start")
                        time.sleep(0.05)
        deadline = time.monotonic() + 15
        while "remote layouts:" not in paths[1].read_text():
            if any(core.poll() is not None for core in cores) or time.monotonic() > deadline:
                raise RuntimeError("Client did not connect; inspect the core logs")
            time.sleep(0.05)
        if "TLSv1.3" not in paths[1].read_text():
            raise RuntimeError("TLS 1.3 was not confirmed")
        input_probe = Path(__file__).resolve().parents[1] / "tests/x11_input_probe.py"
        def verify_input():
            completed = subprocess.run(["python3", str(input_probe), displays[0], displays[1]],
                                       capture_output=True, text=True, check=True, timeout=8)
            return json.loads(completed.stdout)
        verify_input()
        before = [statistics(core.pid) for core in cores]
        start = time.monotonic()
        samples = []
        while time.monotonic() - start < seconds:
            time.sleep(min(5, max(0, seconds - (time.monotonic() - start))))
            sample = {"elapsedSeconds": time.monotonic() - start,
                      "cores": [statistics(core.pid) for core in cores]}
            samples.append(sample)
        elapsed = time.monotonic() - start
        after = [statistics(core.pid) for core in cores]
        try:
            input_result = verify_input()
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as error:
            input_result = {"keyReceived": False, "pointerMoved": False, "error": str(error)}
        return {"elapsedSeconds": elapsed, "tls": "TLSv1.3", "inputAfterIdle": input_result,
                "connectionLosses": paths[1].read_text().count("server is dead"),
                "samples": samples,
                "cores": [{"role": role, "rssBeforeKiB": old["rssKiB"], "rssAfterKiB": new["rssKiB"],
                           "cpuMilliseconds": (new["cpuSeconds"] - old["cpuSeconds"]) * 1000}
                          for role, old, new in zip(["host", "client"], before, after)]}
    finally:
        for process in reversed(processes):
            stop(process)
        for stream in logs:
            stream.close()
        shutil.rmtree(root)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    if args.seconds < 1 or args.repeats < 1:
        parser.error("Duration and repeat count must be positive")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    records = []
    for repeat in range(args.repeats):
        versions = [("before", args.before), ("after", args.after)]
        if repeat % 2:
            versions.reverse()
        for version, binary in versions:
            row = measure(binary, args.seconds, args.output_dir, f"{version}-{repeat}")
            row.update({"version": version, "repeat": repeat})
            records.append(row)
            result = {"platform": "Linux X11", "transport": "loopback", "guiIncluded": False, "results": records}
            (args.output_dir / "results.json").write_text(json.dumps(result, indent=2) + "\n")
            print(json.dumps(row), flush=True)


if __name__ == "__main__":
    main()
