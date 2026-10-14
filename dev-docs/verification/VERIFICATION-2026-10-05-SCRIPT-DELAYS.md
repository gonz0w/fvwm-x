# FvwmScript sleeping widget delays — 2026-10-05

The [widget arithmetic pass](VERIFICATION-2026-10-05-SCRIPT-WIDGETS.md)
is complete. This follow-up removes CPU spinning from MiniScroll repeat waits
and the shared List/double-click delay. It preserves the legacy 16667-microsecond
tick, initial 16-tick delay, subsequent 8-tick delays and callback order. No
runtime library or configuration option is added; authoritative WM state and
its event loop are unchanged.

## Implementation and coverage

The shared delay uses nanosleep and checks elapsed CLOCK_MONOTONIC time before
returning. Nonpositive ticks return immediately. Conversion uses an int64
intermediate and covers INT_MAX without the previous signed multiplication.
Clock/sleep errors return with a diagnostic instead of starting a spin loop.
MiniScroll reuses this helper through Wait rather than maintaining three
separate allocating gettimeofday loops. Signal interruption retains the entire
requested interval; this helper does not dispatch module events.

Native unit coverage checks ordinary/extreme tick conversion, actual elapsed
time across SIGALRM and process CPU use. Actual-X coverage holds MiniScroll for
one or three callbacks, checks ordered values and verifies a following module
command completes. Linux additionally reads only that test module's own process
CPU counters. Raw hold/release/CPU measurements are saved in repeat-profile.json.
The existing eleven widget outcomes, storage, execution and selection/Form
regressions are retained.

## Verification

| Target | Full final tests | Widget outcomes | Default BSD server | Fresh install |
| --- | --- | --- | --- | --- |
| Linux ARM64 GCC | 70/70 | 12/12 | not applicable | passed |
| Linux ARM64 Clang/lld | 70/70 | 12/12 | not applicable | passed |
| Linux AMD64 GCC | 70/70 | 12/12 | not applicable | passed |
| FreeBSD ARM64 | 59/59 | 12/12 | 12/12 on default Xorg | passed |
| OpenBSD ARM64 | 59/59 | 12/12 | 12/12 on Xenocara/wsfb | passed |
| FreeBSD AMD64 | 59/59 | 12/12 | 12/12 on default Xorg | passed |
| OpenBSD AMD64 | 59/59 | 12/12 | 12/12 on default Xenocara | passed |

All seven final archives match 105 source fingerprints and 1149 permission
entries. Six storage, seven execution and 24 prior selection/widget outcomes
also pass under Xvfb and all four default BSD servers. The Linux legs run in
Podman on the macOS host (AMD64 userspace is emulated); BSD legs boot isolated
QEMU snapshots. These are local verification gates, not native Linux CI.


The overlay inherits the previously verified widget snapshot and adds only the
owned delay source, build entries, fixture and manual edits. Complete initial
rebuilds refresh every overlay timestamp. The subsequent monotonic correction
changes only script_delay.c: all other source fingerprints must match the initial
snapshot before rebuilding, and complete tests and default BSD X module checks
are repeated afterward. The core binary is verified unchanged across that
follow-up. Concurrent desktop/theme changes are excluded from this snapshot.

Fresh empty installation stages verify the tested core/module .text and .rodata
and session entry. BSD installation can strip build RPATH, so whole binary hashes
are recorded separately rather than asserted equal. Optional Go is disabled.
Native Linux ASan/UBSan CI remains unrun; ordinary test environment variables
are not sanitizer instrumentation.

## Corrections retained in evidence

The first focused run passed eight GCC checks; Clang rejected a missing log
header. The header is added and the failed log/source archive retained.
Both FreeBSD architectures then rejected the exact minimum-duration assertion
after an interrupted nanosleep. The production helper now checks a monotonic deadline,
and the assertion is retained. Initial full-suite logs and source fingerprints
are preserved separately from the final gates.

The first separate previous-delay comparison attempted to relocate a thin
archive, whose members still referenced the original relative paths. It failed
before running the comparison. The corrected runner builds an ordinary archive
from explicitly resolved objects and compiles/links into its own control
directory. It never replaces production sources, objects, libraries or binaries;
before/after hashes verify this. The failed comparison log is retained.

The final Linux ARM64 previous-delay control consumes 0.26 seconds of process
CPU over a 0.270-second short hold. The new module records zero scheduler CPU
ticks over a 0.318-second hold; its longer hold also records zero ticks. Linux
AMD64 records zero ticks for the short hold and about 0.01 CPU seconds for the
longer hold. These are saved controlled samples with file/X barriers, not a
statistical latency benchmark or a claim of zero CPU use. The prior-delay
fixture fails the new CPU assertion as intended; production hashes remain
unchanged across the separate comparison.

The final held-repeat fixture receives a fourth callback after releasing at
three callbacks, confirming the compatibility behavior remains. Saved
release-to-following-command times vary with scheduling and barriers; they do
not establish an upper latency bound. The optimization concerns CPU consumption
during delays, not immediate module command servicing.

## Remaining behavior and compatibility choice

This removes spinning during the waits, not all CPU use in modal widget loops.
Pointer handling outside MiniScroll arrows, broader List descriptor waits,
module event servicing, signal termination and child lifetime remain open.
The existing hold logic can issue one pending repeat after release and defer
other module commands until the delay expires. Stopping that pending repeat
would change the number of SingleClic callbacks scripts receive; it requires
an explicit compatibility decision.

The proposed next change keeps the immediate first action and held-repeat
cadence, but checks release while waiting. A queued release cancels the pending
step, clears the pressed arrow and returns to ordinary module processing.
Return still selects once. This proposal concerns MiniScroll release; broader
modal event servicing and List descriptor handling are separate work. Approval
is pending because a legacy script can receive one fewer SingleClic callback.
Broader text/list indices, geometry/font arithmetic, legacy FIFO and
large-colorset policy remain tracked separately.

Local evidence: builddir/codex-2026-10-05/script-delays/.

Follow-up: the user subsequently approved release cancellation. It is completed
and verified in the [release report](VERIFICATION-2026-10-05-SCRIPT-RELEASE.md),
which supersedes the pending choice above and adds held-termination coverage.
