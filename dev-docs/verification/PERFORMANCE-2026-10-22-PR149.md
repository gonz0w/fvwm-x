# Periodic performance observation — PR144 to PR149

Due after five merged implementation checkpoints: PR145–149. Three alternating
paired runs use the preserved Linux ARM64 GCC binaries, the same profiler,
128 command samples and approximately 10,000 title/resize requests per second.
No binaries are rebuilt; the Buttons development build is excluded.

| Observation | PR144 | PR149 |
| --- | --- | --- |
| Idle command median | 0.385 ms | 0.375 ms |
| Title command median | 0.400 ms | 0.400 ms |
| Resize command median | 0.352 ms | 0.363 ms |
| Peak RSS (idle) | 7200 KiB | 7196 KiB |
| GNU text/read-only | 1078282 bytes | 1085180 bytes |
| GNU writable data | 26548 bytes | 26556 bytes |
| BSS | 39520 bytes | 39520 bytes |

Command-median ranges overlap in every workload. Title-load p95 increases
from 0.749 to 1.087 ms in this sample; the title median is unchanged and
other p95 observations vary slightly in either direction. CPU and peak RSS
remain similar. No consistent latency regression is established.

These are observations on one VM/container/Xvfb environment, not universal
speed claims or timing pass thresholds. Command/timer median and p95, CPU,
achieved load and peak RSS for all workloads remain in the
[machine receipt](PERFORMANCE-2026-10-22-PR149.json). Peak RSS is a process
high-water mark; debug file size and allocated code/data measure different things.
Source C/header changes over these five checkpoints total 1129 additions and
232 removals; line counts do not quantify readability.

All six runs and eighteen workload profiles pass observation/teardown validation.
Exact source/binary identities, compiler/loader preflight, raw samples and payload
hashes are preserved in the receipt. Raw archive:
`builddir/codex-2026-10-22/buttons-atomic-updates/performance-evidence.tgz`.
The verified PR149 binaries become the next reference. The counter resets to
0/5 subsequent merged implementation checkpoints; this atomic Buttons batch
is not yet merged. Native CPU, physical desktops, external toolkits/compositors
and release qualification remain separate.
