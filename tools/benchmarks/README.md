<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Linux resource comparison

This optional tool measures selected production classes and an idle GUI. It does
not measure a connected desktop session, Windows, or macOS.

Configure the usual GUI build with `-DBUILD_RESOURCE_PROBE=ON`, then build
`ieum-resource-probe` and `ieum`. The comparison needs Python 3 and Xvfb:

```sh
python3 tools/benchmarks/compare_resources.py \
  --before /path/to/baseline/bin/ieum-resource-probe \
  --after build/bin/ieum-resource-probe \
  --before-gui /path/to/baseline/bin/ieum \
  --after-gui build/bin/ieum \
  --output /tmp/ieum-resource-comparison.json
```

Both builds must use the same compiler, Qt, configuration, and probe source. For
an older baseline without this target, add this directory with `add_subdirectory`
from the baseline's root CMake file; leave its production sources unchanged.
Run comparisons without concurrent compilation or other intentional load.

The first three scenarios run three times, alternating build order. Results are
medians; raw records retain each run. RSS includes Qt and widget allocations:

| Scenario | Workload |
| --- | --- |
| `ipc-retry` | 100,000 retry updates with no GUI connected |
| `ipc-events` | 10,000 distinct events, each with 4,096 payload characters, with no GUI connected |
| `logs-hidden` | 2,000 hidden-widget log lines, each with 16,384 payload characters |
| `tail-idle` | One 30-second file-tail run; also counts fallback timer callbacks |
| `gui-idle-core-stopped` | One 30-second GUI run after a 3-second warmup; core, update checking, and expanded log disabled |

CPU time comes from `getrusage` for probes and `/proc` for the GUI. RSS is sampled
from `/proc`; it is not a heap allocation count. Each GUI gets an owned virtual
display and temporary preferences. The script stops its own processes and
removes temporary settings. Repeat longer idle and connected-session measurements
before making claims about typical CPU use or battery life.

The dated results and remaining work are in
[the validation record](../../docs/user/reliability-validation.md).

To repeat only GUI idle measurements in alternating build order:

```sh
python3 tools/benchmarks/compare_resources.py \
  --before /path/to/baseline/bin/ieum-resource-probe \
  --after build/bin/ieum-resource-probe \
  --before-gui /path/to/baseline/bin/ieum --after-gui build/bin/ieum \
  --gui-only --gui-repeats 3 --idle-seconds 60 --output /tmp/ieum-gui-idle.json
```

For connected cores, `compare_connected.py` creates two owned X displays and
mutually trusted temporary TLS certificates. It checks real pointer and key
delivery before and after each idle period and samples both cores every five
seconds. It does not include a GUI or a VPN:

```sh
python3 tools/benchmarks/compare_connected.py \
  --before /path/to/baseline/bin/ieum-core --after build/bin/ieum-core \
  --seconds 60 --repeats 3 --output-dir /tmp/ieum-connected-resources
```

Failed input checks remain in the JSON with their error; inspect those and the
core logs before treating a run as a stable connected-session measurement.
The output directory contains debug logs from these synthetic test sessions.
Temporary private keys and profiles are removed after each run.
