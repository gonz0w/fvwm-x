# Periodic performance observation — merged PR159 to PR164

The due five-merge observation uses preserved qualified Linux ARM64 GCC binaries;
no benchmark build or qualification matrix repeats. The current browser candidate
is excluded. [Exact observations and identities](PERFORMANCE-2026-10-23-PR164.json). Three alternating
paired repetitions use 128 samples, idle/title/resize workloads and requested
10,000 updates/s. Execution takes 30.838 seconds. No other batch
compiler/test job runs concurrently with this comparison. This emulated-host
observation does not establish physical-hardware or native-CPU performance.

| Workload | Command median ms | Command p95 ms | Scheduled 10 ms median ms | WM CPU % | Peak RSS KiB |
| --- | --- | --- | --- | --- | --- |
| Idle | 0.302 → 0.348 | 0.448 → 0.485 | 11.965 → 12.000 | 1.90 → 2.47 | 7200 → 7196 |
| Title load | 0.363 → 0.327 | 0.882 → 0.663 | 11.808 → 11.729 | 7.66 → 6.58 | 7200 → 7196 |
| Resize load | 0.300 → 0.270 | 0.458 → 0.530 | 12.365 → 12.354 | 15.36 → 14.20 | 7200 → 7196 |

Command-median ranges overlap in all three workloads. Mixed small latency/CPU
changes do not establish a general speedup or consistent regression. Peak RSS
is a process high-water mark rather than isolated workload memory. No sample is
rerun to obtain a preferred result; performance tuning remains deferred.

Core allocated text/read-only changes 1,099,301→1,097,293 bytes,
writable data 27,220→27,380 and BSS
39,552→39,824. Its debug-containing file grows
5,346,600→5,396,632 bytes. MFL allocated text/read-only grows 82,994→91,682,
data 2,488→2,544 and BSS 3,720→3,736; its debug-containing file grows
398,912→418,512 bytes. This interval adds operation and appearance/preview
features. These are measured feature costs, not codebase-wide size/readability
or new-browser cost claims. The optional Go browser is outside this comparison.

PR164 binaries become the reference, preserved in the canonical host capture
and dedicated owned guest reference directory. The independent counter resets
to 0/5 merged; the browser batch becomes 1/5 only on merge. The three-PR shared
integration cadence remains independent. All 45 canonical payloads verify
complete gzip EOF/CRC, hashes and modes before duplicate guest capture/archive
retirement. Preserve both dedicated guest references, builds and source.
