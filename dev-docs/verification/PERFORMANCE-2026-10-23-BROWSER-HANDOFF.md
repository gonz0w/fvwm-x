# Periodic browser performance and size observation

Status: **observed**, preserved PR174 versus the qualified portable browser
handoff candidate. [Raw observations, hashes and sizes](PERFORMANCE-2026-10-23-BROWSER-HANDOFF.json).
This is the due five-feature comparison; it uses existing binaries without
another build. Tuning remains deferred.

In the same Linux ARM64 guest/container, scan 4096 empty files after one warmup
per binary. Run five alternating paired samples and capture wall/CPU time and
maximum RSS with `os.wait4`. Temporary fixtures are removed. The guest is storage
constrained (233 MiB available at observation). No native-CPU or UI latency claim
is made; this measures metadata scan/worker handoff only.

| Observation | PR174 | Current |
| --- | ---: | ---: |
| Median scan wall time | 15.274 ms | 16.276 ms |
| Minimum–maximum | 14.833–20.597 ms | 15.288–18.013 ms |
| Median maximum RSS | 29172 KiB | 29172 KiB |

The current median is about 1 ms (6.6%) higher. Ranges overlap; five pairs do
not establish a speedup or consistent regression. Report this observation
without smoothing away the higher median. Core command latency is not remeasured;
its executable remains identical. New MFL/Form bytes do not inherit an older
binary's timing evidence.

| Unstripped executable file bytes | PR174 | Current | Change |
| --- | ---: | ---: | ---: |
| Core | 5396632 | 5396632 | 0.00% |
| MFL | 418512 | 445112 | +6.36% |
| Form | 1623488 | 1624952 | +0.09% |
| Browser | 5523577 | 5620856 | +1.76% |

File sizes include debug data and are not runtime memory usage. Allocated ELF
sections and exact binary identities are also recorded. Added provider/control
and handoff behavior accounts for a larger feature surface; no per-feature cost
or speed benefit is inferred. Qualified current binaries are preserved as the
next independent comparison reference, **0/5 on this feature merge**. The shared
platform cadence remains separate, PR178 0/3 and 1/3 on this merge.
