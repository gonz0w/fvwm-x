# Optimization and architecture review

Recorded 2026-10-05. The [project tracker](PROJECT-TRACKER.md#execution-order)
owns priority, status and execution order. This document expands its items;
it does not establish another queue. Runtime implementation belongs to the
build thread. This review changed documentation only.

Effort ranges are engineer-days including focused verification, not delivery
dates or additive totals. Recheck findings against the build thread's latest
checkpoint before implementation. Reuse existing policy, interaction, module
queue and property helpers rather than introducing parallel owners.

## Placement and dependencies

1. Preserve the current bounded SCRIPT-01 checkpoint and existing security,
   dependency and maintenance scheduling. Under INPUT-01, include the
   button-release wait in the next appropriate interaction batch. Under
   STACK-02, run a focused layer-state correctness check during order 2;
   confirmed defects follow their risk, otherwise fold the result into
   STACK-01. Neither item requires completing a whole event-loop rewrite.
2. At execution order 4, capture a baseline, then take QUEUE-01 before
   OUTPUT-01 and the remaining PERF-01 tuning. Fixing queue complexity first
   avoids tuning budgets around accidental quadratic work. Integrate output
   budgets into the existing event-loop policy and module lifecycle.
3. Establish validated local window identity lookup under STACK-01 before
   XIO-01 skips known frames. Atom batching can proceed independently. Replace
   transient closure passes only after the identity/correctness checkpoint.
4. Use DRAW-01's verified dirty-state and ordering contract before STYLE-01
   changes update planning/server grabs or STATE-01 coalesces derived properties.
   INPUT-01's relevant lifetime/interaction fixes remain gates for moving
   work across event boundaries. Do not require every unrelated audit to finish.
5. CACHE-01 follows the relevant IMAGE-01 ownership/key/reload checkpoint and
   measured lookup evidence. It does not require a decoder or library rewrite.
   LOOP-01 consolidates readiness after INPUT-01/OUTPUT-01 have specified their
   semantics; do not implement competing wait/readiness owners in those fixes.
6. CLEAN-01 accompanies each owning change. A narrow XCB prototype under
   XIO-01 comes only after Xlib round-trip measurements and the LIB-07 package
   baseline; adding or migrating a dependency needs its own review. Keep new
   desktop primitives at their existing later placement.

## New backlog details

| ID | Priority / evidence | Scope, acceptance and effort |
| --- | --- | --- |
| QUEUE-01 | P1 / locally qualified | Explicit O(1) unlocked head transfer now serves module-output and scheduler/event FIFO drains. Generic callbacks, locks, tombstones and destruction retain their existing owner. Mixed insertion, tail reuse, nested mutation and linear drain work pass. Baseline front-removal cleanup visited 32,004,000 records for 8,000 entries; new transfer visits zero. See the combined FIFO/stack/output report. End-to-end remeasurement remains PERF-01 work. |
| OUTPUT-01 | P1 / locally qualified | Approved 64-KiB/32-packet/128-attempt ordinary asynchronous service bounds preserve partial offsets, packet order, accounting, writable continuation and death-row lifetime. Pure policy, controlled actual-core and fast/delayed real-pipe tests pass with Linux/BSD integration. Synchronous ordering remains intact; OUTPUT-02 separately qualifies the approved total deadline. See the combined FIFO/stack/output report. |
| STACK-02 | P1 / locally qualified | Per-window EWMH layer publication and title-highlight identity are corrected in new_layer. Parent/child invocation, focused decorations, sticky/unrelated state, actual stack order and destroyed-parent survivors pass on Linux/BSD and default servers. Baseline child ABOVE/BELOW states were stale. STACK-01 identity/traversal optimization remains separate. See the combined FIFO/stack/output report. |
| STACK-01 | P1 / identity, arithmetic and traversal locally qualified; storage batch locally qualified | Shared X-context identity, invoked-target parent guards, widened ranks, stable ring merging and parent-only worklist closure are locally qualified. Operation-local deep-parent continuation, initialized lists and Layer ownership are locally qualified too. The qualified storage checkpoint includes checked unmanaged storage, bounded ordinary/full restacks and suppressed-icon sibling repair. Group-free closure and proven dead-source removal pass focused checks; callback ownership work is in progress. Retirement/lifecycle work remains queued. [Tracker](PROJECT-TRACKER.md), [traversal](verification/VERIFICATION-2026-10-07-STACK-TRAVERSAL.md), [storage batch](verification/VERIFICATION-2026-10-07-STACK-BATCH.md). |
| STYLE-01 | P2 / style-command ownership checkpoint passes; broader work open | Temporary UseStyle/DestroyStyle selectors are operation-owned; missing Style/FocusStyle/WindowStyle input is refused before scheduling a new pass. [Current report](verification/VERIFICATION-2026-10-19-STYLE-COMMAND-OWNERSHIP.md). PR #116 update work is merged; larger plans/grabs remain separate. |
| CACHE-01 | P2 / source-confirmed lookup ordering; benefit unmeasured | `libs/Picture.c:PCacheFvwmPictureAtScale` resolves paths before lookup and can check file stamps before rejecting cheap variant-key mismatches. Keep the existing cache. Profile path/stat counts; compare cheap key fields before filesystem work where equivalent. Preserve path, file-stamp, image attributes, rational scale, target dimensions and generation semantics. Test explicit reload, changed/missing files, multiple variants, scale transitions and resource lifetime. Consider an index only if cache population justifies it; no indefinite filesystem cache. Coordinate IMAGE-01 and LIB-04 without duplicating decoder work. 1–3 days initial optimization; larger indexing separately justified. |
| STATE-01 | P2 / atom checkpoint merged; broader coalescing remains open | [PR #112](verification/VERIFICATION-2026-10-19-EWMH-ATOMS.md) adds duplicate-ID lower bounds, scoped/ALL name indexes and startup-vector reuse, retaining publication/query points. Earlier client snapshots/work-area aggregation remain accepted. Further burst/coalescing requires measured invalidators and DRAW-01 barriers without a second state authority. |

## Extensions to existing owners

- **OUTPUT-02:** a bounded actual-core observation shows repeated harmless
  module commands extend a configured one-second synchronous wait to 2.049
  seconds. A separate [deadline proposal](decisions/MODULE-SYNC-DEADLINE-PROPOSAL.md)
  keeps the 30-second default/positive override. The user approved one monotonic
  total output/unlock deadline on 2026-10-07. Implementation and combined qualification with DATA-01 pass across Linux/BSD,
  default servers, optional profiles and scoped sanitizers; native release CI
  remains separate.
  OUTPUT-01 frozen qualification remains historical evidence.

- **INPUT-01:** common final button-release waits now replace repeated
  pointer-query/microsecond spinning with shared audited readiness and 20-ms
  monotonic fallback/cursor timing. Their combined qualification is recorded
  in the [release report](verification/VERIFICATION-2026-10-06-BUTTON-RELEASE.md).
  The user-approved active viewport follow-up reuses the 100-ms interaction
  wait, 32-event coalescing and checked masks. Eight focused checks pass,
  including actual RandR and lifecycle events deferred until unwind. The
  separate viewport checkpoint is locally qualified. Approved menu readiness,
  motion fairness and monotonic submenu timing are locally qualified under
  their own frozen combined checkpoint. See the
  [viewport report](verification/VERIFICATION-2026-10-06-VIEWPORT-SERVICE.md).
  Wider modal/resource lifetime and arbitrary mutation handling remain open.
  The saved CPU samples are functional controls, not benchmark conclusions.

- **PERF-01:** collect an initial baseline, then remeasure after QUEUE-01 and
  OUTPUT-01 before choosing elapsed-time budgets or a timer heap. Retain the
  completed dequeue-before-dispatch, count budgets and batched-flush work.
  A budget cannot preempt a blocking handler.
  The [2026-10-08 performance batch](verification/VERIFICATION-2026-10-08-CORE-PERFORMANCE.md)
  records ordered insertion, isolated profiler ownership/identity and bounded
  adjacent AutoDetect resizing. The [combined qualification](verification/VERIFICATION-2026-10-11-CORE-PERFORMANCE.md)
  passes all six targets, full-source/install/default-X and optional/scoped
  gates. Existing synchronization barriers are retained; broader PERF-01 work
  and physical latency checks remain open.
- **DRAW-01:** the [window-update contract](decisions/WINDOW-UPDATE-BARRIERS-2026-10-15.md) records current dirty-state ownership, flush points and ordering barriers
  once. STYLE-01/STATE-01 consume that contract instead of creating independent
  deferred-update systems. Preserve resource generations and final geometry.
- **LOOP-01:** share descriptor readiness, owner lifetime, teardown and deadline
  handling across normal dispatch and interactions. Existing interaction poll
  and main-loop select code are consolidation candidates. First preserve the
  INPUT-01/OUTPUT-01 behavior; test high descriptors, timeout, EINTR, nested
  interactions and removal during dispatch. Portable poll is sufficient unless
  actual profiles demonstrate a need for a different backend.
  Auto and Ident now reuse the existing cancellation-aware poll wrapper in
  focused-qualified module checkpoints; their wider qualification is tracked
  separately. Remaining module owners have different contracts: Pager's
  `My_XNextEvent` checks queued X events first and returns one event before
  servicing a packet; IconMan carries a tooltip deadline through `FTipsCheck`;
  Buttons includes borrowed session-manager watches and a 10-ms pending-work
  fallback through `fsm_fdset`/`fsm_process`. Preserve these contracts with
  actual module controls before changing each owner. Pager's disabled quit body
  is a cleanup candidate, not evidence of an active failure. Do not replace all
  three loops mechanically or introduce a competing module transport.
- **XIO-01:** `stack.c:is_above_unmanaged` queries the root tree then serially
  requests attributes for children above the target: an X11 N+1 path. Use
  STACK-01's validated local identities to skip known managed frames only when
  their classification is owned and current. Preserve unmanaged/override-
  redirect, InputOnly, hidden-frame and destroyed-window behavior; do not assume
  every X context denotes a client. Count calls/replies under relevant stacking
  operations before claiming latency gains. Also batch independent core/EWMH
  atom tables with `XInternAtoms` and remove only individually proven redundant
  `XSync` barriers. Atom batching is roughly 0.5–1 day; managed-query reduction
  2–4 days after the identity contract. No whole-core XCB migration is approved.
- **CLEAN-01:** extract boundaries within owning fixes: window identity/lifetime,
  pure geometry/stack/style policy, explicit X reads/commit writes, and shared
  scheduling/interaction readiness. Use typed operation-local state rather than
  new globals or typeless scratch pointers. Document whether a helper can wait
  for a server reply, retain borrowed references or run mutation callbacks.
  Avoid a generic replacement WM framework or mechanical folder/API rewrites.
- **LIB-05/LIB-07 and XIO-01:** retain Xlib, Xft, XRender and XRandR for their
  existing jobs. A narrow asynchronous XCB query experiment is optional after
  profiles demonstrate remaining serial-query cost. Keep a single authoritative
  connection/event consumer and Xlib event ownership; bound outstanding replies
  and handle BadWindow, cancellation and resource lifetime. Prove six-target
  package/build compatibility and measured benefit before dependency adoption.
  Do not add mandatory xcb-util, GTK/Qt/Cairo or a new event library merely for
  modernization. No private Xlib internals or casual event-ownership toggling.

Xlib writes may already be buffered; reply-producing reads are different from
`XFlush`, which sends queued output, and `XSync`, which waits for processing.
XCB only reduces serial waiting if requests are issued before replies are
collected. Primary references: [Xlib manual](https://www.x.org/releases/X11R7.5/doc/libX11/libX11.html),
[X.Org Xlib/XCB guide](https://www.x.org/guide/xlib-and-xcb/),
[Xlib/XCB interoperability](https://wiki.freedesktop.org/xcb/MixingCalls/).

## Evidence and verification contract

The queue probe used an instrumented copy of `libs/fqueue.c`, counting visits
inside cleanup while repeatedly removing FIFO heads. Source SHA-256:
`33f90046d4316bb2ce808f1c62c803e574a3b190c4926ae3288141774ed801b6`.
The probe ran on native macOS; these counts are algorithmic evidence, not
desktop latency or native Linux performance evidence.

| Entries drained | Cleanup visits |
| ---: | ---: |
| 256 | 32,896 |
| 1,024 | 524,800 |
| 4,096 | 8,390,656 |
| 16,384 | 134,225,920 |

Visits equal `N * (N + 1) / 2`. Clearing the dirty flag alone does not remove
the per-pop scan. Reproduce the probe as an appropriate native regression or
benchmark in QUEUE-01; temporary review files are not release/test artifacts.

Other complexity/query findings are static observations or stated hypotheses,
not completed runtime benchmarks. Retain baseline workload and exact source
fingerprints. Measure native Linux CPU, allocations, X queries/writes and
event/service latency distributions plus maximum observed service time.
Vary window/style counts, transient depth, module reader speed and cold/warm
image variants; preserve ordinary small-desktop cases as well as stress cases.

Pure policy requires native unit coverage; X-visible behavior requires nested-X
Linux integration and relevant Linux/FreeBSD/OpenBSD AMD64/ARM64 gates. Keep
native Linux sanitizer execution under VERIFY-01 and compatibility under
COMPAT-01. Emulated timings are not proof of native desktop responsiveness.
Assert ordering, ownership and bounded work rather than brittle timing ratios.

Do not optimize already necessary fan-out, blindly hash ordered wildcard lists,
add unbounded reply/cache queues, or remove synchronization solely because its
name appears in a search. Cache invalidation, packet ownership and X window
destruction remain correctness/security constraints throughout.

## FIFO/stack/output checkpoint update — 2026-10-06

QUEUE-01 now uses explicit O(1) unlocked FIFO transfer with preserved callback
cleanup. OUTPUT-01 implements the approved 64-KiB/32-packet/128-attempt ordinary
asynchronous bounds and writable continuation. STACK-02 publishes per-window
state/highlight identity. Their [combined report](verification/VERIFICATION-2026-10-06-FIFO-STACK-OUTPUT.md)
locally qualifies all three; the table’s baseline descriptions explain why the
work was selected. Native release CI remains separate. STACK-01’s index/lifetime
measurement can follow, and OUTPUT-02’s approved total deadline is locally qualified; OUTPUT-03’s distinct
startup default was approved on 2026-10-07.

OUTPUT-03 separately implements the approved finite default for `ModuleSynchronous` startup
barriers using the current ModuleTimeout, preserving positive Timeout overrides
and asynchronous continuation after timeout. See its [proposal](decisions/MODULE-STARTUP-DEADLINE-PROPOSAL.md); the exact combined identity/startup checkpoint is locally qualified; native release CI remains open.

## 2026-10-07 runtime checkpoint

Independent atom batching, approved 32-record expose collection/merging and
first-icon WM_STATE identity are locally qualified in the exact 1291-file
[combined checkpoint](verification/VERIFICATION-2026-10-07-ATOM-EXPOSE.md).
Broad dirty-state and query/barrier work remains queued. STACK-01 now has a
finite parent-cycle control and a private guard candidate; the
[Layer fallback decision](decisions/TRANSIENT-PARENT-CYCLE-PROPOSAL.md) precedes
production activation. Native release CI and physical coverage remain separate.

2026-10-07 stacking/query batch: both invoked-window cycle choices are approved
and activated, with finite hint validation and highest-override query reduction.
Eight focused Linux checks pass. The exact 1301-file combined checkpoint is
locally qualified (1144 standard entries, 120 installed identities, 72 default BSD
X invocations, optional 148/128 and 27 scoped sanitizer checks); earlier atom/expose evidence remains scoped to its own source. Reverse
closure, deep acyclic recursion and further barrier/query work remain queued.

## Module popup readiness — 2026-10-08

Auto's shared poll/termination contract is locally qualified in consumer1361.
Ident and Pager now share cancellation-aware two-descriptor readiness while
preserving their X-event/packet order; their combined focused checks pass. Pager
also advances empty configuration lines and drops its disabled quit body.
Buttons (borrowed session readiness and timeout fallback) and IconMan (tooltip
timing) remain module fd_set owners requiring separate contracts. Core readiness
consolidation and nonblocking ordinary module transport are not complete.
See the [popup report](verification/VERIFICATION-2026-10-08-POPUP-READINESS.md).

IconMan now preserves FTipsCheck deadlines through common two-descriptor poll
and observes termination in X drains; focused normal/scoped/native checks pass.
Buttons adds bounded borrowed-session poll snapshots while retaining fsm admission
and callback ownership plus the 10-ms pending fallback. Normal controls pass;
scoped/native checks are pending. Script still has a separate select owner. These
changes are outside the frozen popup checkpoint; see the [Buttons report](verification/VERIFICATION-2026-10-08-BUTTONS-READINESS.md).

Script readiness now joins the focused IconMan/Buttons successor through common
poll, bounded borrowed-session snapshots and monotonic periodic policy. Idle
pending session work no longer needs PeriodicTasks. A tested Swallow placeholder
ownership repair accompanies this batch. Wider service budgets, Script packet
payload admission and whole teardown remain separate work; see the
[combined checkpoint](verification/VERIFICATION-2026-10-08-MODULE-READINESS.md).

## 2026-10-14 picture-cache lookup

CACHE-01’s measured variant-filter/reload checkpoint is locally qualified.
A 65-variant lookup now checks one file stamp instead of 65. Eight standard compiler profiles across six targets pass 1936 entries; optional profiles add 423, default BSD servers pass 12 cache/staged-WM fixtures, and instrumented cache checks pass. 150 installed code identities and 99 catalogs validate. Broader profiling and release/hardware gates remain open.
See the [combined report](verification/VERIFICATION-2026-10-14-PICTURE-CACHE.md).

## 2026-10-14 core readiness

LOOP-01’s native poll/deadline, shared pinned snapshot and actual-core coverage
checkpoint is locally qualified. Typed native poll snapshots and existing death-row pins now serve ordinary/interactive core waits. Eight standard compiler profiles pass 1944 entries across six targets; optional profiles add 425, default BSD servers pass 16 core/cache/interactive geometry fixtures, and two instrumented checks pass. 150 installed code identities and 99 catalogs validate. Broader descriptor/runtime release gates remain open.
See the [combined report](verification/VERIFICATION-2026-10-14-CORE-READINESS.md).

## 2026-10-15 synchronous module waits

PR #96's shared ordinary/interactive core snapshot is merged. LOOP-01 next
consolidates the unlock, blocked-output and startup waits under native poll and
shared absolute-deadline conversion. Combined qualification is locally accepted;
Full-suite and final refinement evidence are recorded separately. Existing
constructor/session admission and broader runtime/release audits remain
separate. See the
[module-wait report](verification/VERIFICATION-2026-10-15-MODULE-WAITS.md).

The wildcard checkpoint merged in PR #103 prepares minimum byte lengths, anchored literal
suffixes and direct single-star matches. Lengths are derived lazily once per
identity field within the synchronous lookup/mark operation. Escaped and general
multi-star patterns retain the original matcher. The three-item checkpoint is locally qualified;
[combined qualification](verification/VERIFICATION-2026-10-16-STYLE-WILDCARDS.md)
records exact full/corrected source scopes and retained failures. This does not
alter rule order, update plans or server grabs.

## 2026-10-16 repeated-ID simplification

After merged PR #103, STYLE-01 batches cached immutable ID fingerprints,
conservative earliest-occurrence scan bounds and explicit-selector disjointness.
Actual-core workloads reduce ID-equality strcmp calls, unrelated prefix visits
and surviving probe rules while preserving outcomes. One combined six-target
checkpoint is locally qualified; general wildcard costs, lookup indexes, update
plans and narrower grabs
remain separate. [Evidence](verification/VERIFICATION-2026-10-16-STYLE-SIMPLIFICATION.md).

## 2026-10-18 unescaped wildcard families

The three-family checkpoint in PR #108 completes the style-specific unescaped
paths: exact-width question spans, anchored one-star question spans and general
constant-stack cursors. Escaped style patterns and other users retain unchanged
`libs/wild.c`. The legacy predicate remains an independent oracle, including its
empty-tail semantics. Two inline masks add eight bytes on the measured ABI;
general cursor retries can revisit spans, so no universal linear-time or latency
claim follows. One combined checkpoint serves all three families; exact original
results and remaining gaps are in the
[report](verification/VERIFICATION-2026-10-18-STYLE-CURSORS.md).

Earlier paragraphs describe their dated checkpoints. Ordered candidate and lazy
subject work are accepted in PRs #105/#106. Remaining update plans and narrower
grabs require measured work, invalidators, ownership and observer contracts.
`check_window_style_change` mutates flags and deletion state; moving it outside
its current owner is a separate change requiring proof and revalidation.

## 2026-10-18 selector disjointness

PR #108 is merged. PR #109 adds exact/minimum byte lengths, fixed-position
literal conflicts and literal membership in unescaped star patterns.
Three selector proofs reduce three selected lists from three records to two, preserving reference/collision/matching/removal owners. Final bounded source passes 40/40 affected entries plus four scoped sanitizers, nine fresh stages and four authenticated BSD displays; original broad source passes 1991/1992 standard and 437/437 optional entries. The unchanged OpenBSD AMD64 SVG startup failure and its fresh-install/staged-geometry gap remain under IMAGE-01. No universal latency claim. [Report](verification/VERIFICATION-2026-10-18-STYLE-OVERLAP.md).

Optional extra proof work has a conservative per-field budget; larger pairs
remain unknown without limiting ordinary matching. Proof helpers add no retained
metadata or heap storage; eligible merges use existing copy/free paths. One
broad checkpoint and one affected correction keep exact source scopes. Earlier
paragraphs remain dated history; the tracker owns current selection.

## 2026-10-18 escaped style matching

PR #109 is merged. Three escaped matching families replace recursive entries with decoded fixed widths, anchored one-star spans and constant-stack general cursors. Nine selected legacy calls become zero with unchanged results/removal. 1991/1991 executed standard and 437/437 optional entries pass; one known required OpenBSD AMD64 SVG test stays omitted under IMAGE-01, preserving its install/staged-geometry gap. Four scoped sanitizers, nine fresh stages, four authenticated BSD displays and 810 family rows pass. Metadata adds sixteen inline bytes; no universal latency/linear-time claim. [Report](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED.md). One source-bound checkpoint serves the three related stories in PR #110. No shared-glob replacement or X/update owner change is implied. Earlier paragraphs remain dated history; the tracker owns current selection.

## 2026-10-18 escaped selector disjointness

PR #110 is merged. Three escaped selector proofs use decoded lengths, quoted fixed positions and bounded literal membership. Three selected lists retain two records instead of three, preserving initialized lookup, precedence and removal. Original breadth passes 1991/1991 executed standard and 437/437 optional entries; the final literal-fast-path correction passes 40/40 affected entries plus four scoped sanitizers. Nine final stages, four authenticated BSD displays and 290 final client cases pass. One required OpenBSD AMD64 SVG test stays omitted under IMAGE-01 with its install/staged-geometry gap. No metadata/allocation owner change or universal latency claim. [Report](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED-OVERLAP.md). One broad matrix and affected-only review correction serve PR #111. No shared-glob replacement or X/update owner change is implied. Earlier entries remain history; the tracker owns current selection.
