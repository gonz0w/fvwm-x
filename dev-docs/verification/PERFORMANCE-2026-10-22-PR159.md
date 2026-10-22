# Periodic performance observation — merged PR154 to PR159

The due five-merge comparison uses existing qualified Linux ARM64 GCC binaries,
without rebuilding or running a qualification matrix. The current unmerged
operation batch is excluded. [Exact observations/identities](PERFORMANCE-2026-10-22-PR159.json).
Three alternating paired repetitions use 128 samples, idle/title/resize workloads
and requested 10,000 updates/s. The comparison takes 31.438 seconds. This is an
emulated-host observation, not physical-hardware or native-CPU certification.

| Workload | Command median ms | Command p95 ms | Scheduled 10 ms median ms | WM CPU % | Peak RSS KiB |
| --- | --- | --- | --- | --- | --- |
| Idle | 0.342 → 0.369 | 0.509 → 0.470 | 12.256 → 12.343 | 2.47 → 2.47 | 7200 → 7196 |
| Title load | 0.346 → 0.364 | 0.854 → 1.022 | 11.804 → 11.887 | 7.66 → 7.57 | 7200 → 7196 |
| Resize load | 0.327 → 0.333 | 0.456 → 0.485 | 12.381 → 12.453 | 15.28 → 15.92 | 7200 → 7196 |

Command-median ranges overlap in all three workloads (idle narrowly). Timer
ranges also overlap. Small mixed latency/CPU changes do not establish a general
speedup or a consistent regression; no case is rerun to obtain a preferred
result. Peak RSS is a process high-water mark, not isolated workload memory.

Core allocated text/read-only grows 1,085,180→1,099,301 bytes, writable data
26,556→27,220 and BSS 39,520→39,552. Its debug-containing file grows
5,319,768→5,346,600 bytes. MFL allocated text/read-only grows 71,121→82,994,
data 2272→2488 and BSS 3656→3720; its debug-containing file grows
366,320→398,912 bytes. This interval adds settings requests, provider/watch
controls and core query features. These are measured feature costs, not a
codebase-wide size or readability claim. Performance tuning stays deferred.

Fully verified PR159 binaries become the next reference, preserved both in the
canonical host capture and the owned guest reference directory. The independent
performance counter resets to 0/5 subsequent merged implementation PRs; the
operation PR advances it only on merge. The three-PR integration checkpoint does
not require an additional benchmark. Gzip EOF/CRC, every payload hash and mode
of the 45-file canonical capture are verified before guest duplicate retirement.
