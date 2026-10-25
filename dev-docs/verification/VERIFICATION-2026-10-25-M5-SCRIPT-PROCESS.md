# M5 Script subprocess ownership

Status: focused checks and independent source review pass; integration pending.
Owning slice: SCRIPT-01 #4, DATA-01 #6, FD-01 #7, PROC-01 #15 and CLEAN-01 #34.
Base `0318fbbf51ed0548833b111d396133afd8fe1ddd` is authorized merged PR230;
canonical signature/DCO and exact reviewed tree verified. #31 closed Completed,
its Project status is Done and the merged branch is retired. 82 issues remain
open after that closure; no broad owner is closed by this finite subprocess slice.

## Behavior and ownership

GetOutput checks the existing module termination flag before construction,
through output/completion waits and before publishing success. Cancellation
returns ECANCELED, clears partial output and uses the existing owned-child group
TERM/grace/KILL/reap path. The child stays unreaped until group signals finish;
successful reap or ECHILD relinquishes its identity before any cleanup signal.

A private close-on-exec pipe now carries errno for child signal reset, group,
stdout duplication/flags, signal restoration or shell exec failure. Successful
exec closes the writer; a successfully executed shell's nonzero status, including
127, still permits stdout. Failure cannot publish an empty-success cache entry.
Common unwind closes owned endpoints, frees output, preserves primary errno
and makes at most two signal-mask restoration attempts when required.

Keep trusted configured shell strings, the 32,000-byte stdout prefix, NUL
rejection, empty-success output, the fixed command deadline and cache semantics.
The caller still retains its prior successful cache when a replacement fails;
no new cache framework, async execution, dependency or configuration migration.
A 100ms polling ceiling bounds the flag-check/wait race; after both pipes close,
child completion retains its 10ms cadence. The existing 100ms cleanup grace,
kernel scheduling/reaping and individual system calls prevent a hard wall-time
claim. Detached descendants and successful commands that relinquish background
work remain outside owned-group cleanup. Persistent OS refusal to restore a
valid mask cannot be repaired reliably; the primary failure is retained.

## Before/after controls

Private evidence: `builddir/codex-2026-10-25/m5-process`. The old real module
fails the 0.8-second exit control after SIGTERM while its shell and descendant
are running. Initial fixture scripts lacked a valid case and used a literal
WriteToFile filename where the grammar requires a variable; both parse failures
are retained and earn no runtime credit. Correct only the fixture before the
actual failing cancellation control. Final readiness is written after the
background child launches, so descendant cleanup is exercised deterministically.

Two private fault probes link the actual old helper and current helper:

| Control | Old helper | Current helper |
| --- | --- | --- |
| Shell execl refused with ENOENT | Success with owned empty text | Failure, ENOENT, NULL output/zero length |
| One-shot parent mask restoration refusal | Failure leaves SIGTERM blocked | Failure restores prior mask, NULL output/zero length |

These are injected local constructor failures, not a vulnerability or external
attack claim. Probe sources, commands and original outputs are retained.

## Combined focused acceptance and repairs

Reuse the owned Linux ARM64 normal GCC and scoped Clang ASan/UBSan incremental
profiles; build only Script and affected process/fault/output tests. Each profile
passes all three affected registered units and the three-case actual-module X
fixture: open stdout termination, closed stdout termination and cache retry.
The normal units span retained initial output success plus the two repaired
process/fault passes; no unchanged output test repeats. Normal termination cases
also retain their earlier passes; only the failed cache case repeats.

The fault unit executes 28 constructor refusals with fd counts, prior-mask and
reap/retry checks: both pipes, all four endpoints' CLOEXEC operations, nonblocking
setup, signal operations, fork, parent/child grouping, stdout duplication/flags
and exec. It also passes two low-fd stdout aliases and actual fd>=4096 pipes. Retained
per-test stdout is empty in both profiles (the unavailable branch prints an
explicit skip); the lab descriptor limit is 524288/524288. This distinguishes
actual execution from the permitted low-hard-limit skip.
Process tests retain prefix/NUL/empty/nonzero-output, deadlines, unrelated EINTR,
pre-spawn cancellation, descendant markers and recovery after cancellation.
Pure/process units enable sanitizer leak detection; actual-module startup retains
the existing leak-disabled scope. No sanitizer diagnostic occurs.

Normal open/closed termination takes 0.127/0.123s; scoped sanitizer takes
0.124/0.121s. The delayed descendant marker stays absent. The cache fixture
verifies exactly old::old:recovered and one original command execution: failed
partial output neither replaces the old cache nor suppresses a successful retry.
These are fixture observations, not general performance or hard real-time claims.

The first combined normal gate fails two checks. The initial implementation
used fvwmPoll; this existing configuration defines HAVE_SIGLONGJMP but not
HAVE_SIGSETJMP, so its legacy longjmp fallback leaves the first cancellation
signal blocked in the recovery test. Keep the correction local: raw poll with
bounded flag checks, without changing shared signal wrappers or build detection.
The new fault test initially wrapped fcntl while glibc large-file objects import
fcntl64. Match the established project wrapper aliases and wrap both symbols;
do not weaken assertions. Only the two failed normal units repeat and pass.

The first X run passes both termination cases, then fails the cache assertion.
Fake native config packets must use the core-normalized directive (no colon),
and WriteToFile retains section framing with leading/trailing empty lines.
Correct the fixture to the existing wire/file contracts, preserve original
failures and repeat only cache. Its first exact-framing correction still misses
the leading empty line; retain that failure too. Final cache passes. Remaining
previously unrun probes and sanitizer checks then pass without repeating the
normal termination cases or full matrix.

Six changed native inputs match host/guest hashes and modes. Final module hashes,
commands/statuses, split retained passes and exact source are recorded in the
[machine inventory](VERIFICATION-2026-10-25-M5-SCRIPT-PROCESS.json). The canonical
private archive was read through every payload before recording its SHA.
Separate review clears both production and the focused polling/wrapper correction;
final fixture/evidence review verifies source parity, recorded outcomes, fault
counts and high-fd execution without another native run.

The like-profile normal Script file grows from 2,206,896 to 2,209,168 bytes
(+2,272 including debug metadata). This is the cost of explicit error/cancellation
ownership, not a speed or memory-use claim. Allocated-section details are retained.
The [due periodic comparison](PERFORMANCE-2026-10-25-M5-PROCESS.md) reuses unchanged
PR204 browser bytes and adds no benchmark build; timings show variation only.
The new checkpoint resets that independent cadence to 0/5 on merge.

## Remaining scope and verification limits

This addresses the named child-lifetime/actual-constructor exclusion in
[Script bounds](VERIFICATION-2026-10-04-SCRIPT-BOUNDS.md) for GetOutput only.
Broader Script resource lifetime/budget disposition, other native helper/session
constructors, shared readiness/interaction/unwind and M6 Perl retirement remain
with their owners. Do not reopen accepted Script text, geometry, storage,
message, modal or M2/M4 behavior. Next M5 pickup follows the
[maintenance register](../decisions/MAINTENANCE-FIRST-BATCHES-2026-10-23.md).

No UI/rendering, installed entry point, dependency, native packet ABI or OS API
contract changes; no theme screenshots, installs, BSD boots or full matrix are
claimed. Prior accepted evidence remains applicable where source is unchanged.
These are local Linux guest checks, not new native-CPU/hardware/release acceptance.
Requested Astra/high analysis, Sol/high implementation and separate Sol/high
review follow local routing; runtime model metadata is unavailable/unverified.
