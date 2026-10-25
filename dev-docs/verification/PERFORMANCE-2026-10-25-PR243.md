# Periodic comparison after merged PR243

The fifth implementation checkpoint since PR238 is merged. This observation
reuses existing Linux ARM64 binaries and a 4096-file metadata scan: one warmup
per label followed by five alternating pairs, with child CPU/RSS from os.wait4.
There is no benchmark rebuild, platform matrix or production change.

The browser still has the exact qualified PR238 bytes (SHA-256
`c1a424baded9a1b1a90c6d1d46cb878f03a01df93068b29a157efd73583dac72`).
Its reference copy is reconstructed from those identical preserved bytes;
this is an environment/workload observation, not two browser implementations.

| Observation | PR238-identical reference | Merged PR243 |
| --- | ---: | ---: |
| Median wall | 8.793 ms | 8.778 ms |
| Median maximum RSS | 25,664 KiB | 25,664 KiB |
| Browser file | 5,623,589 bytes | 5,623,589 bytes |

Identical bytes and overlapping timings do not establish a browser speed gain.
Do not compare absolute timings across dates or infer desktop input latency.
The existing AppleHV/Podman Linux lab is not a bare-metal or release qualification.

Script has changed, so compare its current normal GCC size with the exact
recorded PR238 normal-profile inventory. This does not time the old Script or
isolate one change from the accumulated five implementation checkpoints.

| Script inventory | PR238 recorded | PR243 current | Difference |
| --- | ---: | ---: | ---: |
| GNU text/read-only | 533,477 | 519,906 | -13,571 bytes |
| Data | 14,624 | 14,512 | -112 bytes |
| BSS | 25,752 | 25,752 | 0 |
| Executable file | 2,209,168 | 2,205,840 | -3,328 bytes |

Text/read-only size falls about 2.54%; GNU size includes metadata and padding,
not just executed instructions. The file also includes debugging information.
This establishes a smaller recorded footprint, not a measured WM speedup or
runtime-memory reduction. Resource-release controls retain their separate
source-bound correctness evidence.

[Raw samples, identities, section output and runner method](PERFORMANCE-2026-10-25-PR243.json)
include current core inventory and verified copies of the merged PR243 core,
Script and browser. Preserve those copies as the next periodic reference at
0/5; the next implementation merge is 1/5. Measure earlier when a changed hot
path or memory contract justifies it; do not add a per-PR benchmark/build gate.
