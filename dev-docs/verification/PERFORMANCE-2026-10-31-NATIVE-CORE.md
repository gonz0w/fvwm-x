# Periodic core observation — 2026-10-31

The due paired observation compares retained merged PR310 with merged PR315 in
the same native-CPU Linux ARM64 GCC debugoptimized guest under AppleHV. It uses
already qualified cached binaries, with no benchmark rebuild or full OS matrix.
Three pairs alternate order; each idle/title/resize case has128 command and timer
samples. Title/resize request10,000 events/second. The current profiler drives
both cores and their corresponding, byte-identical MFL modules.

[Raw six-profile samples, binary identities and source deltas](PERFORMANCE-2026-10-31-NATIVE-CORE.json)
retain all49 verified evidence files through their canonical archive manifest.
The guest source05 guard is unchanged before/after observation; native production
inputs are unchanged by PR315's Go/FileManager1 additions. The new application
preference helper and whole default desktop are outside this measurement.

| Measurement | PR310 | PR315 |
| --- | ---: | ---: |
| Idle command median ms | 0.257 | 0.230 |
| Title command median ms | 0.245 | 0.273 |
| Resize command median ms | 0.264 | 0.275 |
| Idle command p95 ms | 0.357 | 0.341 |
| Title command p95 ms | 0.362 | 0.593 |
| Resize command p95 ms | 0.348 | 0.348 |
| Idle10 ms timer median ms | 12.233 | 12.375 |
| Title10 ms timer median ms | 11.506 | 11.588 |
| Resize10 ms timer median ms | 12.408 | 12.400 |
| Median per-run peak core RSS KiB | 7,580 | 7,588 |
| Unstripped core bytes | 5,693,008 | 5,693,488 |
| Core text/data/bss bytes | 1,146,111/36,516/306,336 | 1,158,600/36,524/306,336 |

Command-median ranges overlap in all three cases. Results are mixed, including
the higher observed title p95; this is neither a speedup claim nor evidence of
its cause. Actual title/resize rates remain approximately9,996–9,998 requests/s.
Core peak RSS rises8 KiB and file size480 bytes. File bytes include debug
metadata and differ from text/data/bss or resident memory. CPU tick sampling is
coarse; exact CPU/timer ranges and samples remain in the raw record. Do not infer
whole-desktop behavior, physical performance or marginal service cost.

Baseline core SHA256 is
`0fb343aa0512dc13c32373671642656e2181f321d86872765ffd881f052c068e`;
current core is
`e2b9bf9b77ae3691e7bd82b2fff0f6600d245eb10fd8bb1de74f94872082e4cd`.
Both MFLs are
`f9e4009245440a9ceb1741278bc2ccd9f9a31b534c9ab7ebe2ed2971e501dbca`.
The49-file archive SHA256 is
`18086ebc7514016f22e34d244c9655208034e52b1ef4f4fd70163e1a7a8e0294`.
The comparison resets the merged-checkpoint counter to0/5 at PR315. Subsequent
runtime merges advance it; documentation-only work does not.
