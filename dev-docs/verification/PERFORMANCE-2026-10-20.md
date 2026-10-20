# Periodic performance reference — 2026-10-20

PERF-01 / CLEAN-01. First paired reference under the user-requested
[periodic cadence](../WORKFLOW.md#periodic-performance-comparisons), comparing
merged PR #131 with the visual-startup candidate. The candidate changes startup
ownership, not the measured steady-state command paths; no speedup is expected.

Three repetitions per side alternate A/B, B/A, A/B on the same quiet Linux
ARM64 GCC default guest profile. Each run uses the same current profiler,
owned Xvfb 1280x720x24, 128 command/timer samples per idle/title/resize case and
10,000 requested title/resize requests per second. Both already built binaries
are reused; there is no build, correctness matrix or timing acceptance gate.
The previous checkpoint's canonical GCC binaries provide the reference. The
[receipt](PERFORMANCE-2026-10-20.json) retains binary/profiler hashes, source
binding, raw run paths, per-run statuses, size output and aggregate observations.

| Workload | Command median ms, reference / candidate | Command p95 ms, reference / candidate | CPU %, reference / candidate |
| --- | --- | --- | --- |
| Idle | 0.406 / 0.411 | 0.543 / 0.550 | 2.79 / 2.83 |
| Title | 0.412 / 0.417 | 0.771 / 0.869 | 8.05 / 8.59 |
| Resize | 0.394 / 0.383 | 0.531 / 0.537 | 17.09 / 17.69 |

Values are medians of per-run medians/p95/CPU observations; raw samples and
ranges remain in the canonical archive. Achieved load is 9,993–9,996 requests
per second. The 10-ms scheduled-command medians are 12.25–13.42 ms across
workloads/sides. There are only three repetitions: these small mixed timing
changes do not establish a causal regression, improvement or hardware result.

Linux process peak RSS is 7,200 / 7,192 KiB (reference/candidate). It is a
cumulative high-water mark, not per-case allocation or evidence of an eight-KiB
memory optimization. GNU `size` reports text 1,066,455 / 1,075,279 bytes; data
26,460 and bss 39,520 bytes remain equal. ELF .text grows 8,576 bytes, while
changed function-symbol sizes sum to +388 bytes. The section contains 2,040
additional aligned zero words (8,160 bytes). Function sizes and section totals
measure different aspects of executable size. This checkpoint does not shrink the executable.
File bytes, including debug sections, are retained separately in the receipt.
Production C/header sources under fvwm/libs/modules add 110 and remove 45 lines
(+65 net) against PR131; test and documentation growth is separate.

Canonical raw observations:
`builddir/codex-2026-10-20/visual-startup-ownership/performance-evidence.tgz`.
Source/build/environment and size diagnostics are retained alongside it.
Native-CPU/hardware, toolkit/compositor and release acceptance remain separate.
The next regular comparison is due after five merged implementation checkpoints
following this reference; record each completed checkpoint in the tracker.
