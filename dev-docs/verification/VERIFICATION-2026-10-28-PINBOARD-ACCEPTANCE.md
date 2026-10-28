# Pinboard acceptance supplement — 2026-10-28

Owner: Cam; DESKTOP-ICONS-01 [#64](https://github.com/gonz0w/fvwm-x/issues/64).
Candidate `codex/r1-pinboard-acceptance`, base merged PR290
`b71f1b0726801ae64a49699d03540ad36d16dff0`. This supplements the
[initial feature checkpoint](VERIFICATION-2026-10-27-PINBOARD.md).
The finite #64 support profiles pass with composed source-bound evidence.
Separate integrated source/evidence review accepts the finite closeout; close
#64 only on the reviewed PR merge.

## Delivered corrections

- An established native board keeps its existing 250 ms retirement reply window
  after unmapping. The initial five-second map deadline no longer cuts it short.
  The old native module fails a delayed Status-after-Retire regression; the
  corrected module answers and actually exits0. Original owners now exit0 in
  all three themes before the unchanged five-second fixture escalation.
- Configuration refresh requests only the instance's stored module directives.
  Common monitor/work-area, desktop, image-path and Colorset records and END
  remain unconditional. The old implementation fails the scoped replay check;
  the corrected implementation preserves both instance and common metadata.
  No core dispatch, command order or synchronization barrier changes.
- An unanswered native query marks loss at its existing five-second deadline
  unless the owner context is cancelled. This avoids stacking a failed Cells
  query, another heartbeat interval and a Status timeout after native-child
  loss. Received malformed or stale-generation responses remain distinct;
  cancellation and conservative unacknowledged-image retention are unchanged.

No new default resident, configuration syntax, mandatory toolkit/database,
filesystem mutation through unpin or automatic desktop-directory takeover.
The [manual](../../doc/fvwm-pinboard.adoc) describes explicit restart recovery.

## Combined verification

These are source-bound, locally virtualized/emulated profile results, not a
fresh all-platform final-binary matrix or complete R1 release qualification.
The three-line Go timeout correction has actual Linux normal/race/minimum-SDK
and native failure-path coverage, plus affected BSD checks. Earlier normal-path
results remain attached to their original source/binary identities; their
received-response paths do not execute the new timeout branch. Separate source
review accepted that applicability under the batched testing policy.

| Profile | Observed result and scope |
| --- | --- |
| Linux ARM64 | Final C/Go full Slate SVG, scoped-installed Dune PNG, enlarged Classic SVG and scoped module ASan/UBSan Slate pass. All use original large real-RandR topology, including actual quarters4/8, reservations, negative origin, unplug/replug, workspace and root fallback. Earlier affected C policies, Go model/race/floor and native ownership/lease checks are reused. |
| Linux AMD64 | Debian13.7 guest, kernel6.12.113; four C policies, three Go packages and controlled SHAPE-query refusal pass. Final C passes the original3840x2160 mixed-monitor full owner/recovery profile and a separate fresh scoped-installed owner profile. The later portable Go timeout branch is qualified separately above; no fresh Ubuntu24.04 or native-CPU claim. |
| FreeBSD15.1-RELEASE-p4 ARM64 | Earlier four C policies/three Go packages/SHAPE-refusal passes are reused. Final C/Go build, new owner regression, scoped config replay/install, full installed Slate owner and authenticated default-Xorg native lifecycle/lease/retirement pass. Only affected UI/default-X checks repeat after a Form metadata-readiness correction. |
| FreeBSD15.1-RELEASE-p4 AMD64 | Final C combined build, four C policies, three Go packages, SHAPE-query refusal, scoped installed Slate owner and authenticated default-Xorg native lifecycle pass. Later Go timeout and unexecuted fixture-branch changes are distinguished from this earlier frozen run. |
| OpenBSD8.0 ARM64 | Final C build, four C policies, three Go packages, SHAPE-query refusal and scoped config replay pass. Current desktop/browser peers are explicitly rebuilt; full scoped-installed Slate owner passes. A separate frozen default-only run passes authenticated Xenocara wsfb/native lifecycle, actual root fallback, lease and retirement. The later Go timeout branch is qualified separately; this is not a fresh final-Go owner run. |
| OpenBSD8.0 AMD64 | Final C/Go build, four C policies, three Go packages (including the missing-END regression), SHAPE-query refusal and scoped configuration replay pass. A current scoped install is reused by fixture18: full installed Slate PNG owner passes in133.82s; authenticated Xenocara VESA native lifecycle/root fallback/lease/retirement passes in36.97s. Source guards pass before and after; unauthenticated clients are refused. Earlier failed15/17 runs remain failed. |
| Native Go recovery | Old production code fails the new real five-second missing-Cells-END exchange test. Corrected command-package normal and race tests pass with Go1.27.2; Go1.26.9 minimum-SDK tests pass. Cancellation and received malformed/view/epoch responses remain nonterminal at the shared wait layer; every exchange retires its mailbox. |
| Actual native-child loss | All four final Linux theme/sanitizer runs issue an owner Cells request during actual child loss, receive no END, and exit1 within the unchanged12-second acceptance bound. Exact child Wait/selection release, saved-state/target preservation and surviving concurrent browser are observed. Normal owners exit0; intentionally broken restarted owners exit1. |
| Repository/manual | The revised manual renders through existing Asciidoctor without diagnostics. `make check test lint` passes289 Python test entries, with20 explicit host-platform skips. Later fixture-only changes pass AST/source review, the observer self-test and legacy relay import checks. Final `make check lint` passes; integrated metadata/evidence review accepts the checkpoint. |

The Linux ARM64 lab uses its existing container and Linux6.18.10 AArch64 kernel.
No host-macOS result substitutes for Linux sanitizer execution. Scoped Clang
ASan/UBSan instruments the native module; core and peers are normal binaries,
leak detection is disabled. It establishes neither native-CPU execution nor
leak freedom. Installed profiles use real Meson pinboard/browser tags and
installed owner/browser/native entries, build-tree peers and the configured
private dependency prefix; these are not full system/manual installs.

## SHAPE refusal, test oracles and retained failures

A noninstalled fixture links the exact production module objects and wraps only
`XShapeQueryExtension` to report unavailable. On an independently SHAPE-capable
server, the ordinary positive control becomes ready and retires; the wrapped
module reports unavailable, maps no canvas, preserves selection ownership and
actually exits0. No runtime test switch, replacement server response or security
configuration change is introduced. This qualifies refusal of an unavailable
capability query. Actual execution on a SHAPE-absent server remains unavailable;
this Xvfb cannot provide that precondition.

AMD64 failures are retained, including runs with actual owner acknowledgement
timeouts. One diagnostic pass is not a plain acceptance run. The observer now
skips an unused RandR verbose/output-ID query and anchors Cells acquisition on
its first fully validated page instead of a redundant Status RPC. The original
whole-acquisition five-second deadline, page/retry bounds, publication identity,
counts and row/END checks remain; pure stream/deadline regressions pass. The
final uninstrumented original-large profile passes with no unexpected owner
acknowledgement/readiness/disconnect failures. This accepts that exact functional
profile; it neither proves the earlier stalls' root cause nor guarantees latency
under concurrent load. The optional compact diagnostic attempt remains failed.

OpenBSD failures preserve a cache receipt's initial file-type/permission-bit
mistake, an accidentally cached empty output directory and the initial missing
worker notice covering the input. Passive receipts prove the cut buffer held
the expected bytes; the notice, not a proven Form paste defect, received the
click. Explicit current desktop/browser/worker rebuild changes the cached peer
binary identities. The fixture requires, hashes and binds the actual worker.
No Raise workaround or input assertion relaxation hides the notice. Its initial
private default-X driver also omitted the existing Xenocara `_x11` authority-file
setup: the unauthenticated-client preflight fails and is retained. A fresh
check uses the repository's existing private server-group contract and verifies
unauthenticated-client refusal; no authentication weakening or trust change.

FreeBSD's later Form title/mapping could be visible before its class hint.
Missing or retired class metadata now remains not-ready under the same
8-second predicate; strict native canvas alias/schema checks remain. Expected
BadWindow is scoped to the exact property read; other X errors remain recorded.
The affected owner/default-X rerun passes. Known Form diagnostic warning classes
remain in logs and are not relabeled as empty logs or new product fixes.

AMD64's first duplicate Status preceded its initial monitor configuration. The
original full and isolated native controls fail; a diagnostic native copy emits
no protocol-error marker, so its cleanup connection-loss exit is inconclusive.
A focused control then observes the exact instance's complete queued MonitorUI
bundle and configuration END before the original transaction/refusal check; it
passes without disturbing the incumbent. Fixture12 retains a launch-anchored
five-second startup predicate deadline and the three-second refusal polling
deadline. Each existing Status exchange can itself wait up to five seconds;
the poll cannot preempt that call. The other desktop-conflict probe likewise
keeps its four-second polling deadline and five-second Status exchange. FIFO queue evidence
is distinct from processing: the subsequent actual Status must report refusal
and a nonzero epoch. Earlier failures remain failed; a passing control does not
prove the original Echo/MFL delivery latency's cause.

A later AMD64 trace observes manager input for view14 immediately before image
publication advances to view15. The exact-generation guard can correctly reject
that queued input. Fixture15 moves the existing eight-second moved-icon check
before dependent manager input: actual current Picture/hash, Status/Cells and
pixel checks require the visible committed `(4,2)` cell in an advanced view.
The later duplicate paint poll is removed. Manager, context/grab and lifecycle
checks remain; the original failed trace is retained. This qualifies a settled
workflow, not lossless arbitrary rapid input across generation replacement.

A fresh AMD64 attempt then has no native enrollment receipt before its initial
five-second observer deadline. The complete UI fixture costs3.55s to import on
the warm emulated guest (3.96s including interpreter startup); every old proxy
entry imported that fixture first. Missing instrumentation progress alone does
not prove whether C launched. Fixture17 extracts the unchanged relay into a
stdlib-only entry, preserves legacy import exports and routes this checkpoint's
native wrappers directly to it. Relay/termination/atomic-write equivalence,
constants and import compatibility are checked. Queue/log/partial-write/EOF/
signal/actual-Wait behavior and all acceptance deadlines remain unchanged.
Affected fixture18 owner/default execution passes; import timing
is test-infrastructure evidence, not a WM performance comparison.

The first lightweight-relay run reaches MFL, but sends the theme query at
1087.926 before Desktop's provider-ready notice at1091.799. A missing five-second
reply escapes the old outer poll. Fixture18 first observes the existing strict
`MFL_PROVIDER_1` notice from the exact enrolled MFL trace: Desktop emits it after
configuration and setting ready. One original twelve-second deadline now covers
that readiness and correlated actual settings queries; each exchange is capped
at five seconds and remaining time. Missing replies still fail without retry.
Separate source review accepts the correction. The affected installed-owner and
authenticated default-X runs pass; the original missing-reply run remains failed.

## Captures and source identities

[Three fresh inspected theme captures](../../doc/screenshots/pinboard-acceptance-manifest.json)
come unchanged from passing final runtime runs. Slate shows the manager, Dune
shows the concurrent browser after recovery, and Classic shows enlarged text.
The black transition-probe window is a fixture client. Cell labels can clip;
manager reference labels remain available. Panel counters are whole-guest values,
not a WM performance benchmark. Physical, assistive/IME, arbitrary-theme,
external-toolkit/compositor and hardware hotplug behavior retain separate owners.

Final native C SHA-256:
`ac19c01a8ffd3cd1362539899e3d7a6fa61344d86b31d128cc3c6f9edd3a6c29`.
Final Go acknowledgement code:
`2b1f9e624de0fb05dd03c4487d19f6c64f78c71e124d422f33b48415e9433973`.
Final fixture18:
`ad8efe08a8e82711ac4dd39d1b3a9bf356e3a2314f26023ddd73de45230cae4d`.
Linux final passing captures use fixture09 `a3a453dc6cc5017cc4254fa8a472662ad0a45a475272658363162c37d06c041d`;
Fixture11 changes pending class-hint observation; fixture13 observes both
refusal instances' initial configuration before scene submission; fixture15
moves the existing paint check before dependent manager input; fixture17 extracts
the unchanged relay and fixture18 gates the first theme query on provider readiness.
Earlier target fixtures
and Go code remain identified in the accompanying receipt.

Canonical raw evidence lives in the owning local checkpoint
`builddir/codex-2026-10-28/r1-pinboard-acceptance`. Linux/FreeBSD collections verify
exact regular-file membership, modes, hashes and gzip CRC before deleting exact
guest duplicates; install views, source, builds, caches and VM disks remain.
OpenBSD raw archives and selected receipt hashes are retained separately from
verified incremental caches. The final AMD64 raw archive retains281 selected
regular receipts, including failed15/17 and passing18. Its original driver exits1
for the retained15 failure; the independent18 continuation exits0. Full gzip CRC,
archive SHA and all extracted selected hashes pass. The guest then shuts down
with QEMU exit0 and its per-job key removed; all owned labs are stopped.
Source guards verify2162 regular inputs (2163 with the extracted relay) and exact
583-file Go membership on composed Linux/FreeBSD profiles. OpenBSD validates
its frozen input archive and post-run source bytes. See the compact
[receipt](VERIFICATION-2026-10-28-PINBOARD-ACCEPTANCE.json) for archive identities.

#64's finite remaining profile criteria pass; close on merge after integrated
review. Broad R1
release/native-CPU/physical/assistive acceptance remains separate. Performance
comparison is not due:2/5 after PR290,3/5 after this next merge. The existing
qualified performance reference is preserved. #257/#258 remain parked.

## Separate review

`temper_reviewer` accepts integrated tree
`8de0c42f17c245f31c017403c0aa1dd366e6cc35` against the stated base,
including all source18 inputs, the three inspected captures and independently
checked final AMD64 archive membership/hashes/modes/CRC and actual passing
results. The polling-deadline documentation finding is corrected; no actionable
findings remain. Subsequent changes reconcile README/current status and record
this review, without changing tested inputs. The PR records its final commit/tree.
This is a separate read-only agent examination, not independent human assurance.
The requested role was available; actual backend model/effort is unattested.
