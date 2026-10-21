# Periodic performance observation — PR138 to PR144

Due after five merged implementation checkpoints: PR139/141/142/143/144.
Uses the preserved PR138 Linux ARM64 GCC reference and verified PR144 installed
core/MFL bytes. The development manager-handoff build is excluded. One quiet
paired comparison alternates three runs per side with the same current profiler,
128 command samples and approximately 10,000 title/resize requests per second.
No binaries are rebuilt and no platform qualification is repeated for this sample.

| Observation | PR138 | PR144 |
| --- | --- | --- |
| Idle command median | 0.390 ms | 0.399 ms |
| Title-load command median | 0.400 ms | 0.397 ms |
| Resize-load command median | 0.389 ms | 0.378 ms |
| Peak RSS (all three cases) | 7192 KiB | 7196 KiB |
| GNU text/read-only | 1,067,087 bytes | 1,078,282 bytes |
| GNU writable data | 26,460 bytes | 26,548 bytes |
| BSS | 39,520 bytes | 39,520 bytes |

Command-median ranges overlap in every workload. Scheduled 10 ms actions remain
around 12 ms; p95 observations vary in both directions. CPU medians are similar
or lower, with achieved loads near the requested rate. This is an observation
on one VM/container/Xvfb environment, not a universal speedup or pass threshold.
Code/read-only grows 11,195 bytes and writable data 88 bytes over the five
safety/maintenance checkpoints. Peak RSS grows one 4 KiB page. These are recorded
costs, with no consistent latency regression in this sample; they do not reopen
performance-only tuning. The production C/header delta is 576 additions and
103 removals; counts do not quantify readability. Debug file size and allocated
code/data remain separate measurements.

All six runs and eighteen per-workload profiles pass observation/teardown
validation. The [machine receipt](PERFORMANCE-2026-10-21-PR144.json) records raw
samples, source/binary identities, compiler/loader/environment preflight, exact
source changes and canonical payload hashes. Raw archive:
`builddir/codex-2026-10-21/manager-handoff-safety/performance-evidence.tgz`.

Measured PR144 core/MFL bytes are copied into the guarded next reference and
preserved in a validated host archive. The counter resets to zero subsequent
merged implementation checkpoints after PR144. The unmerged manager-handoff
batch does not increment it. Keep the five-checkpoint cadence using existing
binaries; no new benchmark build is queued. Native-CPU, physical hardware,
external toolkits/compositors and release qualification remain separate.
