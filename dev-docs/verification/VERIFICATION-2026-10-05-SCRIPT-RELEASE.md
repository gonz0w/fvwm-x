# FvwmScript repeat cancellation — 2026-10-05

The user approved cancelling MiniScroll's pending repeat on release, including
removing one legacy SingleClic callback. That change is complete. The first
action remains immediate, and held-repeat timing retains the 16667-microsecond
tick, initial 16-tick delay and subsequent 8-tick delay before each repeat.
Return selects once. Termination also ends a held repeat. No runtime library
or configuration option is added; core WM state and its event loop are unchanged.

## Implementation

The shared monotonic delay supports a cancellation callback and poll on the
module's X descriptor. It checks already queued release/termination before
waiting and again before expiry. Other input and signals do not restart the
deadline. A 100-ms maximum poll interval bounds the signal-before-poll race;
ordinary X release wakes the descriptor. This is a wait policy bound, not a
real-world scheduling or response-time guarantee. Non-cancelling callers retain
the sleeping nanosleep path.

MiniScroll cancels before incrementing its value or sending the pending
callback, clears both pressed arrows and returns to ordinary processing.
Unrelated X events discovered while checking release remain queued for later
dispatch. The repeat-stage counter is bounded instead of incrementing forever.
Delay errors end the repeat with a diagnostic. No arbitrary module commands
are dispatched inside the held wait.

Native delay coverage includes queued cancellation at INT_MAX ticks, a high
FD, input that does not reset the deadline, signal cancellation, invalid/missing
callback inputs, descriptor hangup and invalid-descriptor failure. Existing
minimum-duration, conversion and process-CPU checks remain.

Actual-X coverage requires exactly one and three callbacks for short and long
holds, ordered values, unrelated Expose traffic during the longer hold,
once-only Return, immediate queued release, restored arrow pixels and
DestroyNotify on termination of the test's own held module. Existing arithmetic,
rendering, scrollbar and descriptor coverage remains. Known output artifacts
are cleared before each fixture run so a fatal exit cannot reuse prior results.

## Final verification

| Target | Full tests | Widget outcomes | Default BSD X widget outcomes | Fresh install |
| --- | --- | --- | --- | --- |
| Linux ARM64 GCC | 70/70 | 16/16 | not applicable | passed |
| Linux ARM64 Clang/lld | 70/70 | 16/16 | not applicable | passed |
| Linux AMD64 GCC | 70/70 | 16/16 | not applicable | passed |
| FreeBSD ARM64 | 59/59 | 16/16 | 16/16, default Xorg | passed |
| OpenBSD ARM64 | 59/59 | 16/16 | 16/16, Xenocara/wsfb | passed |
| FreeBSD AMD64 | 59/59 | 16/16 | 16/16, default Xorg | passed |
| OpenBSD AMD64 | 59/59 | 16/16 | 16/16, default Xenocara | passed |

Six storage, seven execution and 24 selection/Form outcomes also pass under
nested X and all four default BSD servers. Seven collected archives match
105 source fingerprints and 1149 permission entries. All overlaid source
timestamps are refreshed before complete rebuilds. Fresh empty installations
match tested core/module .text and .rodata; current build hashes also match the
installation records. BSD RPATH changes are recorded separately. Optional Go
is disabled. Concurrent desktop/theme work is excluded from the frozen source.

These are local Podman and isolated QEMU snapshot gates on the macOS host,
including emulated AMD64 userspace. Native Linux ASan/UBSan CI remains unrun.
The four snapshots are shut down after evidence collection; unrelated desktop
VMs and Linux verification containers are preserved.

## Comparison and corrected fixture

A separate module links only the previous MiniScroll handler against current
shared code. It receives four callbacks when released after three and fails
the new exact-count assertion as expected. Production source, archive and
binary hashes are unchanged before/after that comparison and match the final
Linux ARM64 GCC installation record. The prior handler already used sleeping
delays; this control isolates release cancellation rather than CPU spinning.

The previous-handler samples record release-to-following-command times of
about 219 ms and 120 ms; final Linux samples are about 52–57 ms and default BSD
samples about 52–71 ms. File/X barriers and scheduling contribute to these
measurements. They are controlled samples, not a statistical benchmark or an
upper latency bound. Final Linux holds record zero scheduler CPU ticks, which
also does not imply zero CPU use.

The first focused termination observer queried a window after destruction,
causing a BadWindow X error and fixture failure. It now subscribes to structure
notifications before termination and checks DestroyNotify without querying a
destroyed window. Initial failed logs/source and corrected focused evidence
are retained. Final full gates run the corrected fixture and final source.

## Remaining scope

Outside-arrow pointer polling, broader List/main-loop descriptor waits,
ordinary module servicing during holds and child lifetime remain open. Other
text/list indices, geometry/font arithmetic and legacy FIFO processing also
remain tracked. This is not a complete interpreter-safety claim.

The next explicit compatibility decision is the distinct-colorset count limit
in the [storage proposal](../decisions/COLORSET-STORAGE-PROPOSAL.md). Sparse storage is
proposed to preserve large numeric theme IDs while avoiding allocation and
initialization of unused gaps; no colorset migration is implemented here.

Local evidence: builddir/codex-2026-10-05/script-release/, including
matrix-summary.json, source-final.tgz, per-target evidence archives,
legacy-release-control.json and initial/corrected focused logs.
