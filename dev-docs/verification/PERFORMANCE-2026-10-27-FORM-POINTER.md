# Periodic comparison: PR281 reference to Form pointer/paste

The due five-batch comparison is complete. No speed improvement is established.
[Pairs, identities and section sizes](PERFORMANCE-2026-10-27-FORM-POINTER.json)
bind the retained PR281 artifacts and final candidate to the same owned Linux
ARM64 normal GCC profile; no benchmark rebuild or full matrix was required.

One warmup per side precedes five alternating pairs of the existing 4,096-file
browser worker scan and 4,096-reference Script resident workload. Script ready
sampling includes a 20 ms settle and `/proc` memory/CPU observations. These are
observations, not timing gates or isolated allocation measurements.

| Median | PR281 reference | Candidate |
| --- | ---: | ---: |
| Browser worker elapsed | 9.092 ms | 9.120 ms |
| Browser launch-child peak RSS | 27,624 KiB | 27,624 KiB |
| Script ready latency | 52.328 ms | 52.478 ms |
| Script status RSS | 5,252 KiB | 5,248 KiB |
| Script private dirty | 928 KiB | 1,436 KiB |

Worker ranges overlap: 8.890–9.220 and 8.678–9.173 ms. Script is byte-identical;
its +508 KiB private-dirty observation is unexplained variation, consistent with
the earlier checkpoint's roughly 512 KiB difference. No allocation or leak cause
is inferred. Core/Script/browser file identities and section sizes remain in the
JSON. Candidate Form is 1,800,376 bytes including debug data, +2,848 bytes over
PR287. This comparison does not execute Form or measure pointer/paste latency,
frontend responsiveness, whole-session wakeups, physical CPU or release budgets.

Preserve `m5-performance-pointer-20261027/{core,script,browser}` as the next
measured reference and its separately retained `form` for future observations.
The due 5/5 comparison is satisfied; reset to 0/5 on this candidate's merge.
Correctness and theme evidence belongs to the
[focused Form checkpoint](VERIFICATION-2026-10-27-FORM-POINTER-PASTE.md).
