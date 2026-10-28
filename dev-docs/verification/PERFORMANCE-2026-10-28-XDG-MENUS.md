# Periodic observation after merged PR293

The due five-batch comparison is complete on the retained Linux ARM64 normal GCC
profile. No speed improvement is established. The [sample/identity receipt](PERFORMANCE-2026-10-28-XDG-MENUS.json)
compares preserved PR288 pointer/paste artifacts with the current qualified build
available after PR293. It does not claim every binary was rebuilt from that merge.

Reuse the previous reference's micro-workloads: one warmup per side, five
alternating pairs of a 4,096-entry browser worker scan and a Script resident
performing 4,096 self-Set references. Script ready sampling includes a 20 ms
settle, then /proc memory/CPU observations. No benchmark rebuild, correctness
rerun, all-OS matrix or timing threshold is introduced. Core and Script binaries
are byte-identical, so repeating the complete idle/title/resize profiler would
not isolate these consumer changes; that broader responsiveness is not measured.

| Median | PR288 reference | Current retained build |
| --- | ---: | ---: |
| Browser worker elapsed | 8.711 ms | 8.784 ms |
| Launch-child peak RSS | 32,016 KiB | 32,016 KiB |
| Script ready | 52.893 ms | 52.830 ms |
| Script RSS | 5,248 KiB | 5,248 KiB |
| Script private dirty | 924 KiB | 1,436 KiB |

Worker ranges overlap: 8.203–9.568 and 8.468–9.309 ms. Launch-child high-water
RSS includes process startup and is not steady Go-worker allocation. Script's
+512 KiB private-dirty observation is unexplained variation despite identical
binaries, repeating the previous comparison's pattern. No allocation/leak cause
is inferred. Ready latency includes harness scheduling and CPU ticks are coarse.

The browser image grows 370,595 file bytes including debug information; `size`
reports +226,716 text/read-only and +480 initialized-data bytes, unchanged bss.
These are different quantities from runtime memory. Source C/header changes
since PR288 include the work-area and pinboard features and their tests, recorded
separately in the receipt; they are not all code executed by this workload.
The current Form image is retained independently, but this observation does not
execute Form, the new full-menu actions/icons, pinboard, or a whole desktop.

Preserve `m5-performance-xdg-20261028/{core,script,browser}` as the new measured
reference, with its separately retained Form. Reference identities and section
sizes are recorded before sampling. Raw evidence and normal owned-lab teardown
are retained with this feature checkpoint after canonical retrieval. Full-menu
frontend latency, whole-session cost/wakeups, physical/native CPU and R1 resource
budgets remain unmeasured. Reset the periodic count to 0/5 after merged PR293;
the next runtime candidate reaches 1/5 when merged, without a second comparison.
