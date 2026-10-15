<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Linux resource comparison — 2026-10-15

This comparison supports the bounded queue and log changes. It does **not** show
an improvement in typical whole-app CPU or memory use.
The last section records the later common-clock and X11 wait fixes; earlier
tables describe the intermediate candidate and retain the unsuccessful runs.

Baseline: `d3841e13d1b72f1c53240424bf80dcfe64e2e0a3`, exported with
`git archive`. Only the measurement target was added to that exported source.
Candidate: the working changes based on the same commit, including the bounded
IPC queue, UI log-line cap, file-tail timer, and interface changes.
Both builds used Debian 13 x86-64, GCC 14.2, Qt 6.8.2, and RelWithDebInfo.
Compilation finished before measurement. This was a shared cloud environment,
without control over other tenants' load.

The burst scenarios are medians of three runs in alternating build order. Memory
is process RSS sampled while the tested queue or widget still exists; CPU is the
process CPU time for the workload, including cleanup.

| Workload | RSS before → after | CPU time before → after |
| --- | --- | --- |
| 100,000 retry updates, GUI absent | 26.56 → 18.14 MiB | 50.36 → 41.18 ms |
| 10,000 events × 4,096 payload characters, GUI absent | 97.36 → 18.42 MiB | 74.44 → 29.38 ms |
| 2,000 hidden log lines × 16,384 payload characters | 93.92 → 47.09 MiB | 69.82 → 49.80 ms |

The queue now retains at most 64 messages and 128 Ki UTF-16 characters, with one
latest snapshot per status command. Old event notifications are discarded first
when the budget is exhausted. Individual pending messages over 64 KiB in UTF-8
are discarded. The GUI log caps each displayed line at 4,096 characters; this
does not shorten the underlying log file.

Idle scenarios had **one sample per build**, lasting 30 seconds each:

| Workload | Before | After |
| --- | --- | --- |
| File-tail fallback timer callbacks | 120 | 15 |
| File-tail process CPU time | 36.69 ms | 47.01 ms |
| GUI RSS, core stopped | 35.54 MiB | 36.71 MiB |
| GUI CPU time, core stopped | 30 ms | 100 ms |

The fallback timer wakes less often, while the native file watcher remains
enabled. These samples do not establish a CPU saving: measured CPU increased.
GUI CPU corresponds to approximately 0.10% → 0.33% of one core over the sample.
Short runs, scheduler noise, and `/proc` CPU resolution limit the conclusion.
The candidate GUI used about 1.17 MiB more RSS in this run.

Before declaring resource optimization complete, compare longer repeated runs
with connected real computers, input and clipboard traffic, reconnects, and
sleep/wake on Windows and macOS. Profile any persistent idle regression rather
than extrapolating burst savings to normal use.

Reproduction: [benchmark instructions](../../tools/benchmarks/README.md).
Raw samples: [JSON](resource-comparison-2026-10-15.json).


## Follow-up: repeated GUI idle and connected cores

The GUI idle test was repeated for 60 seconds, three times per build, in
alternating order. The measured binaries included the initial connection guide
and report-dialog work; the later computer-card paint changes were not involved
in this stopped-core screen.

| GUI idle, core stopped | Baseline | Candidate |
| --- | --- | --- |
| CPU time for each 60-second run | 70 / 70 / 20 ms | 70 / 10 / 20 ms |
| Median RSS | 35.61 MiB | 36.77 MiB |

This did not reproduce the direction of the earlier 30-second CPU increase.
The very small CPU totals vary considerably; a persistent idle CPU regression
or a general CPU improvement is not established. The extra GUI RSS remained
approximately 1.17 MiB. [Raw GUI samples](resource-idle-repeat-2026-10-15.json).

A separate harness connected two real cores on owned X11 displays over mutually
trusted TLS 1.3. It checked pointer and key delivery before and after a 60-second
idle period and sampled RSS/CPU every five seconds. There was no GUI or VPN.
The baseline commit, compiler, Qt, and build mode matched the earlier comparison.

| Build / repeat | Host CPU time | Client CPU time | Input after idle |
| --- | --- | --- | --- |
| Baseline 0 | 7,940 ms | 6,420 ms | Failed; connection loss in client log |
| Candidate 0 | 7,720 ms | 9,830 ms | Received |
| Candidate 1 | 200 ms | 430 ms | Received |
| Baseline 1 | 1,300 ms | 1,750 ms | Received |
| Baseline 2 | 210 ms | 180 ms | Received |
| Candidate 2 | 140 ms | 160 ms | Received |

Baseline 0 is **not a successful stable connected-idle sample**. An earlier
attempt also timed out during its final input probe and is not counted as a
successful comparison. Candidate logs contained no `server is dead` event in
these three runs; the baseline had one in repeat 0. The candidate result is a
small Linux experiment, not evidence of resolved native-platform instability.
Both builds showed large CPU variation, so no whole-app CPU reduction is claimed.
Host RSS stayed around 30.1–30.2 MiB, and candidate client RSS around 29.4 MiB.
This is too short to rule out long-session leaks or sleep/wake problems.

[Raw connected samples, including the failure](resource-connected-2026-10-15.json).
The failed runs and CPU variation remain open for reproduction/profiling; do not
turn this table into a headline optimization percentage.

## Common clock and event-wait correction

`Arch::time()` divided an integer microsecond count by integer `1,000,000`
before converting to `double`. It therefore returned whole seconds, despite
callers using it for subsecond input timers, timeouts, and retries. It now returns
`std::chrono::duration<double>` from the monotonic clock. This is shared by all
platforms; native Windows/macOS execution is still outstanding.

The new 60 ms clock regression test fails against the unchanged baseline and
passes against the candidate. Two separate X11 tests also fail against the
baseline: an early-return path left `m_waiting` set, and a 500 ms idle wait caused
20 voluntary context switches. The candidate clears the flag and uses a 250 ms
safety poll with real elapsed-time accounting and upward millisecond rounding.
The idle test observed **2** voluntary context switches. A posted event wakes the
pipe directly; a separate test verifies it does not wait for the safety poll.

Windows message-wait rounding was also changed so positive sub-millisecond
delays do not become zero-time polls. That native implementation has not been
built or executed in this Linux environment.

The two-core TLS comparison was repeated with the clock and X11 corrections,
three 60-second runs per build in alternating order:

| Measurement | Baseline median | Corrected candidate median |
| --- | --- | --- |
| Host CPU time per 60-second run | 550 ms | 320 ms |
| Client CPU time per 60-second run | 1,070 ms | 380 ms |
| Host RSS | 30.11 MiB | 30.14 MiB |
| Client RSS | 29.43 MiB | 29.35 MiB |
| Input delivered after idle | 3 / 3 | 3 / 3 |
| Connection-loss events | 0 | 0 |

These are Linux core measurements without a GUI or Tailscale VPN. The median
CPU time fell in this experiment, while RSS was essentially unchanged. Ranges
overlap, sample size is small, and no reduction in native-platform whole-app CPU
or battery use is established. Input checks poll every 20 ms; their reported
probe duration is not a precise end-to-end keyboard latency measurement.

[Raw corrected-clock comparison](resource-clock-fix-2026-10-15.json).

Reproduce the deterministic regressions with `ArchTimeTests` in the portable
CTest suite and the X11-specific test on an owned display:

```sh
xvfb-run -a build/src/unittests/platform/XWindowsEventQueueBufferTests
```
