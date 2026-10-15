#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT
"""Exercise the real GUI host picker against two TLS cores on three owned X displays."""
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

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build-dir", type=Path, default=Path("build"))
parser.add_argument("--output-dir", type=Path, required=True)
args = parser.parse_args()
args.output_dir.mkdir(parents=True, exist_ok=True)
build = args.build_dir.resolve()
core = build / "bin/ieum-core"
test = build / "src/unittests/gui/HostSwitchIntegrationTests"
root = Path(tempfile.mkdtemp(prefix="ieum-switch-"))
processes, files = [], []

def launch(command, environment, name):
    log = open(args.output_dir / f"{name}.log", "w")
    files.append(log)
    process = subprocess.Popen(command, env=environment, stdout=log, stderr=subprocess.STDOUT)
    processes.append(process)
    return process

def display():
    process = subprocess.Popen(["Xvfb", "-displayfd", "1", "-screen", "0", "1280x800x24", "-nolisten", "tcp"],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    processes.append(process)
    if not select.select([process.stdout], [], [], 10)[0]:
        raise RuntimeError("Owned Xvfb did not become ready")
    number = process.stdout.readline().strip()
    if not number.isdigit():
        raise RuntimeError("Owned Xvfb returned no display")
    return ":" + number

def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=6)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)

try:
    displays = [display() for _ in range(3)]
    cert, key = root / "certificate.pem", root / "key.pem"
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-sha256", "-nodes", "-days", "1",
                    "-subj", "/CN=ieum-integration", "-keyout", str(key), "-out", str(cert)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    key.chmod(0o600)
    fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(cert.read_text())).hexdigest()
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    configurations, environments = [], []
    for index, name in enumerate(["host-a", "host-b", "switch-client"]):
        directory = root / name
        tls = directory / "tls"
        tls.mkdir(parents=True, mode=0o700)
        combined = tls / "ieum.pem"
        combined.write_bytes(cert.read_bytes() + key.read_bytes())
        combined.chmod(0o600)
        for peers in ["trusted-clients", "trusted-servers"]:
            (tls / peers).write_text(f"v2:sha256:{fingerprint}\n")
        environment = os.environ.copy()
        for variable, path in {"TMPDIR": directory / "tmp", "XDG_CONFIG_HOME": directory / "config",
                               "XDG_STATE_HOME": directory / "state", "XDG_RUNTIME_DIR": directory / "runtime"}.items():
            path.mkdir(mode=0o700)
            environment[variable] = str(path)
        environment.update({"DISPLAY": displays[index], "QT_QPA_PLATFORM": "xcb"})
        configuration = Path(environment["XDG_CONFIG_HOME"]) / "Ieum/Ieum.conf"
        configuration.parent.mkdir()
        trust_dir = configuration.parent / "tls"
        trust_dir.mkdir(mode=0o700)
        for peers in ["trusted-clients", "trusted-servers"]:
            shutil.copyfile(tls / peers, trust_dir / peers)
        content = (f"[core]\ncomputerName={name}\nport={port}\ndisplay={displays[index]}\nprocessMode=Desktop\n"
                   f"[security]\ntlsEnabled=true\ncheckPeerFingerprints=true\ncertificate={combined}\n"
                   "[gui]\nstartCoreWithGui=false\nenableUpdateCheck=false\nstartAtLogin=false\n[log]\nlevel=DEBUG\n")
        if index < 2:
            layout = directory / "server.conf"
            layout.write_text(f"section: screens\n {name}:\n switch-client:\nend\nsection: links\n"
                              f" {name}:\n  right = switch-client\n switch-client:\n  left = {name}\nend\n")
            content += f"[server]\nexternalConfig=true\nexternalConfigFile={layout}\n"
            content += f"[core]\ninterface=127.0.0.{index + 1}\n"
        configuration.write_text(content)
        configurations.append(configuration)
        environments.append(environment)
    hosts = []
    for index in range(2):
        hosts.append(launch([str(core), "server", "--new-instance", "--settings", str(configurations[index])],
                            environments[index], f"host-{index}"))
    deadline = time.monotonic() + 10
    for index in range(2):
        while True:
            try:
                with socket.create_connection((f"127.0.0.{index + 1}", port), timeout=0.5):
                    break
            except ConnectionRefusedError:
                if time.monotonic() > deadline:
                    raise RuntimeError("Owned host core did not listen")
                time.sleep(0.05)
    environment = environments[2]
    environment.update({"IEUM_INTEGRATION_CLIENT_SETTINGS": str(configurations[2]),
                        "IEUM_INTEGRATION_CORE": str(core),
                        "IEUM_INTEGRATION_INPUT_PROBE": str(Path(__file__).with_name("x11_input_probe.py").resolve()),
                        "IEUM_INTEGRATION_HOST_A_DISPLAY": displays[0],
                        "IEUM_INTEGRATION_HOST_A_PID": str(hosts[0].pid),
                        "IEUM_INTEGRATION_HOST_A_SETTINGS": str(configurations[0]),
                        "IEUM_INTEGRATION_HOST_B_DISPLAY": displays[1]})
    runner = launch([str(test)], environment, "gui-switch")
    runner.wait(timeout=180)
    if runner.returncode != 0:
        raise RuntimeError(f"GUI switch integration failed: {runner.returncode}; see {args.output_dir}")
    result = {"switches": 10, "tls": True, "keyAndPointerVerifiedOnEverySwitch": True,
              "rapidSelectionFinalHostOnly": True, "hostTerminationRecovery": True,
              "stopDuringSwitchCancelsRestart": True, "quitWaitsForCore": True,
              "transport": "loopback with injected device list", "platform": "Linux X11"}
    (args.output_dir / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
finally:
    for process in reversed(processes):
        stop(process)
    for file in files:
        file.close()
    shutil.rmtree(root)
