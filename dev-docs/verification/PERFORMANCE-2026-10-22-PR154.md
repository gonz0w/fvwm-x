# Five-merge performance observation — PR149 to PR154

PR150–154 complete five merged implementation checkpoints. One existing Linux
ARM64 GCC profile compares preserved qualified PR149 and PR154 executables;
the unmerged settings-provider candidate is not part of this observation.
No benchmark build, install, platform matrix or timing threshold is added.
[Machine receipt](PERFORMANCE-2026-10-22-PR154.json).

The unchanged profiler alternates three paired runs with 128 samples in each
idle/title/resize workload, using owned Xvfb sessions and a requested 10,000
requests/second. All six runs/eighteen workloads complete. Observation time is
31.536 seconds, excluding collection. Raw samples, compiler/configuration,
binary hashes, load and teardown records remain in the canonical capture linked
by the receipt. This is emulated development telemetry, not a hardware result.

| Workload | Command median ms, PR149 → PR154 | Command p95 ms | Timer median ms | WM CPU % | WM peak RSS KiB |
| --- | --- | --- | --- | --- | --- |
| Idle | 0.379 → 0.384 | 0.478 → 0.507 | 12.353 → 12.457 | 2.47 → 2.47 | 7192 → 7196 |
| Title load | 0.379 → 0.373 | 0.980 → 0.930 | 12.015 → 11.880 | 8.75 → 8.25 | 7192 → 7196 |
| Resize load | 0.356 → 0.362 | 0.451 → 0.461 | 12.423 → 12.442 | 15.89 → 16.95 | 7192 → 7196 |

Idle/title command-median ranges overlap; resize ranges are narrowly separated
(0.311–0.357 versus 0.357–0.366 ms). Timer ranges overlap. The mixed small changes
do not establish a general speedup or consistent latency regression. CPU/RSS
are recorded observations; peak RSS is a process high-water mark and does not
isolate each workload. No noisy case is rerun for a preferred result.

Core allocated size is unchanged: text/read-only 1,085,180 bytes, writable data
26,556 and BSS 39,520. Its debug-containing file grows 40 bytes. Native MFL
text/read-only grows from 61,921 to 71,121 bytes, data 2176→2272 and BSS 3640→3656;
its debug-containing file grows 92,944 bytes with the first request/correlation
features. These are different size measures, not codebase-wide size claims.
This interval adds features and makes no readability or tuning speedup claim.

Verified copies of the qualified PR154 binaries become the next reference,
preserved both in the canonical host capture and an owned guest reference
directory. The counter resets to 0/5 subsequent merged implementation PRs.
The current settings-provider PR advances it only when merged. The independent
three-PR integration cycle does not trigger a benchmark per PR; continue using
already-built references and keep tuning deferred.
