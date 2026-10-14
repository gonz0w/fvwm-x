# Core responsiveness: ordered timers and adjacent resize bursts

Tracker owners: PERF-01 / QUEUE-01 / CLEAN-01. Four related stories are
implemented and focused-qualified. Full combined-source qualification is
accepted in the linked 2026-10-11 report; this report retains focused scope.
Configuration files and command arguments remain usable without migration.

The [2026-10-11 combined qualification](VERIFICATION-2026-10-11-CORE-PERFORMANCE.md)
records the resumed whole-source gate; the focused results below retain their
original source and profile scope.

## Implemented batch

1. Ordered/equal scheduler insertion uses the existing unlocked queue tail.
   Equal-deadline FIFO and locked callback cleanup retain their existing owner.
   [Insertion evidence](VERIFICATION-2026-10-08-QUEUE-INSERTION.md) includes six
   native target profiles and scoped sanitizer checks.
2. The responsiveness profiler selects independent workloads with `--cases`
   and records executable, MFL and profiler source hashes, selected cases,
   sample count and server scope. Existing three-workload defaults remain.
3. Profiling teardown stops all owned children before collecting their final
   logs. Cleanup failures retain evidence and do not conceal an earlier
   measurement failure. Three real-process/ownership controls pass on macOS
   and in the normal Linux/BSD focused harnesses.
4. `AutoDetect` move policy merges adjacent size-only ConfigureRequests from
   the already queued snapshot. Each operation consumes at most 32 additional
   records. Other windows, moves, borders, stacking requests and every other
   event stop the batch. The shared FEvent helper does not read socket input,
   allocate storage, run callbacks or advance cached input events/timestamps.
   Dispatch reapplies its existing request normalization to the merged fields.
   Explicit move policies retain their prior merge path; synchronization
   barriers are not removed.

The existing AutoDetect routine uses position requests for inference. Adjacent
size-only merging leaves those requests intact. The user manual documents the
batch behavior; no dependency, configuration directive or background worker is
added.

## Measurements

Disposable link-time wrappers count ten direct project Xlib entry points without
changing their result or adding server requests. Counts cover whole process
startup, workload and shutdown; they exclude internal shared-library calls.
These are API-call counts, not a complete X11 protocol request trace.

The isolated Linux ARM64/AMD64 baseline runs use qualified `3c37` core objects.
The measured resize workload repeatedly changes client size at a requested
1,000 requests/second. Candidate load achieves roughly the same requested rate.
Each isolated profile collects sixteen command and sixteen scheduled-10-ms
samples. Raw timings, achieved rate and call counts are retained under
`builddir/codex-2026-10-08/resize-call-profile/` and `resize-coalescing/`.

| Linux AMD64 isolated resize profile | Baseline | Initial candidate |
| --- | ---: | ---: |
| Median direct command latency | 101.15 ms | 1.76 ms |
| XSync calls, whole process | 1,072 | 56 |
| XGetGeometry calls, whole process | 1,041 | 26 |

Total requests and run durations differ because each run completes a fixed sample
count; absolute call counts are not a fixed-work speed ratio. Emulated timings,
short samples and concurrent host activity limit interpretation. They support
the next qualification step, not a physical-desktop performance claim. No
percentile or timing ratio is a pass gate, and no elapsed-time budget was tuned
around these samples.

## Focused checks and final source

The final overlay contains ten exact code/test files. Its source fingerprints,
complete native archives, object/executable identities and exact outcomes validate
in `resize-coalescing/focused-validation-summary.json`.

Four resize queue function cases cover latest partial dimensions, eleven event
barriers with preserved contents/order, budget/snapshot continuation and empty
or ineligible requests. Eight queue cases preserve timer/FIFO/recursive cleanup
behavior. These pass on Linux ARM64, both BSD ARM64 targets and scoped ASan/UBSan
with leak detection enabled. The earlier insertion-only six-target checks remain
valid for the unchanged queue owners.

Final actual-core checks pass on Linux ARM64 and AMD64, FreeBSD ARM64 and OpenBSD
ARM64, plus the scoped sanitizer core profile. Each runs three configure modes
(AutoDetect, UseGravity, IgnoreGravity), three scheduler controls and an isolated
resize latency profile with sixteen direct/scheduled samples. Both BSD checks
use their default Xorg/Xenocara server, with recorded 800×600 root dimensions.
Owned-server startup/termination guards remain in place.

Linux ARM64 regular/scoped profiles also pass seven real geometry cases at scale
1.5, including pointer interactions, configure feedback and restore behavior.
Scoped actual-process leaks are disabled, separately from the leak-enabled
policy checks. No final core failure or widened deadline occurred.

The initial overlay and measurements remain historical evidence. The final delta
changes only `fvwm/events.c` to reapply existing dispatch normalization after
coalescing. Unchanged policy owners/tests retain their initial passing evidence;
affected actual-core controls run again on all four target environments and the
scoped profile. Python-generated bytecode is cataloged separately from the ten
declared source identities.

Initial private-header include and inherited probe-link flag failures are
retained. The harness corrections add the accepted header search directory,
remove an unused inherited wrapper and redirect new compiler dependency files to
private output paths. Earlier overlay invocations refreshed scratch compiler
dependency metadata; accepted sources, objects, libraries and executables retain
their identities. Validator path-prefix/filename and bytecode inventory fixes
are collection corrections, not weakened runtime checks.

## Frozen successor and remaining gates

The four-story successor source is frozen as SHA
`005f5eb698d4a2115dc202c80d32c7480ab49f216515c73a539ff285454bb5bd`:
1,409 files, twelve owning paths, based on the separately retained `3074` source.
`builddir/codex-2026-10-08/core-performance-production/source-preflight.json`
records ownership. Only the two new test registrations and manual paragraph are
applied to their frozen predecessor files; unrelated shared-workspace changes
are excluded.

Focused executable overlays use qualified `3c37` objects. They do not qualify
the entire successor or its inherited monitor/Pager changes. Whole-source
compiler, install, optional-feature and four-default-BSD-server gates remain;
BSD AMD64 coalescing coverage is still pending. Extensive runners have not been
prepared or dispatched. The paused predecessor matrices remain paused. Native
Linux release sanitizer CI and physical desktop qualification are separate.

See the [current tracker](../PROJECT-TRACKER.md#current-checkpoint-status) and
[optimization roadmap](../OPTIMIZATION-ROADMAP-2026-10-05.md).
