# Session checkpoint core performance — 2026-10-29

The paired core observation compares cached merged PR299 (`9cde299b`) with
merged PR305 (`8402e098`). Command median ranges overlap in all three cases.
The idle 10 ms timer median increased by 0.324 ms, and its three-run median
ranges do not overlap. This observation alone establishes neither cause nor a
performance tuning result.

[Raw comparison, samples and binary identities](PERFORMANCE-2026-10-29-SESSION-CORE.json),
SHA256 `0ec456e4d407e18857e1cdb6ca38cbe54820377a1c5732c074cdbbb16e950f75`.
The source closure is
`86aaf460cf51a9b6d3b308171016a10b23d730d2703fffa9f4b0f2e4ef8ccc00`.

## Method and observations

The owned native Linux ARM64 GCC debugoptimized lab used an owned Xvfb and
existing binaries, without a rebuild. Three pairs alternated execution order;
each case used 128 command and timer samples. Title and resize requested 10,000
requests per second. Both cores used the same current MFL driver, with the exact
Echo/profile contract and no native packet ABI change. The summary is the
median of run medians; ranges span the three run medians.

| Measurement | PR299 | PR305 | PR305 / PR299 |
| --- | ---: | ---: | ---: |
| Idle command median ms (range) | 0.274 (0.261–0.297) | 0.290 (0.253–0.299) | 1.056 |
| Title command median ms (range) | 0.301 (0.294–0.305) | 0.304 (0.303–0.308) | 1.012 |
| Resize command median ms (range) | 0.253 (0.239–0.256) | 0.249 (0.229–0.251) | 0.984 |
| Idle 10 ms timer median ms (range) | 12.252 (12.222–12.445) | 12.577 (12.506–12.586) | 1.026 |
| Title 10 ms timer median ms | 11.734 | 11.712 | 0.998 |
| Resize 10 ms timer median ms | 12.425 | 12.365 | 0.995 |
| Median per-run peak core RSS KiB | 7,512 | 7,568 | 1.007 |
| Unstripped core file bytes | 5,590,368 | 5,689,360 | 1.018 |

Median per-run peak RSS rose by 56 KiB and the unstripped core by 98,992 bytes. File size
includes debug metadata and is not resident memory. CPU percentages in the raw
record use coarse 100 Hz ticks. Measured title rates were approximately 9,999
and 9,998 requests/s; resize rates approximately 9,995 and 9,997 requests/s.

The initial attempt failed because the MFL filename was `mfl`; it was renamed
`FvwmMFL` with the same binary hash. The single repeat (`paired02`) succeeded.
The `receipt02` postprocessing read the stale initial summary; `receipt03`
corrected the metadata without another measurement.

## Identities and limits

The PR299 core SHA256 is
`4862dd0f4db74937d7e0c0081ecb797d29e5d41bcffd81a4bb00eadc3cb4d347`;
PR305 core is `d66a52661b4507fa5084241ae540438e6948d0ae70acd12a622212852cbc7a0b`.
The common MFL is `ea499dcffe250ef913ec6ebdce7e3dd13ffb5453bf00041c494649467dbd9dd0`
(453,232 bytes). The new Form reference is
`40f1c9b760b5520d4a33666f7505d8418705af6e9d5453667e86c57af94ca63c`
(1,804,472 bytes); no Form comparison is asserted. The prior component browser
reference is retained in the raw inventory; browser and Script workloads were
not repeated.

This is a scoped core observation, not a measurement of the whole default
desktop, the power parent, or physical performance. The full OS matrix was not
rerun. Correctness and platform qualification remain separate from this timing
observation.
