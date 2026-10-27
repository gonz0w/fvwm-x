# Browser continuity, keyboard actions and FILE-ACTION closeout

Owner: Cam. Base merged PR270 `fe6318d1d032a1b726eef060aa1b78f8f22cc3c6`.
Candidate: `codex/r1-browser-continuity`. #65 closes only on this PR's merge
with all five frozen rows qualified. #69 receives the continuity/keyboard slice
and remains open for actual automatic module-scale acceptance under #51.

[Contract](../decisions/BROWSER-CONTINUITY-2026-10-27.md),
[receipt](VERIFICATION-2026-10-27-BROWSER-CONTINUITY.json),
[actual captures](../../doc/development-preview.md#browser-view-continuity-and-keyboard-actions).
The fixed R1 criteria are unchanged; no new feature or evaluation scope was
invented to obtain closure. #66's storage comparison, mutation jobs and protected
clipboard/session work retain separate owners.

## Delivered behavior

Same-folder F7/provider list/grid switches use the existing prepared Form viewport
in place. Real native ListStatus acknowledgements commit a new layout generation;
old callbacks cannot act on the new projection. Form, input and list windows,
unsent Path/filter text, selected identities, focus, range and viewport anchors
survive. Existing shared reflow owns drawing, hit rectangles and legal edge clamp.
An offscreen focused entry does not force the viewport back to it. Folder
navigation and uncertain-state recovery retain their documented rebuild boundary.

A too-small parent refuses atomically: WM size hints do not prove resize. The
browser keeps its observed mode, generation, bindings, controls and drafts, shows
enlarge/retry guidance, and permits retry after actual enlargement. A provider
request returns correlated `operation_refused`, without a false applied result.
Content updates commit before a simultaneous layout request. Malformed, pending,
wrong-generation and lost observations keep the explicit recovery boundary.

Additive Alt-letter/digit native button shortcuts give every browser action a
keyboard path. Actual Mod1 is required; Control/Mod4 combinations are excluded,
letters normalize Shift/Caps, and unmatched keys keep legacy behavior. Unique
visible browser mnemonics supplement existing F2..F8, Enter and Escape. Labels
remain action discovery, never executable argument data. Existing worker behavior
now has explicit valid-target missing/non-executable opener, terminal and custom
program evidence: correlated failed results, reaping, no duplicate effects and
refresh/successful single-launch recovery.

## #65 frozen acceptance map

| Row | Implemented authority and qualifying evidence |
| --- | --- |
| 1. Typed navigation/open/terminal/context schemas and literal IDs/paths/URIs | Existing sixteen-operation provider catalog, strict schemas/value admission, file target/URI adapter and at most four literal configured selection actions. [PR177 work contract](VERIFICATION-2026-10-23-FILE-PROVIDER-WORK.md), [PR178 targets](VERIFICATION-2026-10-23-FILE-PROVIDER-TARGETS.md), [handoff](VERIFICATION-2026-10-23-FILE-BROWSER-HANDOFF.md), [PR187 catalog](VERIFICATION-2026-10-23-BROWSER-PROVIDER-CLEANUP.md), current browser/filemodel/transport regression. Context actions are configured selection actions; no new context-menu widget is required. |
| 2. Revalidation and correlated terminal outcomes | Worker checks parent/entry identity immediately before literal launch; provider retains serial-correlated status, cancellation and supersession. Existing accepted domain names include loaded/accepted/started; no illustrative acceptance word silently renames them. Current stale/mixed/malformed/refusal tests and new native failure evidence apply. Captured view refusal reports actual operation_refused. No label-to-command reconstruction. |
| 3. Bounded slots/queues, cancellation through reap and handoff limits | One owned active worker and one latest navigation; cancellation retains ownership until Wait. [PR177](VERIFICATION-2026-10-23-FILE-PROVIDER-WORK.md)/[six-target action cycle](VERIFICATION-2026-10-23-BROWSER-ACTIONS-CYCLE.md) cover blocked work, unrelated responsiveness, cancellation/reaping and supersession. Current worker isolation/cancellation/outcome tests pass. Started means process creation, not application completion or rollback. |
| 4. Installed reusable consumer, selection, no duplicate launch and missing programs | [PR186](VERIFICATION-2026-10-23-BROWSER-CONSUMER-HANDOFF.md) records exact staged browser/desktop-entry installation, folder/reveal/properties CLI reuse, native presentation and no execution during reveal; PR187/269 retain their explicit changed-input scope. Current missing/nonexecutable opener/terminal/custom tests and native custom-failure/refresh recovery close the finite missing-program proof gap. Existing ambiguous submitted-outcome tests prohibit duplicate fallback. Installation paths/rules are unchanged. |
| 5. Source-bound policy/real-X, themes and affected support | Current focused Linux policy/race/vet, scoped sanitizer, legacy native Form and three-theme continuity/action cases below. Accepted PR178 six-target/seven-profile cycle, PR186 Linux/FreeBSD/staged consumer and PR269 source-bound binding/platform receipts retain exact historic scope. No fresh all-platform/install/default-X claim. This ordinary portable Form/browser change adds no OS API, native packet type, installed dependency or core state path requiring another all-OS cycle. |

## Current affected checkpoint

One retained Linux ARM64 GCC 13.3.0 / Go 1.27.2 / Meson profile in the owned
Podman VM. Four C policies (`form-list`, `form-input`, `ui-layout`,
`form-command`), five Go package outcomes (browser, collection, filemodel,
viewbinding, transport), browser race and scoped vet pass. Three ASan/UBSan
leak-enabled policy tests pass. Existing native input/button/fields/geometry
Form clients pass. Only affected Form/browser and policy targets build. Both
manuals render through Asciidoctor without diagnostics; no fresh mandoc lint.
Recorded command time is 36.548 seconds, including the initial configure failure,
excluding transport, native-theme checkpoint and review.

The three actual Xvfb/XTest profiles all pass: Slate dark, Dune light, and
DejaVu Sans Mono 18pt with legacy colors. Each uses a 1920×1800 nested root,
4,096 entries, 27 unchanged native children and 14 acknowledged layout switches.
They cover range/multiple/hidden selection, both wheel/top-anchor directions,
resize/F7, exact minimum-size refusal/enlargement/retry, complete byte drafts and
input focus, native Alt actions, provider loss/recovery, real correlated missing
program failure, vanished entries, stale/malformed/pending layout refusal,
refresh interleave and producer teardown/restart with old-callback refusal.
The top-anchor observation clicks the first cell after switching, changing focus
and anchor; the independent range case establishes the retained range invariant.

All 21 captures are inspected; six are published. No final observer X errors or
cleanup failures. Each profile has six real module-child Wait receipts, all zero;
Xvfb, core, browser and successor browser exit zero. The shutdown fence remains
bounded at five seconds and never invents a successful reap. Native actions use
actual messages, workers and file targets; no ACK or application effect is fake.

The original 1,529-file source/build/policy input manifest remains preserved.
All compiled C/Go/build/policy inputs remain exact. One shared Python native
proxy helper changes afterward for the reviewed receipt fix below; its exact
hash and the new driver hash bind the final native run separately. This fixture
change triggers only native completion, not repeat compilation, Go or sanitizers.
No all-OS/compiler/optional/install/default-X matrix runs: no platform API, packet
type, installation rule or core window-state contract changes. Prior platform
and staged-consumer receipts in the acceptance map keep their original scope.

## Independent review and original failures

Source review identified a real geometry error before the first qualification:
new minima could exceed a shrunk parent while child geometry was acknowledged.
The native path now refuses before mutation; browser/provider preserve and report
the old observed mode. Source review reconfirmed the fix; native minimum-size
refusal/enlargement/retry supplies the runtime regression. No runtime correction
follows the qualified source freeze.

The first configure stopped before build/tests because the new native fixture
was not yet synchronized into the guest. Its original log/command result remains;
after exact fixture transfer the same affected configure/build gate passes.

Two original native runs fail and remain archived. First, an observer asserted
`pending=0` during a real 4,096-entry selection transaction. Original proxy bytes
show ListBegin 2→3 followed by the exact expected committed generation, focus
and empty selection. Ordinary status observation now waits boundedly for actual
pending-zero publication; the final exact selection oracle is unchanged.

Second, a nonzero-offset items query omitted its required generation and was
correctly rejected. The driver now binds pages 50/100 to the first page token,
checks stable generation and expected names, and audits the remaining request
schemas. It never accepts invalid_request as success. During that failed run's
unwind, a repeated SIGTERM interrupted the proxy's real receipt write after its
child had exited. All six recorded proxy/child PIDs were independently observed
absent. The shared fixture now ignores repeated TERM only during its existing
bounded terminate/Wait/close/actual-receipt critical section, then restores the
prior handler. No timeout, exit status or receipt is fabricated. The original
cleanup failure is retained, not relabeled passed. Separate review accepts both
fixture corrections. Product/runtime inputs do not change after qualification.

## Scope, cost and remaining work

#69 is not closed. Enlarged text is not evidence of the separate 1.25/1.5/2 UI-scale
paths; Form/browser automatic module-scale adoption remains #51/#69-owned.
Fixed status/properties and narrow collection cells can clip; visible labels,
keyboard actions and observed-state semantics are reviewed in the actual captures.
No IME/shaping, optional AT-SPI, screen-reader, physical display/toolkit/compositor,
full release profile or native-CPU sanitizer qualification is claimed. The native
keyboard path uses the documented Mod1 convention and can be superseded by a
user's global WM bindings; no modifier map is rewritten.

Unstripped Form grows 1,636,240→1,643,048 bytes (+6,808); browser grows
5,634,701→5,637,354 (+2,653). Core remains 5,521,504 bytes with the same hash.
No new installed executable, dependency, mandatory toolkit, resident, persistence
or per-entry X resource is added. These sizes are not RSS/latency measurements;
there is no paired speed or kernel-wakeup claim. PR269's periodic reference is
1/5 after PR270, becoming 2/5 on this runtime merge; no comparison is due.

Only exact completed guest evidence duplicates retire after complete host
archive/member/hash/size/mode/gzip-CRC verification and actual child-exit proof.
Source/build caches, stages, SDKs, VM disks and performance references remain.
An old 181-file module-qualification guest capture was preserved/verified and
retired to free 76,575,601 bytes before this gate; its canonical archive remains.
All owned resources are restored stopped at delivery. No Actions or success-status
publisher is added. Requested project Sol/high implementation/review and Astra/high
analysis routing is recorded; actual model execution is not independently attested.
