# Periodic performance comparison — 2026-10-21

PERF-01 #29 / CLEAN-01 #34. The five merged implementation checkpoints
PR133–136 and PR138 make this comparison due under the
[existing cadence](../WORKFLOW.md#periodic-performance-comparisons).
Compare the preserved PR132 reference with merged PR138 using existing Linux
ARM64 GCC default binaries. This is an observation, not a timing acceptance gate.

Three paired runs alternate A/B, B/A, A/B in the same guest. Both sides use the
same current profiler, private Xvfb 1280×720×24, 128 command/timer samples per
idle/title/resize case and 10,000 requested title/resize operations per second.
The original reference binary hashes match PR132's published qualification;
current hashes match PR138's qualified build. The native preflight records
compiler, platform, loader and profiler identities. Neither binary is rebuilt,
and no full correctness matrix is run for this measurement.

| Workload | Command median ms, PR132 / PR138 | Command p95 ms, PR132 / PR138 | CPU %, PR132 / PR138 |
| --- | --- | --- | --- |
| Idle | 0.399 / 0.405 | 0.577 / 0.574 | 2.47 / 2.49 |
| Title | 0.408 / 0.405 | 0.999 / 1.019 | 8.60 / 8.16 |
| Resize | 0.336 / 0.382 | 0.563 / 0.537 | 16.55 / 17.10 |

Each value is a median of three per-run values. The [receipt](PERFORMANCE-2026-10-21.json)
keeps individual results and ranges, raw sample paths and achieved load. Command
median ranges overlap in all three cases. Resize median rises from 0.336 to
0.382 ms, while its p95 falls from 0.563 to 0.537 ms; other changes are mixed.
Three repetitions do not establish a causal speedup, regression or universal
responsiveness guarantee. Achieved load is about 9,993–9,998 requests/second.
Scheduled 10-ms command medians remain about 12 ms. Linux peak RSS is 7,196 KiB
on both sides; it is a process high-water mark, not isolated per-case allocation.

GNU `size` text/read-only totals fall 1,075,279 → 1,067,087 bytes (−8,192), with
data 26,460 and bss 39,520 unchanged. The ELF `.text` section falls 774,268 →
766,012 bytes (−8,256). Of that difference, 8,152 bytes are fewer aligned zero
words and 32 bytes are fewer AArch64 NOP instructions. This is consistent with predominantly
layout/fill changes; it is not evidence of 8 KiB less executed work. Whole ELF files, including
debug information, fall 5,317,096 → 5,298,072 bytes (−19,024). These are distinct
size measures; no latency improvement is inferred from either one.
The sum of distinct nonzero `.text` function-symbol spans changes 755,132 →
755,088 bytes (−44). That narrower measure excludes gaps and zero-sized symbols;
it is not a count of instructions executed during the benchmark.

Across the five checkpoints, production C/header changes under `fvwm`, `libs`
and `modules` add 353 and remove 314 lines (+39 net across 21 paths). Tests and
documentation are separate. This is not a source-size reduction: constructor
failure ownership adds checks, record boundaries move definitions and the three
shared string-copy operations remove 28 lines from their owning implementation.
Line counts alone do not measure readability.

The source-bound raw archive is
`builddir/codex-2026-10-21/bsd-preparation-evidence/performance-evidence.tgz`.
PR138's measured core/MFL are preserved and independently hashed in
`performance-reference-pr138.tgz` and the owned guest reference directory.
PR138 becomes the next reference, at zero subsequent merged checkpoints.
The current tooling PR is unmerged; its merge will increment the count to one.
The next regular comparison follows five later merged implementation checkpoints,
or can be brought forward for an expected hot-path/memory change. Hardware,
native-CPU sanitizer and physical/toolkit/compositor acceptance remain separate.
