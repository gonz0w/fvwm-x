# Due comparison: PR194 to merged PR204

The preserved PR194 browser and current qualified browser run one warmup each,
then five alternating 4096-empty-file metadata scans in the existing Linux ARM64
lab. No benchmark build, tuning or platform matrix is performed.
[Samples, hashes, allocated sections, build and environment metadata](PERFORMANCE-2026-10-25-PR204.json)
bind this observation to the same AppleHV AArch64 Linux environment exercised by
[M2.10](VERIFICATION-2026-10-25-M2-NATIVE-ACCEPTANCE.md). This is exercised native
AArch64 instruction execution in hardware-virtualized Linux, not bare metal or
native UI latency. Historical TCG/native_cpu flags remain unchanged.

| Observation | Preserved PR194 | Current at merged PR204 |
| --- | ---: | ---: |
| Median wall time | 15.887 ms | 16.062 ms |
| Median user + system CPU | 17.857 ms | 18.316 ms |
| Median maximum RSS | 29472 KiB | 29472 KiB |
| Browser executable bytes | 5620310 | 5623589 |

Wall ranges overlap; five pairs do not establish a consistent speed improvement
or regression. Browser file size grows by 3,279 bytes (about 0.06%) following
M2.7's runtime-path work; debug-containing file size is not runtime memory.
Core/MFL/Form hashes and sizes remain identical to the preserved PR194 reference.
This observation covers metadata scan/worker handoff only, with no claim about
session startup, physical latency or this PR's C-linkage change. It does not
compare absolute timings or RSS with another date's measurements.

Preserve these current binaries as the new merged-PR204 reference at 0/5.
The M3.2 implementation advances the independent cadence to 1/5 on merge.
No performance tuning is selected; full M3 platform/install qualification remains
with its combined checkpoint.
