# Content checkpoint core observation — 2026-11-01

One due observation compared existing PR325 and PR327 core/MFL binaries in the
same owned Linux ARM64 GCC debugoptimized Podman/AppleHV lab. PR328/329 and the
current Script correction do not change those core binaries. The
[comparison and six sample records](PERFORMANCE-2026-11-01-CONTENT-CHECKPOINT.json)
retain exact identities, profile, raw samples and source guards.

Three alternating pairs used Xvfb 1280×720×24, 128 command/timer samples per
case and 10,000 requested title/resize operations per second. The complete run
took 32.34 seconds, without another build or VM. This measures core behavior,
not Script, protected memory, clipboard workloads or physical performance.

| Measurement | PR325 | PR327 |
| --- | ---: | ---: |
| Idle command median ms (range) | 0.256 (0.254–0.261) | 0.319 (0.286–0.325) |
| Idle 10 ms timer median ms (range) | 14.496 (13.323–14.584) | 14.304 (13.694–14.613) |
| Title command median ms (range) | 0.300 (0.280–0.312) | 0.282 (0.260–0.291) |
| Title 10 ms timer median ms (range) | 12.406 (12.389–12.413) | 12.275 (12.249–12.411) |
| Resize command median ms (range) | 0.233 (0.218–0.243) | 0.230 (0.225–0.238) |
| Resize 10 ms timer median ms (range) | 12.523 (12.516–12.539) | 12.401 (12.372–12.495) |
| Median per-run peak core RSS KiB | 7,516 | 7,512 |
| Unstripped core file bytes | 5,832,128 | 5,868,432 |
| Core text/data/bss bytes | 1,167,928 / 36,580 / 306,768 | 1,182,784 / 36,564 / 313,760 |

Idle command latency is higher by 0.064 ms in this observation; its three-run
median ranges do not overlap. Resize timer latency is lower by 0.122 ms, also
with separate ranges. Other command/timer ranges overlap. This is mixed scoped
measurement, not evidence of a general improvement or a causal regression.
Keep the idle observation visible at the next scheduled comparison; no tuning
or repeated timing attempt was selected. Achieved title/resize rates are about
9,991–9,994 operations/s. CPU uses coarse tick sampling. A 4 KiB RSS median
difference establishes no meaningful memory improvement. Core file size rises
36,304 bytes including debug metadata; text +14,856, data -16, bss +6,992.
File size is not resident memory.

The one due comparison resets the core cadence at this checkpoint. Retain the
current core/MFL as the next reference; candidate-memory cost evaluation stays
separate. No timing gate, complete desktop budget or release acceptance follows.
