# Periodic performance observation after PR248 — 2026-10-25

Owners: PERF-01 / CLEAN-01 #34. Five merged checkpoints after PR243 (#244–248)
make this lightweight comparison due. It runs before the next property-ownership
candidate is uploaded, using preserved binaries without another build or matrix.
The [receipt](PERFORMANCE-2026-10-25-PR248.json) retains all measured samples,
identities, GNU size output, guest information and next reference paths.

One reused Linux ARM64 development guest, normal GCC cache, one Xvfb, same
workload and alternating order. One warmup per side, then five pairs per workload.
The Script workload performs 4,096 self-referential Set operations, writes a ready
marker and remains resident with one ItemDraw. After the marker and a 20 ms settle,
/proc supplies resident/high-water/private-dirty KiB and coarse CPU ticks; SIGTERM
then exits cleanly. Ready time includes harness polling and scheduling. Both sides
use the exact same source and font directive; this is a bounded workload, not a
whole-desktop or input-latency measurement. Actual executable hashes are checked
against the committed reference/candidate receipts before execution.

| Script metric | PR243 reference | PR248 current |
| --- | ---: | ---: |
| Median resident / high-water KiB | 5,448 | 5,188 |
| Resident range KiB | 5,432–5,452 | 5,172–5,192 |
| Median private dirty KiB | 1,184 | 924 |
| Private dirty range KiB | 1,184–1,188 | 924–928 |
| Median ready ms | 55.125 | 55.375 |
| Ready range ms | 53.379–58.721 | 53.796–59.828 |
| GNU size total bytes | 560,170 | 560,024 |
| Executable bytes, including debug data | 2,205,840 | 2,208,288 |

Resident and private-dirty medians are 260 KiB lower (resident about 4.77%). This
observes accumulated PR243→248 changes on repeated variable references; it cannot
attribute the whole result to one patch or predict ordinary desktop memory.
Timing ranges overlap; no speed improvement is established. Script text/read-only
shrinks 210 bytes, data shrinks 32, BSS grows 96: total −146 bytes. Debug executable
size grows 2,448 bytes. Core inventory grows from 1,164,334 to 1,170,469 GNU-size
bytes and 5,395,312 to 5,454,744 executable bytes; no core timing workload ran.

The browser scans the same 4,096-file directory through its worker protocol.
Every scan returns all entries. Its executable is byte-identical on both sides:
median wall time 9.788 versus 9.983 ms; median peak RSS 27,780 KiB on both.
These values are environment telemetry, not a browser code-performance change.
No cross-date absolute timing conclusion is made.

The first Script warmup observer wrongly required the complete WriteToFile output
to equal the ready payload. That command wraps its payload in metadata; the file
already contained the expected payload. The corrected observer checks the payload
substring, verifies unchanged binaries, and reruns only Script warmups/pairs.
Successful browser samples are retained without repetition. Original observer,
failed warmup output/logs and corrected runner remain in the canonical archive;
this was an observer failure, not a production timeout.

No new OS/compiler/install matrix, CPU-native/hardware qualification, core idle/
title/resize timing or general speed claim. The preserved PR248 binaries become
the next five-checkpoint reference (0/5; the next candidate merge makes 1/5).
Canonical raw evidence and verified duplicate retirement are recorded with the
[owning property checkpoint](VERIFICATION-2026-10-25-M5-SCRIPT-PROPERTIES.md).
