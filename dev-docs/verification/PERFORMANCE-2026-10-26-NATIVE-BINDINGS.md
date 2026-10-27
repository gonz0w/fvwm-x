# Periodic comparison: PR264 to native bindings candidate

The due five-checkpoint comparison is complete before the BIND-01 candidate's
merge. No speed improvement is established. Use the [paired raw receipt](PERFORMANCE-2026-10-26-NATIVE-BINDINGS.json),
not these selected medians, when assessing variance.

Same owned Linux ARM64 normal GCC cache; preserved PR264 binaries verified by
SHA-256. No benchmark rebuild. The measured browser precedes exactly two final shortened
display literals; the final UI binary identity is separate in the receipt.
Worker policy is unchanged, and no second comparison is implied. One warmup per side and five alternating pairs:
4,096 empty-file browser worker scan; standalone Script with 4,096 self-Set
references, ready marker and 20 ms settle, followed by `/proc` observations.

| Median | PR264 | Candidate |
| --- | ---: | ---: |
| Browser worker scan elapsed | 8.720 ms | 8.713 ms |
| Browser launch-child peak RSS | 27,800 KiB | 27,800 KiB |
| Script ready latency | 52.357 ms | 52.232 ms |
| Script status RSS | 5,248 KiB | 5,248 KiB |
| Script smaps private dirty | 924 KiB | 1,436 KiB |
| Core debug file bytes | 5,387,608 | 5,479,448 |
| Script debug file bytes | 2,211,368 | 2,211,368 |
| Browser file bytes | 5,623,589 | 5,634,701 |

The Script binary is identical. Its +512 KiB smaps private-dirty median is
retained as observed variability, with no inferred allocation cause or leak.
Ready latency includes harness scheduling; coarse CPU ticks and launch-child
highwater are not isolated allocation metrics. Core changes accumulated since
PR264 are inventoried, not executed by this standalone Script workload. Browser
worker algorithm is unchanged; this comparison does not measure the new frontend
binding/UI paths. File bytes include build/debug data and differ from runtime
memory or section sums, which the receipt also retains.

No whole desktop, core responsiveness, scheduler wakeup, physical-CPU or release
resource-budget claim is made. Binding activity has its own causal native
[checkpoint](VERIFICATION-2026-10-26-NATIVE-BINDINGS.md). Preserve the actual
`m5-performance-bindings-20261026/{core,script,browser}` candidate files as the
next reference. Counter is 4/5 after merged PR268; this observation satisfies the
comparison due on the candidate's merge and resets that cadence to 0/5 then.
