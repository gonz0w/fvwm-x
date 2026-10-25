# M5 Script message ownership and stacking reconciliation

Status: focused checks and independent review pass; maintainer integration
is pending. Base:
`1f80945c91e26cf39ece071ba3b338dba683f964` (merged PR #229). Canonical merge
signature/DCO and exact reviewed tree `ccc3b70d1b2fe264a636eb42fa2d2611ef754dfb`
were verified; #183 closed and its Project status is Done. This checkpoint
owns SCRIPT-01 #4 / CLEAN-01 #34 message teardown and STACK-01 #31 reconciliation.

## Finite behavior and scope

Preserve the 40-message outgoing cap, per-target FIFO and literal message bytes,
selection refusal/target echo and omitted-property fallback. Retain selection
ownership during one monotonic two-second shutdown grace period; interruptions
and unrelated requests cannot restart it. Clear remaining owned queue strings
once on exit; an empty/drained queue must not incur a fixed two-second sleep.
The client-loop deadline cannot preempt a blocking Xlib/server operation.

No configuration syntax, core dispatch, installed dependency or UI presentation
changes are intended. Whole-Script memory/font budgets and subprocess lifecycle
are later finite owners; #4/#34 are not closed by this slice. No screenshots or
new theme comparisons are needed for nonvisual queue/lifetime behavior.

## Before-change controls

Private evidence: `builddir/codex-2026-10-25/m5-streams`. The existing owned local
Linux ARM64 container and incremental GCC binary are reused. The preserved
baseline Script SHA-256 is
`5e7458ee144e8a9fc36f29c35d09d2c95e40d5b7dd76f22f30875255af77d8ee`.

The first silent-peer fixture ended at an incorrect final-owner assertion: the
X server clears selection ownership when the owning connection closes, even if
the selected window was root. Correct the fixture to expect None and establish
40 actual peer wake requests before Quit. That corrected silent control passes
the old binary; no indefinite-hang reproduction is claimed. The old loop's
source still permits timeouts without advancing its retry counter and spins on
queued unrelated events. The old binary fails delivery after Quit at the first real message request and
fails the 1.5-second empty-queue exit expectation, consistent with its fixed sleep. Original failures and runner corrections are retained.

## Combined focused acceptance

One reused Linux ARM64 GCC incremental profile and the existing scoped Clang
ASan/UBSan profile both compile the affected module/helpers. Each passes three
registered units (`script-message`, `script-delay`, `script-packet`), the five-case
real-X message fixture and the retained eight-group packet-admission fixture.
Pure sanitizer units enable leak detection; the actual module fixture retains
its existing process-startup leak-disabled scope. No sanitizer diagnostic occurs.
These are local Linux guest executions, not native-CPU release qualification.

The queue unit checks first/middle/last removal, duplicate-target FIFO, unmatched
refusal, invalid/full admission, caller ownership on refusal, reused capacity,
cleared tail aliases and repeated cleanup. Existing delay tests gain exact
shutdown boundaries, rounding, distant endpoints and fixed-start checks.
Real-X controls admit 40 of 41 messages, deliver all 40 literal values in order,
exercise both explicit and omitted-property replies and target-echo refusal,
then verify cancellation, empty exit and final server ownership clearing.

Normal/ASan silent exits take 2.021/2.028 seconds; drained exits take
0.062/0.112 seconds and empty exits 0.051/0.056 seconds. These are fixture
observations, not a general performance benchmark or consumption acknowledgement.

Independent review found the first refusal test allowed a reset deadline to
pass: requests stopped at 1.5 seconds but exit was allowed until four seconds.
Tighten only that case to three seconds (one second of scheduling allowance,
not a product timeout). Repeat only this affected X case on both existing
binaries; both pass at 2.049/2.053 seconds after 115/111 refused requests.
No production change, rebuild or wider repeat follows the fixture correction.
The reviewer rechecks and closes the finding; no remaining review findings.

Initial configure accidentally selected system Go 1.22.2 and correctly failed
the existing baseline gate before compilation. Selecting the preserved supported
SDK in PATH fixes the runner; no source or policy weakening. The original log
is retained. `make check lint` passes after correcting one report-link date.
Twelve changed native source/test files match host/guest hashes and modes;
final binary identities and command/status records are in the
[machine inventory](VERIFICATION-2026-10-25-M5-SCRIPT-OWNERSHIP.json).
The private compressed archive was read through every payload before its SHA
was recorded. Unchanged M4/native contracts retain their accepted evidence;
no new full OS, install, feature or theme matrix is claimed or required here.

PR204 remains the periodic comparison reference: 3/5 after PR229, 4/5 on this
implementation merge. No benchmark is due; performance tuning is outside this
owner slice. Requested delegation settings are recorded, not independently
observed runtime model metadata.

## Original-scope reconciliation: STACK-01 #31

A separate read-only audit inspected the captured current issue bodies, accepted
reports, production helpers and registered tests at the merged base. No runtime
checks were repeated for this bookkeeping.

| Original scope | Accepted evidence and retained ownership |
| --- | --- |
| Indexed identity, cycle/arithmetic guards and stable walks | [Identity](VERIFICATION-2026-10-07-STACK-IDENTITY.md), [query](VERIFICATION-2026-10-07-STACK-PARENT-QUERY.md), [arithmetic](VERIFICATION-2026-10-07-STACK-ARITHMETIC.md) and [traversal](VERIFICATION-2026-10-07-STACK-TRAVERSAL.md); `find_managed_client`, `stack_sort.c`, `stack_subtree.c`. |
| Deep-parent continuations, initialized lists and storage refusal | [Parent](VERIFICATION-2026-10-07-PARENT-CONTINUATION.md), [window list](VERIFICATION-2026-10-07-STACK-WINDOW-LIST.md), [Layer storage](VERIFICATION-2026-10-07-LAYER-STORAGE.md) and [bounded batches](VERIFICATION-2026-10-07-STACK-BATCH.md); operation-local parent, window-list/buffer and stack callers. |
| Group handling, callback publication and retirement | [Groups](VERIFICATION-2026-10-07-STACK-GROUP-CLEANUP.md), [publication](VERIFICATION-2026-10-07-RESTACK-PUBLICATION.md), [retirement](VERIFICATION-2026-10-07-STACK-RETIREMENT.md) and [final lifecycle acceptance](VERIFICATION-2026-10-07-MODULE-LIFECYCLE.md); captured identity, owned numeric snapshots, parent client IDs and final outer-boundary publication. |

Permanent parent/subtree/sort/window-list/buffer/batch/restack/retirement fixtures
remain in `tests/meson.build`. The final historical lifecycle checkpoint records
1288 standard entries, 120 installed identities, 116 default BSD-X invocations
and 45 scoped sanitizer checks for its immutable source. These are retained
results, not fresh M5 runs. Subsequent [UI](VERIFICATION-2026-10-19-UI-TYPE-BOUNDARIES.md)
and [core-record](VERIFICATION-2026-10-20-CORE-RECORD-BOUNDARIES.md) include-boundary
changes record unchanged runtime statements and allocated-code/data parity.

The imported “group/retirement queued” sentence is stale: original finite #31
scope is complete and can close through this PR. Its modernization handoffs are
preserved; later work must reuse accepted stacking semantics. Broader release,
native-CPU, hardware and compositor qualification retain their existing owners.

## Remaining named M5 scope

[Script readiness](VERIFICATION-2026-10-08-SCRIPT-READINESS.md) and
[packet](VERIFICATION-2026-10-08-SCRIPT-PACKET.md) explicitly left shutdown
message deadlines open. This slice addresses that exclusion. The
[storage](VERIFICATION-2026-10-05-SCRIPT-STORAGE.md) and
[font/modal](VERIFICATION-2026-10-11-FONT-MODAL.md) reports leave broader resource
budgets outside table-text MemoryLimit. This is scope, not evidence of a new
vulnerability or authority to introduce a global cap.

Next bounded pickup: Script GetOutput constructor-failure unwind, termination
while owning child/descendants, and output/cache failure/retry ownership. The
[Script bounds report](VERIFICATION-2026-10-04-SCRIPT-BOUNDS.md) explicitly leaves
actual helper construction/module-termination coverage open. Preserve accepted
32,000-byte stdout, nonzero-exit output, command deadlines and pinned-child group
cleanup; inspect and reproduce before changing already-correct paths. Shared
readiness/interaction/unwind and Perl retirement remain later M5/M6 slices.

Requested routing followed project policy: Astra/high closure analyst and
Sol/high implementer, with a separate final reviewer. Runtime model metadata is
unavailable, so requested routing is not independent execution verification.
