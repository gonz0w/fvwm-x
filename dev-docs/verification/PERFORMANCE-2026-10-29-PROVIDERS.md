# Provider checkpoint performance — 2026-10-29

The cached merged PR299 artifacts show no clear timing change against the PR293
reference in these small workloads. Browser scan peak RSS is unchanged. Script
RSS/PSS rises by 4 KiB with byte-identical executables. These observations do not
measure whole-desktop performance or the subsequent startup-group changes.

[Raw samples and binary identities](PERFORMANCE-2026-10-29-PROVIDERS.json), SHA256
`5d38c22d3248264170c3003428ccc9f2c798e18a7241ef14a4d6a0d69dcab737`.
Lead-executed in the owned native Linux ARM64 GCC lab; no rebuild, full matrix or
performance tuning was performed for this comparison. PR294–297 and PR299 are the
five runtime checkpoints since PR293; documentation-only PR298 is excluded.

## Method and observations

One warmup per executable followed by five alternating reference/current pairs.
The real browser worker scans 4,096 empty files; `os.wait4` supplies per-child CPU
and peak RSS. The real Script module runs 4,096 self-Set references against one
owned Xvfb with the existing handshake. Readiness precedes a 20 ms settling
interval and `/proc` memory sampling; actual termination/wait returns zero.

| Observation | PR293 reference | Merged PR299 |
| --- | ---: | ---: |
| Browser scan median wall ms | 9.220 | 9.088 |
| Browser wall range ms | 8.668–9.849 | 8.699–9.374 |
| Browser median peak RSS KiB | 32,580 | 32,580 |
| Script median ready ms | 52.389 | 52.181 |
| Script ready range ms | 52.188–53.556 | 52.137–52.476 |
| Script median RSS / PSS KiB | 5,248 / 3,639 | 5,252 / 3,643 |
| Script anonymous / file RSS KiB | 924 / 4,324 | 928 / 4,324 |
| Script private clean / dirty KiB | 1,860 / 924 | 1,348 / 1,440 |

Timing ranges overlap. Readiness includes harness scheduling, CPU ticks are coarse,
and peak RSS is not allocation accounting. The private-clean/dirty redistribution
is observed with identical Script bytes and almost unchanged RSS/PSS; its cause
was not established. It is not evidence of 516 KiB of new Script code or a leak.

## Artifact size

Unstripped file bytes include debug metadata; section sizes and static storage
are distinct from measured resident memory.

| Artifact | Reference file bytes | Current file bytes | Difference |
| --- | ---: | ---: | ---: |
| Core | 5,539,656 | 5,590,368 | +50,712 |
| Script | 2,211,368 | 2,211,368 | 0; identical SHA256 |
| Browser | 6,091,518 | 6,454,834 | +363,316 |

Core text/data/BSS changes are +1,854 / +40 / +216,144 bytes. The preceding
startup observer introduces bounded fixed storage; BSS is not a core RSS
measurement. Browser text/data/BSS changes are +255,761 / +352 / +48 bytes.
The retained Form binary is 1,802,864 bytes, unchanged from the prior reference;
no Form runtime workload was run here.

## Continuation and limits

The next reference is `m5-performance-providers-20261029` with the exact core,
Script, browser and Form hashes in the raw record. Reset the comparison cadence
to 0/5 at merged PR299; the next runtime PR advances it to 1/5 when merged.
Preserve this reference until the next verified comparison. Eight superseded owned
reference files (30,917,729 bytes) were retired after hash/ownership/live-use
checks and verification of the compact record and all four current copies.

No whole-WM idle/title/resize, browser frontend, Form, GPU, hardware or correctness
acceptance is inferred. The current startup-group implementation is outside this
measurement. Existing correctness gates retain their own source and scope.
