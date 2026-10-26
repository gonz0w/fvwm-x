# Periodic comparison: merged PR248 to PR253

The due lightweight observation is complete before the next candidate upload.
No speed gain is established. Script's selected memory observations increase;
these aggregates do not identify allocation growth or a leak.

## Matched method and observations

Use the preserved PR248 binaries and the already-built merged PR253 binaries in
one owned Linux ARM64 GCC development environment. All six binary identities
match their prior accepted receipts before copying the new reference. No rebuild
or platform matrix is run. Each side receives one warmup and five alternating
pairs with unchanged workload and observers. Existing Xvfb supplies the Script
display; a 4096-self-Set script reaches its ready marker, waits 20 ms, and samples
`/proc` before normal termination. The browser scans 4096 empty files.

| Median observation | PR248 | PR253 | Interpretation |
| --- | ---: | ---: | --- |
| Script ready latency | 52.431 ms | 52.321 ms | Overlapping samples; no demonstrated speed gain. |
| Script `/proc/status` RSS | 5184 KiB | 5248 KiB | +64 KiB; resident mappings, not allocation accounting. |
| Script `/proc/status` anonymous RSS | 924 KiB | 924 KiB | Unchanged median. |
| Script `smaps_rollup` private dirty | 924 KiB | 1436 KiB | +512 KiB; distinct aggregate observation, not proof of heap growth. |
| Browser scan wall time | 8.872 ms | 8.993 ms | Identical executable bytes; environment telemetry. |
| Browser peak RSS | 27720 KiB | 27720 KiB | Identical executable bytes; environment telemetry. |

All Script CPU samples remain below one scheduler tick. Status and smaps fields
are separate observations with different accounting; do not equate their deltas
or infer a per-allocation cause. Preserve the raw values without a timing gate,
expectation adjustment or additional runs to obtain a preferred result.

| Size inventory | PR248 | PR253 | Change |
| --- | ---: | ---: | ---: |
| Core GNU text+data+bss | 1170469 B | 1157173 B | -13296 B |
| Core debug executable | 5454744 B | 5386800 B | -67944 B |
| Script GNU text+data+bss | 560024 B | 567830 B | +7806 B |
| Script debug executable | 2208288 B | 2207672 B | -616 B |

Debug file size and GNU section sums measure different things. This accumulated
comparison includes ownership fixes and private-code deletion; neither is a
whole-desktop memory or responsiveness result. No core idle/title/resize/redraw,
X-request, installed-dependency or physical/native-CPU workload runs here. The
finite #209 handoff and release requirements retain their separate scope.

## Provenance and continuation

PR253 canonical merge `965787c6b4e0da2f3115d3df724a0778609ed691`
preserves the reviewed tree; GitHub signature/DCO are verified. The existing VM
and container were stopped. A launch reported startup then exited; keeping the
launch session open restored access to the same container/cache. No benchmark
sample ran before identity verification, and no VM/disk/profile was replaced.

Private canonical evidence is
`builddir/codex-2026-10-26/m5-next/performance-evidence.tgz`: 23 payloads plus
manifest, all payload hashes, sizes and modes verified. Actual two harness files
match the owning checkout. The [machine receipt](PERFORMANCE-2026-10-26-PR253.json)
contains exact binary identities, all paired samples, observer source, profile
and archive identity. Verified duplicate guest capture/archive are retired; the
canonical host archive and all references remain. No failed timing samples or suppressions are omitted.

Preserved `m5-performance-pr253/{core,script,browser}` becomes the next reference;
PR248 and PR243 references remain intact. The regular counter resets to 0/5
at merged PR253. Broader qualification is pending, not supplied by this comparison.
