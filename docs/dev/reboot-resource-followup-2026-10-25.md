<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Login startup and resource follow-up — 2026-10-25

The supplied macOS report identifies the GUI launched by
`io.github.victoriousian.ieum.login`. Termination is `CODESIGNING`, code 4,
`Launch Constraint Violation`, before application code runs. The report has no
Team ID. The large Guard reservation is virtual address space, not resident RAM;
neither that reservation nor the shared-cache reporting error diagnoses a leak.
The report establishes an enforced launch-constraint mismatch, not which cached
signing requirement failed. A stale requirement after an ad-hoc alpha replacement
is consistent with the existing direct-executable LaunchAgent design.

## Startup change

Use `SMAppService.mainAppService` for the GUI application login item. Reconcile
the saved preference, then retire the former agent, including when the main app
is already enabled or awaiting approval. Register the replacement first because
unregistering the old agent can terminate its process. Cleanup is attempted even
if the new registration fails; errors remain visible to the caller and the next
reconciliation retries failed cleanup. The old plist stays bundled solely to
identify the service to unregister.

Capture the launch Apple Event during the AppKit launch notification and defer
opening the window until its login context is known. This preserves background
startup without depending on a `--background` argument that mainAppService does
not supply. Native observation begins before QApplication, so startup dialogs
cannot consume the launch event first. Offscreen/minimal tools bypass the wait.

The package smoke check now invokes the GUI's `--version` as well as the core's,
in addition to verifying the sealed bundle. This exercises both loaders but
does **not** test a real login or prove that ServiceManagement registration works.

API references: [mainAppService](https://developer.apple.com/documentation/servicemanagement/smappservice/mainapp?language=objc),
[launch Apple Event constants](https://developer.apple.com/documentation/coreservices/apple_events/1556410-launch_apple_event_constants).
Apple also documents a [launch-constraint failure after updating a registered service](https://developer.apple.com/forums/thread/795022).

## Resource change and measurements

The hidden log queue now retains at most 256 Ki UTF-16 code units (plus its
omission notice), still bounded by 2,000 entries. It evicts oldest entries so the
latest failure survives. Rendered text is bounded by 1 Mi code units and 10,000
blocks. The original log file is unaffected.

Native file-change notifications still deliver appended lines immediately. When
both file and directory watches exist, the fallback timer checks every 30 seconds
instead of every 2 seconds. Missing watches use the 2-second recovery interval;
the directory watch is retried if the parent directory appears later.
Windows retains its existing 2-second interval even with registered watches:
the first native Windows CI exposed a file-tail regression when trusting watch
registration for a 30-second interval. Windows test diagnostics are forced to
stderr so future failures include the individual failing case.

Both builds use Debian 13 x86-64, GCC 14, Qt 6.8.2, and **Debug** configuration.
The baseline is `d9578dec4e6928579a1822615b4a297ec71aa25f`, with only the existing
resource-probe target enabled. Baseline binaries were preserved before production
edits. The probe source is identical in both builds. No compilation or tests ran
during the comparison. The shared cloud environment's other load is uncontrolled.

Burst results are medians of 3 runs per build in alternating order. The hidden
log workload is 2,000 lines of 16,384 payload characters, truncated to the
existing per-line display limit. RSS covers the whole probe process, not just
the queue. CPU time includes workload cleanup.

| Workload | RSS before → after | CPU time before → after |
| --- | --- | --- |
| Hidden log burst | 48.39 → 25.65 MiB | 30.09 → 21.80 ms |
| 100,000 pending retry snapshots, unchanged production path | 19.37 → 19.31 MiB | 140.24 → 149.02 ms |
| 10,000 pending IPC events, unchanged production path | 19.64 → 19.59 MiB | 39.29 → 41.26 ms |

The log workload used about 22.74 MiB less RSS in this experiment. This does not
establish a whole-app memory saving under ordinary input traffic. The unchanged
IPC controls show small CPU increases; no optimization is claimed for them.

One 30-second file-tail run per build recorded 14 → 0 timer callbacks and
4.15 → 1.45 ms process CPU. Zero is a sampling-boundary effect: the new 30-second
coarse timer can first fire just after the measurement ends. It remains enabled,
and its configured period, rather than this short count, describes steady usage.

The stopped-core GUI ran for 30 seconds after a 3-second warmup, 3 times per build
in alternating order. CPU totals were 480 / 430 / 430 ms before and
440 / 440 / 440 ms after. Median final RSS was 37.35 → 37.30 MiB. RSS grew from
roughly 18–19 MiB during each sample, so these windows include delayed startup
allocations and should not be treated as settled long-session idle profiles.
**No general idle CPU improvement or native macOS saving is established.**

Raw samples: [JSON](resource-followup-2026-10-25.json).

Reproduction with preserved matching binaries:

```sh
python3 tools/benchmarks/compare_resources.py \
  --before /path/to/baseline/bin/ieum-resource-probe \
  --after build/bin/ieum-resource-probe \
  --before-gui /path/to/baseline/bin/ieum --after-gui build/bin/ieum \
  --repeats 3 --gui-repeats 3 --idle-seconds 30 --output /tmp/ieum-resources.json
```

## Validation and remaining native work

- Linux Debug GUI/core build and GUI startup/clean IPC shutdown passed.
- All 7 targeted CTest programs passed, including migration failure/retry,
  bounded logs retaining newest context, native notification delivery, fallback
  polling and recovery when the parent directory appears.
- Full CTest: 72/74 programs passed. `ServerProxyTests` and `ServerTests` still
  abort because their fixtures omit the `CoreIpcServer` singleton. These same
  failures were recorded in cloud onboarding before these edits.
- Python CI regressions: 26 passed. Package smoke script Bash syntax and changed
  C++/Objective-C++ formatting were checked.

Release preparation subsequently fixed the two test fixtures by constructing
the missing `CoreIpcServer`, without changing production assertions or excluding
tests. The full Linux Debug suite then passed **74/74** programs.

No macOS compiler or connected Mac is available in this workspace. Native
compilation, tests and package verification run in release CI; consult that run
for the final build result. The records above describe local validation before
publishing alpha.30. Reboot acceptance still requires an installed Mac.

On an installed Mac build, verify fresh installation and an upgrade from the
old agent; preference on/off; failure and retry of legacy cleanup; manual launch;
hidden login startup even with autohide disabled; Dock reopen; menu-bar/core
recovery; and absence of new code-signing crash reports after reboot. Then
measure CPU/RAM during connected idle, input, clipboard traffic and sleep/wake.
See the [installation recovery steps](../user/installation.md#로그인-직후-launch-constraint-violation이-나올-때).
