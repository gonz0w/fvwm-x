# Due comparison: PR186 to merged PR194

Use the preserved pre-cleanup PR186 browser and the already qualified cleanup
browser still used at merged PR194. No benchmark build or platform matrix.
[Raw samples, binary hashes, allocated sections and build metadata](PERFORMANCE-2026-10-24-PR194.json)
retain one warmup and five alternating 4096-empty-file metadata scans per binary
in the same Linux ARM64 guest, with os.wait4 CPU and maximum RSS.

| Observation | Preserved PR186 | PR194 cleanup browser |
| --- | ---: | ---: |
| Median scan wall time | 16.332 ms | 16.041 ms |
| Median maximum RSS | 29460 KiB | 29460 KiB |
| Browser executable bytes | 5620856 | 5620310 |

Ranges overlap; this sample does not establish a consistent speed improvement
or regression. Core/MFL/Form hashes and file sizes are identical to the
reference. Browser file bytes fall by 546; debug-containing file size is not
runtime memory. The old/new binaries share the current observation's environment;
RSS from a prior observation is not a direct comparison with today's samples.

This measures metadata scan/worker handoff, not native UI latency, physical CPU,
or the new resident controller's session startup/RSS/idle/input. Those remain
required at M2 closure. Preserve current binaries as reference PR194 at 0/5;
the current M2.4 merge advances to 1/5. No tuning work is selected.
