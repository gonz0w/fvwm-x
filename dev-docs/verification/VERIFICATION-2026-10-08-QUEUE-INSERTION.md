# Ordered scheduler insertion and responsiveness baseline

Tracker owners: PERF-01 / QUEUE-01. Implementation and focused checks pass;
combined source qualification is accepted in the
[2026-10-11 checkpoint](VERIFICATION-2026-10-11-CORE-PERFORMANCE.md). The focused
results below retain their original scope and preparation history.

## Change and algorithm evidence

The two shipped sorted-queue consumers are scheduler insertion and periodic
rescheduling. Both compare immutable deadlines. An unlocked queue can now append
directly when its tail deadline is no later than the new deadline. Equal keys
retain FIFO order. Locked callback traversals keep the existing insertion path,
deferred visibility and cleanup ownership. No new data structure, runtime
dependency, configuration syntax or scheduler budget is introduced.

The baseline queue matches qualified source `3c37`. The candidate owns exactly
`libs/fqueue.c`, `libs/fqueue.h` and `tests/test_fqueue.c`. Exact fingerprints,
sources, comparison probes and complete collected evidence are under
`builddir/codex-2026-10-08/queue-insertion-performance/`.
`validation-summary.json` validates candidate hashes, archive payloads and all
required focused outcomes; it is not a full-project qualification record.

| 2,048 insertions | Baseline comparisons | Candidate comparisons |
| --- | ---: | ---: |
| Ascending deadlines | 2,096,128 | 2,047 |
| Equal deadlines | 2,096,128 | 2,047 |
| Descending deadlines | 2,047 | 4,094 |

The shortcut removes quadratic work for ordered/equal arrivals. Descending
arrivals incur one additional tail comparison per insertion and remain linear.
Arbitrary mixed insertion still uses the existing ordered-list search. These
counts do not establish an end-to-end desktop latency improvement or justify a
timer heap.

## Focused qualification

All six native target environments pass the eight queue function cases and nine
comparison/order measurements: Linux, FreeBSD and OpenBSD, AMD64 and ARM64.
The cases include mixed/equal ordering, locked insertion, nested cleanup, FIFO
transfer, tail reuse and recoverable append ownership. The scoped Linux
ASan/UBSan profile passes the same controls with leak detection enabled.

Each target links the candidate queue into a separate actual core using verified
unchanged `3c37` objects/library. Accepted source and executables are not replaced.
All six pass three scheduler integration controls: nested Wait exactly once,
group cancellation/periodic stop and 256 overdue jobs in order with independent
module progress. Linux uses nested Xvfb; all four BSD environments use their
default Xorg/Xenocara server. Root dimensions, executable/object identities and
logs are retained. This is an object-reuse focused overlay, not a clean build.

The first FreeBSD/OpenBSD ARM64 core attempts failed because the earlier test
server had been stopped. Their archives remain. The corrected harness explicitly
checks server ownership, starts its own server and terminates only that owned
process. All four corrected BSD profiles pass without queue changes or widened
deadlines. Initial validator path-prefix failure is also retained in session
output; normalization of archive-relative names fixes that collection issue.

## Responsiveness observations and remaining work

Each actual-core target completes idle, title and resize workloads at a requested
1,000 X requests/second, collecting 32 direct-command and 32 scheduled-10-ms
samples per workload. Raw samples, achieved load rate and available CPU data
are retained. No five-second starvation deadline fails. An initial same-scope
Linux ARM64 `3c37` baseline is also retained.

Resize floods show substantial delays in the emulated AMD64 guests: candidate
direct-command medians are about 162 ms on Linux AMD64 and 355 ms on OpenBSD
AMD64. These short virtualized samples warrant call/request profiling. They
do not isolate the cause, measure physical hardware, or prove a before/after
queue latency improvement. No latency percentile is used as a brittle pass gate.

Next, measure resize-path X calls and barriers before changing rendering or
event ordering. Freeze related performance stories into one combined checkpoint;
full compiler/install/optional-source gates remain pending for this queue change.
Native Linux release sanitizer CI and physical desktop coverage remain open.

See the [current tracker](../PROJECT-TRACKER.md#current-checkpoint-status) and
[optimization roadmap](../OPTIMIZATION-ROADMAP-2026-10-05.md).
