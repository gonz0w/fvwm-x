# FVWM-X project tracker

Updated: 2026-10-24. This is the active tracker. Retired milestone planning and
upstream notes live under [history](history/README.md). Update status with
evidence, not estimates; use [the current index](README.md) for project guides.

Current findings and verification gaps are summarized in
[SECURITY-STATUS.md](SECURITY-STATUS.md).

## Goals and constraints

- A simpler codebase that runs existing FVWM 3.x configuration files. Compatibility
  is limited to those files and their format: supported command/directive names,
  arguments, bindings, functions and theme configuration remain usable without
  a rewrite. Stop for a compatibility decision only for a configuration break or
  required migration. Internal APIs/ABIs, module protocols, exact legacy rendering,
  incidental timing and error/failure behavior do not need separate compatibility
  approval; update shipped consumers, docs and verification together.
  See the [clarified scope](decisions/CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
- A responsive desktop: keep X state single-threaded, bound work and queues,
  and measure input/geometry responsiveness under load before and after changes.
- Security and availability: validate external data before allocation or use,
  contain abusive or stalled clients, and make descriptor ownership explicit.
- Linux, FreeBSD default Xorg, and OpenBSD default Xenocara, on AMD64 and ARM64.
  Local BSD verification uses QEMU. Keep the root-sized fallback for servers
  reporting no RandR monitors. No native Wayland backend or built-in compositor.

## Agent Temper routing update — MAINT-02/TEMPER-02

Owner: Cam; assigned fleet-adoption lead. Status: reviewed and owner-approved
candidate; canonical completion remains conditional below. Mirrored in
[issue #196](https://github.com/gonz0w/fvwm-x/issues/196) under MAINT-02 #14.
The October 24 owner-selected fleet update is a bounded maintenance checkpoint.
Outcome: pristine immutable source update, explicit Codex selection, bounded
implementation delegation and four local roles. No product/runtime/native effects,
new tracker or broader acceptance. Preserve other owners and active work.

Acceptance: exact upstream bytes/license/source/manifest/inventory; coherent local
policy, role/model/effort/fallback/limit mapping; affected links/anchors and
appropriate existing checks; signed actual-author-DCO candidate; separate final
review; deliberate maintainer-approved integration and canonical verification.
Use the owning issue for approach/evidence and mirror meaningful status here.
Source preparation and static source/inventory/hash/TOML audit are complete at
the selected pin. `make verify` passed the portable repository/lint/policy tests;
15 native Meson/compiler/tool integration cases were explicitly skipped. These
are source/portable checks, not WM, X11, native-platform or runtime-routing
qualification. Separate parent peer-agent review cleared signed head
`4936938eb421db64c7f3a054f6fdda8fe7654058`, tree
`67f16a9417f0653efd6b0e708ca6bacfd5a826e8`, including the retained verification
log and signature/DCO evidence. It did not independently rerun tests or observe
runtime routing. The owner explicitly authorized gated merge of PR #197 on
2026-10-24, with existing checks, resolved review and canonical signature/DCO
verification. This is finite approval for this checkpoint; no production,
credentials, permissions/protection or general future merge authority expands.
Issue #196 records the exact final source and canonical verification. The
checkpoint becomes complete when PR #197 merges and its canonical identity,
signature and DCO pass that recorded verification; until then integration is
pending. Broader MAINT-02 #14 and native/release outcomes remain open.

Existing native/X11/install/performance/release gates remain separate. This
checkpoint ends only at accepted merge/canonical verification or abandonment;
configuration parsing does not establish runtime role/model discovery.

## Agent Temper adoption — MAINT-02/TEMPER-01

Owner: Cam (@gonz0w). Status: open until authorized integration and canonical
verification are complete. Review mirror:
[issue #192](https://github.com/gonz0w/fvwm-x/issues/192), bounded child of
[MAINT-02 #14](https://github.com/gonz0w/fvwm-x/issues/14). This is the current
owner-requested methodology checkpoint, not a new product feature batch.

Outcome: adopt the portable Agent Temper core at
`588698ff019fbde5bfc01f426c02af66c4987b6d` with no optional profiles, while keeping
this tracker authoritative and preserving FVWM-X's local product, compatibility,
license, documentation and risk-based verification requirements.

Acceptance: retain exact upstream bytes/license/source/checksums; compose durable
policy in PROJECT.md; align agent/contributor/governance/documentation routes;
record the owner's 2026-10-24 permission for bounded read-only agent reviews;
pass existing documentation checks plus bundle/link/anchor/source and reader
review; preserve signed/DCO history and obtain required maintainer integration.

Non-goals: runtime/build/checker changes, new tools, native/VM execution, release,
deployment, changes to other owners' files or requalification of historical
results. Broad MAINT-02, native and release gates remain open. This documentation
checkpoint does not advance feature/performance qualification counters.

Verification: `make check`, both working/staged `git diff --check`, exact bundle
inventory/upstream/hash checks and affected Markdown links/anchors; separate
policy/reader review. No artificial runtime tests for unchanged behavior.

Evidence on 2026-10-24: `make check` passed. Exact eight-file upstream comparison,
ten-file inventory, nine-entry digest coverage, affected local links/anchors and
working/staged whitespace checks passed. A separate read-only reviewer inspected
the work-item walkthrough and preserved local contracts. Two wording findings
(commit convention and authoritative tracker routing) were corrected. This is
source/documentation evidence, not new runtime or platform qualification.

Completion rule: this checkpoint becomes complete when its reviewed candidate is
merged with explicit maintainer authority and the canonical commit/tree,
signature and retained author DCO are verified. Record the exact identities and
results in [PR #193](https://github.com/gonz0w/fvwm-x/pull/193), linked here as
evidence. Until both conditions are satisfied, the checkpoint remains open.
This tracker owns the completion rule; GitHub mirrors the resulting state.
Broad MAINT-02 and product/native/release acceptance remain open.

## M2.3 configuration-checker runtime port

Owner: current M2 implementation chat; merged in PR194.
Broader RUNTIME-GO-01 #23 remains open.
Outcome: remove the Python literal-source walker and use the existing bounded
Go worker for read-only configuration reports. This independent named M2 owner
is selected ahead of the larger resident state/lifetime port to keep that port
atomic; no settings file commits or preview ownership cross a worker boundary.
Acceptance: preserve literal Read search order, source hashes/include locations,
strict shared settings validation, explicit unchecked coverage, regular-file-only
reads and all work/diagnostic limits. No execution of configuration, new syntax,
UI redesign, dependency removal claim or broad #23 closure. Dependencies: merged
M2.1 transport and M2.2 settings registry. Verify Go source/budget regressions,
worker request boundaries and retained controller/report integration on one
incremental Linux profile; compile affected targets for all supported systems.
Existing report UI stays resident; capture its affected native-X view. Broader
resident/session/capture and full platform acceptance remain pending.

One combined Linux gate passes 31 Go top cases/26 subcases in normal and race,
75 controller and 12 worker boundary cases, adjacent installed CLI, native-X
report paging, three theme profiles and manual rendering in 42.022 command
seconds. Six workers cross-build; no BSD execution claim. Source/limits and
symlink-parent traversal preserve the declared literal subset.
[Report](verification/VERIFICATION-2026-10-24-M2-CONFIGURATION-CHECKER.md).

## M2.4 resident desktop controller

Owner: current M2 implementation chat. Status: merged in PR195 under Cam’s explicit 2026-10-24 merge instruction.
Outcome: port the resident desktop module to Go as one state/dispatch/lifetime
owner, importing existing settings policy directly. Group private persistence,
settings generations/overrides, editor/draft and core-preview lifetimes, typed
provider replies, bounded jobs/operations and existing native UI/services.
Acceptance: preserve FVWM3 configuration and installed entry names, atomic
revision-checked private commits, no false refusal after rename, stale-owner/
editor/preview rejection, ordered bounded native packets and commands, bounded
job admission/cancellation and explicit service failures. Retire the Python
resident implementation after installed parity. Keep the existing Xlib/PNG
capture code in a separately supervised helper for its later native port.
Non-goals: new UI/features, native capture replacement, M3 work, broad #23 or
release closure. Dependencies: merged M2.1–M2.3 packages. Use focused Go policy/
race/process tests, independent packet peers and existing actual-X settings/
operations/default-desktop fixtures with three theme profiles/screenshots.
Check private-file/descriptor portability early on an affected BSD target;
compile supported pairs offline. Full owner/platform/session qualification
remains explicit. Run the now-due preserved-binary performance comparison without
new benchmark builds. The simpler alternative, another one-shot state service,
would split commit/lease authority; resident ownership avoids that ambiguity.

Delivery: the Go resident now owns private persistence, generations, editor/core
preview recovery, typed replies, ordered packets, bounded jobs and existing UI.
The Python resident retires; only isolated capture remains Python. Existing
vendored Unix wrappers and a pinned Unicode fold table add no vendor payload.
Linux passes 55 normal/53 race top cases (36 subcases each); FreeBSD ARM64 passes
53 normal top cases/36 subcases. Six independent installed peers pass on both;
native-X settings/operations, Linux desktop services, three themes/28 captures,
manual rendering and twelve offline resident/worker cross-builds pass. Review
corrections repeat affected gates only; original failures stay preserved.
[Source-bound report](verification/VERIFICATION-2026-10-24-M2-RESIDENT-CONTROLLER.md).
Next: native capture, complete Python-free installed/session measurements and
remaining owner/platform qualification before M3. #23/#7/#15 remain open.

## M2.5 native desktop capture

Owner: current M2 implementation chat. Status: merged in PR198 under Cam’s explicit 2026-10-24 merge instruction.
Outcome: replace the last installed desktop Python helper with a separately
supervised C Xlib/libpng capture executable, preserving Print/module commands,
private unique PNG files, true-color channel scaling, 16-megapixel admission,
explicit failure and the resident's deadline/process-group ownership. Update
build-tree/installed helper discovery and every shipped test/generator consumer;
remove the predecessor after parity. Reuse existing image-size policy and target
PNG/X11 dependencies. A C helper avoids adding a Go X11 binding or cgo toolchain
while keeping X work outside the resident/core loops.
Acceptance: check dimensions/masks/stride before use, preserve 16/24-bit RGB,
clean partial output on write failure, keep missing-display/unavailable-PNG
failures explicit and install no Python desktop entry/helper. Verify pure pixel
policy boundaries natively, actual-X capture/installed and build-tree dispatch,
three theme captures, and an affected BSD build/run for Xlib/PNG/file APIs.
Non-goals: new screenshot UI, clipboard/file features, build/test Python
retirement (M7), full M2/session or broad #23/#7/#15 closure. Dependencies: merged
M2.4. Run one combined focused Linux gate and selected affected BSD checks;
retain unrelated controller/worker coverage, with broad owner/platform/session
qualification still pending. Performance reference PR194 is 1/5 after PR195;
no new periodic comparison is due.

Delivery: native C capture replaces the Python helper. Linux/FreeBSD ARM64
pure policy, two Go entry cases, six installed peers, build-tree and installed
module dispatch, exact 16/24/30-bit RGB/PNG and failure/private-file checks pass.
Linux also passes the existing desktop flow, three themes/28 captures, scoped
ASan/UBSan, warning-free compilation and man rendering. Combined commands take
47.299 seconds, including directed C/install-evidence corrections. Retain exact
unchanged UI/module/Go results; no full matrix repeat. Retire 245 verified guest
duplicates (49,253,154 bytes), preserving canonical host evidence and reusable
builds/stages/disks. [Report](verification/VERIFICATION-2026-10-24-M2-NATIVE-CAPTURE.md).
Next: remaining runtime inventory, complete Python-free installed/session
measurements and justified owner/platform acceptance before M3.

Merge record: PR195 is squash-merged as
`ce9916de2eed0a2556e653497673f8a73352b256`, preserving the author DCO.
Cam explicitly authorized this merge and cleanup in chat. Canonical signature,
tree and remote state are verified; completed local/remote
branches retire, while canonical evidence, reusable builds and VM disks remain.

## M2.6 runtime/session action completion

Owner: current M2 implementation chat. Status: focused checks passed; ready for review after merged PR198.
Outcome: reconcile the actual installed runtime inventory, remove the unconsumed
settings RPC adapter after direct imports, and finish ownership of existing
locker/power actions. Power admission uses its own job identity instead of the
audio slot and reports busy refusal. One pending locker invocation reports
construction/exit failures and refuses duplicates without claiming readiness;
external lockers/applications retain detached restart behavior. Preserve native
configuration commands, literal application argv and intentional Run shell use.
Acceptance: retire only unused RPC conversion/helpers, retaining direct typed
settings/UI/persistence policy and malformed-input coverage at current consumers;
prove audio/power isolation, busy/invalid action refusal, one locker invocation,
late exit/exec failure reporting and no inferred lock-before-sleep guarantee.
Demonstrate an installed desktop/command/menu environment without Python in its
runtime namespace, record startup/process/RSS/idle/input observations and compare
with preserved PR194's Python resident when available. Keep renderer/theme
coverage source-bound; capture any new notices in the three established themes.
Non-goals: new SESSION-01/02 autostart/bus/service manager features, M3 linkage,
M6 Perl/M7 tooling retirement, broad #7/#15 closure or release acceptance.
Dependencies: merged M2.1–M2.5. Implement together, then one incremental Linux
policy/race/installed/native-X gate; add a scoped BSD check only if changed
contracts justify it. Full affected six-target/default/reduced/session acceptance
remains explicit before M2 owner closure; do not claim it from focused checks.
The simpler alternative is to leave action aliases and dormant request decoding;
separate existing action lifetimes and direct policy imports avoid ambiguous
admission and needless protocol maintenance without another daemon.

Discovery during installed proof: the minimal Python-free root has no procfs.
The resident silently ignored os.Executable failure and lost sibling helpers.
Extend only this failed-lookup path: use FVWM's absolute invocation path when
procfs lookup is unavailable, retaining normal lookup and explicit overrides.
Verify the pure path choice and actual installed root; no relative-PATH guess or
new helper setting is introduced. The same installed-root failure occurs in the
full menu's self-worker path; browser inspection and directory-menu reuse have
the same source dependency. Consolidate these four consumers in one stdlib-only
runtimepath helper, including symlink resolution and explicit relative failure,
then run their affected tests together. Retire an unused resident helper-path
function rather than leave another competing lookup. This is a reproduced
installed-runtime dependency family, not a new feature or utility reimplementation.
The container also forbids mknod, so this
availability fixture uses an explicit ordinary /dev/null test sink. It does not
qualify device semantics, a complete OS image or a hostile-code sandbox.

Inventory: a bounded read-only workflow agent finds current resident/worker/
capture and utility install targets in Meson, and no production launcher.py or
session.py entry. These old planning names are historical. Remaining Python is
build/test/artwork tooling; the Perl framework remains M6. Source inspection is
not installed interpreter-free or full-session evidence.
Routing follows the newly merged PR197 policy. Native role loading is unavailable
in this orchestration surface; use explicit fresh-context spawn settings from
the selected table. Workflow inventory requests `gpt-6-luna`/medium; separate final
review will request `gpt-6.1-sol`/high. The API reports agent identity, without
observed model/client-version metadata; runtime selection remains unverified.
No routing cost/speed saving is claimed. Lead owns implementation/integration.

Delivery: power/audio admission, locker lifecycle, direct-policy retirement and
shared self-worker paths pass their focused checks. A source overlay reproduces
the lost power request. Initial 57/36 and shared-family 65/37 Go top/subcases pass
in normal/race, then only the changed callers' 10/5 normal cases/vet repeat after
review. Installed peers and actual-X desktop/worker/capture/command/full-menu/
directory reuse/browser inspection pass without a Python interpreter in the
runtime root. Same locker PID/starttime survives both controller restart and
core logout, then is reaped. All three final notice themes are captured/inspected;
reviewed source/evidence has no unresolved actionable findings. The reduced root
uses a regular null sink and has no procfs; it is not a complete OS/device image.
All 100 installed files remain exact and temporary roots are removed. Retire
260 verified guest duplicates (58,733,469 bytes), retaining canonical host
evidence and reusable source/build/cache/stages/disks.
[Report](verification/VERIFICATION-2026-10-24-M2-SESSION-RUNTIME.md).

Three alternating ordinary installed-session pairs compare the preserved PR194
Python resident/qualified Linux worker with the Go owner on identical current
native foundations: median startup 192.411/114.954 ms and summed RSS
45472/27588 KiB; key response 0.303/0.311 ms overlaps. CPU ticks are coarse,
thread-leader switches are not wakeups, and sampled idle RSS is not startup peak.
No hardware/GC/general speed guarantee follows. This distinct session observation
does not reset the independent PR194 periodic reference, now 2/5 after PR198,
3/5 on this merge. [Observation](verification/PERFORMANCE-2026-10-24-M2-SESSION.md).
Next: one combined M2 platform/default/reduced/install/session acceptance handoff
and remaining true wakeup/peak/native-CPU limits before M3. Broader #23/#7/#15
and release gates remain open; new features and tuning remain paused.

## Current checkpoint status

This table is the current summary for this thread. The dated entries below
retain earlier development states; a historical "pending" entry does not
supersede a later accepted result. Counts describe each frozen checkpoint,
not the changing shared checkout or another thread's desktop/runtime work.

Current priority, 2026-10-23: **maintenance first; new feature pickup paused**.
Merged [PR189](https://github.com/gonz0w/fvwm-x/pull/189) completes M1;
#179/#180/#181 are Done. [PR190](https://github.com/gonz0w/fvwm-x/pull/190)
merges M2.1's bounded desktop acquisition/actions worker with focused Linux and
FreeBSD, installed/actual-X/race and six-target compile evidence retained.
Merged [PR191](https://github.com/gonz0w/fvwm-x/pull/191) completes M2.2:
ordered settings registry/types/schema migration,
candidate/query policy and settings/appearance/schedule/preview UI plans use
one pure Go package. Direct preference commits, state generations,
editor/preview lifetimes and publication stay with the resident Python authority.
This avoids introducing ambiguous file commits across worker failure. Complete
plans validate before consumer replacement; scalar-only caching is bounded.
76 controller cases, 21 composed Go top cases per normal/race profile, installed/adjacent checks,
three native-X drivers and three settings theme profiles/six captures pass.
Six workers cross-build offline. Fixture/storage-only corrections retain all
valid evidence. Final count-before-copy review repeats only the affected
worker normal/race/build/invalid-count/adjacent checks; settings/UI stay qualified.
No full-OS matrix repeat.
[Scope](decisions/M2-SETTINGS-POLICY-2026-10-23.md),
[qualification](verification/VERIFICATION-2026-10-23-M2-SETTINGS-POLICY.md).

Current: M2.3 merged in [PR194](https://github.com/gonz0w/fvwm-x/pull/194).
M2.5 merged in [PR198](https://github.com/gonz0w/fvwm-x/pull/198); M2.6 runtime/
session action completion is ready for review, as scoped above. Next: one combined
M2 affected-platform/default/reduced/install/session acceptance handoff and honest
remaining measurement limits before M3. #23/#7/#15 and
M2 remain open. New feature pickup remains paused.

Verification cadence, revised by the user on 2026-10-23: implement larger
related cleanup/feature batches before one focused Linux gate. Aim for six to
ten substantive related improvements where useful, or a complete owner slice.
BSD/architecture/install/default-X checks follow changed contracts or concrete
failures. Review accumulated risk after roughly five merged batches or a major
handoff; this is not an automatic matrix or a blocker on the next ordinary
batch. Full required qualification stays at milestone/release boundaries.
[Decision](decisions/BATCHED-CLEANUP-CADENCE-2026-10-23.md),
[workflow](WORKFLOW.md#implementation-batches-and-test-cadence).

Integration cycle ledger: merged PR176–178 are **closed before the next feature
batch**: 84 selected native entries on six targets/seven profiles (60 fresh,
24 retained), 556 composed Go cases/35 package outcomes, eight normal browser
native-X runs plus one retained sanitized-Form browser, and five retained scoped
sanitizer exercises. A 1493-input equality check binds retained PR178 ARM64
results. FreeBSD AMD64's synthetic-input observation correction repeats only UI;
all product sources remain unchanged. Both OpenBSD 8.0 architectures pass and
all owned guests shut down normally. [Report](verification/VERIFICATION-2026-10-23-BROWSER-ACTIONS-CYCLE.md).
PR178 `e2f61e65b0e5277dedcf28fe6c602efa8b2d1762` is integrated reference
with eight implementation batches merged since it (PR186, PR187, PR189, PR190,
PR191, PR194, PR195 and PR198), nine on M2.6 merge. Coverage debt was reviewed at PR191 and
this substantial resident handoff. Focused Linux and affected FreeBSD checks
cover the new owner without an automatic full matrix. M1 qualifies utility/compiler/install changes on six targets; M2.1
adds affected FreeBSD transport/worker evidence. Remaining controller/default-X/
full-session gates stay at the M2 handoff; pure settings planning adds no native
C, OS-specific I/O or new dependency. Planning
and documentation do not advance the count. Pending broader coverage for those
feature/cleanup sources remains explicit; the last affected six-target closure
is still PR178. The automatic three-PR platform trigger is retired; review
coverage debt after roughly five merged batches or a substantial handoff.
Full install/default-X/compiler-feature and owner/milestone/release acceptance
remain at their required boundary. Native-CPU/hardware/toolkit/compositor/
assistive checks remain separate.

The due PR186→PR194 periodic comparison is complete using preserved qualified
Linux ARM64 binaries, without a benchmark build. Five alternating 4096-file scans
have overlapping ranges, medians 16.332/16.041 ms and unchanged 29460 KiB peak RSS.
Core/MFL/Form hashes and sizes stay exact; browser file bytes fall by 546.
No consistent speed improvement or regression is established. This does not
measure the new resident controller's startup/RSS/idle/input; those measurements
remain at M2 closure. Preserve merged PR194 as the new reference at **2/5** after PR198,
3/5 on M2.6 merge. Planning/documentation do not advance the counter; tuning
stays deferred. [Report](verification/PERFORMANCE-2026-10-24-PR194.md).

Representative theme smoke checks **pass** on reused Linux native-X binaries:
full shipped Slate dark, Dune light and a controlled classic/minimal configuration
with 9x15 font, explicit legacy Form colors and different borders. List/grid,
empty/large/escaped labels, focus/selection, keyboard, hit targets/scroll and
view switches pass; exact config/font/scale inputs and screenshots are preserved.
The refresh and selection-action checkpoints pass all three; the latest run
exercises four configured action buttons, multi-selection properties and literal
process handoff. Browser live refresh/filter/sort now retains input/selection
and the same X windows; fixed status/properties labels, 4096-entry live reorder
and short timeout renewal/expiry pass. The directory-watch checkpoint now adds
automatic refresh, loss/manual recovery and F8 controls, with 27 screenshots per
profile. [Watch report](verification/VERIFICATION-2026-10-23-BROWSER-DIRECTORY-WATCH.md).
Wider theme/scale/accessibility acceptance remains separate; these runs do not
multiply every compiler/install/platform leg by all themes.
[Public development screenshots and runnable configuration examples](../doc/development-preview.md)
reuse four unmodified captures from this checkpoint. The README stays explicitly
in development; publishing this subset does not close the remaining primitives
or release gates, or advance either verification counter.
[Smoke contract](THEME-STANDARDS.md#primitive-theme-compatibility-smoke-set),
[cadence](WORKFLOW.md#representative-theme-compatibility).

| Checkpoint | Current state | Evidence and next step |
| --- | --- | --- |
| M2.6 runtime/session actions | Focused checks passed; ready for review | Power/locker ownership, retired policy RPC, shared self-worker resolution; Python-free installed proof, three themes and paired session observations. Full owner/platform acceptance remains. [Report](verification/VERIFICATION-2026-10-24-M2-SESSION-RUNTIME.md). |
| M2.5 native capture | Merged PR198 | Last installed desktop Python helper retires; Linux/FreeBSD RGB/refusal/install dispatch, three-theme screenshots and scoped sanitizer pass. Full owner/session acceptance remains. [Report](verification/VERIFICATION-2026-10-24-M2-NATIVE-CAPTURE.md). |
| M2.4 resident controller | Merged PR195 | One Go state/dispatch/lifetime owner; Linux/FreeBSD, race, installed peers, native-X and three-theme evidence. Native capture and full owner/session acceptance remain. [Report](verification/VERIFICATION-2026-10-24-M2-RESIDENT-CONTROLLER.md). |
| M2.3 read-only configuration checker | Merged PR194; broad owners remain open | Go source walking, diagnostics and limits; resident report validation. [Report](verification/VERIFICATION-2026-10-24-M2-CONFIGURATION-CHECKER.md). |
| M2.2 settings policy/native UI planning | Merged PR191; broad owners remain open | One Go registry/types/schema/candidate/query owner and settings/appearance/schedule/preview plans; resident file commits and lifetimes stay together. [Report](verification/VERIFICATION-2026-10-23-M2-SETTINGS-POLICY.md). |
| M2.1 desktop acquisition/actions, RUNTIME-GO/FD/PROC-01 | Merged in [PR190](https://github.com/gonz0w/fvwm-x/pull/190); broad #23/#7/#15 remain open | Apps, Linux/BSD status, audio and power jobs use a private bounded Go worker. Linux/FreeBSD focused policy/CLI/install/native-X and Linux race pass; six workers cross-build without new VM matrix. Final consumer setup/fork rollback, literal quoting and adjacent lookup pass scoped completion. Settings/preview/checker/capture owners retain exact source; full controller/session migration remains next. [Report](verification/VERIFICATION-2026-10-23-M2-DESKTOP-WORKERS.md). |
| M1 required runtime baseline, DEP-GO/XDG-01 and LEGACY-01 | Merged in [PR189](https://github.com/gonz0w/fvwm-x/pull/189); #179/#180/#181 Done | One combined six-target utility/compiler/default/reduced/install checkpoint, 30 native-X utility runs, four BSD default-server settings runs and a fresh signed source-package build pass. All 27 manuals render; 181 portable tests pass and 15 host environment skips remain; compiler controls pass freshly on all six targets. Exact replaced/pre-3.x files retire; the framework adapter, current menus, Script, XPM/ICCCM and user files remain. [Report](verification/VERIFICATION-2026-10-23-M1-RUNTIME-BASELINE.md). |
| Combined provider cleanup, CLEAN-01 / FILE-DATA/ACTION/VIEW-01 | Merged in [PR #187](https://github.com/gonz0w/fvwm-x/pull/187); focused Linux checks pass | One catalog, strict typed value decoding, query/mutation boundaries and concrete query results. All four land before one 22.399-second combined policy/race/vet/build/native-X gate: 85 policy cases, 45 race cases, one actual-X driver. Native/OS/build/install/rendering inputs unchanged; no new platform/install/theme matrix. Earlier feature receipts keep their exact source scope. [Report](verification/VERIFICATION-2026-10-23-BROWSER-PROVIDER-CLEANUP.md). |
| Portable browser consumer handoff, FILE-ACTION/DATA/VIEW/INTEGRATION-01 | Merged in [PR #186](https://github.com/gonz0w/fvwm-x/pull/186); source-bound feature checks pass | Shared correlated RPC, opt-in matching folder/reveal/properties reuse, core presentation and an optional installed desktop entry. Ten native entries, 251 Go top-case executions/15 package outcomes including race, four browser native-X runs and three scoped sanitizer exercises pass; three themes/41 screenshots each. Scoped completion resolves storage-only linking; a manual paragraph placement rerenders only that manual. PR178 is the last integrated reference; PR186 is the first merged combined batch; due performance comparison complete, new reference 0/5 on merge. [Report](verification/VERIFICATION-2026-10-23-BROWSER-CONSUMER-HANDOFF.md), [contract](decisions/BROWSER-CONSUMER-HANDOFF-2026-10-23.md). |
| Accumulated browser action cycle, PR176–178 | Closed before next feature implementation | Six targets/seven profiles: 84 native entries, 556 Go cases/35 outcomes, eight normal browser runs plus retained sanitized Form and five scoped sanitizer exercises. One UI-only observation completion; no product changes. PR178 is the integrated reference 0/3. [Report](verification/VERIFICATION-2026-10-23-BROWSER-ACTIONS-CYCLE.md). |
| Live target and observation controls, FILE-ACTION/DATA/VIEW-01 | Merged in [PR #178](https://github.com/gonz0w/fvwm-x/pull/178); focused Linux and early FreeBSD checks pass | Live reveal/inspect, explicit hidden/watch controls, configured action metadata and generation-checked native focus reuse the existing workers/publication. Twenty-four native entries, 239 Go cases/15 package outcomes, five native-X runs and five scoped sanitizer exercises pass; three themes/39 screenshots each. No product/fixture correction follows freeze. Cycle 2/3, 3/3 on merge: close accumulated platform checkpoint before another feature. Performance PR174 3/5, 4/5 on merge. [Report](verification/VERIFICATION-2026-10-23-FILE-PROVIDER-TARGETS.md), [contract](decisions/FILE-PROVIDER-TARGETS-2026-10-23.md). |
| Isolated browser worker requests, FILE-ACTION/DATA/VIEW-01 | Merged in [PR #177](https://github.com/gonz0w/fvwm-x/pull/177); focused Linux and early FreeBSD checks pass | Typed navigation/refresh and revalidated application/configured actions now have one retained outcome and explicit cancellation/supersession. Ten native entries, 227 Go cases/15 package outcomes including race, four native-X runs and three scoped sanitizer exercises pass; three themes/35 screenshots each. A driver helper naming correction repeats only UI, then completes the skipped manual/sanitizer gates. Merged cycle 2/3; performance PR174 3/5. [Report](verification/VERIFICATION-2026-10-23-FILE-PROVIDER-WORK.md), [contract](decisions/FILE-PROVIDER-WORK-2026-10-23.md). |
| Conditional browser controls, FILE-ACTION/DATA/VIEW-01 | Merged in [PR #176](https://github.com/gonz0w/fvwm-x/pull/176); focused Linux and early FreeBSD checks pass | Five generation-checked selection/filter/sort/view/properties capabilities share native publication and pure selection policy. Ten registered entries, 215 Go cases/15 package outcomes, three scoped sanitizer exercises and four composed browser native-X runs pass; three themes/31 screenshots each. Paint-observer and table corrections repeat only UI/manual work. The PR173–175 platform cycle closes before implementation. Merged cycle 1/3; performance PR174 2/5. [Report](verification/VERIFICATION-2026-10-23-FILE-PROVIDER-CONTROLS.md), [contract](decisions/FILE-PROVIDER-CONTROLS-2026-10-23.md). |
| Accumulated browser/provider cycle, PR173–175 | Closed before the next feature batch | Six targets/seven profiles: 84 native entries, 472 composed Go cases/35 outcomes, eight browser native-X runs and retained three themes/sanitizers. PR175 becomes integrated reference 0/3. [Report](verification/VERIFICATION-2026-10-23-BROWSER-PROVIDER-CYCLE.md). |
| Read-only browser queries, FILE-DATA/VIEW-01 | Merged in [PR #175](https://github.com/gonz0w/fvwm-x/pull/175); accumulated integration qualified | Four related discovery, paged metadata/current selection, typed properties and correlated CLI capabilities. Ten registered entries, 203 composed Go cases/15 package outcomes, three scoped sanitizer exercises and four browser native-X drivers pass. Three themes/28 screenshots each are inspected. Disk-blocked race links and a version-stamp comparison require only scoped completions; C line wrapping retains equal tokens and repeats two focused request checks. The cycle closes at PR175; performance PR174 is 1/5. Broader owners remain open. [Report](verification/VERIFICATION-2026-10-23-FILE-PROVIDER-QUERIES.md), [contract](decisions/FILE-PROVIDER-QUERIES-2026-10-23.md). |
| Native directory watching, FILE-DATA/VIEW-01 | Merged in [PR #174](https://github.com/gonz0w/fvwm-x/pull/174); focused Linux/FreeBSD/OpenBSD 8.0 ARM64 checks pass | Isolated inotify/kqueue observer, coalesced/periodic reconciliation, F8 toggle, explicit manual fallback and F5 recovery. Five Go packages pass 62/61/61 cases plus Linux race 62 (246 composed executions); four native-X drivers and three themes/27 screenshots each pass. A retired-instance observer correction repeats only UI, and a storage repair repeats only the Meson helper/manual target. Native C identities are retained. Cycle 2/3 after merge; the due comparison resets performance to PR174 at 0/5. [Report](verification/VERIFICATION-2026-10-23-BROWSER-DIRECTORY-WATCH.md), [contract](decisions/BROWSER-DIRECTORY-WATCH-2026-10-23.md). |
| Browser live-update adoption, FILE-VIEW/ACTION-01 | Merged in [PR #173](https://github.com/gonz0w/fvwm-x/pull/173); focused Linux and early FreeBSD checks pass | Live refresh/filter/sort/status/properties, hidden/late selection reconciliation, direct native-acknowledged batches and replay-safe callbacks preserve windows/drafts. Native timeout renewal retains bounded orphan cleanup. Final 17 native entries, five Go packages/53 cases on both targets plus Linux race (159 executions), five native-X drivers and three themes/24 screenshots each pass. The PR170–172 six-target cycle closes first. Cycle 1/3 and performance 4/5 after merge. [Report](verification/VERIFICATION-2026-10-23-BROWSER-LIVE-ADOPTION.md), [contract](decisions/BROWSER-LIVE-ADOPTION-2026-10-23.md). |
| Native live collection updates, FILE-VIEW-01 | Merged in [PR #172](https://github.com/gonz0w/fvwm-x/pull/172); accumulated integration qualified | Atomic bounded snapshots, current-input identity remapping, generation callbacks and native status/abort/replay ownership. Seven selected entries pass on each target, three Linux scoped sanitizer entries and five composed actual-X drivers pass; three themes/20 screenshots each. Go/core/MFL qualification is retained. The accumulated PR170–172 checkpoint is closed before browser adoption. PR172 is the integrated reference; performance remains PR169 at 3/5. [Report](verification/VERIFICATION-2026-10-23-LIVE-COLLECTION-UPDATES.md), [contract](decisions/LIVE-COLLECTION-UPDATES-2026-10-23.md). |
| Configured file-selection actions, FILE-ACTION/DATA/VIEW-01 | Merged in [PR #171](https://github.com/gonz0w/fvwm-x/pull/171); focused checks pass | Up to four configured literal executables receive a fully validated selected-path argument set; multi-entry observed properties and repeated initial --properties preserve selection ownership. Immediate cancellation has a consistent result. Five Go packages/51 cases pass on both targets and in Linux race; three real-theme drivers pass with 15 screenshots each. A classic observer selector fix repeats only that driver; qualified native binaries are unchanged. Cycle 2/3 and performance 2/5 after merge. [Report](verification/VERIFICATION-2026-10-23-FILE-SELECTION-ACTIONS.md), [contract](decisions/FILE-SELECTION-ACTIONS-2026-10-23.md). |
| Atomic metadata refresh, FILE-VIEW/DATA/ACTION-01 | Merged in [PR #170](https://github.com/gonz0w/fvwm-x/pull/170); focused checks pass | Bounded generation-checked transactions, snapshot reconciliation/no-op handling, observation revisions, visible-neighbor focus recovery and browser refresh/lost-selection feedback. Five Go packages/43 cases and the separate race run, vet and three real-theme native-X drivers pass; 13 screenshots per theme and updated manual captured/inspected. The PR167–169 six-target cycle and periodic comparison close first. PR170 advances the counters to 1/3 and 1/5; broader provider/native-update and protected-content handoffs remain open. [Report](verification/VERIFICATION-2026-10-23-COLLECTION-REFRESH.md), [contract](decisions/COLLECTION-REFRESH-2026-10-23.md). |
| Collection grid/ranges, FILE-VIEW/ACTION-01 | Merged in [PR #169](https://github.com/gonz0w/fvwm-x/pull/169); accumulated integration qualified | Native icon-grid over the shared model, keyboard/mouse additive ranges and browser --view/F7 switching preserve IDs/focus/selection. Both Linux/FreeBSD ARM64 pass seven native entries and five Go packages/36 cases; Linux race and three scoped sanitizer policy checks pass. A retiring-window observer correction reruns only three actual-X drivers; unchanged product/policy evidence is retained. Eleven screenshots per driver, new grid views inspected. The accumulated PR167–169 cycle is now closed; the new reference is PR169. [Report](verification/VERIFICATION-2026-10-23-COLLECTION-GRID.md), [contract](decisions/COLLECTION-GRID-2026-10-23.md). |
| Native scrolling collection, FILE-VIEW/ACTION-01 | Merged in [PR #168](https://github.com/gonz0w/fvwm-x/pull/168); accumulated integration qualified | One native viewport, keyboard/mouse focus/selection, bounded whole-view bitmap and browser adoption with acknowledged configuration batches. Linux/FreeBSD ARM64 each pass seven selected native entries and five Go packages/35 tests; Linux race, three scoped policy sanitizers and three composed actual-X drivers pass. Eight screenshots per driver captured; new UI inspected. Accumulated qualification now closes with PR169. Broader owner acceptance, incremental providers and protected clipboard content remain open. [Report](verification/VERIFICATION-2026-10-23-COLLECTION-LIST.md), [contract](decisions/COLLECTION-LIST-2026-10-23.md). |
| Portable browser target requests, FILE-DATA/ACTION/INTEGRATION-01 | Merged in [PR #167](https://github.com/gonz0w/fvwm-x/pull/167); accumulated integration qualified | Local path/URI requests preserve bytes; isolated atomic reveal and initial properties use generation-checked selection. Five Go packages/33 cases pass on Linux and FreeBSD ARM64; Linux race/vet/native-X with three browser instances pass. Nine screenshots are captured; two new views are inspected. The prior PR164–166 six-target cycle is qualified before this implementation; the PR167 merge advances the current cycle to 1/3. SQLite remains deferred behind the existing native content/storage contracts. [Report](verification/VERIFICATION-2026-10-23-FILE-BROWSER-HANDOFF.md), [contract](decisions/FILE-BROWSER-HANDOFF-2026-10-23.md). |
| File collection controls, FILE-VIEW/ACTION-01 | Merged in [PR #166](https://github.com/gonz0w/fvwm-x/pull/166); accumulated integration qualified | Bounded label filtering, deterministic observed sorting, cross-page selection and an observed-properties panel pass five Go packages/26 tests, scoped Go race/vet and the actual-X consumer. Seven screenshots are captured; four new/overview images are inspected. The UI fixture-only correction retains unchanged frozen policy results. The accumulated PR164–166 six-target checkpoint is now qualified; broader owner acceptance remains open. [Report](verification/VERIFICATION-2026-10-23-FILE-COLLECTION-CONTROLS.md), [contract](decisions/FILE-COLLECTION-CONTROLS-2026-10-23.md). |
| Collection/browser foundations, FILE-VIEW/DATA/ACTION-01 | Merged in [PR #165](https://github.com/gonz0w/fvwm-x/pull/165); accumulated integration qualified | Stable metadata/selection, byte-preserving isolated directory work and validated literal actions have a native paged consumer. Five Go packages/21 cases pass on Linux, FreeBSD and OpenBSD 8.0 ARM64; Linux race/vet and staged executable/manual/native UI pass. One command-client pass is retained after a test-flag-only registration correction. Full scrolling/grid, files provider, mutation and protected clipboard layers remain open. [Report](verification/VERIFICATION-2026-10-23-FILE-BROWSER-FOUNDATIONS.md), [contract](decisions/FILE-BROWSER-FOUNDATIONS-2026-10-23.md). |
| Saved launcher-opacity preview, CFG-01 / IPC-02 | Merged in [PR #164](https://github.com/gonz0w/fvwm-x/pull/164); accumulated integration qualified | Integer preference/schema migration, scoped Keep and save, captured draft return and orphan Form timeout pass nine selected Linux entries (74 desktop cases) and four scoped guest sanitizer exercises. Native screenshots expose and verify a label/notice correction. Core/file outcomes stay separate; general appearance preview remains open. The accumulated PR164–166 affected checkpoint now qualifies this subset; broader appearance acceptance stays open. [Report](verification/VERIFICATION-2026-10-22-SAVED-OPACITY-PREVIEW.md), [contract](decisions/SAVED-OPACITY-PREVIEW-2026-10-22.md). |
| Runtime menu-opacity preview, IPC-02 / CFG-01 | Merged in [PR #163](https://github.com/gonz0w/fvwm-x/pull/163); previous integrated reference | One atomic core scalar lease, typed connection/view controls and native Keep/Revert qualify accumulated PR161/162/current work: 140 selected passes across six targets/nine profiles, seven scoped guest core/MFL sanitizer exercises and portable 177 passes/14 skips. Actual-X screenshots are inspected. Saved-preference preview remains open; runtime Keep never persists. No C/helper/fixture correction follows freeze; data/docs amendments and original failures remain explicit. On merge reset cycle 0/3; performance PR159 becomes 4/5. [Report](verification/VERIFICATION-2026-10-22-RUNTIME-OPACITY-PREVIEW.md), [receipt](verification/VERIFICATION-2026-10-22-RUNTIME-OPACITY-PREVIEW.json). |
| Appearance policy and copied opacity, IPC-02 / CFG-01 | Merged in [PR #162](https://github.com/gonz0w/fvwm-x/pull/162); cycle 2/3 | Four related shared-freshness, menu/decor-page and CopyMenuStyle opacity improvements pass eight Linux entries and three scoped leak-enabled guest sanitizer exercises. Actual-X preserves new/existing target opacity and literal definitions; native desktop screenshots are captured/inspected. Original header/control failures stay explicit; no correction follows freeze. Cycle 1/3 merged, 2/3 on merge; accumulated platform/release gates remain due. [Report](verification/VERIFICATION-2026-10-22-IPC-APPEARANCE-STATE.md), [receipt](verification/VERIFICATION-2026-10-22-IPC-APPEARANCE-STATE.json). |
| Core colorset inspection, IPC-02 / CFG-01 | Merged in [PR #161](https://github.com/gonz0w/fvwm-x/pull/161); cycle 1/3 | Three bounded inventory, exact sparse-ID and summary/freshness capabilities expose published values and pending/refused preparation. Seven focused Linux entries and three scoped leak-enabled guest sanitizer exercises pass; final unit readability edits repeat policy only with exact product/fixture retention. Cycle 0/3 merged, 1/3 on merge. Style/decor generations and safe preview stay open. No new UI, matrix/install/BSD or benchmark repeat. [Report](verification/VERIFICATION-2026-10-22-IPC-COLORSET-STATE.md), [receipt](verification/VERIFICATION-2026-10-22-IPC-COLORSET-STATE.json). |
| Owned desktop operations, IPC-02 | Merged in [PR #160](https://github.com/gonz0w/fvwm-x/pull/160); integrated reference | Five typed refresh/state/result/cancel/retention/notification capabilities adopt existing desktop scans. One combined checkpoint passes 122 selected entries across six targets/nine profiles, five scoped guest sanitizer exercises and portable 177 passes/14 skips. Read-only, busy, stale, failures, late output, reconnect and bounds are explicit. No new UI or install/default-X repeats; wider domain/release acceptance stays open. [Report](verification/VERIFICATION-2026-10-22-IPC-DESKTOP-OPERATIONS.md), [receipt](verification/VERIFICATION-2026-10-22-IPC-DESKTOP-OPERATIONS.json). |
| Desktop-provider observation, IPC-02 | Merged in [PR #159](https://github.com/gonz0w/fvwm-x/pull/159); cycle 2/3 | Desktop-owner capability/configuration state, opt-in identity/generation invalidations and conditional settings snapshots pass seven focused Linux entries (54 desktop policy cases) and two scoped leak-enabled guest sanitizer exercises. Core bytes remain PR158; source/queue/reconnect/absence boundaries are explicit. Shared cycle 1/3 merged, 2/3 on merge; six-target/alternate-profile and release acceptance remain deferred. No new UI. [Report](verification/VERIFICATION-2026-10-22-IPC-PROVIDER-OBSERVATION.md), [receipt](verification/VERIFICATION-2026-10-22-IPC-PROVIDER-OBSERVATION.json). |
| Binding scope inspection, IPC-02 | Merged in [PR #158](https://github.com/gonz0w/fvwm-x/pull/158); cycle 1/3 | Three menu/fallback, normalized placement and context/modifier policy features pass seven selected Linux GCC entries and three scoped leak-enabled guest sanitizer exercises. Frozen source guards pass; actual-X verifies overrides, literals and policy generations. Shared cycle 0/3 merged, 1/3 after merge; six-target/alternate-profile and release gates remain explicitly deferred. No new UI. [Report](verification/VERIFICATION-2026-10-22-IPC-BINDING-SCOPES.md), [receipt](verification/VERIFICATION-2026-10-22-IPC-BINDING-SCOPES.json). |
| Directed core state queries/readiness, IPC-02 | Merged in [PR #157](https://github.com/gonz0w/fvwm-x/pull/157); integrated reference | Three topology/workspace/global-binding features and a reproduced provider-readiness fix qualify accumulated PR155/156 work: 113 composed selected passes across six targets/nine profiles, five scoped guest sanitizer exercises and portable 177 passes/14 skips. The correction retains 74 unchanged entries and runs 39 affected/missing entries; original failures and incomplete AMD64 capture remain explicit. No new UI or install/default-X repeats. Broader owner/release gates stay open. [Report](verification/VERIFICATION-2026-10-22-IPC-CORE-STATE.md), [receipt](verification/VERIFICATION-2026-10-22-IPC-CORE-STATE.json). |
| Settings validation and pending waits, IPC-02 | Merged in [PR #156](https://github.com/gonz0w/fvwm-x/pull/156); included in the current combined cycle | Non-publishing generation-checked validation, connection-local pending snapshots and wait-only cancellation pass six Linux ARM64 GCC entries (51 desktop policy cases), plus three scoped leak-enabled guest sanitizer exercises. Actual-X proves cancellation releases partial buffers/slots yet a submitted write can still commit. An initial fixture expiry-order assumption is corrected and final focused checks pass. No new UI, installs/BSD/full matrix; deferred cycle gates remain explicit. [Report](verification/VERIFICATION-2026-10-22-IPC-LIFECYCLE.md), [receipt](verification/VERIFICATION-2026-10-22-IPC-LIFECYCLE.json). |
| Desktop settings provider, IPC-02 | Merged in [PR #155](https://github.com/gonz0w/fvwm-x/pull/155); included in the current combined cycle | Registry/value snapshots, generation-checked atomic changes and correlated scoped reset/reload pass six selected registered entries on one Linux ARM64 GCC profile (49 desktop policy cases), plus two leak-enabled scoped guest sanitizer exercises. Review adds explicit provider identity after a multiple-owner reply ambiguity; final affected Linux and sanitizer gates pass. The earlier test-vector warning cleanup repeats only native policy; all original captures remain source-scoped. No installs/BSD/full matrix; deferred cycle/release gates are explicit above. No new UI. [Report](verification/VERIFICATION-2026-10-22-IPC-SETTINGS.md), [receipt](verification/VERIFICATION-2026-10-22-IPC-SETTINGS.json). |
| Correlated MFL requests, IPC-02 | Merged in [PR #154](https://github.com/gonz0w/fvwm-x/pull/154); first subset qualified, broader owner open | Three request/error, core query/barrier and read-only admission features pass 82 selected registered entries across six targets/ten profiles, ten staged request-X sessions, four authenticated default BSD displays and two leak-enabled guest sanitizer exercises. Ten MFL executables change; 140 other native ELF targets / 77 catalogs retain exact PR153 identities. All 927 install files are verified. Fourteen OpenBSD selected tests and both complete installs are retained through exact cache/package identity; only unfinished installed-session gates run in completion jobs. Original private runner failures remain explicit. Old broad MFL-dependent passes are historical, not current composed qualification. No new UI is selected. Full providers, applied actions and preview/recovery remain open. [Report](verification/VERIFICATION-2026-10-22-IPC-REQUESTS.md), [receipt](verification/VERIFICATION-2026-10-22-IPC-REQUESTS.json). |
| Non-executing desktop configuration checks, CFG-02 | Merged in [PR #153](https://github.com/gonz0w/fvwm-x/pull/153); first subset functionally qualified | Literal settings/Read analysis, source/continuation/include context with offline text/JSON, and a native paged report pass 20 affected entries across six targets/ten profiles. Ten stages and four authenticated default BSD displays pass; all 150 native targets / 77 catalogs retain exact identities and 2931 unchanged composed entries remain retained. One original Linux software staged timeout and its unchanged isolated completion remain explicit. General core runtime diagnostics stay open; IPC-02 follows. [Report](verification/VERIFICATION-2026-10-22-CONFIG-CHECKER.md), [receipt](verification/VERIFICATION-2026-10-22-CONFIG-CHECKER.json). |
| Grouped desktop settings editor, CFG-01 | Merged in [PR #152](https://github.com/gonz0w/fvwm-x/pull/152); functionally qualified, with existing Form exit-cleanup gap | Grouped atomic Apply, scoped unsaved Defaults/Cancel and bounded generation-bound consumer retirement pass 20 affected entries across six targets/ten profiles, two observer follow-ups, ten helper stages and four authenticated default BSD displays. All 150 native ELF targets / 77 catalogs remain exact; 2931 unchanged PR151 entries are retained. The leak-enabled functional exercise exposes existing native Form fontconfig exit allocations, independently reproduced without the editor and retained under CLEAN-01 / Later. Live preview stays unavailable pending shared-core conflict recovery. [Report](verification/VERIFICATION-2026-10-22-SETTINGS-EDITOR.md), [receipt](verification/VERIFICATION-2026-10-22-SETTINGS-EDITOR.json). |
| Typed desktop settings, CFG-01 | Merged in [PR #151](https://github.com/gonz0w/fvwm-x/pull/151); locally qualified | Registry/query, private saved/effective change/reset/reload and native menu consumer pass 2420 composed standard / 531 optional entries, ten installs, four authenticated default BSD displays and three guest sanitizer checks. OpenBSD AMD64 retains 223 completed entries and runs only 73 unfinished entries; original coordinator/capture failures remain explicit. Preview/recovery stays open and unavailable. [Report](verification/VERIFICATION-2026-10-22-DESKTOP-SETTINGS.md), [receipt](verification/VERIFICATION-2026-10-22-DESKTOP-SETTINGS.json). |
| Atomic grouped button updates, BUTTON-02 | Merged in [PR #150](https://github.com/gonz0w/fvwm-x/pull/150); locally qualified | Whole-group validation, resource rollback/startup recovery and final-state focus/repaint pass 106 fresh affected entries across six targets/ten profiles, five leak-enabled guest sanitizers, ten fresh stages and four authenticated default BSD displays. Exact unchanged consumers retain 2332 PR149 foundation entries; composed standard/selected optional totals are 2420/18. BUTTON-02 is complete; CFG-01 follows. [Report](verification/VERIFICATION-2026-10-22-BUTTONS-ATOMIC.md), [receipt](verification/VERIFICATION-2026-10-22-BUTTONS-ATOMIC.json). |
| Native button navigation and state, BUTTON-01 / BUTTON-02 | Merged in [PR #149](https://github.com/gonz0w/fvwm-x/pull/149); locally qualified | Four feature improvements pass 2412 standard / 8 selected optional entries, four guest sanitizers, ten installs and four authenticated default BSD displays. BUTTON-01 is complete; BUTTON-02's live state subset is accepted. [Report](verification/VERIFICATION-2026-10-22-BUTTONS-KEYBOARD.md). |
| Manager target admission / feature cutoff, SELECTION-01 / DATA-01 | Merged in PR #148; composed checkpoint locally qualified | One bounded request-admission repair and independent existing-conversion dispositions pass 2396 standard / 38 selected optional entries, 72 affected follow-ups, nine fixture-continuation groups and four scoped guest sanitizers. Ten final installs and composed authenticated BSD-X checks pass; original storage, observer and cleanup-assertion failures remain explicit. [Report](verification/VERIFICATION-2026-10-21-MANAGER-TARGETS.md), [receipt](verification/VERIFICATION-2026-10-21-MANAGER-TARGETS.json). Final general prerequisite checkpoint; BUTTON/CFG/IPC features follow. |
| Legacy strut / root work-area safety, EWMH-01 / DATA-01 / INPUT-01 | Merged in PR #147; locally qualified composed checkpoint | Three admission, revocation and root-publication repairs pass one broad run (2372 standard / 30 selected optional) plus 90 affected review-continuation entries across six targets/ten profiles. Four final scoped sanitizers, ten final installs and four final authenticated BSD displays pass. Original selector/compiler/storage failures and the reproduced sticky-page correction remain explicit. [Report](verification/VERIFICATION-2026-10-21-WORKAREA.md). |
| Desktop root publication, EWMH-01 / DATA-01 / INPUT-01 | Merged in PR #146; bounded checkpoint locally qualified | Three pager-index, count-adoption and bounded dependent-array repairs pass 2340 standard / 22 selected optional entries across six targets, four scoped sanitizer entries, ten fresh installs and four authenticated BSD displays. Original configure/manual-observer failures and independent old-core controls remain explicit. [Report](verification/VERIFICATION-2026-10-21-DESKTOP-PUBLICATION.md). |
| Manager startup handoff, SELECTION-01 / DATA-01 | Merged in PR #145; bounded checkpoint locally qualified | Three manager announcement, bounded/cancellable handoff and ownership/refusal repairs pass 2308 standard / 14 selected optional entries on six targets, four scoped sanitizer entries, ten fresh installs and four authenticated BSD displays. Original fixture failures and isolated baseline controls remain explicit. [Report](verification/VERIFICATION-2026-10-21-MANAGER-HANDOFF.md). |
| Xlib dependency contract, LIB-05 / MAINT-02 | Merged in PR #144; bounded checkpoint locally qualified | 2284 standard / 33 selected optional entries pass across six targets/ten profiles; 30 native configure controls, ten fresh installs and four authenticated BSD displays pass. Generated configuration hashes remain exact; dependency introspection and emitted link commands contain no direct libxkbcommon requirement. [Report](verification/VERIFICATION-2026-10-21-XLIB-DEPENDENCY.md). |
| Safety/maintainability closeout | All three selected review families locally qualified; merged in PR #143 | Three final cancellation repairs pass 2276 standard / 47 selected optional entries on six targets, scoped sanitizers, ten installs and four authenticated BSD displays. Every selected stream/input/lifetime entry point has a fixed/verified or already-covered disposition. The approved feature cutoff supersedes further general stage 5 pickup; broad audit/release owners remain open. [Report](verification/VERIFICATION-2026-10-21-FUNCTION-LIFETIMES.md), [handoff criteria](#hardening-closeout). |
| BSD preparation evidence, CI-01 / MAINT-02 / CLEAN-01 | Merged in [PR #139](https://github.com/gonz0w/fvwm-x/pull/139); affected checkpoint passes | Three preparation/evidence repairs pass 282 native policy cases on six targets, two fresh OpenBSD runner cases and six actual loader invocations; original WM/installed bytes remain exact. Live phases and an expected exit 23 case validate diagnostics/cleanup. Portable 177 passes / 11 skips; PR138 runtime evidence retained. [Report](verification/VERIFICATION-2026-10-21-BSD-PREPARATION.md). |
| Shared string ownership, DATA-01 / CLEAN-01 | Merged in [PR #138](https://github.com/gonz0w/fvwm-x/pull/138); composed checkpoint passes | Three shared first-line, multiline and quoted-copy operations retain existing bytes and ownership while avoiding before-source pointers and removing one quoted strlen rescan. One composed six-target checkpoint passes 2208 standard entries, 28 affected optional entries and one scoped guest sanitizer. Ten installs, staged geometry/styles and four authenticated default BSD displays pass; harness failures and wider release limits remain explicit. [Report](verification/VERIFICATION-2026-10-21-STRING-COPIES.md). |
| Protocol requirement register, STANDARDS-01 / EWMH-01 / SELECTION-01 | Merged on main through [PR #136](https://github.com/gonz0w/fvwm-x/pull/136), including PR #137; runtime acceptance remains open | Forty clause/role records refine the existing E/S reconciliation: root capabilities/desktops, window hints/requests and selection ownership/transfer. Historical evidence retains exact profile/source limits; manager announcement and desktop-publication source findings have concrete owner handoffs. No runtime, benchmark or full matrix repeat. [Register](verification/STANDARDS-2026-10-21-PROTOCOL-REQUIREMENTS.md). |
| OpenBSD 8.0 release baseline, CI-01 / COMPAT-01 / IMAGE-01 / LIB-07 | Merged in [PR #136](https://github.com/gonz0w/fvwm-x/pull/136); composed local checkpoint passes | OpenBSD follows new releases; 8.0 is the current qualified baseline on AMD64/ARM64 and 7.9 is out of scope. 538 standard and 34 optional entries pass, with full installs, 44 authenticated default servers and staged/default-X 1x/1.5x geometry. All formerly missing entries run; original failures and focused corrections remain recorded. Linux/FreeBSD scope and wider release gates remain. [Report](verification/VERIFICATION-2026-10-20-OPENBSD-80.md). |
| Core module/icon/event records, MAINT-02 / CLEAN-01 | Merged in [PR #135](https://github.com/gonz0w/fvwm-x/pull/135); affected checkpoint passes | Three module/icon-box/event-filter dependency improvements pass 185 affected native checks across six targets/ten profiles (19 focused retained / 166 fresh). All 5981 object pairs, ten WM executables and 249 install-output pairs preserve allocated code/data or non-ELF bytes. Ten scoped stages verify 249 identities. ARM GCC layout fanout falls 68/216/37 to 30/6/2 objects. Broader PR134 runtime evidence and inherited OpenBSD gaps remain source-scoped. [Report](verification/VERIFICATION-2026-10-20-CORE-RECORD-BOUNDARIES.md). |
| Swallowed-client session replies, DATA-01 / FD-01 / CLEAN-01 | Merged in [PR #134](https://github.com/gonz0w/fvwm-x/pull/134); composed checkpoint passes | Three registration/pending-queue/property repairs pass a composed six-target/ten-profile checkpoint after one original full run: 2197 standard and 475 optional results. The composition retains 2516 unaffected original passes and uses 156 affected entries (122 fresh / 34 retained by exact inputs and eleven binaries per focused target). Fifty callback entries, twenty pending-peer entries, nineteen real-session drivers with 57 independent clients and 171 property replies, fifteen scoped guest sanitizer entries, nine fresh stages and four final authenticated default BSD session displays pass. Three inherited required OpenBSD AMD64 omissions and its install/two staged-geometry gap remain without retries. The existing no-automatic-save policy is preserved. Performance reference PR132 remains at 1/5; no comparison is due. [Report](verification/VERIFICATION-2026-10-20-SESSION-CALLBACKS.md). |
| Process and session startup transactions, FD-01 / PROC-01 / DATA-01 | Merged in [PR #133](https://github.com/gonz0w/fvwm-x/pull/133); composed checkpoint passes | Three process/session constructor repairs pass one composed checkpoint after a single full matrix: 2141 composed standard results (2116 original fresh passes, 24 retained after unchanged-input/eight-binary checks per focused target, and one corrected Exec probe), plus 468 optional results across six targets/ten profiles. Eleven actual-core Exec drivers verify 77 observations; seventy session fault/retry entries and nineteen real session integrations pass. Nine scoped guest ASan/UBSan checks, nine fresh stages and four authenticated default BSD displays pass. Three inherited required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. Performance reference PR132 is accepted, at 1/5 subsequent merged implementation checkpoints; no comparison is due. [Report](verification/VERIFICATION-2026-10-20-PROCESS-STARTUP.md). |
| Visual startup ownership, DATA-01 / IMAGE-01 / CLEAN-01 | Merged in [PR #132](https://github.com/gonz0w/fvwm-x/pull/132); combined checkpoint passes | Three startup ownership improvements pass one combined checkpoint: 2077 standard results (2071 fresh / six retained by unchanged runtime/test/build inputs and six exact binary identities), plus 459 optional results across six targets/ten profiles. Eleven pure policy drivers verify 352 observations; fifteen real-X drivers verify 60 initialization observations. Three scoped guest ASan/UBSan checks, nine fresh stages and four authenticated default BSD displays pass. Three inherited required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. First paired performance observation recorded; regular comparisons every five merged implementation checkpoints. [Runtime report](verification/VERIFICATION-2026-10-20-VISUAL-STARTUP.md), [performance reference](verification/PERFORMANCE-2026-10-20.md). |
| Window-tree walks, DATA-01 / CLEAN-01 | Merged in [PR #131](https://github.com/gonz0w/fvwm-x/pull/131); combined checkpoint passes | Three ancestor/pointer/frame lookup improvements pass one combined checkpoint: 2061/2061 standard results (2055 fresh / six retained by unchanged runtime/test/build inputs and eight binary identities), plus 455/455 optional entries across six targets/ten profiles. Fifteen real-X drivers verify 45 hierarchy/pointer/reparent observations; ten boundary-policy profiles and three scoped guest sanitizers pass. Nine fresh stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-20-WINDOW-TREE-WALKS.md). |
| Donated icon retention, DATA-01 / IMAGE-01 / CLEAN-01 | Merged in [PR #130](https://github.com/gonz0w/fvwm-x/pull/130); combined checkpoint passes | Three donated-icon record/size/retirement improvements pass one combined checkpoint: 2045/2045 standard results (2037 fresh / eight retained by unchanged production/test/build inputs and nine binary identities), plus 451/451 optional entries across six targets/ten profiles. Selected partial deletion allocations fall three to two. Fifteen actual-core drivers verify 510 icon cases; four scoped guest sanitizers, nine fresh stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-20-EWMH-ICON-RETENTION.md). |
| Picture-cache lookup and retirement, CACHE-01 / CLEAN-01 | Merged in [PR #129](https://github.com/gonz0w/fvwm-x/pull/129); combined checkpoint passes | Three picture-cache variant/name/retirement improvements pass one combined checkpoint: 2045/2045 standard results (2038 fresh / seven retained by exact source and nine binary identities), plus 451/451 optional entries across six targets/ten profiles. Selected pathname comparisons fall 65 to one/two; two predecessor walks fall 16,384/32,385 to zero. Five scoped guest sanitizers, nine fresh full stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-20-PICTURE-CACHE-WORK.md). |
| Private condition/startup/monitor records, MAINT-02 / CLEAN-01 | Merged in [PR #128](https://github.com/gonz0w/fvwm-x/pull/128); affected checkpoint passes | Three condition/startup/monitor-adapter dependency improvements pass 149/149 affected native entries across six targets/ten profiles (15 retained focused / 134 fresh). All 5914 object pairs, ten WM executables and 249 install-output pairs preserve allocated code/data or non-ELF bytes. Ten scoped stages verify 249 identities. ARM GCC layout fanout falls 214/214/230 to 2/22/2 objects. Broader PR127 runtime evidence and required OpenBSD gaps remain source-scoped. [Report](verification/VERIFICATION-2026-10-20-PRIVATE-RECORD-BOUNDARIES.md). |
| Style flag mutations, STYLE-01 / CLEAN-01 | Merged in [PR #127](https://github.com/gonz0w/fvwm-x/pull/127); combined checkpoint passes | Three typed flag union/intersection/clearing operations pass one combined checkpoint: 2045/2045 standard results (2035 fresh / ten retained by exact source and twelve binary identities), plus 451/451 optional entries across six targets/ten profiles. Eleven mutation drivers verify 10,982,730 reference cases; fifteen actual-core drivers retain independent style-order/simplification coverage. Four scoped guest sanitizers, nine fresh full stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-20-STYLE-FLAG-MUTATIONS.md). |
| Style flag predicates, STYLE-01 / CLEAN-01 | Merged in [PR #126](https://github.com/gonz0w/fvwm-x/pull/126); combined checkpoint passes | Three read-only subset/intersection/masked-equality improvements pass one combined checkpoint: 2037/2037 standard results (2028 fresh / nine retained by exact non-Markdown source and eleven binary identities), plus 449/449 optional entries across six targets/ten profiles. Eleven pure drivers verify 14,864,729 reference cases; fifteen actual-core drivers verify 17,310 wrapper cases and 1,920 parsed conditions. Four scoped guest sanitizers, nine fresh full stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-19-STYLE-FLAGS.md). |
| Style matching work, STYLE-01 / CLEAN-01 | Merged in [PR #125](https://github.com/gonz0w/fvwm-x/pull/125); combined checkpoint passes | Three related style preparation/predicate/candidate-merge improvements pass one combined checkpoint: 2029/2029 standard results (2021 fresh / eight retained by exact source and ten binary identities), plus 447/447 optional entries across six targets/ten profiles. Instrumented escape scans fall eight to zero for twelve preparations. 180 actual-core merge cases and 132 preparation cases pass. Four scoped guest sanitizers, nine fresh full stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-19-STYLE-MATCH-WORK.md). |
| Legacy tray membership, STATE-01 / CLEAN-01 | Merged in [PR #124](https://github.com/gonz0w/fvwm-x/pull/124); combined checkpoint passes | Three existing legacy-tray insertion/retirement/publication improvements pass one combined checkpoint: 2029/2029 standard results, fresh or retained after source/binary verification, and 447/447 optional entries across six targets/ten profiles. Four focused standard entries are retained; 2025 standard entries run fresh. Selected 64-window add/remove snapshots each reduce heap requests from one to zero. Source review shows two insertion/retirement searches become one, and two publication walks become one. Four scoped sanitizers, 9 fresh full stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its broader install/staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-19-EWMH-TRAY.md). |
| UI record type boundaries, MAINT-02 / CLEAN-01 | Merged in [PR #123](https://github.com/gonz0w/fvwm-x/pull/123); affected checkpoint passes | Three font/picture/monitor dependency improvements pass 170/170 affected native entries across six targets/ten profiles (17 focused entries retained by exact source/binary identity, 153 fresh). All 5874 object pairs, ten WM executables and 249 install-output pairs preserve allocated code/data or non-ELF bytes. Ten scoped stages verify 249 code/catalog identities. ARM GCC layout fanout falls from 252/289/226 to 49/66/92 objects. Broader runtime evidence and required OpenBSD gaps remain source-scoped to PR122. [Report](verification/VERIFICATION-2026-10-19-UI-TYPE-BOUNDARIES.md). |
| EWMH publication work, STATE-01 #37 | Merged in [PR #122](https://github.com/gonz0w/fvwm-x/pull/122); affected checkpoint passes | Three EWMH publication improvements pass one combined checkpoint: 2029/2029 executed standard and 447/447 optional entries across six targets/ten profiles. Selected strut walks fall from two to one, permission queries from nine to six and converted-name length scans from 189 to 63. Four scoped sanitizers, 9 fresh stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its broader install/staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-19-EWMH-PUBLICATION.md). |
| Command/update type boundaries, MAINT-02 / CLEAN-01 | Merged in [PR #121](https://github.com/gonz0w/fvwm-x/pull/121); affected checkpoint passes | Three command/update dependency improvements pass 100/100 affected native entries across six targets/ten profiles. All 5864 object pairs and ten WM executables retain identical allocated code/data sections; 249 installed-output pairs preserve their code/data or non-ELF bytes. Ten scoped stages verify 249 code/catalog identities. Parser, complex-function and update-plan layout edits now compile six, four and four objects instead of 204–209, 71 and 116–121. Broader PR120 runtime evidence/failures and required OpenBSD CI/SVG/install gaps remain without retries. [Report](verification/VERIFICATION-2026-10-19-COMMAND-TYPE-BOUNDARIES.md). |
| Style index retention, STYLE-01 #38 | Merged in [PR #120](https://github.com/gonz0w/fvwm-x/pull/120); affected checkpoint passes | Three related improvements pass a composed affected checkpoint: one original broad run records 2029/2030 executed standard and 447/447 optional entries across six targets/ten profiles, plus one unchanged isolated legacy-property control. Fifteen actual-core retention drivers verify live-domain counts, empty-table release and seven-category precedence; fifteen retained maintenance drivers cover incremental mutation and simplification. Four scoped sanitizers, nine fresh stages and four authenticated BSD displays pass. Two known required OpenBSD AMD64 CI/SVG omissions and its install/staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-19-STYLE-INDEX-RETENTION.md). |
| Style type boundaries, MAINT-02 / CLEAN-01 | Merged in [PR #119](https://github.com/gonz0w/fvwm-x/pull/119); affected checkpoint passes | Three style dependency boundaries pass 70/70 affected native entries across six targets/ten profiles. 5844 object pairs and all production WM/module outputs retain allocated code/data sections; ten matcher-test objects have separately proved assertion-line-only differences. Ten scoped stages verify 249 code/catalog identities. Broader PR118 runtime failures/controls, required OpenBSD CI/SVG omissions and install/staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-19-STYLE-TYPE-BOUNDARIES.md). |
| Incremental style indexes, STYLE-01 #38 | Merged in [PR #118](https://github.com/gonz0w/fvwm-x/pull/118); composed affected checkpoint passes; required gaps retained | Three incremental style-index improvements pass a composed checkpoint: one original broad run across six targets/ten profiles (2019/2022 executed standard, 445/445 optional), followed by three unchanged isolated fixture controls. Fifteen actual-core index drivers retain zero selected lookup allocations after append, deletion and all three simplification paths. Three scoped sanitizers, nine fresh stages and four authenticated BSD displays pass. Two known required OpenBSD AMD64 CI/SVG omissions and its install/staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-19-STYLE-INDEX-MAINTENANCE.md). |
| Style command ownership, STYLE-01 #38 | Merged in [PR #117](https://github.com/gonz0w/fvwm-x/pull/117); composed affected checkpoint passes; required gaps retained | Three style-command fixes pass a composed checkpoint: one original broad run (2013/2014 executed standard, 443/443 optional), then ten scheduler-fixture correction checks on unchanged production binaries. Fifteen ownership drivers retain zero selected fields and perform zero refused-input grabs. Three scoped sanitizer checks, nine composed fresh stages and four authenticated BSD displays pass. Two known required OpenBSD AMD64 CI/SVG omissions and its install/staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-19-STYLE-COMMAND-OWNERSHIP.md). |
| Style update work, STYLE-01 #38 | Merged in [PR #116](https://github.com/gonz0w/fvwm-x/pull/116); composed affected checkpoint passes; CI/SVG gaps retained | Three style-update improvements pass a composed affected checkpoint: one original broad run (2005/2006 executed standard, 441/441 optional), seventy production-correction checks and eighteen fixture-correction checks. The original image-effects timeout is traced to a discarded early acknowledgement and corrected without changing production deadlines. Six original sanitizer checks, three corrected actual-core checks, nine final fresh stages and four final authenticated BSD displays pass. Two known required OpenBSD AMD64 CI/SVG omissions and its install gap remain without retries. [Report](verification/VERIFICATION-2026-10-19-STYLE-UPDATE-WORK.md). |
| Shared wildcard matching, STYLE-01 #38 | Merged in [PR #115](https://github.com/gonz0w/fvwm-x/pull/115); affected checks pass; CI/SVG gaps retained | Three shared wildcard improvements pass 50/50 affected entries across six targets/ten profiles. One combined checkpoint passes 1998/1998 executed standard and 439/439 optional entries, five scoped sanitizers, nine fresh stages and four authenticated default BSD displays. Two known required OpenBSD AMD64 CI/SVG entries stay omitted, preserving its install/staged-geometry gap without retries. [Report](verification/VERIFICATION-2026-10-19-SHARED-WILDCARDS.md). |
| Style field work, STYLE-01 #38 | Merged in [PR #114](https://github.com/gonz0w/fvwm-x/pull/114); affected checks pass; CI/SVG gaps retained | Three style field improvements pass 40/40 affected entries across six targets/ten profiles. One combined checkpoint passes 1990/1990 executed standard and 437/437 optional entries, four scoped sanitizers, nine fresh stages and four authenticated default BSD displays. Two known required OpenBSD AMD64 CI/SVG entries stay omitted, preserving its install/staged-geometry gap without retries. [Report](verification/VERIFICATION-2026-10-19-STYLE-FIELDS.md). |
| Decoded style anchors, STYLE-01 #38 | Merged in [PR #113](https://github.com/gonz0w/fvwm-x/pull/113); affected checks pass; broader CI/SVG gaps retained | Three decoded style proofs pass the affected scope of one combined checkpoint: 1990/1991 executed standard (one retained CI-worker timeout) and 437/437 optional entries across six targets/ten profiles, four scoped sanitizers, nine fresh stages and four authenticated default BSD displays. OpenBSD AMD64 retains the unchanged CI-01 two-second worker failure and IMAGE-01 required SVG omission/install gap. Three selected lists retain two records instead of three. Independent reference/collision/precedence/removal owners remain. No retained metadata/allocation or ordinary matching limit is added. [Report](verification/VERIFICATION-2026-10-19-STYLE-ANCHORS.md). |
| EWMH atom work, STATE-01 #37 | Merged in [PR #112](https://github.com/gonz0w/fvwm-x/pull/112); combined checkpoint locally qualified | Three EWMH atom improvements pass one combined checkpoint: 1991/1991 executed standard and 437/437 optional entries across six targets/ten profiles, three scoped sanitizers, nine fresh stages, four authenticated default BSD displays and 18 geometry invocations/126 cases. The known required OpenBSD AMD64 SVG omission/install gap remains under IMAGE-01. Duplicate priority, configuration and publication barriers remain. [Report](verification/VERIFICATION-2026-10-19-EWMH-ATOMS.md). |
| Escaped selector disjointness, STYLE-01 #38 | Merged in [PR #111](https://github.com/gonz0w/fvwm-x/pull/111); composed affected checkpoint passes | Three escaped selector proofs use decoded lengths, quoted fixed positions and bounded literal membership. Three selected lists retain two records instead of three, preserving initialized lookup, precedence and removal. Original breadth passes 1991/1991 executed standard and 437/437 optional entries; the final literal-fast-path correction passes 40/40 affected entries plus four scoped sanitizers. Nine final stages, four authenticated BSD displays and 290 final client cases pass. One required OpenBSD AMD64 SVG test stays omitted under IMAGE-01 with its install/staged-geometry gap. No metadata/allocation owner change or universal latency claim. [Report](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED-OVERLAP.md). |
| Escaped style matching, STYLE-01 #38 | Merged in [PR #110](https://github.com/gonz0w/fvwm-x/pull/110); affected checkpoint passes | Three escaped matching families replace recursive entries with decoded fixed widths, anchored one-star spans and constant-stack general cursors. Nine selected legacy calls become zero with unchanged results/removal. 1991/1991 executed standard and 437/437 optional entries pass; one known required OpenBSD AMD64 SVG test stays omitted under IMAGE-01, preserving its install/staged-geometry gap. Four scoped sanitizers, nine fresh stages, four authenticated BSD displays and 810 family rows pass. Metadata adds sixteen inline bytes; no universal latency/linear-time claim. [Report](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED.md). |
| Style selector disjointness, STYLE-01 #38 | Merged in [PR #109](https://github.com/gonz0w/fvwm-x/pull/109); affected checkpoint passes | Three selector proofs reduce three selected lists from three records to two, preserving reference/collision/matching/removal owners. Final bounded source passes 40/40 affected entries plus four scoped sanitizers, nine fresh stages and four authenticated BSD displays; original broad source passes 1991/1992 standard and 437/437 optional entries. The unchanged OpenBSD AMD64 SVG startup failure and its fresh-install/staged-geometry gap remain under IMAGE-01. No universal latency claim. [Report](verification/VERIFICATION-2026-10-18-STYLE-OVERLAP.md). |
| Style wildcard families, STYLE-01 #38 | Merged in [PR #108](https://github.com/gonz0w/fvwm-x/pull/108); locally qualified | Three families share one combined checkpoint: exact-width questions, anchored one-star question spans and constant-stack general unescaped cursors. Original results pass 1992/1992 standard and 437/437 optional entries; four scoped sanitizers pass. 10 fresh stages, 4 authenticated default BSD displays and 630 family observations retain exact scope. Legacy empty-tail/escaped behavior and all ordering barriers remain. [Report](verification/VERIFICATION-2026-10-18-STYLE-CURSORS.md). |
| Development build reuse, MAINT-02 / CLEAN-01 | Merged in [PR #107](https://github.com/gonz0w/fvwm-x/pull/107); affected checkpoint locally qualified | Three build/dependency improvements remove 74 duplicate Linux core compilations, reduce operation-API fanout to 7 objects, and keep private index edits in 2 owner objects. All 70 affected entries pass ten profiles/six targets; 1502 core/probe objects and ten WM executables retain identical allocated code/data sections. Ten fresh stages verify 249 code/catalog identities. Collector repairs preserve successful phases; unchanged broader runtime evidence stays with PR106. [Report](verification/VERIFICATION-2026-10-18-DEVELOPMENT-BUILD-REUSE.md). |
| Style subject work, STYLE-01 #38 | Merged in [PR #106](https://github.com/gonz0w/fvwm-x/pull/106); locally qualified with composed evidence | Deferred compatibility work, domain-selective index queries and shared hash/length scans form one batch. The measured numeric-only string work drops to zero; explicit-name hashing drops from 114,688 to 16,384 byte steps and avoids a separate 16,384-byte length scan. One combined checkpoint passes 1991 standard/437 optional entries, four scoped sanitizers, ten fresh stages and five default BSD displays; the one original failed entry passes an unchanged isolated control. No universal latency claim. [Report](verification/VERIFICATION-2026-10-18-STYLE-SUBJECT.md). |
| Ordered style candidate lookup, STYLE-01 #38 | Merged in [PR #105](https://github.com/gonz0w/fvwm-x/pull/105); locally qualified with composed evidence | Three ordered candidate paths share one checkpoint. Original observations pass 1989/1992 standard and 437/437 optional entries across six targets / ten profiles; 4 scoped sanitizer entries pass. 10 fresh stages bind 150 code/99 catalog identities. 5 authenticated default BSD displays and 10 staged real-client invocations pass. One full matrix serves the batch; hashing/collision/cold-build and release limitations remain explicit. [Report](verification/VERIFICATION-2026-10-18-STYLE-LOOKUP.md). |
| Style simplification batch, STYLE-01 #38 | Merged in [PR #104](https://github.com/gonz0w/fvwm-x/pull/104); locally qualified | Three repeated-ID improvements are locally qualified. Original observations pass 1991/1992 standard and 437/437 optional entries across six targets / ten profiles. The one failed startup fixture is corrected and passes on unchanged binaries; its original failure remains visible. 4 scoped sanitizer entries pass. 10 fresh stages bind 150 code/99 catalogs; four default BSD displays and 10 staged real-client invocations pass. One full checkpoint serves the batch; preparation failures and scoped limitations remain explicit. [Report](verification/VERIFICATION-2026-10-16-STYLE-SIMPLIFICATION.md). |
| Style wildcard batch, STYLE-01 #38 | Merged in [PR #103](https://github.com/gonz0w/fvwm-x/pull/103); locally qualified | Three related wildcard improvements are locally qualified. The original matrix records 1988/1992 standard and 437/437 optional passes; its four BSD counter assertions are resolved by portable outer-call accounting. Final affected scope passes 30 native entries, three scoped sanitizer entries and ten staged client probes / 70 cases; all ten stages verify 150 code/99 catalogs. Four final default BSD displays add eight geometry invocations / 56 cases. One full matrix serves the batch; corrected results and failures retain exact source scopes. [Report](verification/VERIFICATION-2026-10-16-STYLE-WILDCARDS.md). |
| EWMH state traversal, STATE-01 #37 | Merged in [PR #102](https://github.com/gonz0w/fvwm-x/pull/102); affected checks pass; full platform gaps retained | Three related improvements share one checkpoint. Eight affected/retained entries pass ten profiles; core probes pass four default BSD X targets, and three scoped sanitizer entries pass. Original full runs pass 1990/1992 standard and 437/437 optional entries. Fresh 8 stages verify 120 code/77 catalog identities; 16 geometry invocations pass. Exact broader failures/install gaps remain in the report. [Report](verification/VERIFICATION-2026-10-15-EWMH-STATE.md). |
| Style matching batch, STYLE-01 #38 | Merged in [PR #101](https://github.com/gonz0w/fvwm-x/pull/101); affected checks pass; full platform gaps retained | Five related matching improvements share one checkpoint. All four affected tests pass ten profiles; core/update probes pass four default BSD X targets, and four scoped sanitizer checks pass. Original full runs pass 1967/1976 standard and 433/433 optional entries. Controls leave five OpenBSD AMD64 gaps and its fresh-install gap open. Nine fresh stages verify 135 code/88 catalog identities; 18 geometry invocations, 330 workloads and 90 matching-cost rows pass. [Report](verification/VERIFICATION-2026-10-15-STYLE-MATCHING.md). |
| Distinct style-ID filter, STYLE-01 #38 | Merged in [PR #100](https://github.com/gonz0w/fvwm-x/pull/100); affected checks pass; full platform gaps retained | Fixed 4-KiB operation-owned storage reduces measured 64/128/256-rule backward comparisons from 2016/8128/32640 to zero. Policy/paired-core/update checks pass ten profiles, four default BSD X targets and scoped sanitizer. Full scope executes 1968 standard/431 optional entries; after seven unchanged controls, 1964 standard and all optional pass, with four OpenBSD AMD64 failures remaining. Nine fresh stages validate 135 code/88 catalog identities; OpenBSD AMD64 fresh install/staged geometry are unperformed. Eighteen staged geometry invocations, 330 workloads and 120 update observations pass. [Report](verification/VERIFICATION-2026-10-15-STYLE-FILTER.md). |
| Style work baseline, STYLE-01 #38 | Locally qualified and merged in [PR #99](https://github.com/gonz0w/fvwm-x/pull/99) | Three actual-core stories measure quadratic simplification, ordered lookup and matching inside the bulk server grab. Ten profiles, four authenticated default BSD X servers and scoped sanitizer pass; 165 workloads, retained update-ordering coverage and 150 code/99 catalog identities validate. Production optimization remains open. [Report](verification/VERIFICATION-2026-10-15-STYLE-PROFILE.md). |
| Window-update ordering contract, DRAW-01 #25 | Merged in [PR #98](https://github.com/gonz0w/fvwm-x/pull/98); affected checks pass; full-suite gaps retained | Contract/manual and actual-core explicit/outer-loop regression. New fixture passes ten profiles, four default BSD X displays and scoped sanitizer; final wraps pass ten profiles plus sanitizer. Full scope executes 1952 standard/427 optional entries: after exact controls, 1950 standard and all optional pass; Linux GCC topology and OpenBSD CI-worker timing failures remain open. 150 code/99 catalog identities validate; production targets unchanged. [Report](verification/VERIFICATION-2026-10-15-WINDOW-UPDATES.md). |
| Synchronous module readiness, LOOP-01 #36 | Locally qualified and merged in [PR #97](https://github.com/gonz0w/fvwm-x/pull/97) | Unlock, blocked-output and startup waits use native poll with shared absolute-deadline conversion. Retained full coverage passes 1944 standard and 425 optional entries; the final timeout refinement passes 50 affected entries across the same profiles. 150 final installed code identities and 99 catalogs match. Default BSD X passes 20+8 fixtures; three scoped sanitizer entries pass. Broader admission/runtime/release audits remain open. [Report](verification/VERIFICATION-2026-10-15-MODULE-WAITS.md). |
| Core descriptor readiness, LOOP-01 #36 | Locally qualified and merged in [PR #96](https://github.com/gonz0w/fvwm-x/pull/96) | Typed native poll snapshots and existing death-row pins now serve ordinary/interactive core waits. Eight standard compiler profiles pass 1944 entries across six targets; optional profiles add 425, default BSD servers pass 16 core/cache/interactive geometry fixtures, and two instrumented checks pass. 150 installed code identities and 99 catalogs validate. Broader descriptor/runtime release gates remain open. [Report](verification/VERIFICATION-2026-10-14-CORE-READINESS.md). |
| Image-cache variant lookup, CACHE-01 #33 | Locally qualified and merged in [PR #95](https://github.com/gonz0w/fvwm-x/pull/95) | A 65-variant lookup now checks one file stamp instead of 65. Eight standard compiler profiles across six targets pass 1936 entries; optional profiles add 423, default BSD servers pass 12 cache/staged-WM fixtures, and instrumented cache checks pass. 150 installed code identities and 99 catalogs validate. Broader profiling and release/hardware gates remain open. [Report](verification/VERIFICATION-2026-10-14-PICTURE-CACHE.md). |
| Default-BSD-X startup evidence, LIB-07 #93 | Four-story diagnostic/comparison checkpoint locally qualified | Final startup/QEMU policy passes 282 checks on six targets; six real teardown probes, 80 server-only starts/refusals (including 32 retained extra controls), ten existing application fixtures and 36 unchanged executable observations pass. A fresh signed-updated OpenBSD AMD64 image verifies a new boot and 16 starts. Failed preparation/dispatch attempts remain recorded. No WM rebuild or FVWM configuration change. The original VESA/DDC fault remains open; further cause investigation is deferred until much later by user direction. [Report](verification/VERIFICATION-2026-10-14-BSD-X-STARTUP.md). |
| Optional Go audit/compiler adoption, LIB-03 #91 | Affected implementation locally qualified and merged in PR #92 | The approved Go 1.26.9/1.27.2+ compiler policy, same-pin vendor cleanup and fixed BSD SDK recipe pass their combined affected checkpoint. Six targets pass 108 registered entries, 60 Go package results, 3114 test/subtest records, 42 staged identities and current native advisory scans; actual Go 1.26.9 passes on Linux ARM64. Required BSD default-X paths pass after unchanged replacement controls; two pre-client VESA startup crashes remain under LIB-07 #93. [Adoption report](verification/VERIFICATION-2026-10-14-GO-BASELINE.md), [earlier audit](verification/VERIFICATION-2026-10-14-GO-DEPENDENCIES.md). |
| CI owned QEMU / approved PNG bootstrap | Locally qualified; private GitHub foundation merged | Four BSD standard/install and Go/default-X families, both Linux Go/race profiles, dependency/compiler checks and helper policy pass in their exact scopes. All1082 C/header/Go identities are preserved. The user selected gonz0w/fvwm-x with local review/tests/lint/validation and Actions disabled; remote execution/workflow adoption requirements are retired, not falsely marked passing. Native CPU/sanitizer and physical evidence gaps remain separate. Repository issues/Project mirror tracker execution manually; [Foundation PR #83](https://github.com/gonz0w/fvwm-x/pull/83) was merged by the maintainer; local-only repository management is adopted. [Hosting decision](decisions/LOCAL-REVIEW-GITHUB-2026-10-13.md), [PNG report](verification/VERIFICATION-2026-10-13-CI-PNG-BOOTSTRAP.md). |
| Documentation/evidence publication, MAINT-02 #89 | Availability/link-policy checkpoint qualified and merged in PR #90 | 370 original guides/decisions/reports restored with exact import identities and bounded navigation changes; zero current local-link errors. 96 Python policy checks pass across six targets; local 137 passes/seven native-Meson skips are explicit. Dated source/acceptance scopes, separate desktop ownership and inactive archive controls are preserved. No WM build or full native matrix repeated. [Report](verification/VERIFICATION-2026-10-13-DOCUMENTATION-EVIDENCE.md). |
| Build reuse and PR batching, MAINT-02 #87 | Affected checkpoint locally qualified and merged | Exact scalar/source/build identity reuse avoids needless setup; install skips its redundant rebuild while test-only prerequisites remain. Six targets pass 72 actual policy/Meson fixture cases; macOS portable 121 passes/seven native-Meson skips are explicit. One coherent checkpoint normally means one PR with related docs/evidence, reused profile builds and one required matrix. Native qualification cache refresh and broader owner scope remain. [Report](verification/VERIFICATION-2026-10-13-BUILD-EFFICIENCY.md). |
| Source-tool maintenance, implementation `168a` | Affected checkpoint locally qualified and merged | MAINT-02 child #84/#85 share bounded file/archive memory, Go manifest provenance and explicit missing pkg-config observations. macOS passes 111 portable checks with five native-Meson skips; six target environments pass 75 affected checks with fifteen real-Git skips on guests without Git. A 32 MiB fixture preserves archive bytes and lowers Python peak memory about 74%. Source-release/Meson-dist adoption and LIB-03 advisory/reachability remain separate. [Report](verification/VERIFICATION-2026-10-13-SOURCE-TOOLS.md). |
| CI fixed asset preflight, final `4bd0` | Earlier affected checkpoint qualified | Six targets pass 23 units each; thirteen actual downloads and thirteen offline cache hits verify independent private bytes. The approved owned runner now consumes verified inputs at execution; this earlier helper checkpoint remains distinct. [Report](verification/VERIFICATION-2026-10-13-CI-ASSETS.md). |
| Full XDG integration, final `04c1` / runtime `cb13` | Locally qualified and adopted in optional Go builds | Six targets/eight compiler profiles compose 1982 baseline full entries plus 134 affected callback entries; 25 final archives validate source/modes, runners, registrations and installed identities. Sixty ordinary/twenty race Go package results pass 98 top tests/421 named subtests each. Six staged native core/Form/provider controls, six legacy entry consumers, four authenticated default BSD servers, minimal CLI30, scoped17 and six manual profiles pass. Disabled-Go Python/Perl remains; COMPAT-02 retirement, native release CI and physical support stay separate. [Acceptance report](verification/VERIFICATION-2026-10-12-XDG-PORT.md). |
| XDG ordered allocation, frozen `be8e` | Three-story pure policy checkpoint locally qualified | Ordered Include/Exclude, child-first two-pass OnlyUnallocated and operation-owned category caching/work budgets pass. Six targets qualify eight Go packages, 24 top tests/265 named subtests, vet/stdlib checks; both Linux policy race runs pass. 48 ordinary/two race package outcomes, 2120 subtest executions; all 492 predecessor files unchanged. This frozen policy source had no production activation; the full integration is now qualified separately at `04c1`. [Report](verification/VERIFICATION-2026-10-12-XDG-ALLOCATION.md). |
| XDG discovery/preference policy, frozen `9337` | Four-story pure policy checkpoint locally qualified | Explicit XDG search paths, ordered menu inventories, application-pool precedence and legacy preference data are implemented without activation. Six targets pass 20 top tests/230 named subtests, eight Go packages, vet and stdlib graph checks; both Linux race runs pass. 48 ordinary/two race package outcomes, 1840 subtest executions; all 486 predecessor files unchanged. Allocation and the full integration are now qualified separately; retirement remains queued. [Report](verification/VERIFICATION-2026-10-12-XDG-DISCOVERY.md). |
| XDG input policy foundation, frozen `d33f` | Three-story pure policy checkpoint locally qualified | Six targets pass 11 top-level tests and 121 named subtests; eight Go package regressions and vet pass per target. Both Linux targets pass actual race instrumentation: 48 ordinary/two race package outcomes, 968 named subtest executions. Standard library only; all 480 predecessor Go files unchanged. No production caller/install/X activation at this foundation source. Discovery/preferences/allocation and full integration are qualified separately; retirement remains queued. [Report](verification/VERIFICATION-2026-10-12-XDG-POLICY.md). |
| Full XDG provider/frontend fixtures, frozen `efe8` | Affected fixture checkpoint locally qualified | Six targets pass 52 distinct observations each (13 base/paired Form, 39 real XML/provider), plus six exact registered base repeats: 390 observation executions. Pinned PyXDG oracle is test-only; production producers match the preceding checkpoint. Existing defects are recorded without becoming parity requirements. The full coordinated Go port is now qualified at `04c1`; COMPAT-02 retirement remains separately queued. [Report](verification/VERIFICATION-2026-10-12-XDG-FIXTURES.md). |
| fvwm-convert-2.6 Go port, final `b6ca` / initial `63c9` | Affected local checkpoint qualified | Six targets pass966 ordinary/race CLI/conversion/file cases,270 recorded observations,36 gate controls and twelve tagged stages. Exact source/installed identities validate; final multi-file reference accounting passes nine units per package run. Manual renders and consumer classification pass. Disabled-Go Perl remains; the installed Script communication example is qualified separately at `6fcc`. [Report](verification/VERIFICATION-2026-10-12-CONVERTER-PORT.md), [consumer ownership](verification/RECONCILIATION-2026-10-12-UTILITY-CONSUMERS.md). |
| Script communication example, final `6fcc` | Affected local checkpoint qualified | Six targets pass 546 normal/race transport controls, twelve tagged stages and eighteen native-X invocations/90 real button requests. Four default BSD servers pass with private authority. Final source/runner/installed identities and unchanged evidence reuse validate. Optional stdlib Go and paired Script selection preserve configuration syntax and the installed Perl file. [Report](verification/VERIFICATION-2026-10-12-SCRIPT-COM-PORT.md). |
| fvwm-menu-directory Go port, source `a7e6` | Affected local checkpoint qualified | Six targets pass728 ordinary/race CLI/filesystem cases,270 recorded observations,84 pure driver/gate cases and48 actual menu mappings. Owned atomic reuse state and dynamic reload pass; twelve tagged profiles and native identities validate. Go-disabled Perl remains; the converter is now qualified separately. [Report](verification/VERIFICATION-2026-10-12-DIRECTORY-PORT.md). |
| fvwm-menu-xlock Go port, final `9ab4` / runtime `c019` | Affected local checkpoint qualified | Six targets pass378 ordinary/race CLI/probe cases,270 legacy contract observations,84 final pure contracts and24 actual menu map/dismiss cases. Twelve tagged stages/installed identities and a fresh Go gate pass; exact source/driver derivation validates. Go-disabled Perl remains; the directory and converter ports are now qualified separately. [Report](verification/VERIFICATION-2026-10-12-XLOCK-PORT.md). |
| CI Go profiles/default-BSD-X harness, final `d751` | Affected local checkpoint qualified; candidates inactive | Six targets pass444 ordinary peers,78 final pure contracts,12 nested-X fixtures,8 authenticated default-BSD-X fixtures and4 unauthenticated-client refusals. Installed clients match native tested bytes; Linux actual race clients and one fresh end-to-end Go gate pass. Thirteen selected asset downloads verify; two inherited container identities remain observations. Provider hash enforcement/default-X and actual native CI remain open. [Report](verification/VERIFICATION-2026-10-12-CI-PROFILES.md). |
| FvwmCommand Go client, final `295e` / runtime controls `f274` | Affected local checkpoint qualified | Six targets:162 final peer cases,60 Prompt regressions,18 Go package results,14 real-X invocations/84 cases,12 tagged stages and12 exact CLI parity comparisons. Actual Linux race clients add54 peer cases/two X invocations. Every C/header and accepted core/MFL identity is unchanged. Go-disabled Python profile remains; no configuration migration or new dependency. [Report](verification/VERIFICATION-2026-10-12-COMMAND-PORT.md). |
| Test reliability/native setup and stage 3 audit, final `ebd1` | Affected local checkpoint qualified | Six targets pass30 X fixture invocations/ten profiles and30 gate contract cases; exact unchanged native identities and deadline-only composition validate. Fixed32-row monitor/protocol reconciliation retains required work-area and selection gaps. Action candidates remain inactive; native CI/assets and later runtime adoption remain open. [Report](verification/VERIFICATION-2026-10-12-COVERAGE-CI.md), [coverage](verification/RECONCILIATION-2026-10-12-STAGE3.md). |
| Prompt/protocol/utility foundations, final `397c` / baseline `adc5` | Strictly locally qualified | Four stories pass 2276 full/optional entries, 159 staged code identities and 99 catalogs. Final affected checks pass 27 entries, four Go race/vet controls, nine Go stages and eight default-BSD-X fixtures; three scoped C sanitizer checks pass. Twelve archives validate source/modes, runners, registrations and installed consumers. No configuration migration or new dependency. [Report](verification/VERIFICATION-2026-10-11-PROMPT-METADATA.md). |
| Pager ownership, source `7284` | Locally qualified | Eight compiler profiles: 1712 standard entries, 120 installed identities, 268 default-BSD-X invocations; optional 219/199 and scoped sanitizer 105. Retained topology timing failures and unchanged recoveries remain part of the evidence. [Report](verification/VERIFICATION-2026-10-08-PAGER-MONITOR.md). |
| Canonical UI text, source `3c37` | Strictly locally qualified | Eight standard compiler profiles: 1736 entries, 120 installed identities and 276 default-BSD-X invocations; optional 222/202 and scoped 108. Seven complete final archives validate all 1393 frozen source hashes/modes and outcomes. [Report](verification/VERIFICATION-2026-10-08-UI-TEXT.md). |
| Monitor/numeric/Pager startup, source `a653` | Locally qualified within final `01c2` | Twelve stories, including the approved 4096-desk default and override through 65536, pass the combined core checkpoint. Separate predecessor matrices are superseded. [Focused report](verification/VERIFICATION-2026-10-08-PAGER-STARTUP.md), [combined qualification](verification/VERIFICATION-2026-10-11-CORE-PERFORMANCE.md). |
| Parser/Pager layout and projection, source `3074` | Locally qualified within final `01c2` | Fourteen stories pass the combined core checkpoint; no separate parser, grid, dimension or projection matrix. [Focused report](verification/VERIFICATION-2026-10-08-PAGER-PROJECTION.md), [combined qualification](verification/VERIFICATION-2026-10-11-CORE-PERFORMANCE.md). |
| FvwmScript generator/runtime adoption, source `2514` | Locally qualified and adopted | Four GEN-01/MAINT-02 stories pass1816 standard entries,120 staged code identities,48 affected default-BSD-X fixtures, optional/scoped checks and24 affected tooling passes. Retained wider missing-Git failures and the earlier unexplained candidate timing observation remain explicit. [Report](verification/VERIFICATION-2026-10-11-PARSER-ADOPTION.md). |
| AMD64 translation-tool follow-up, source `9053` | Focused checkpoint qualified | Linux/FreeBSD/OpenBSD AMD64 pass15 units, native138-key parity per target,66 catalog comparisons,33 updates and33 catalog-only staged identities. Initial missing tools and affected recoveries are retained; whole-source adoption/release gates remain separate. [Report](verification/VERIFICATION-2026-10-11-I18N-AMD64.md). |
| Translation maintenance adoption, distribution source `2663` | Locally qualified and adopted | Six default and two Linux ARM64 feature profiles pass1743 registered entries,120 staged code identities,77 catalogs and138-key extraction parity per target. Thirty NLS fixtures and thirty retained translation units pass. Deterministic committed-fixture archives, exact source/mode/runner/registration/install and reference checks validate. Ten unused GMO files and the old helper are retired with a verified backup; no configuration migration. [Report](verification/VERIFICATION-2026-10-11-I18N-ADOPTION.md). |
| Script font/modal service, final `69af` / baseline `3bb3` | Strictly locally qualified | Five implementation stories plus resource-revision ownership and native/live-X coverage. Baseline passes2229 full/optional entries,150 staged identities,99 catalogs,52 default-BSD-X and8 scoped checks. Final correction passes104 standard,23 optional,13 scoped and52 default-BSD-X drivers with11 module stages. Exact source/consumer/runner/registration/stage validation passes; no configuration migration or new dependency. [Report](verification/VERIFICATION-2026-10-11-FONT-MODAL.md). |
| Core performance, production `005f` / final fixtures `01c2` | Strictly locally qualified | Four performance stories and inherited monitor/Pager batches pass eight compiler profiles: 1816 entries, 120 installed identities and 300 default-BSD-X drivers; optional 232/212 and scoped 118. Seven archives validate all 1409 source hashes/modes and exact affected-fixture derivations. Configuration migration is unnecessary. Native release CI and physical checks remain separate. [Focused report](verification/VERIFICATION-2026-10-08-CORE-PERFORMANCE.md), [combined qualification](verification/VERIFICATION-2026-10-11-CORE-PERFORMANCE.md). |

User direction on 2026-10-08: repeated platform content restrictions interrupt
this thread's work. Pause further boundary-condition implementation and move
to routine maintenance/documentation cleanup. Preserve completed source and
evidence, finish checks already running, and leave successor matrices queued.
This changes the immediate work selection, not configuration requirements or
the acceptance status of unfinished checkpoints. It does not authorize bypassing
platform restrictions. Native Linux release sanitizer CI and physical desktop
qualification remain separate open gates.

User direction on 2026-10-11 resumes completion of the current core responsiveness
milestone. Qualify frozen `005f` once, including its inherited `a653`/`3074`
changes, instead of running three overlapping matrices. Preserve accepted `3c37`
and original VM disks; use independent source/build trees and new persistent
QEMU overlays. The former work-selection pause remains historical. No unrelated
boundary implementation or broader first-release feature scope is added by this
checkpoint. Configuration-file/format migration remains the compatibility stop.
Evidence preparation and exact required counts are in
`builddir/codex-2026-10-11/core-performance-qualification/qualification-plan.json`.
Native release CI and physical checks remain separate release requirements.

The 2026-10-08 pause retained the four-story performance successor and its
inherited qualification gaps. The 2026-10-11 direction resumes their combined
qualification only; further unrelated boundary implementation remains outside
this milestone. Three test-only repairs produce final source `01c2` from `005f`:
Pager fixtures select the actual monitor output (zero for the root fallback),
and the desktop fixture uses one private module lookup directory. Preserve base
full-suite results and rerun only affected drivers; production code is unchanged.


The optional desktop fixture also exposed the existing Read stream's 1024-byte
line buffer when its generated ModulePath exceeded that size. The fixture no
longer creates the oversized setup line; DATA-01 retains the separate line-
length/continuation contract audit, with the original failure in
`builddir/codex-2026-10-11/core-performance-qualification/`. This checkpoint does
not change configuration-reader behavior or claim that audit complete.

## Earlier image and interaction checkpoints

The approved image-source and compressed-XPM limits are adopted in production:
regular primary files, 128 MiB source/expanded caps and a five-second decompression
deadline, all with finite overrides. Seven full compiler legs pass 537 suite
executions; staged installed cores pass 192 Linux image outcomes and 232 BSD
image plus 40 FIFO outcomes on default servers. Optional format builds pass
12 focused entries and scoped local ARM64 ASan/UBSan passes 4/4 with leak
checking. All six archives match 1186 source hashes/modes; 98 installed native
binary identities match expected Meson output. Concurrent desktop work is
excluded. See the [image report](verification/VERIFICATION-2026-10-05-IMAGE-INPUT.md).

The separately approved SVG worker now contains secondary-resource loading,
SVGZ expansion and rendering with five seconds and 1024 MiB, preserving relative
images and finite overrides. Seven final compiler legs pass 572 suite executions;
staged Linux passes 192 image and 15 SVG outcomes, default BSD X passes 232 image,
20 SVG and 40 FIFO outcomes. Six archives match 1199 hashes/modes and 105 installed
native identities. Optional profiles pass 20 entries and scoped local sanitizers
pass nine entries with documented worker/fixture exclusions. The AMD64 gate uses
a full-system QEMU Linux guest because user emulation cannot enforce its address
space limits. See the [SVG checkpoint](verification/VERIFICATION-2026-10-06-SVG-ISOLATION.md).

The approved Script geometry batch now refuses unrepresentable sizes and
positions before mutation, preserves previous runtime state, retains legacy
zero/omitted startup sizing and shares checked widget/font/popup preparation.
Seven full compiler legs pass 586 suite executions. Six archives match 1205
source hashes/modes and 105 exact staged identities. Installed checks pass 63
geometry and 224 prior widget/text-list outcomes across Linux and default BSD
X; seven font pixel comparisons and three scoped local sanitizer checks pass.
See the [geometry checkpoint](verification/VERIFICATION-2026-10-06-SCRIPT-GEOMETRY.md).

The approved shared font-shadow limit is qualified: 4096 positions per draw,
with build-time adjustability from 1 through 65536. Checked counting and direct
Center-border iteration preserve admitted ordering; core and module refusal
paths retain usable fonts or handle startup failure. Seven compiler legs pass
600 full-suite executions; six archives match 1209 hashes/modes and 105 staged
identities. Installed checks pass 77 shared consumer, 63 geometry, 224 prior
widget/text-list and 21 core font pixel-state outcomes. Five scoped local
sanitizer and six override checks pass. See the [font-shadow checkpoint](verification/VERIFICATION-2026-10-06-FONT-SHADOW.md).
Asynchronous core image activation is locally qualified on Linux/FreeBSD/OpenBSD
AMD64/ARM64. Eight compiler profiles pass 908 qualified entries, 120 staged code
identities and 52 installed default-BSD-X driver invocations. Software and
SVG-disabled profiles pass 118/101 entries; scoped sanitizer has 30 unique checks.
All 1261 frozen source hashes/modes match. Modules retain their existing completion
path; newer desktop/theme/runtime-language changes have separate qualification.
See the [activation checkpoint](verification/VERIFICATION-2026-10-06-IMAGE-ACTIVATION.md).
The common button-release checkpoint is locally qualified: eight profiles/924
entries, 120 code identities, 20 installed default-BSD-X drivers, optional
120/102 entries and five scoped sanitizer checks. All 1262 hashes/modes match.
The approved active viewport follow-up has eight focused entries passed and
its separate combined checkpoint is running.
Wider decoder/storage/font budgets, modal servicing and package review remain open.
Native Linux release sanitizer CI is configured but unrun. The earlier
unexplained FreeBSD ARM64 interaction timing observation remains in VERIFY-01.

## Latest maintenance checkpoint

2026-10-08 documentation upkeep is complete: current checkpoint states have one
summary, the module ownership row links it, and older progress entries remain
historical. The maintenance guide now distinguishes initial candidate checks
from accepted six-target build/manual adoption. Local guide links and the shared
26-page/three-section manual manifest pass focused checks; no runtime source or
manual content changed. The website/manual redesign stays deferred. See the
[upkeep report](verification/DOCUMENTATION-UPKEEP-2026-10-08.md).

THEME-01 has adopted the user-approved richer SVG artwork: 377 self-contained
assets replace the default geometric PNGs. PNG remains for screenshots and
existing raster-image compatibility. Focused Linux ARM64 builds pass with PNG
disabled, including 51 policy checks, the full native desktop/two-monitor gate,
377 worker render/install comparisons and 12 additional scale renders. An
SVG/PNG-disabled build also passes native Session fallback checks. Actual
composited running-desktop screenshots are recorded in the
[SVG theme report](verification/VERIFICATION-2026-10-06-SLATE-SVG.md).
Broader six-target theme qualification and native sanitizer CI remain pending;
the active asynchronous IMAGE-01 batch is unchanged.

MAINT-02 has adopted the compatible build entry point and shared Meson/manual
tooling. Six target default/minimal/feature build, test, install and
Asciidoctor comparisons pass; Linux ARM64 Clang also passes. Linux AMD64 Go
under user emulation and native sanitizer CI remain unqualified. Evidence
and remaining limitations are recorded in the
[tooling report](verification/VERIFICATION-2026-10-05-TOOLING.md). The public website
migration is deferred to
[DOCS-01: static website and manual rewrite](decisions/WEBSITE-MANUALS-BRIEF.md).
Its former output decision no longer blocks the active queue.
Metadata, parser, catalog, source-release and installed utility migrations
remain individually gated; this does not close the whole maintenance backlog.

## Documentation organization checkpoint

Developer documentation now has maintained project, architecture, development,
workflow and manual-policy guides. Retired `.planning`/`.gsd` material and upstream
notes are under history; dated reports are under verification and proposals under
decisions. Existing source/evidence content was retained and links rebased.
The ignored graph cache has automatic updates disabled. See the
[migration report](verification/DOCS-STRUCTURE-2026-10-05.md).
Man pages are the primary install documentation; optional HTML uses the existing
source/renderer. This checkpoint does not change runtime/build targets or claim
new passing native CI, package/man gates or a completed manual rewrite.

Earlier file-browser selection, superseded by maintenance-first scheduling:
the PR176–178 platform
checkpoint is closed; PR186 is merged. Batch its related provider cleanup
with operation-catalog consolidation, strict typed value decoding, separate
query/mutation ownership and compiler-checked query results. Run one combined
focused Linux gate after the cleanup group; no additional OS/theme/install
matrix for portable internal code. The merged feature subset already supplies portable
matching-browser handoff, shared bounded correlated request transport and an
optional installed folder desktop entry. Preserve
existing independent-window behavior unless reuse is requested, literal target
bytes, unknown-outcome refusal and default-association ownership. File mutation,
FileManager1 and protected clipboard payload/storage remain separate. The last integrated reference is PR178; PR186 is the first merged batch
since that reference, and cleanup becomes the second on merge.
The feature and later cleanup pass their source-bound focused gates. The due
PR174 comparison retains pre-cleanup PR186 preserved binaries; it is not a final
cleanup-browser speed claim.
Retain PR178 as integrated reference with one merged batch, two on cleanup
merge. The independent performance reference is PR186's preserved pre-cleanup
handoff binaries at 0/5, advancing to 1/5 on cleanup merge. Next pick up
the next bounded consumer/file action story; protected clipboard content and
FileManager1/default-role contracts retain their separate acceptance gates.



## Execution order

Current execution selection, 2026-10-23: the user's maintenance-first request
supersedes the feature-first cutoff below. Feature pickup stays paused until
the user resumes it. Preserve accepted checkpoints and existing feature plans.
Use this ordered batch queue; the linked register contains exact issue mapping,
prerequisites, exclusions and finite acceptance. Scope each implementation PR
to a complete related owner slice; tests/docs do not inflate story counts.

| Order | Batch | Main outcome |
| --- | --- | --- |
| M1 — Done | Supported runtime baseline and replaced entry points | PR189 merged; #179/#180/#181 Done. Qualified required compiler/utility/profile/install contract retained. |
| M2 — Active | Remaining first-party runtime controllers | M2.1–M2.5 merged PR190/191/194/195/198; M2.6 runtime/session action and installed handoff checks pass, ready for review. One combined affected-platform/default/reduced/install and remaining session/measurement acceptance handoff precedes M3. |
| M3 | Target-owned linkage and dependency closure | Map real symbols/static transitives and replace broad dependency bags by core/module/worker target groups. Remove only unused edges, with package/license/installed closure measurements. |
| M4 | Obsolete SVG backend removal | Consolidate the maintained librsvg/Cairo path and remove obsolete backend detection/options/branches; retain SVG-disabled, raster/XPM, scale/alpha and bounded worker behavior. |
| M5 | Core/module source and ownership cleanup | Batch named ownership/error/unwind families and shared readiness consumers. Preserve accepted fixes and single-thread dispatch/order; no global rewrite or performance-only restructuring. |
| M6 | Perl framework retirement and scripting disposition | Qualify the bounded scripting migration floor, then retire FvwmPerl/perllib/fvwm-perllib and Perl-only build/templates. Keep unrelated FVWM3/FvwmScript behavior and user files. |
| M7 | Owned build/generator/test tool retirement | Port first-party tools in build/source, generation/docs and independent test/VM-driver groups; remove predecessors only after parity, failure capture and offline/reproducibility checks. |
| M8 | Distribution, portability and documentation closure | Reconcile install/source archives, manuals/examples, package/license references and platform branches; complete accumulated required maintenance qualification once, with release/hardware gaps explicit. |

D1 build-backend, D2 JSON and D3 helper-sandbox evaluations are bounded decision
lanes, not automatic adoption or full source rewrites. Source removal requires
an exact disposition/replacement list. #41's old geometry title is corrected to
its existing helper-sandbox scope. Native parent/blocking relations remain
unchanged. Conditional qualification owners are consulted only for affected
contracts; performance-only owners stay Later; new feature/polish owners are
Backlog under their existing milestones. [Complete register](decisions/MAINTENANCE-FIRST-BATCHES-2026-10-23.md).

Development cadence, revised by the user on 2026-10-23: combine larger sets of
similar issues before one focused Linux checkpoint. Aim for six to ten related
substantive improvements where useful, or a complete reviewable owner slice;
tests/probes/docs are not separate improvements. Use cheap affected compilation
while editing when useful; do not run a suite per helper. Add targeted BSD or
other platforms for actual API/ABI/build/server changes or concrete failures.
Review coverage debt after roughly five merged batches or a substantial handoff,
without automatically running every OS; preserve full milestone/release coverage.
Retain unrelated failed evidence with its owner rather than repeatedly rerunning
it. Use frozen-test wait time for independent work without modifying tested
inputs. [Detailed cadence](WORKFLOW.md#implementation-batches-and-test-cadence).


Stream ownership is merged in
[PR #141](https://github.com/gonz0w/fvwm-x/pull/141). Four FIFO
construction, status continuation, log adoption and log-failure repairs pass
one composed six-target checkpoint: 2236 standard / 58 affected optional
entries, four scoped sanitizer entries, ten installs and four authenticated BSD
displays. Preserve original harness failures and affected continuations in the
[report](verification/VERIFICATION-2026-10-21-STREAM-OWNERSHIP.md). The selected input/ownership checkpoint is now locally qualified in
[PR #142](https://github.com/gonz0w/fvwm-x/pull/142): fork-time LoggingFD identity
and once-only native adoption, terminal malformed framing, Script message token
ownership and root Send_ConfigInfo recipient refusal. Six targets pass 2252
composed standard / 117 optional entries, scoped sanitizers and ten final
installs. Preserve original probe/setup failures and affected completions in
its [report](verification/VERIFICATION-2026-10-21-MODULE-INPUT-OWNERSHIP.md). Existing configuration-view and Script
packet/value helpers have explicit already-covered dispositions.
The final interaction pickup is locally qualified in PR143: three reproduced
cancellation failures are repaired across six targets, and existing retained
context/function/module/window owners have explicit dispositions. [Report](verification/VERIFICATION-2026-10-21-FUNCTION-LIFETIMES.md).
No comparison is due.

Historical feature selection, 2026-10-22 (superseded by maintenance-first scheduling):
the user approved the finite feature handoff
in [the milestone cutoff](decisions/FEATURE-HANDOFF-2026-10-21.md).
Safety closeout PR141–143 and dependency/protocol checkpoints PR144–147 are
merged. The bounded manager-selection S04–S06 target checkpoint is locally qualified
in merged PR148. BUTTON-01 and related BUTTON-02 live state/presentation work
are merged in PR149; atomic BUTTON-02 groups are merged in PR150.
CFG-01 registry/persistence/native menus are merged in PR151. Grouped
saved-preference editing, scoped unsaved defaults and generation-bound consumer
retirement are merged in PR152. Live preview/recovery remains gated
until external core theme changes can be observed safely; coordinate that
contract with IPC-02. CFG-02’s first non-executing desktop-settings subset,
source/include diagnostics and native report consumer are functionally qualified
in merged PR153. General core runtime diagnostics remain open. IPC-02’s first
versioned correlated MFL request/error, core desktop/work-area query/barrier and
one-way read-only admission subset is merged in PR154. The settings-provider
query, generation-checked atomic update and scoped reset/reload batch is merged
in PR155. The next batch adds non-publishing typed validation, local pending
snapshots and wait-only cancellation, merged in PR156. The current topology,
workspace/cache and global-binding query batch, plus a reproduced settings
readiness fence, is merged in PR157 and qualifies the accumulated affected
six-target cycle. Menu/fallback bindings, normalized
placement controls and modifier/context policy inspection are merged in PR158.
Provider capability queries, opt-in invalidations and conditional preference
queries are merged in PR159. PR160 adopts bounded operation
lifecycle with the existing desktop catalog/status workers and qualifies the
accumulated affected six-target cycle. PR161 adds bounded colorset inventory,
exact sparse-ID lookup and summary/freshness with published versus pending/refused
image state. PR162 merges appearance freshness, menu/decor pages and the
CopyMenuStyle opacity repair. Merged PR163 adds one core-owned runtime opacity
lease, typed controls and native Keep/Revert with deadline/conflict recovery.
Merged PR164 connects that lease to the owned launcher-opacity preference,
scoped confirmation/persistence and captured editor draft return.
User selection, 2026-10-23: stop extending opacity and advance FILE-DATA-01 /
FILE-VIEW-01 / FILE-ACTION-01 foundations. Implement stable collection identity,
bounded isolated local directory loading and generation-checked typed browse/
open actions with a native paged consumer. This proves supporting contracts;
full scrolling/grid browser handoff, mutations and protected clipboard content
remain separate. Clipboard will reuse collection metadata/selection policy,
not this ordinary file-path callback channel for sensitive payloads.
Merged PR166 adds reusable bounded literal filtering, snapshot
sort modes, cross-page selection controls and an observed-metadata inspector.
This advances FILE-VIEW-01 / FILE-ACTION-01 and the shared clipboard metadata
foundation without admitting clipboard payloads. See the
[collection-controls contract](decisions/FILE-COLLECTION-CONTROLS-2026-10-23.md).
Earlier selection after merged PR173: native live browser adoption is merged.
Batch FILE-DATA-01 native directory invalidations, coalesced/periodic snapshot
reconciliation and explicit manual fallback/controls. One observer child owns
native registrations separately from the enumeration/action child; slot/reap
and supersession limits remain explicit. Manual refresh stays the default.
The batch is ready for review: Linux/FreeBSD/OpenBSD 8.0 ARM64 Go checks,
Linux race and four native-X drivers including all three themes pass. Native C
artifacts are retained; original fixture/storage failures and precise affected
completions remain in the [report](verification/VERIFICATION-2026-10-23-BROWSER-DIRECTORY-WATCH.md). Preserve the metadata/private-content and
default/theme boundaries. See the
[watch contract](decisions/BROWSER-DIRECTORY-WATCH-2026-10-23.md).
SQLite is not a current prerequisite: live file state and
opaque content/collection handles stay independent of a storage backend. STORE-01
still owns protected memory first and optional versioned encrypted persistence
later; no database dependency/schema or protected clipboard payload is adopted.
General saved-preference confirmation and owned image/resource snapshots remain
open. Other
providers, typed actions beyond settings, domain
progress/decisions/cancellation and shared-core generations/CFG-01 preview remain
open under their owners.
The runtime preview adds native UI and actual-X screenshot review. See the current checkpoint and request report.
Do not add another general maintenance checkpoint before those features.
The [manager target report](verification/VERIFICATION-2026-10-21-MANAGER-TARGETS.md)
records composed six-target qualification, exact inputs and retained failures.

All unselected general stage 5 work moves to the future-release backlog
(GitHub **Later** milestone): broad
BOX-01 sandboxing, dependency/linkage review and replacements, COMPAT-02
retirement, general portability/reproducibility tooling, monitor/scale audits,
remaining EWMH and selection roles/transfers, XDG interoperability and broad
TEXT-01 adoption. Preserve their open owners and acceptance gaps; deferral is
not completion or a claim of full standards/release qualification. A feature
may bring forward only its concrete prerequisite (for example text composition
for an editable control), without reopening the whole audit. Button traversal
and disabled-state work do not wait for a general text/shaping replacement.
A newly reproduced serious safety defect still advances with its owner.

Stage 5 below remains the longer-term inventory; this current selection
supersedes its blanket placement before stage 6 for this milestone. Existing
configuration compatibility, six-target affected qualification and separate
native-CPU/hardware/release gates remain. No release publishing is authorized.

The CI-01 / MAINT-02 / CLEAN-01 preparation evidence checkpoint is merged in
PR #139. Source modes, atomic live phase status and separate PNG loader proof
outputs pass 282 native policy cases across six targets and two real OpenBSD
runner cases, preserving existing WM builds. [Report](verification/VERIFICATION-2026-10-21-BSD-PREPARATION.md).
Shared string ownership is merged in PR #138. The forty-row standards inventory
is complete as an inventory; its bounded runtime handoffs retain their owners.

The module/icon-box/event-filter dependency batch is merged in PR #135.
Its affected checkpoint and exact code/data parity remain source-scoped.
[Report](verification/VERIFICATION-2026-10-20-CORE-RECORD-BOUNDARIES.md).

Immediate selection, clarified 2026-10-21: follow new OpenBSD releases on
AMD64/ARM64, with 8.0 the current qualified baseline and 7.9 out of scope,
retaining Linux/FreeBSD scope. Advance fixed inputs and both architecture
gates together; qualification does not transfer to an untested release.
The composed local baseline passes all selected standard, optional, install and staged/default-X gates;
ready for maintainer review. Preserve original failures and separate native-CPU,
physical and wider release acceptance. The older 7.9 VESA cause remains
historical/deferred. PR #135 and PR #136 are merged on main; PR #137
contributed its inventory through PR #136.
[Baseline decision](decisions/OPENBSD-8.0-BASELINE-2026-10-20.md),
[checkpoint](verification/VERIFICATION-2026-10-20-OPENBSD-80.md).

Performance tracking: the due PR144→PR149 comparison uses preserved Linux
ARM64 GCC binaries after five merged implementation checkpoints. Command-median
ranges overlap; code/read-only grows 6898 bytes and writable data 8 bytes,
with roughly unchanged RSS. Title-load p95 varies upward in this sample while
other p95 observations are similar/lower; no consistent latency regression is
established. The next due PR149→PR154 observation after PR150–154 uses
preserved qualified binaries: mixed small latency/CPU changes, core allocated
size unchanged, MFL feature growth explicit. It establishes no general speedup
or consistent latency regression. PR154 is the reference at **2/5** subsequent
merged implementation PRs (PR155/156); the current core-state batch becomes
**3/5 on merge**. [Latest observation and receipt](verification/PERFORMANCE-2026-10-22-PR154.md).
Keep the [five-checkpoint cadence](WORKFLOW.md#periodic-performance-comparisons)
without new benchmark builds or performance-only tuning.

This is the authoritative sequence. Priority describes risk/importance;
the table below describes dependencies and what to take next. Detailed
roadmaps and the history do not create competing queues. No new open P0
reproducer is recorded; a reproduced serious security or availability failure
jumps ahead of this sequence.

| Order | Work | Why it goes here |
| --- | --- | --- |
| 1 | TextField/List repairs have reached the verified checkpoint. The approved interactive ValueLimit extension is verified. The reproduced PNG recovery finding, patched baseline and signed snapshot refresh are verified. The approved primary-image/compressed-XPM batch has reached its verified checkpoint. The approved SVG isolation and asynchronous core activation batches are locally qualified. The approved SCRIPT-01 geometry batch is qualified. The approved font-shadow limit, checked iteration and shared consumer refusal/ownership are qualified. Font/modal servicing is strictly locally qualified at final69af; asynchronous core SVG completion is locally qualified. Prepare VERIFY-01 and maintain COMPAT-01; existing maintenance candidates may be used explicitly now. | Finish the current concrete findings to a checkpoint. Local native sanitizer execution remains a release gate; tool preparation does not wait for all runtime work. |
| 2 | Short dependency/package triage and early MAINT-02 adoption gates. Qualify the approved [PNG security baseline](decisions/PNG-SECURITY-BASELINE-PROPOSAL.md) with patched dependencies. Then interleave bounded DATA-01/IMAGE-01, FD-01/PROC-01 and INPUT-01 batches with a verified tool migration; include button-release waits in the owning interaction batch and the focused STACK-02 correctness check. Begin STANDARDS-01’s fixed requirement inventory alongside these audits; do not interrupt the active asynchronous image batch. | Build/manual adoption has reached its recorded maintenance checkpoint; interleave the remaining migrations with runtime work. The website/manual redesign is deferred under DOCS-01 and does not block adoption. Security findings advance immediately; routine audit work does not postpone all tools until hardening is exhausted. STACK-02’s per-window publication repair is now locally qualified, enabling the separate STACK-01 identity/traversal work. |
| 3 | PERL-PROTO-01/PROMPT-01, checked metadata, Go-enabled FvwmCommand, three standalone utilities and the installed Script example are locally qualified. The coordinated full XDG provider/native Form port is now locally qualified and adopted at `04c1`, closing this utility-family port milestone. Disabled-Go predecessors remain until COMPAT-02's stage5 retirement gate. CI-01 shared setup, optional Go profiles and authenticated local default-BSD-X are qualified; CI-01's fixed asset preflight and approved owned QEMU runner are locally qualified in their affected scopes (`4bd0` / `5200`). Verified image/boot inputs are consumed at execution, 294 native units and four default-X startup setups pass. The [approved patched PNG bootstrap](decisions/CI-PNG-INPUT-PROPOSAL-2026-10-13.md) and fresh provider build/install/default-X consumers are now locally qualified at finalcec1; unaffected standard evidence is retained with precise affected completions. The user selected private gonz0w/fvwm-x and local-only verification; Actions/workflow adoption are retired requirements. CI-01 now owns local review recipes, source-bound evidence and manual GitHub roadmap/PR management; native execution gaps remain with VERIFY-01. MONITOR-01/EWMH-01/SELECTION-01 retain their fixed32-row reconciliation; adopt only verified runtime gaps at stage5. The other chat's desktop runtime work stays separately owned. | Reuse the accepted [XDG port checkpoint](verification/VERIFICATION-2026-10-12-XDG-PORT.md); no additional provider implementation matrix is queued. Later CI asset/runtime/retirement owners retain their own gates. |
| 4 | The finite safety closeout below is locally qualified in PR141/142/143: stream/process ownership, selected external-input boundaries and pending window/resource lifetime. PR143 is merged; finish the bounded manager-target handoff in PR148, then advance the selected stage 6 features. Attach CLEAN-01/DEAD-01 or tool changes only to a concrete safety contract or named feature dependency. Retain accepted QUEUE/OUTPUT/STACK/DRAW/STYLE/STATE/CACHE/LOOP results. Defer further performance-only profiling/tuning, cache/style micro-work, timer-heap and XCB experiments to the next release. | Prevent crashes, malformed-input failures, hangs and ownership mistakes without an indefinite optimization phase. Bounds, fairness, ordering and cleanup remain safety work even when historically listed under performance. Use the explicit handoff criteria below, then advance stage 5/6 dependencies and features. |
| 5 | Future-release inventory after the approved feature cutoff (only named feature prerequisites may be brought forward): larger BOX-01, dependency/linkage, compatibility packaging and remaining portability changes; GEN-01 parser/runtime adoption is locally qualified; verified MONITOR-01 gaps and MODULE-SCALE-01 consumers; REPRO-01 and remaining release tooling. ARCHIVE-01 follows each verified migration. Adopt bounded EWMH-01/SELECTION-01 protocol gaps after INPUT/DRAW contracts; XDG-01 follows launcher/parser/process contracts, and TEXT-01 establishes text/input contracts before new desktop UI. | Broad or compatibility-sensitive changes retain their individual gates. Remove a predecessor after its replacement is accepted; do not wait for every unrelated migration or remove anything before parity. |
| 6 | BUTTON-01, BUTTON-02, CFG-01, CFG-02 and IPC-02 in the owning build thread, then SESSION-01's portable default session foundation. Then FILE-VIEW-01 / FILE-DATA-01 / FILE-ACTION-01 deliver the configurable browser checkpoint, followed by FILE-JOBS-01 and DESKTOP-ICONS-01; FILE-INTEGRATION-01's portable entry point follows XDG-01 and the browser/property contracts. CONTENT-01 / RETENTION-01 / STORE-01 and CLIPBOARD-01's text/image handoff follow qualified selection/collection/action contracts; encrypted persistence follows protected memory, with file mutation and D-Bus not prerequisites. A11Y-01 follows the button/text contracts; SETTINGS-01’s native X11 preference bridge follows CFG-01 and SESSION-01. THEME-01 configuration review can proceed here after standards documentation; runtime/generation gaps follow their existing owners. | Desktop primitives follow the finite safety closeout and their named dependencies; deferred tuning does not block them. Session lifecycle reuses shared settings, process/readiness helpers and operation results. These are requirements, not implemented features or permission for a broad redesign. A qualified browser handoff lets a separate config/theme build thread use the documented primitives while core work continues. See [session scope](decisions/SESSION-INTEGRATION-2026-10-06.md), [file-manager scope](decisions/FILE-MANAGER-PRIMITIVES-2026-10-06.md) and [clipboard scope](decisions/CLIPBOARD-PRIMITIVES-2026-10-06.md). |
| 7 | SESSION-02's optional bus bootstrap/dependency contract, then NOTIFY-01 and TRAY-01's XEmbed/StatusNotifier checkpoints; finish SESSION-02 integration/qualification with those owners. FILE-INTEGRATION-01's optional FileManager1 adapter follows the shared bus and qualified folder/reveal/properties operations. Qualify SETTINGS-01’s optional portal bridge and A11Y-01’s selected assistive profile with the same bus/service lifecycle. | The optional D-Bus contribution extends the same default session settings/UI. Select the common bus contract before protocol adoption to avoid competing lifecycle/transport work. Preserve portable login without optional packages and respect existing owners; the native locker is deferred. See [session scope](decisions/SESSION-INTEGRATION-2026-10-06.md) and [service scope](decisions/NATIVE-DESKTOP-SERVICES-2026-10-06.md). |
| 8 | RELEASE-01: final public-release gate, after planned coding/qualification checkpoints and the user's decision to prepare a release. Assemble STANDARDS-01 requirement evidence, THEME-01 review and truthful profile-specific claims at this gate. | Review identity/README, community/security reporting, install/trial, artifacts/provenance, man quality, contributor onboarding and public presentation. Reuse existing owners; record optional ideas' disposition. This is the final release stage and does not reorder coding or authorize publication. See [release gate](RELEASE-GATE.md). |
| 9 | I18N-02: complete language polish after the existing program; translated UI, manuals and shipped configuration comments for all eleven current catalog locales. | Stable sources and qualified TEXT/I18N/DOCS foundations reduce translation churn. Conditional release preparation stays conditional; this adds no existing release prerequisite. See [localization scope](decisions/LOCALIZATION-POLISH-2026-10-08.md). |

Shared desktop contract scope is assigned to the existing stage 6/7 owners in
[the shared-contract backlog](#shared-desktop-contracts-and-later-exploration):
capability/state reporting, typed actions, asynchronous progress/cancellation,
collection filtering and owned-setting preview/revert. Use current session,
file and clipboard consumers; preserve the finite hardening closeout and named dependencies.
DESKTOP-EXPLORE-01 waits until the existing stages 1 through 8 are handled
(RELEASE-01 when requested). It adds no release prerequisite or publication step.

The approved [runtime language migration](decisions/RUNTIME-GO-LUA-2026-10-06.md)
adds Go transport/utility fixtures alongside stage 3 PROMPT-01, incremental
RUNTIME-GO-01/UTIL-01 ports between existing checkpoints, and qualified desktop
helper replacement plus COMPAT-02 Perl retirement in stage 5 before dependent
stage 6 services. LUA-01's isolated scripting host and primitive surfaces follow qualified
CFG/IPC/collection contracts; engine selection remains open. The 2026-10-23
[dependency decision](decisions/DEPENDENCY-SIMPLIFICATION-2026-10-23.md) keeps
these general migrations in Later under the existing feature handoff.
TOOLING-GO-01 follows runtime qualification separately; adopted Python build/test
tooling remains for now.

Late language polish requested on 2026-10-08: I18N-02 follows the existing
implementation/qualification program (stages 1 through 8 when applicable).
Complete all eleven current catalog locales across UI, manuals and shipped
configuration comments; reuse earlier I18N-01/TEXT-01/DOCS-01 foundations.
RELEASE-01 remains conditional and gains no new prerequisite. Tool/service
choices are proposed, with no implementation or external integration adopted.
See the [localization scope](decisions/LOCALIZATION-POLISH-2026-10-08.md).

## Active implementation batch and test cadence

User direction on 2026-10-06: complete related stories before extensive testing.
Use [focused development checks and combined qualification](WORKFLOW.md#implementation-batches-and-test-cadence)
as the default. Full Linux/BSD coverage remains required at the batch checkpoint;
it is not repeated after each story. Platform-sensitive work gets an early focused
check on the affected target. Keep implementation progress separate from passing
full qualification, and preserve valid earlier evidence when repairing failures.

The current IMAGE-01 / INPUT-01 / FD-01 work can proceed through these reviewable stories
with image activation locally qualified and the next input batch in development:

| Story | Work and next focused gate | Status |
| --- | --- | --- |
| Module ownership and construction follow-up | Freeze related ownership, transport and verification stories and run one combined gate per batch. | Use the [current checkpoint summary](#current-checkpoint-status) for accepted, running and queued sources. Earlier lifecycle, transport, producer, reader, Form and X-service evidence remains in the dated reports and progress ledger. Retain failures and exact affected recoveries; timing causes remain unestablished. The current combined core milestone is locally qualified; unrelated boundary implementation remains outside its scope. Native release CI remains open. |
| Shared image foundation | Owned process transport, immutable picture views, bounded conversion, X acknowledgement and format fallback; affected transport/ownership/X fixtures. | Implemented; preparation evidence recorded. Core adoption passes the combined local activation checkpoint. |
| Core scheduler adoption | Connect ordinary readiness/deadlines, nested/interaction transport-only servicing, owned X filters and shutdown; focused independent-input/timer/cancellation checks. | Normal startup activation is implemented before defaults/user configuration. Eleven focused Linux owner/startup/restart checks pass, including 24/8-bit startup/restart and retained effects during other input. The combined frozen-source matrix is accepted; see the activation report. [Report](verification/VERIFICATION-2026-10-06-SVG-CORE-ADAPTER.md). |
| Background and decoration owners | Prepare colorset/decor replacements before release; retain old backing and resolve variants at each window’s current scale; focused first/replacement/refusal and scale checks. | Absent drawing and borrowed-view cleanup guards implemented. Confirmed colorset shape duplicate release fixed with focused actual-core normal/sanitizer coverage. Colorset background/shape and decoration replacement owners are implemented. Focused actual-core colorset packets and synthetic/real SVG decoration pixels pass at 1×/2×. The direct face-transfer/scale/cancellation probe passes on 24/8-bit X with local ASan/UBSan. Colorset shapes now borrow acknowledged immutable masks without an extra copy/barrier; shared-owner cleanup passes 24/8-bit focused normal/sanitizer checks. [Shape/average report](verification/VERIFICATION-2026-10-06-COLORSET-SHAPE.md). Derived background/tint acknowledgement and bounded completion now pass the [effects checkpoint](verification/VERIFICATION-2026-10-06-COLORSET-EFFECTS.md); the approved [decoded-RGB averages](decisions/COLORSET-AVERAGE-PROPOSAL.md) are implemented with 16 focused normal and 16 scoped sanitizer checks passing. [Average report](verification/VERIFICATION-2026-10-06-COLORSET-AVERAGE.md). Valid 4096-pixel extents now survive packets; checked packet numbers reject overflow before mutation, with focused normal/sanitizer/GCC software coverage. [Geometry/effects controls](verification/VERIFICATION-2026-10-06-COLORSET-GEOMETRY.md), [packet report](verification/VERIFICATION-2026-10-06-COLORSET-PACKET.md). The [effect preparation proposal](decisions/COLORSET-EFFECTS-PROPOSAL.md) preserves backend arithmetic and extends finite asynchronous software preparation; implemented after user approval: exact backend pixels, retained source/color/context lifetimes, bounded worker upload and atomic acknowledged revisions pass focused Linux/BSD gates. [Effects report](verification/VERIFICATION-2026-10-06-COLORSET-EFFECTS.md). Combined image activation is locally qualified. [Owner report](verification/VERIFICATION-2026-10-06-IMAGE-OWNERS.md). |
| Menu and icon owners | Preserve mapped menu snapshots, retain file/mini-icon leases and reconcile ready geometry; focused owner destruction, override and mapped-menu checks. | WindowList immutable mini-icon snapshots and prepare-before-release mini-icon changes implemented; retention, same-key pixmap identity and actual popup/dismiss/reload checks pass. Mini-icon identity/ready-packet/donation owners and general menu item/root/style owners are implemented. Focused 1×/2× actual-core checks pass, including mapped popup stability and an independent completion with a persistent tear-off. Direct owner policy/lifetime checks pass with scoped sanitizers. File-icon pending/published leases, resize/donation ownership and native application icon priority are implemented; three focused normal and three scoped sanitizer entries pass at 24/8-bit and 1×/2×. Combined image activation is locally qualified; live mixed-monitor movement remains a separate open check. [File-icon report](verification/VERIFICATION-2026-10-06-FILE-ICON-OWNERS.md). [Owner report](verification/VERIFICATION-2026-10-06-IMAGE-OWNERS.md). |
| Cursor and root-tile owners | Share the one active worker, preserve cursor alpha/hotspot and root visual, cancel superseded requests; focused cursor/XorPixmap lifetime checks. | XorPixmap immutable tile/pending ownership is implemented, with its duplicate pixmap copy removed; 24/8-bit focused normal and leak-enabled sanitizer probes pass. [Root-owner report](verification/VERIFICATION-2026-10-06-ROOT-IMAGE-OWNER.md). Cursor preparation and acknowledged handle transfer are implemented; the final owner regression passes 16 normal and 16 scoped sanitizer entries, plus one Xcursor/XRender-disabled entry. [Cursor report](verification/VERIFICATION-2026-10-06-CURSOR-IMAGE-OWNER.md). The user approved the simpler FVWM bitmap [fallback](decisions/CURSOR-CORE-FALLBACK-PROPOSAL.md) on 2026-10-06. A focused Xcursor-enabled/XRender-disabled GCC profile now passes both 24/8-bit cursor entries, closing that preparation gap; [evidence](verification/VERIFICATION-2026-10-06-COLORSET-SHAPE.md). Derived colorset acknowledgement and bounded completion pass the [effects checkpoint](verification/VERIFICATION-2026-10-06-COLORSET-EFFECTS.md); combined activation is accepted locally. Combined image activation is locally qualified. |
| Combined activation and qualification | Enable only after all owners are ready; run actual slow-load WM responsiveness checks, then the full frozen-source Linux/BSD/install/default-X/optional/sanitizer gates once for the combined batch. | Accepted local qualification: eight compiler legs / 908 entries, 120 staged identities, 52 default-BSD-X driver invocations, software/SVG-disabled 118/101 entries and 30 scoped sanitizer checks. All 1261 source hashes/modes match. Native release sanitizer CI and physical checks remain open. [Activation report](verification/VERIFICATION-2026-10-06-IMAGE-ACTIVATION.md). |

The user approved the [ambiguous-image timing decision](decisions/IMAGE-ASYNC-FORMAT-PROPOSAL.md)
on 2026-10-06. This clears the timing decision for compressed XPM and unusual
formats; core activation and owned-resource qualification are accepted locally.

## Tools, porting and cleanup scheduling

The other thread's tools are an early workstream, not one late rewrite.
MAINT-01 began with 20 isolated tests; the adoption batch has 25 focused
checks. MAINT-02 remains P1. The build wrapper and shared manual tooling are
active; other generators remain candidates and release publishing is unchanged.
See the [migration crosswalk](../tools/maintenance/README.md#migration-crosswalk)
and [tool evidence](verification/MAINTENANCE-VERIFICATION-2026-10-05.md).

| Work | Placement and next gate |
| --- | --- |
| Inventory and explicit candidate tools | Available now; use source/dependency inventory for early audits and run candidates against isolated inputs/builds. |
| Python build wrapper and shared manual tooling | Adopted; actual default/minimal/feature/install and Asciidoctor evidence is in the tooling report. Linux AMD64 Go remains an emulation qualification gap. The public website migration is deferred under DOCS-01; predecessors remain for comparison and rollback. |
| Constants/command metadata | Locally adopted with PERL-PROTO-01: native wire values/masks/decoders/dispatch, committed exports/POD and installed consumers pass six targets. Checked generation emits JSON companions and exact input hashes; normal builds need no generation. |
| Standalone utility ports | Prepare CLI/output/quoting/Unicode/filesystem fixtures alongside PROC-01; port one utility to Go at a checkpoint under UTIL-01/RUNTIME-GO-01. Preserve applicable CLI behavior; COMPAT-02 now owns the explicitly approved Perl API retirement and migration notices. |
| Shared cleanup and proven inactive blocks | Small early batches or part of the owning fix; CLEAN-01/DEAD-01 need not wait for every performance item. |
| Gettext and source-release tooling | Incremental adoption once catalog/NLS and clean source-distribution gates pass. A fixture archive is not a verified FVWM release. |
| Parser/scanner and Perl retirement | GEN-01 modern parser/runtime and developer wrapper are locally adopted. COMPAT-02 Perl retirement remains separately gated; its runtime/install/manual changes are queued. |
| Old tools and historical packaging cleanup | ARCHIVE-01 follows the accepted replacement's parity, reference and distribution checks. Retain predecessors until then. |

Take one maintenance migration to a checkpoint between bounded runtime batches;
this is a scheduling policy, not an instruction to spawn agents or modify
another thread's active build. Any reproduced urgent defect still takes priority.

## Dependency simplification backlog

Accepted 2026-10-23: mandatory Go for building, all first-party Perl/Python
retirement, FVWM 3.x baseline, obsolete converter/backend removal and explicit
target linkage. The [decision](decisions/DEPENDENCY-SIMPLIFICATION-2026-10-23.md)
owns scope and adoption criteria. The later maintenance-first request selects
these children now and pauses feature pickup. Meson and optional-Go builds
remain the actual contract until their respective implementation lands.

All six children move to milestone 05 / P2, retaining stage 05 ownership.
#179/#181 are Ready; the other four stay Backlog with their batch/dependencies:

| Child | Parent and checkpoint |
| --- | --- |
| [DEP-GO-01 #179](https://github.com/gonz0w/fvwm-x/issues/179) | RUNTIME-GO-01 #23: require the approved compiler and retire disabled-Go utility install branches. |
| [DEP-XDG-01 #180](https://github.com/gonz0w/fvwm-x/issues/180) | COMPAT-02 #42: retire replaced PyXDG/Perl menu and obsolete PIL/Pillow converters after #179; preserve modern menus. |
| [LEGACY-01 #181](https://github.com/gonz0w/fvwm-x/issues/181) | ARCHIVE-01 #40: retire verified pre-FVWM3-only installed entries, including fvwm-convert-2.6; coordinate DEAD-01 #35. |
| [BUILD-EVAL-01 #182](https://github.com/gonz0w/fvwm-x/issues/182) | TOOLING-GO-01 #55: compare Muon/CMake with real representative targets before a separate build adoption decision. |
| [SVG-CONSOLIDATE-01 #183](https://github.com/gonz0w/fvwm-x/issues/183) | LIB-04 #46: remove obsolete SVG backend branches while retaining maintained rendering and SVG-disabled profiles. |
| [LINK-TARGETS-01 #184](https://github.com/gonz0w/fvwm-x/issues/184) | LINK-01 #49: per-target linkage with measured symbol/transitive dependency evidence. |

#180 is natively blocked by #179; #42 is blocked by #179 and LUA-01 #50 for the
selected scripting migration handoff. No general feature dependency is added.
#23 retains remaining Python runtime ports; #55 owns remaining first-party tools
and whole-build/test interpreter closure. #50 compares Go-hosted and official
native Lua before engine adoption. #48 compares cJSON/yyjson on actual workloads;
#46 may compare LunaSVG separately, with no renderer replacement implied.
No dependency removal or performance/security improvement is claimed by this
planning checkpoint. [Roadmap verification](verification/DEPENDENCY-PLANNING-2026-10-23.md)
records the readback and local documentation checks.

## Active backlog

Within each order, bundle roughly three to five related stories behind one
reviewable acceptance checkpoint and PR. Finish that checkpoint before switching;
a story is not automatically a separate PR. Small safe cleanup can accompany
its owning fix. Forcing a library
update, timer heap, decoder helper or JSON replacement is not an acceptance
criterion; choose the least complex implementation that meets the evidence.

Release qualification also needs the remaining evidence recorded in
[SUPPORT.md](SUPPORT.md): representative third-party themes, physical GPU/input
and mixed-monitor hotplug, external compositors and actual GTK/Qt feedback.
These are hardware/compatibility qualification tasks alongside VERIFY-01 and
COMPAT-01, not evidence supplied by the local QEMU checkpoint.

| ID | Priority / status | Work and acceptance |
| --- | --- | --- |
| SCRIPT-01 | P1 / in progress | Bounded files/messages/commands/output, checked parser/widget arithmetic, literal tags and user-approved execution/storage/drawing policies are implemented. Shared sleeping delays and MiniScroll release cancellation preserve first action/held cadence. Sparse colorsets preserve large IDs with stable records, checked admission and ordered packets; approved limits are 4096 distinct sets/65536 override and zero cloning for undefined module IDs. SendMsgAndGet now uses checked owned FIFO endpoints, nonblocking transport, terminated replies and the approved five-second overall deadline with override. Latest full gates pass Linux 75/75 on three legs and BSD 64/64 on four targets; ten FIFO outcomes plus prior colorset/widget/storage/execution/selection checks pass on all four default BSD servers. Seven archives match 160 fingerprints/1159 modes and fresh tested-code installs. [FIFO report](verification/VERIFICATION-2026-10-05-SCRIPT-FIFO.md), [colorset report](verification/VERIFICATION-2026-10-05-COLORSETS.md). Text/list findings are now repaired and verified on Linux 77/77 and BSD 66/66 with staged default-server checks; see the [text/list report](verification/VERIFICATION-2026-10-05-SCRIPT-TEXT.md). The [interactive text budget](verification/VERIFICATION-2026-10-05-SCRIPT-TEXT-LIMIT.md) is approved and fully verified. The approved [geometry/font-layout checkpoint](verification/VERIFICATION-2026-10-06-SCRIPT-GEOMETRY.md) passes 586 full-suite executions, exact installs and all default BSD X checks. The approved [font-shadow checkpoint](verification/VERIFICATION-2026-10-06-FONT-SHADOW.md) passes 600 full-suite executions, exact installs and shared consumer recovery across Linux/default BSD X. Asynchronous core image activation is locally qualified; next, common button-release servicing is locally qualified; active viewport and menu follow-ups are locally qualified under their separate frozen checkpoints. Unicode/font-set measurement and bounded modal/descriptor service are strictly locally qualified under the [font/modal checkpoint](verification/VERIFICATION-2026-10-11-FONT-MODAL.md), including explicit resource revisions. Broader resource lifetime/budgets, TEXT-01 shaping/IME/font fallback and native Linux release sanitizer execution remain open. |
| VERIFY-01 | P1 / release gate; broader native qualification open | Run actual native Linux ASan/UBSan and real-X checks locally; add bounded parser/protocol fuzz entry points, verify ELF hardening and preserve failures/skips. Emulated passes and configuration alone do not qualify native execution. No remote CI executor or GitHub Actions is required under the [local-review decision](decisions/LOCAL-REVIEW-GITHUB-2026-10-13.md). Reuse exact accepted profiles and continue affected local regressions; physical hardware/toolkit/compositor checks retain their own gates. The window-update checkpoint retains an unchanged Linux AMD64 GCC topology pressure-noise 100 ms xrandr failure after isolated/idle controls. [Evidence](verification/VERIFICATION-2026-10-15-WINDOW-UPDATES.md). The current style-filter full run passes that test but retains OpenBSD AMD64 desktop/MFL, harness readiness and verification-mock deadline failures after one unchanged control; its fresh install/staged geometry remain unperformed. [Current gaps](verification/VERIFICATION-2026-10-15-STYLE-FILTER.md). |
| COMPAT-01 | P1 / configuration compatibility gate; scope clarified | Representative six-platform baseline is implemented. Check existing configuration syntax and useful configured command/binding/function/theme behavior with affected real-X fixtures. Module-protocol tests establish correctness of shipped consumers, not an unchanged external ABI promise. Exact legacy rendering and incidental behavior are not blanket release gates. Ask before deliberate configuration incompatibilities; approved Perl retirement remains recorded. Reconcile open compatibility-only work against the [clarified scope](decisions/CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md); retain prior evidence and independent safety/platform gates. |
| MONITOR-01 | P1 / stage3 reconciled; runtime/release gaps queued | Reconcile archived v0.1 hotplug/removal, selective scale reload, diagnostics/simulation and geometry requirements against current code/tests; namespace historical IDs to avoid collisions. Reuse existing topology/transition/interaction policies, record passing evidence and identify only actual gaps. Audit with VERIFY-01/COMPAT-01; implement verified gaps at later bounded runtime checkpoints before release. No reset of completed geometry work. [Stage3 audit](verification/RECONCILIATION-2026-10-12-STAGE3.md) retains output-loss recovery evidence and selective runtime reload gaps. See [crosswalk](PROJECT.md#monitor-and-hidpi-requirements-migration). |
| MODULE-SCALE-01 | P2 / queued consumer audit | Audit existing additive monitor/scale metadata and every bundled module's UI consumer before adding missing behavior. Preserve actual-pixel packet contracts and public Perl consumers; coordinate MONITOR-01, IPC-02 and COMPAT-01 rather than creating parallel protocol work. Implement proven gaps after current hardening/responsiveness, with native and real-X six-target evidence. See project crosswalk. |
| LIB-02 | P1 / in progress | Actual six-target image/dependency inventories and PNG path review are recorded. PNG recovery is verified; several installed PNG packages lack matching fix proof. The [secure PNG default](decisions/PNG-SECURITY-BASELINE-PROPOSAL.md) was explicitly approved; its configure guard and six-target patched-dependency qualification are accepted. Patched PNG qualification is accepted; continue transitive review; preserve optional formats and coordinate IMAGE-01. [Audit](DEPENDENCY-AUDIT-2026-10-05.md). |
| LIB-07 | P1 / in progress | Actual XPM/X11/font/server packages, binary paths and advisory candidates are recorded. Ubuntu XPM backport is confirmed; Debian fixed XPM is qualified in a private authenticated package profile; its older system package is unchanged. FreeBSD server/font upstream baselines are current for reviewed July fixes; XPM is absent in this profile. All signed OpenBSD snapshot updates are applied and rebooted; both full/default-X revalidation profiles pass. OpenBSD XPM libraries match signed release sets and their signed source contains the vendor fix. Ubuntu server and broader transitive qualification remain open. A retained OpenBSD AMD64 lab guest has two pre-client VESA/DDC startup faults; unchanged client/Script controls pass, cause unestablished. The four-story [startup diagnostic/comparison checkpoint](verification/VERIFICATION-2026-10-14-BSD-X-STARTUP.md) is locally qualified with retained/fresh authenticated controls and unchanged installed clients. Child [#93](https://github.com/gonz0w/fvwm-x/issues/93) keeps the cause/repair investigation open, deferred until much later at the user's request on 2026-10-14; this does not revoke or relabel the earlier separately owned patched-image results. [Audit](DEPENDENCY-AUDIT-2026-10-05.md). |
| LIB-01 | P1 / reviewed import and encoding locally qualified | Reviewed pinned cJSON 1.7.19 retains its MIT notice and public ABI. Ordinary output parity passes on six targets; downstream numeric fixes cover finite-max decimal overflow and NaN integer cache. User-approved per-byte U+FFFD conversion applies only to JSON keys/strings; valid UTF-8 and stored/native bytes remain unchanged. Checked expansion/offset arithmetic, allocation failure and strict real-MFL tests pass, plus Linux GCC/Clang/leak-enabled sanitizer and early BSD ARM64 policy checks. Exact 1282-file combined checkpoint passes 1056 standard entries/eight profiles, 120 staged identities, 40 default BSD X drivers, optional 137/117 and sixteen scoped sanitizers; all seven archives validate hashes/modes and EOF. Native release qualification remains open; no version-only security verdict or whole-checkout claim. [Report](verification/VERIFICATION-2026-10-07-JSON-BOUNDARY.md), [decision](decisions/JSON-UTF8-BOUNDARY-PROPOSAL.md). LIB-06 replacement evaluation remains separate. |
| LIB-03 | Current dependency/compiler checkpoint locally qualified and merged | User-approved patched optional compiler policy is implemented, with six native Go 1.27.2 SDKs and a real Go 1.26.9 minimum check. Used pins/426 retained third-party payload files remain unchanged after unused Logrus removal; native package/symbol scans have only three unimported module-level observations. Offline builds, registered/installed/nested-X/default-BSD-X and Linux race checks pass in their recorded scope. Fixed SDK preparation replaces the retired OpenBSD Go package pin. Ongoing patch maintenance and release/native-CPU/physical qualification remain distinct. [Report](verification/VERIFICATION-2026-10-14-GO-BASELINE.md); unrelated VESA startup reliability is LIB-07 #93. |
| MAINT-02 | P1 / partial adoption | Compatible build wrapper, shared Meson/manual tooling and modern GEN-01 parser/runtime and I18N-01 translation maintenance adopted, with BSD ports gettext discovery fixed. The tooling report records actual profiles, installs, documentation comparisons, source/mode provenance and historical Go gaps and separate native CI/release limits; the Prompt checkpoint now qualifies isolated native AMD64 Go/Meson builds. Public website migration is deferred to DOCS-01 and is not a maintenance blocker. Checked metadata is locally adopted with PERL-PROTO-01; source inventory/archive candidate memory and provenance are locally qualified in child #84/#85 ([report](verification/VERIFICATION-2026-10-13-SOURCE-TOOLS.md)); build reuse and consolidated PR cadence are locally qualified in child #87 ([report](verification/VERIFICATION-2026-10-13-BUILD-EFFICIENCY.md)); published referenced developer docs/evidence and current file-link checks are locally qualified in child #89 ([report](verification/VERIFICATION-2026-10-13-DOCUMENTATION-EVIDENCE.md)); source archive workflow adoption retains its separate release gates; utility ports and predecessor removal remain queued. The [preparation/evidence checkpoint](verification/VERIFICATION-2026-10-21-BSD-PREPARATION.md) passes affected native qualification; broader scope remains open. |
| DATA-01 | P1 / in progress | Typed, capped EWMH acquisition and complete fixed records implemented. Motif/Open Look records and ICCCM MULTIPLE pairs validated before indexing, with native and real-X coverage. Legacy names/class/command/session strings, protocols and colormap lists are now capped; startup WM_STATE and root restart markers are validated. Standard text encodings, empty command arguments and Corel protocol types remain supported. Module selection/text readers now validate complete bounded properties; timed private selection requests isolate late replies, and root cut-buffer/title/pixmap replies have explicit ownership. The tree-query ownership follow-up now fixes stale failure output and parent-mismatch child-list leaks, with focused Linux/BSD ARM64/leak-enabled sanitizer coverage and the accepted 2026-10-07 combined Linux/BSD/default-X/optional/sanitizer checkpoint. [Report](verification/VERIFICATION-2026-10-06-TREE-QUERY.md). Active ancestor/pointer/frame bounds are qualified in PR131; the [visual-startup checkpoint](verification/VERIFICATION-2026-10-20-VISUAL-STARTUP.md) qualifies inherited ID admission, visual reply ownership and initial-color refusal. Swallow-proxy registration, pending-peer retention and property replies are qualified in the session-ownership checkpoint. Remaining: broader tree-walk inventory, icon acquisition/decoder stress and module interpreter value handling. Shared string copies are qualified in [PR #138](https://github.com/gonz0w/fvwm-x/pull/138); [report](verification/VERIFICATION-2026-10-21-STRING-COPIES.md). Broader owner scope stays open. |
| IMAGE-01 | P1 / asynchronous core activation locally qualified; wider audit open | Regular bounded primary inputs, finite compressed-XPM transport and isolated SVG rendering retain approved limits. Normal startup now enables asynchronous completion and acknowledged immutable owners for colorsets/effects, decor, menus, icons, cursors and XOR tiles. Eight compiler profiles pass 908 entries, 120 staged identities and 52 default-BSD-X driver invocations; optional/scoped sanitizer coverage and all 1261 source identities are recorded. Wider decoder CPU/storage/permissions isolation and mixed-monitor/physical/release-CI checks remain open. [Activation report](verification/VERIFICATION-2026-10-06-IMAGE-ACTIVATION.md). |
| FD-01 | P1 / partially implemented | Core module pipes get close-on-exec before fork; child keeps only protocol endpoints; failed exec uses _exit. Read/PipeRead streams fail closed on flag setup. MFL locks reject unsafe files, retain one inode across restart, and direct signals exit libevent. Native lifecycle and default-X restart tests pass. ICE listeners, accepted connections, authentication streams and core session connections now get checked close-on-exec before use. Literal iceauth argv removes shell interpretation of save-directory paths. Disconnect callbacks no longer leave freed watch/pending nodes; authenticated registration, early disconnect, shutdown and cleanup pass real-X tests. Six injected pipe/flag/stat failures now verify closed descriptors, reset outputs and preserved errors; the module selection helper also passes above the select descriptor ceiling. Native startup now validates decimal endpoints, applies checked close-on-exec and preserves borrowed ownership on failure; five legacy native modules adopt the shared parser. Listen-only incoming -1 remains supported. Four injected parser flag failures, high descriptors where permitted, fork/exec and actual Script subprocess checks pass. Module fork refusal cleanup, finite single-query descriptor width and ICE/session range admission plus watched-record cleanup are locally qualified in schema/descriptor checkpoints. Auto poll readiness is locally qualified in consumer1361. Ident poll/payload admission and Pager poll/configuration progression are locally qualified in popup1363; IconMan/Buttons/Script readiness and Script placeholder ownership are locally qualified in readiness1366; Form poll/timer/draw ownership is locally qualified in module-ui1370. Core Exec and session authentication/publication constructor failures are now locally qualified in the process-startup checkpoint. Swallow-proxy registration/refusal and pending-close ownership are qualified in the session-ownership checkpoint. Remaining: other actual helper/session/subprocess constructor failures, forced high live admission, wider readiness owners, Perl and broader descriptor ownership. Shared module logging descriptor remains intentional. [Follow-up](verification/VERIFICATION-2026-10-04-SCRIPT-BOUNDS.md). Stream construction/continuation and diagnostic adoption/failure contracts are locally qualified in PR141; remaining broad audit and descriptor-provenance review stay open. [Stream report](verification/VERIFICATION-2026-10-21-STREAM-OWNERSHIP.md). |
| PROC-01 | P1 / partially implemented | Core Exec stdin/fork admission and non-flushing child failure exit, plus complete session-authentication preparation and publication rollback, are locally qualified in the process-startup checkpoint. Continue utility/dialog subprocess construction and descriptor ownership with FD-01; use literal argv where compatible and reproduce findings before asserting exploitability. See cleanup roadmap. Stream construction/continuation and diagnostic adoption/failure contracts are locally qualified in PR141; remaining broad audit and descriptor-provenance review stay open. [Stream report](verification/VERIFICATION-2026-10-21-STREAM-OWNERSHIP.md). |
| INPUT-01 | P1 / partially implemented | Shared Move/Resize wait and cancellation state now holds IDs, retains lifecycle events for normal dispatch after return, and ends on client destruction/withdrawal or termination. Motion coalescing has a 32-event budget and release/key barriers. Client/titlebar and click-to-focus cancellation, real wireframe server-grab release, unrelated destruction and queue ordering pass native/real-X gates across all six platforms and four BSD default servers. The enclosing function resumes after a normal release or cancellation. Context clones now own triggering-event snapshots, and each module has a 256-command pending-input limit with independent lifetime pins. Audited direct commands and safe timer heads now run in bounded passes during held drags and continuous motion; independent producers can pass deferred work while each producer retains its active continuation and command order. Temporary execution restores both cached X events. Synchronous frame snapshots are owned, bounded and coalesced, then refreshed/delivered after outer unwind; intermediate replies cannot hold the grab open. Unsafe timer heads retain deadline order. The user approved common button-release servicing and 20-ms pointer/wait-cursor timing. Release-state policy, bounded readiness and the shared audited pump are implemented; local qualification passes eight profiles/924 entries, 120 installed identities, all default BSD X, optional profiles and five scoped sanitizer checks. Active viewport service is locally qualified: eight profiles/952 entries, 120 installed identities, 16 default-BSD-X drivers, optional 124/105 and nine scoped sanitizer checks. Active-menu servicing, bounded motion and monotonic submenu timing are locally qualified: eight profiles/984 entries, 120 installed identities, 12 default-BSD-X drivers, optional 128/108 and eight scoped sanitizer checks. [Release-wait report](verification/VERIFICATION-2026-10-06-BUTTON-RELEASE.md). Remaining: target lifetime pinning, arbitrary style/resource mutation, hints/topology and nested interactions. The selected function-wait lifetime family is locally qualified in [PR143](verification/VERIFICATION-2026-10-21-FUNCTION-LIFETIMES.md); broader mutation/topology/nested owners remain open without silently expanding the finite closeout. |
| PERL-PROTO-01 | P1 / repair strictly locally qualified | Ten extended event values, exact native mask, three-word Echo decoding and category-aware dispatch are repaired. Checked generation preserves exports/POD and emits provenance plus JSON companions. Six-target native/installed/default-BSD-X consumers pass. Perl retirement remains under COMPAT-02 with its own source/install/reference gates. See [current report](verification/VERIFICATION-2026-10-11-PROMPT-METADATA.md). |
| PROMPT-01 | P1 / strictly locally qualified | Shared standard-library transport repairs framing, checked writes, bounded incremental JSON and coordinated EOF/lifetime. One-shot writes finish before exit; Quit confirms locally. Startup profile/EOF ordering is covered. Native transport, actual MFL, installed/default-BSD-X and Linux Go race/vet checks pass; broader CLI consolidation remains separate. See [current report](verification/VERIFICATION-2026-10-11-PROMPT-METADATA.md). |
| CI-01 | P1 / local verification foundation qualified; GitHub foundation merged | Accepted owned runner5200 and PNG/provider finalcec1/helper027890 qualify fixed inputs, fresh disks, private authentication and bounded process/evidence cleanup. Six-target helper336, Linux GCC/Clang dependency builds and Go17/install/race/PNG2, and four BSD standard/install plus Go17/installed7/default-X6/PNG consumers pass. gonz0w/fvwm-x is private with Actions disabled. The approved local-only policy retires remote execution and workflow-adoption requirements; native CPU/sanitizer and inherited container-content evidence remain explicit. Signed PRs, source-bound local evidence and manual roadmap mirrors replace hosted checks. Repository administration is configured; all81 tracker issues, ten milestones, matching Project fields and27 native prerequisites are present. [Foundation PR #83](https://github.com/gonz0w/fvwm-x/pull/83) is merged; [stable index](GITHUB-ROADMAP.md) preserves mappings. Publication/releases retain separate ownership. [Hosting decision](decisions/LOCAL-REVIEW-GITHUB-2026-10-13.md), [bootstrap report](verification/VERIFICATION-2026-10-13-CI-PNG-BOOTSTRAP.md). The window-update checkpoint retains an unchanged OpenBSD AMD64 ci-asset-contract two-second worker deadline failure in full and isolated runs. [Evidence](verification/VERIFICATION-2026-10-15-WINDOW-UPDATES.md). The current style-filter full run and single unchanged control retain that failure; four broad OpenBSD AMD64 cases remain failed and its fresh installation is prevented. FreeBSD AMD64 passes its single unchanged CI-worker control. [Current gaps](verification/VERIFICATION-2026-10-15-STYLE-FILTER.md). The merged decoded-anchor checkpoint retains the same unchanged two-second worker timeout without retry for green. [Current receipt](verification/VERIFICATION-2026-10-19-STYLE-ANCHORS.md). The [preparation/evidence checkpoint](verification/VERIFICATION-2026-10-21-BSD-PREPARATION.md) passes affected native qualification; broader scope remains open. |
| CLEAN-01 | P2 / cross-cutting, partially implemented | Extract tested pure protocol/geometry/ownership policy and consolidate duplicated cleanup within affected fixes. Own command-event snapshots; audit remaining borrowed window/module/resource references. Establish window identity/lifetime, pure policy, X reads/commit writes and shared readiness boundaries; replace typeless scratch state within its owning fix. Document reply waits, callback mutation and borrowed lifetimes. Avoid a separate broad rewrite, mechanical renaming or style-only churn. See optimization roadmap. Shared string copies are qualified in [PR #138](https://github.com/gonz0w/fvwm-x/pull/138); [report](verification/VERIFICATION-2026-10-21-STRING-COPIES.md). Broader owner scope stays open. Native Form fontconfig/Xft process-exit allocations are independently reproduced during the [settings editor checkpoint](verification/VERIFICATION-2026-10-22-SETTINGS-EDITOR.md); exit cleanup remains in Later and the leak-enabled gate is not clean. The [preparation/evidence checkpoint](verification/VERIFICATION-2026-10-21-BSD-PREPARATION.md) passes affected native qualification; broader scope remains open. Stream construction/continuation and diagnostic adoption/failure contracts are locally qualified in PR141; remaining broad audit and descriptor-provenance review stay open. [Stream report](verification/VERIFICATION-2026-10-21-STREAM-OWNERSHIP.md). |
| DEAD-01 | P2 / inactive-function batch locally qualified; unbuilt template/fallback removal focused-passed | The six listed inactive functions and one additional unused stack-ring function are removed after reference and normalized active-preprocessing equality checks (234 lines). The exact 1314-file combined traversal qualification passes eight compiler profiles/default BSD X/install/optional/scoped sanitizer gates. Review unbuilt templates/fallback sources separately; do not infer active dead code from lexical searches. See cleanup roadmap. |
| QUEUE-01 | P1 / locally qualified | Baseline shared fqueue front removal scanned the remaining list; 4096/16384-item probes produced 8,390,656/134,225,920 cleanup visits. Explicit O(1) unlocked FIFO transfer now serves output and ready scheduler/event drains; recursive callback/locking/tombstone ownership remains in the existing API. The combined FIFO/stack/output checkpoint is locally accepted: 1008 entries across eight compiler profiles, 120 installed identities, 24 default-BSD-X drivers, optional 131/111 entries and ten scoped sanitizer checks. [Report](verification/VERIFICATION-2026-10-06-FIFO-STACK-OUTPUT.md). Linear drain work and scheduler/output consumers pass; the existing intrusive module-input queue is retained. Remaining remeasurement belongs to PERF-01. See optimization roadmap. |
| OUTPUT-01 | P1 / locally qualified | Bound module output work per turn and continue fairly even when a reader keeps accepting data. The actual-core always-ready baseline drains 1,000 packets/2,048,000 bytes in one pass. The memory cap is not a service budget. The user approved 64-KiB/32-packet/128-attempt asynchronous limits; implementation, controlled/real-pipe checks and the combined FIFO/stack/output checkpoint pass; synchronous ordering remains intact. OUTPUT-02 separately qualifies the approved total deadline. See [proposal](decisions/MODULE-OUTPUT-SERVICE-PROPOSAL.md). Preserve partial packets, producer/synchronous ordering, byte accounting and death-row ownership; test fast/slow readers with input/timers, cancellation and disconnect. After QUEUE-01, coordinate existing INPUT-01/PERF-01 policy and LOOP-01 readiness; no parallel scheduler. See optimization roadmap. |
| OUTPUT-02 | P1 / locally qualified | Approved 2026-10-07: one total monotonic deadline through synchronous packet delivery and UNLOCK, retaining the 30-second ModuleTimeout default and positive override. Partial I/O, commands, readiness and EINTR cannot refresh it; nested operations keep separate deadlines while the parent elapses. Retirement/clock failure and final-command/death-row lifetime are checked. No preemption of a blocking command. Combined DATA-01 checkpoint passes 1024 entries/eight profiles, 120 staged identities, all default BSD X, optional 133/113 and twelve scoped sanitizers from an exact 1274-file snapshot. Native release qualification remains open. See [decision](decisions/MODULE-SYNC-DEADLINE-PROPOSAL.md) and [report](verification/VERIFICATION-2026-10-07-MODULE-SYNC-TREE.md). Startup OUTPUT-03 has its separate approval. |
| OUTPUT-03 | P1 / locally qualified | Approved 2026-10-07: capture ModuleTimeout when startup Timeout is omitted (30 seconds by default), retain positive overrides and live-module asynchronous continuation after timeout. One monotonic deadline, readiness up to 100 ms with X Escape handling, Expect and inline command ordering, checked partial input and retirement pin. Zero/negative explicit timeouts remain refused. The startup frame borrows the module through nested cleanup; a last read at the deadline retains even a complete frame for later asynchronous handoff. Baseline with ModuleTimeout=1 took 2.030 seconds; all eight focused Linux checks, both eight-entry native BSD ARM64 groups and fourteen scoped sanitizers pass. Final 1279-file STACK-01 checkpoint passes 1040 standard entries/eight profiles, 120 staged identities, 36 default BSD X drivers, optional 135/115 and fourteen scoped sanitizers; seven archives validate source hashes/modes and EOF. Native release qualification remains open. [Report](verification/VERIFICATION-2026-10-07-STARTUP-IDENTITY.md). See [decision](decisions/MODULE-STARTUP-DEADLINE-PROPOSAL.md). Separate from packet/unlock OUTPUT-02. |
| PERF-01 | P2 / accepted improvements retained; remaining performance-only tuning deferred to next release | Ordered/equal insertion is linear; adjacent size-only AutoDetect requests coalesce at most 32 queued records without crossing other events or removing synchronization. Isolated workload identities and final-log ownership improve measurements. The combined core milestone passes eight profiles, all six OS/architecture targets, four default BSD servers, installed identities, optional builds and scoped sanitizers. No timing ratio is an acceptance test. Remaining tuning, physical-latency profiling and timer-heap experiments are deferred; concrete availability regressions retain their safety owners and release physical checks remain required; one blocking handler cannot be preempted by a budget. [Combined report](verification/VERIFICATION-2026-10-11-CORE-PERFORMANCE.md). Periodic Linux paired observations and size/CPU/RSS tracking are established in the [2026-10-20 reference](verification/PERFORMANCE-2026-10-20.md); next regular comparison follows five merged implementation checkpoints. The earlier [PR132/138 comparison](verification/PERFORMANCE-2026-10-21.md) remains recorded. The current [PR144/149 observation](verification/PERFORMANCE-2026-10-22-PR149.md) establishes PR149 at 1/5 subsequent merged checkpoints after PR150. Observations remain lightweight/nonblocking for safety closeout; documentation-only changes do not increment the counter. |
| DRAW-01 | P1 / bounded expose servicing locally qualified; broader dirty-state work queued | Actual-core synchronous expose control invalidates 1000 selected redraws with zero handler calls. [Proposal](decisions/EXPOSE-SERVICE-PROPOSAL.md) corrects delivery and bounds each service/merge to 32 queued records; the user approved large Read/Raise/Refresh asynchronous redraw completion timing on 2026-10-07. Queue/FIFO/cache policy, actual-core damage/retirement and real icon/tear-off pixels pass. The 1291-file atom/expose/icon checkpoint is locally qualified: 1096 standard entries/eight profiles, 120 installed identities, 52 default BSD X invocations, optional 142/122 and 21 scoped sanitizers. [Report](verification/VERIFICATION-2026-10-07-ATOM-EXPOSE.md). The [window-update contract](decisions/WINDOW-UPDATE-BARRIERS-2026-10-15.md) records current owners, explicit/outer-loop barriers, retirement and query/publication constraints; its actual-core fixture passes all six targets/ten profiles, four default BSD X displays and scoped sanitizer checks. Two unchanged full-suite timing gaps are retained separately. [Report](verification/VERIFICATION-2026-10-15-WINDOW-UPDATES.md). Broader dirty-state optimization remains queued. Defer redundant decoration/geometry work using explicit dirty state; skip no-op updates. Define shared ownership, flush points and observable ordering barriers before STYLE-01/STATE-01 add deferred work. Preserve resource generations and final geometry. Compare latency, CPU and X request counts under drag/title/resize floods. See optimization roadmap. |
| STACK-02 | P1 / locally qualified | The baseline parent/child/grandchild fixture verified actual layers 6/2/4 and stable root stack order, while only the parent received ABOVE/BELOW EWMH updates; children retained stale states. Per-window EWMH publication and title-highlight identity are corrected; parent/child invocation, unrelated-window, actual stack/focus/decor and destroyed-parent survivor checks pass. Combined FIFO/stack/output qualification is locally accepted; STACK-01 identity work can follow. This is a desktop correctness finding, not a demonstrated security compromise. The baseline is separate from frozen INPUT-01 qualification; see optimization roadmap. |
| STACK-01 | P1 / identity, guards, query, arithmetic and traversal locally qualified; parent continuation, initialized lists and Layer storage locally qualified; unmanaged correction/bounded restacks/sibling selection locally qualified; group cleanup, callback publication and retirement continuation locally qualified | Indexed parent identity remains locally qualified. The user approved the originally invoked window for cyclic Layer and Raise/Lower/RestackTransients redirection on 2026-10-07. Both guards reuse operation-local constant-space policy; hint validation also finishes on a pre-existing parent cycle, retaining existing rejection rules. Eight focused Linux checks pass, including two/three-node/prefixed cycles, direct-target equivalence with transient inclusion, authoritative ring/server order, acyclic ancestry, hints and retirement. The policy also has exact-byte six-target coverage; first three stories pass six focused BSD ARM64/scoped-sanitizer checks per environment. [Layer decision](decisions/TRANSIENT-PARENT-CYCLE-PROPOSAL.md) and [other redirection decision](decisions/STACK-REDIRECTION-CYCLE-PROPOSAL.md). The four-story combined stacking/query checkpoint is locally qualified (1144 entries/eight profiles, 120 installed identities, 72 default BSD X invocations, optional 148/128 and 27 scoped sanitizers; exact 1301-file snapshot). The subsequent four-story arithmetic checkpoint is locally qualified from 1306 files: 1168 entries/eight profiles, 120 installed identities, 80 default BSD X invocations, optional 151/131 and 30 scoped sanitizers. Stable sorting, parent-only worklist closure and inactive cleanup are locally qualified in the subsequent 1314-file checkpoint (1192 entries/eight profiles, 120 installed identities, 84 default BSD X invocations, optional 154/134 and 33 scoped sanitizers); group ordering is preserved. The user approved invoked-window fallback for deep-walk storage failure on 2026-10-07. Production now uses operation-local parent preparation and leaf-first continuation; pure ownership and 13 real-frame controls pass focused Linux/sanitizer and both native BSD ARM64 checks. The 1319-file parent-continuation checkpoint is locally qualified (1208 entries/eight profiles, 120 installed identities, 88 default BSD X drivers, optional 156/136 and 35 scoped sanitizers). The 1323-file initialized-list checkpoint is locally qualified (1224 standard entries, 120 installed identities, 92 default BSD X invocations, optional 158/138 and 37 scoped sanitizers). The approved Layer storage contract is locally qualified in a separate 1328-file checkpoint (1240 accepted entries, 120 installed identities, 96 default BSD X invocations, optional 160/140 and 39 scoped sanitizer checks); original failures and unchanged affected reruns are retained. RaiseOverUnmanaged storage handling proceeds under configuration-only compatibility; further group/retirement work remains queued. See [identity report](verification/VERIFICATION-2026-10-07-STACK-IDENTITY.md). |
| STYLE-01 | P2 / mutation checkpoint qualified; broader work open | Three typed mutations share one qualified checkpoint ready for review. [Mutation report](verification/VERIFICATION-2026-10-20-STYLE-FLAG-MUTATIONS.md). The preceding flag-predicate checkpoint is merged in PR126. [Flag report](verification/VERIFICATION-2026-10-19-STYLE-FLAGS.md). The preceding matching checkpoint is merged in PR125. [Matching report](verification/VERIFICATION-2026-10-19-STYLE-MATCH-WORK.md). Three incremental style-index improvements pass a composed checkpoint: one original broad run across six targets/ten profiles (2019/2022 executed standard, 445/445 optional), followed by three unchanged isolated fixture controls. Fifteen actual-core index drivers retain zero selected lookup allocations after append, deletion and all three simplification paths. Three scoped sanitizers, nine fresh stages and four authenticated BSD displays pass. Two known required OpenBSD AMD64 CI/SVG omissions and its install/staged-geometry gap remain without retries. Existing update/grab barriers remain. [Report](verification/VERIFICATION-2026-10-19-STYLE-INDEX-MAINTENANCE.md). |
| STATE-01 | P2 / tray checkpoint qualified; broader burst/coalescing open | Three existing legacy-tray insertion/retirement/publication operations share a qualified checkpoint merged in PR124. [Tray report](verification/VERIFICATION-2026-10-19-EWMH-TRAY.md). Three earlier publication improvements share one qualified checkpoint: paired strut refresh, local permission reuse and converted-name length reuse. [Report](verification/VERIFICATION-2026-10-19-EWMH-PUBLICATION.md).  [Merged PR #112 atom batch](verification/VERIFICATION-2026-10-19-EWMH-ATOMS.md) adds duplicate-ID lower bounds, scoped/ALL name lookup and startup vector reuse. Broader burst/coalescing work remains open.  [PR #102](verification/VERIFICATION-2026-10-15-EWMH-STATE.md) implements inline client snapshots, paired work-area traversal and fixed scoped/ALL atom-ID indexes. Three related improvements share one checkpoint. Eight affected/retained entries pass ten profiles; core probes pass four default BSD X targets, and three scoped sanitizer entries pass. Original full runs pass 1990/1992 standard and 437/437 optional entries. Fresh 8 stages verify 120 code/77 catalog identities; 16 geometry invocations pass. Exact broader failures/install gaps remain in the report. Current publication/query points, original priority and configuration syntax remain unchanged. No persistent window cache or generic property-write coalescing is introduced; later dirty-state work requires separate measured invalidation/barrier evidence. |
| CACHE-01 | P2 / variant lookup checkpoint locally qualified; broader profiling open | A 65-variant lookup now checks one file stamp instead of 65. Eight standard compiler profiles across six targets pass 1936 entries; optional profiles add 423, default BSD servers pass 12 cache/staged-WM fixtures, and instrumented cache checks pass. 150 installed code identities and 99 catalogs validate. Broader profiling and release/hardware gates remain open. Preserve path resolution, keys, generations and IMAGE-01 leases. [Report](verification/VERIFICATION-2026-10-14-PICTURE-CACHE.md). |
| LOOP-01 | P2 / ordinary, interactive and synchronous core readiness locally qualified; wider audits remain | Unlock, blocked-output and startup waits use native poll with shared absolute-deadline conversion. Retained full coverage passes 1944 standard and 425 optional entries; the final timeout refinement passes 50 affected entries across the same profiles. 150 final installed code identities and 99 catalogs match. Default BSD X passes 20+8 fixtures; three scoped sanitizer entries pass. Broader admission/runtime/release audits remain open. Preserve ordinary-only session callbacks and INPUT-01/OUTPUT-01 ordering. [Latest report](verification/VERIFICATION-2026-10-15-MODULE-WAITS.md). Stream construction/continuation and diagnostic adoption/failure contracts are locally qualified in PR141; remaining broad audit and descriptor-provenance review stay open. [Stream report](verification/VERIFICATION-2026-10-21-STREAM-OWNERSHIP.md). |
| XIO-01 | P2 / atom batching locally qualified; query work queued | Independent atom tables now use three batches for 100 core/ICCCM/EWMH names, with finite missing-entry fallback. Exact mappings, selection ownership and supported-property order pass actual Linux and native FreeBSD/OpenBSD ARM64 controls; Scoped pure leak-enabled and actual-core leak-disabled sanitizer checks pass. [Report](verification/VERIFICATION-2026-10-07-ATOM-BATCH.md). Combined atom/expose/icon qualification passes eight profiles/1096 entries, 120 installed identities, 52 default BSD X invocations, optional 142/122 and 21 scoped sanitizers. [Combined report](verification/VERIFICATION-2026-10-07-ATOM-EXPOSE.md). Highest-override query selection now scans the root snapshot from top to bottom, skips already-excluded internal windows and stops at the highest eligible sibling. The controlled 108-child fixture uses 3 attribute reads instead of 108; full current attributes remain queried for managed frames. Real override/InputOnly order, focus and popup teardown pass focused Linux/BSD ARM64/scoped sanitizer checks; combined qualification passes eight profiles/1144 entries, 120 installed identities, 72 default BSD X invocations, optional 148/128 and 27 scoped sanitizers from the exact 1301-file snapshot. Other query/barrier work remains queued. Batch independent atom tables and remove individually proven unnecessary XSync barriers. Profile root-tree plus per-child attribute queries in stacking; use STACK-01 identities to skip only known/current managed frames. Preserve unmanaged/InputOnly/hidden/destroyed-window behavior and error/order boundaries. A narrow bounded XCB experiment requires measured remaining query cost, LIB-07 and six-target review; retain Xlib event ownership. Whole-core migration and new mandatory packages are not approved. See optimization roadmap. |
| BOX-01 | P2 / queued | Sandbox narrow helpers/modules first with OpenBSD pledge/unveil and FreeBSD Capsicum where appropriate; evaluate Linux helper restrictions. Preserve reload/spawn/font/theme access. Whole-core sandbox policy requires an explicit compatibility review. |
| LIB-05 | P2 / Xlib requirement locally qualified; remaining floors queued | Unused direct libxkbcommon requirement removed and actual Xlib XKB header/symbol checked in PR144; six-target build/install/X checkpoint passes. Xt/Xtrans header consumers remain. Actual per-target dependency/interpreter floors and broader linkage review stay open; retain Xlib/libevent/Xft unless evidence justifies an approved change. [Report](verification/VERIFICATION-2026-10-21-XLIB-DEPENDENCY.md). |
| LINK-01 | P2 / Later; child #184 queued | Declare target-owned dependencies using symbol/link-map and static/transitive evidence; qualify default/reduced/optional six-target installs. Reuse PR144 xkbcommon removal. See [dependency decision](decisions/DEPENDENCY-SIMPLIFICATION-2026-10-23.md). |
| LIB-04 | P2 / Later; child #183 queued | Consolidate obsolete SVG backends against a maintained librsvg/Cairo floor; preserve SVG-disabled builds and rendering/worker contracts. LunaSVG comparison remains optional and separate; no adoption approved. See [dependency decision](decisions/DEPENDENCY-SIMPLIFICATION-2026-10-23.md). |
| LIB-06 | P2 / Later evaluation | Compare cJSON and yyjson on actual bounded serialization, numeric/UTF-8/failure behavior, memory, latency, maintenance and pinned/transitive licenses. Record adopt/defer/reject; no automatic replacement. See [dependency decision](decisions/DEPENDENCY-SIMPLIFICATION-2026-10-23.md). |
| COMPAT-02 | P2 / full Perl retirement approved; Later #180 and #179/#50 handoffs; implementation queued | User explicitly requests Perl out of the future runtime install, superseding optional default-preserving packaging. Inventory and retire FvwmPerl/perllib/Perl-only tooling with precise compatibility/migration notices, updated install/manifests/references and qualified Go utility/Lua selected-use-case handoffs. No transparent translation of arbitrary Perl or deletion of user configs; preserve unrelated FVWM/Script configuration and update native producers/consumers together. Stage 5 retirement gate; see [runtime scope](decisions/RUNTIME-GO-LUA-2026-10-06.md). |
| UTIL-01 | P2 / utility-family port milestone locally qualified | [Xlock checkpoint](verification/VERIFICATION-2026-10-12-XLOCK-PORT.md), [directory checkpoint](verification/VERIFICATION-2026-10-12-DIRECTORY-PORT.md) and [converter checkpoint](verification/VERIFICATION-2026-10-12-CONVERTER-PORT.md) install Go when enabled and retain disabled-Go Perl. The converter preserves ordinary heuristic bytes with finite input/reference work and atomic private outputs. [Consumer classification](verification/RECONCILIATION-2026-10-12-UTILITY-CONSUMERS.md) separates the installed Script FIFO example, development cursor generator, optional status recipes and full XDG provider/frontend. The [Script example checkpoint](verification/VERIFICATION-2026-10-12-SCRIPT-COM-PORT.md) qualifies stdlib Go/paired selection, bounded owned FIFOs, staged and actual native widget checks. Copied explicit Perl invocations remain usable. The [full XDG provider/frontend fixture checkpoint](verification/VERIFICATION-2026-10-12-XDG-FIXTURES.md) now passes across six targets; its [inactive input-policy foundation](verification/VERIFICATION-2026-10-12-XDG-POLICY.md) is qualified at `d33f`, with [discovery/preference policy](verification/VERIFICATION-2026-10-12-XDG-DISCOVERY.md) qualified at `9337` and [ordered allocation](verification/VERIFICATION-2026-10-12-XDG-ALLOCATION.md) at `be8e`; the [full coordinated Go provider/native Form port](verification/VERIFICATION-2026-10-12-XDG-PORT.md) is now locally qualified and adopted at `04c1`. COMPAT-02 retirement remains a separate stage5 gate. This supersedes earlier Python port recommendations. Preserve applicable configured behavior. See [runtime scope](decisions/RUNTIME-GO-LUA-2026-10-06.md). |
| RUNTIME-GO-01 | P2 / M2 partially implemented; focused component checks pass, full handoff pending | Mandatory-Go utility/install baseline is accepted in M1; M2.1–M2.5 desktop workers, direct settings/checker, Go resident and native capture are merged. M2.6 completes existing action ownership, removes the transitional policy RPC and shares procfs-independent self-worker paths. Linux Python-free installed execution and paired session observations pass with explicit root/measurement limits. Full affected six-target/default/reduced/install/session acceptance and true wakeup/startup-peak/native-CPU gaps remain before M3; broad #23 stays open. [Current report](verification/VERIFICATION-2026-10-24-M2-SESSION-RUNTIME.md), [utility baseline](verification/VERIFICATION-2026-10-23-M1-RUNTIME-BASELINE.md), [scope](decisions/RUNTIME-GO-LUA-2026-10-06.md). |
| LUA-01 | P2 / Later; engine selection open | Deliver isolated Lua policies and Perl migration examples over shared typed primitives. Compare preferred Shopify/go-lua with official native Lua; decide version/coroutines, exact IDs, reload/shutdown and resource limits before adoption. No VM/blocking work in the C core; preserve FvwmScript. See [dependency decision](decisions/DEPENDENCY-SIMPLIFICATION-2026-10-23.md). |
| TOOLING-GO-01 | P2 / Later; child #182 queued | Remove all owned Python/Perl build/docs/metadata/artwork/test/maintenance tools in bounded parity groups after runtime qualification. Keep coverage and provenance. Compare Muon/CMake before separate adoption; distinguish installed runtime, owned tooling and whole-build/test interpreter closure. See [dependency decision](decisions/DEPENDENCY-SIMPLIFICATION-2026-10-23.md). |
| GEN-01 | P2 / modern runtime and developer wrapper locally adopted | Four related stories preserve grammar/token rules and normal generator-free builds. Frozen2514 passes eight clean compiler profiles/1816 standard entries,120 staged code identities,48 affected installed-module fixtures on four default BSD servers, optional232/212 suites and15 scoped sanitizer checks. All24 affected developer-tool checks pass; ten wider inventory errors from absent Git are retained separately. The original candidate Clang colorset timeout remains unexplained; unchanged control/retry and fresh full production suite pass at original deadlines. No configuration migration. [Report](verification/VERIFICATION-2026-10-11-PARSER-ADOPTION.md). |
| PORT-01 | P2 / queued | Prove active orphaned code and obsolete portability branches through compiled/conditional configurations; classify legacy public tools/examples before optional packaging or archival. Preserve BSD/theme behavior. See cleanup roadmap. |
| I18N-01 | P2 / maintenance locally adopted; broader UI coverage remains | Shared LINGUAS discovery, complete candidate publication, four approved Language-header corrections and honest NLS dependency/catalog selection pass the whole-source2663 checkpoint:1743 entries/six standard plus two feature profiles,120 staged code identities,77 catalogs,30 native NLS fixtures,30 retained translation units and138-key extraction parity per target. Twelve final source/install/metadata archives validate; deterministic clean-fixture export and reference checks pass. Ten unused GMO binaries and the old shell helper are retired with exact backup. New UI coverage/font/input and language polish retain their owners; native release CI/public release remain separate. [Adoption report](verification/VERIFICATION-2026-10-11-I18N-ADOPTION.md), [original maintenance](verification/VERIFICATION-2026-10-08-I18N-MAINTENANCE.md), [AMD64 focused evidence](verification/VERIFICATION-2026-10-11-I18N-AMD64.md). |
| I18N-02 | P2 / late polish scope requested; implementation queued | After the existing program, complete reviewed UI, man/derived HTML and shipped config-comment coverage for ar/da/de/es/fr/ka/ru/sv_SE/uk/zh_CN/zh_TW. Preserve command/config syntax and user files; qualify RTL/font/input, layout, locale lookup/fallback and six-target/offline installs. Evaluate Git-owned PO, Weblate/AI drafts with language review and AsciiDoc extraction; dependencies/services remain proposed. Coordinate I18N-01/DOCS-01/TEXT-01/TOOLING-GO-01; no new existing release prerequisite. See [scope](decisions/LOCALIZATION-POLISH-2026-10-08.md). |
| REPRO-01 | P2 / queued | Pin refreshable development/container/Nix and generator environments while retaining platform-native package and offline builds. See cleanup roadmap. |
| DOCS-01 | P2 / man quality queued; website design deferred | Prioritize accurate OS-installed man pages, command/module coverage, examples, references, roff rendering/lint and staged package inclusion. Keep one reviewed AsciiDoc source and lightweight optional static HTML; no JavaScript/site framework or mandatory HTML install. Preserve runtime/configuration contracts; ASD-STE100 claims require explicit rule/dictionary review. Developer-doc migration is complete and does not claim a manual rewrite or fresh man render. Website visual design and public-helper compatibility adoption remain separate. See [policy](DOCUMENTATION.md) and [brief](decisions/WEBSITE-MANUALS-BRIEF.md). |
| ARCHIVE-01 | P2 / after verified migration | Retired planning/upstream notes and dated reports are now relocated with hashes and repaired links; graph auto-update is disabled. Remaining: remove predecessor tooling only after reference, parity, packaging and distribution checks. Preserve history/evidence and user data; do not delete ignored build trees or VM disks. See cleanup roadmap. |
| BUTTON-01 | P2 / merged PR149; locally qualified | Native FvwmButtons keyboard focus, navigation and disabled state. Add module-local Tab/arrow navigation, visible focus, Enter/Space activation and disabled-action refusal. Preserve existing bindings and activation behavior through an opt-in compatibility path. See the configuration/module backlog below. |
| BUTTON-02 | P2 / merged PR150; locally qualified atomic groups | Complete bounded dynamic button updates, including enabled/selected/focus and hover/pressed colorset assignments. Validate a whole per-module update before applying it and repaint once; reject invalid updates without partial mutation. See the configuration/module backlog below. |
| CFG-01 | P2 / consumers merged; preview/recovery queued with IPC-02 | Typed desktop settings registry and a configuration-center consumer using existing modules/runtime. Define defaults, ranges, choices, descriptions, capabilities and reset behavior; keep persisted preferences data-only, private and atomic. See the configuration/module backlog below. |
| CFG-02 | P2 / first subset in review; general runtime diagnostics open | Source-aware configuration diagnostics and non-executing validation of supported declarative syntax. Report file/line/include context and explicitly identify unchecked dynamic constructs. Never implement validation by executing Read/PipeRead/Exec. See the configuration/module backlog below. |
| IPC-02 | P2 / PR154–161 merged; appearance inspection/copy repair focused checks passed, integration pending | Additive structured state queries and correlated operation results over existing module/MFL transport. Bound requests/replies, retain command ordering, distinguish accepted/applied/failed operations and preserve legacy text commands/events. See the configuration/module backlog below. |
| SESSION-01 | P2 / queued, default product scope approved | Portable default session lifecycle integrated with existing FVWM UI/configuration: environment, controlled XDG autostart, owned/borrowed services, bounded startup/readiness/recovery, restart vs cooperative logout, OS capability/power/locker coordination and commented installed configuration/man pages. No D-Bus requirement, new authentication implementation or mandatory desktop packages. Preserve direct FVWM/custom/XSMP compatibility; reuse CFG/IPC and process/readiness owners. Follow stage 6 BUTTON/CFG/IPC primitives; file-manager additions are not session startup prerequisites. See [session scope](decisions/SESSION-INTEGRATION-2026-10-06.md). |
| SESSION-02 | P2 / queued, optional contribution scope approved | Comprehensive D-Bus extension of SESSION-01 using the same settings/actions/UI and one documented opt-in: reuse or explicitly create an owned session bus before child startup, honest no-bus fallback, available login1/elogind/inhibitor adapters, native notification/tray integration and configurations for already installed selected agents/portal backends. Select the shared optional dependency contract before NOTIFY/TRAY protocol adoption; qualify enabled/disabled installs, ownership, restart/logout and OS-specific capabilities. No system-bus startup, separate settings store or implicit package installation. See [session scope](decisions/SESSION-INTEGRATION-2026-10-06.md). |
| NOTIFY-01 | P2 / queued, roadmap scope approved | Native process-isolated notifications with the freedesktop interface, bounded popups/history/actions, do-not-disturb and live palette/settings updates. Reuse shared primitives/CFG-01/IPC-02; define optional D-Bus feature/dependency policy before adoption, preserve the minimal baseline and respect existing servers. Native/real-X six-target and manual/install gates. See [service scope](decisions/NATIVE-DESKTOP-SERVICES-2026-10-06.md). |
| TRAY-01 | P2 / queued, roadmap scope approved | Native tray host: XEmbed checkpoint followed by StatusNotifier/application-menu checkpoint; dynamic icon/layout/visibility settings and adaptive bar integration. Bound foreign-client data/lifetimes and respect existing owners. Follow NOTIFY-01 and shared desktop primitives; modern D-Bus support retains its dependency gate. Native/real-X six-target and manual/install gates. See [service scope](decisions/NATIVE-DESKTOP-SERVICES-2026-10-06.md). |
| FILE-VIEW-01 | P2 / queued, scope approved | Reusable structured collection view: scrollable list first, shared icon-grid model next, stable IDs, multiple selection, keyboard focus/actions, visible-entry rendering and bounded incremental updates. Stage 6 after shared button/text/image/scale contracts; qualify with FILE-DATA-01/FILE-ACTION-01 as the first browser handoff. Native policy/real-X large-collection, refresh/focus/lifetime/scale gates. See [file-manager scope](decisions/FILE-MANAGER-PRIMITIVES-2026-10-06.md). |
| FILE-DATA-01 | P2 / queued, scope approved | Isolated cancellable local directory enumeration/metadata, generations, Home/Up/Back/path entry, sorting/hidden files, manual refresh and honest unavailable/stale results. Preserve filename bytes separately from display text; bounded native watching/fallback follows platform review. Stage 6 with FILE-VIEW-01; slow filesystem work must not stall WM input. See file-manager scope. |
| FILE-ACTION-01 | P2 / queued, scope approved | Typed selected-item/path/URI action data and explicit correlated results using IPC-02/PROC-01/XDG-01; no label-derived FVWM/shell evaluation. Define request/view generations, cancellation and stale-target refusal; navigation/open/terminal/context actions form the first browser checkpoint. Stage 6 with FILE-VIEW-01/FILE-DATA-01; documented working consumer example is the config/theme handoff. See file-manager scope. |
| FILE-JOBS-01 | P2 / queued, scope approved | Second file-manager checkpoint: owned create-folder/rename/copy/move/link/trash/restore and explicit permanent-delete jobs, progress/cancel/conflict decisions and per-item/final outcomes. Define cross-filesystem, symlink, partial failure/cleanup and metadata guarantees; no silent trash-to-delete fallback or implied undo. Stage 6 after browser/action and process/ownership contracts; native filesystem plus real-X responsiveness/teardown and six-target gates. See file-manager scope. |
| DESKTOP-ICONS-01 | P2 / queued, scope approved | Third checkpoint: optional persistent pinboard using the qualified collection/actions, keyboard access, movable references and monitor/work-area/scale-aware placement. Removing a pin preserves its target; actual Desktop-directory mutation waits for FILE-JOBS-01. Preserve root menus, wallpaper ownership, stacking/workspaces and other desktop managers. Stage 6 after browser primitives; native real-X restart/hotplug/mixed-scale/input gates. See file-manager scope. |
| FILE-INTEGRATION-01 | P2 / queued, scope approved | Fourth checkpoint: portable installed folder/reveal/properties entry point and XDG directory/application roles, then optional FileManager1 ShowFolders/ShowItems/ShowItemProperties adapter under SESSION-02. Design request contracts with the first browser; do not overwrite defaults or execute reveal targets. Reuse SELECTION-01 for file clipboard/XDND. Portable portion in stage 6; optional adapter in stage 7 after the common bus contract. Independent-client, competing-owner/no-bus and installed-platform gates; a new Open/Save portal backend is separate scope. See file-manager scope. |
| FILE-ADVANCED-01 | P2 / deferred candidates | Thumbnails/previews, tabs/dual panes, search, bulk rename, archives, devices, remote locations and undo require individually bounded adoption decisions and prerequisite gates. Reuse qualified collection/jobs/image/process contracts and optional providers; no autorun, mandatory desktop stack or aggregate rewrite. Not a prerequisite for the initial primitive/config handoff or earlier file-manager checkpoints. See file-manager scope. |
| CONTENT-01 | P2 / queued, primitives-first scope approved | Reusable bounded content items with stable IDs/generations, original bytes and multiple text/image/URI representations separate from labels/previews. Explicit leases, sensitivity/lifetime propagation and stale-reference refusal; deduplication/pinning preserve budgets and policy. Stage 6 with SELECTION-01 and shared collection/action contracts; no filesystem-path impersonation or arbitrary database framework. See [clipboard scope](decisions/CLIPBOARD-PRIMITIVES-2026-10-06.md). |
| RETENTION-01 | P2 / queued, scope approved; defaults pending | CFG-01-controlled capture/pause, formats, source exclusions, expiry/pinning/clear/lock and separate current-selection/history/persistence states. Honor supported secret hints before retention/previews/actions; remove derivatives and pending work on expiry without clearing newer clipboard data. Password-history and persistence defaults remain open decisions. Stage 6 with CONTENT-01/STORE-01; native policy and real-X lifecycle/failure gates. See clipboard scope. |
| STORE-01 | P2 / queued, scope approved; backend/protection policy pending | Protected content memory first: minimize copies, checked locking/wiping/dump restrictions and explicit refusal when required protection is unavailable. Then optional authenticated encrypted persistence, private safe files, protected previews/indexes, key/unlock/recovery/migration contracts and honest deletion limits. Bound encoded/decoded image resources and audit transport/UI/decoder copies. Stage 6; no custom crypto, mandatory keyring or plaintext fallback. Native six-target protection/failure/storage and security-review gates. See clipboard scope. |
| CLIPBOARD-01 | P2 / queued, consumer/handoff scope approved | Text/image clipboard consumer over CONTENT/RETENTION/STORE, SELECTION-01, FILE-VIEW-01 and typed IPC/actions. First qualify an in-memory example plus installed syntax/contracts/capabilities/manuals; a separate user-directed config/theme thread then builds the full UI while runtime work continues. Encrypted persistence follows as a separate checkpoint; optional secret history requires a policy decision. Independent-client/real-X, image privacy, ownership, six-target/install and native sanitizer gates; no claim of confidentiality against hostile X11 clients. See clipboard scope. |
| DESKTOP-EXPLORE-01 | Deferred exploration / after existing stages 1–8 | Explore additional desktop consumers, display/input controls, network/Bluetooth, full audio/media, devices/power, search/advanced files, choosers/portals, credentials, printing/calendar/capture and richer native layouts after current work. Reuse FILE-ADVANCED/SESSION and other existing owners; no feature implementation, dependency/default selection or release blocker implied. Cross-functional contracts are scoped now under existing CFG/IPC/SESSION/FILE owners; see [shared scope and candidate inventory](decisions/DESKTOP-EXPLORATION-2026-10-06.md). |
| STANDARDS-01 | P1 / fixed protocol inventory complete; wider audit open | A first-icon WM_STATE zero-XID defect is reproduced and corrected in the atom/expose batch; real first/reused title XID checks and combined 1291-file qualification pass. Maintain requirement-level specification/revision/owner/test/profile/evidence coverage using [STANDARDS.md](STANDARDS.md). The [forty-row protocol register](verification/STANDARDS-2026-10-21-PROTOCOL-REQUIREMENTS.md) completes root/window/selection requirement inventory and owner handoffs, retaining historical evidence boundaries; other standards areas remain queued. Continue bounded inventories in stage 2; map independent fixtures and CI with VERIFY/COMPAT, reuse passing scoped evidence and assign real gaps to their owners. Required adopted clauses and accurate advertised capabilities are release gates; no blanket compliance badge or endless audit restart. See acceptance below. |
| EWMH-01 | P1 / bounded desktop publication qualified; broader geometry/roles open | Reconcile ICCCM 2.0/EWMH 1.5 implementation, advertised atoms and independent-client evidence. Classify required vs optional hints, then implement bounded agreed gaps such as partial struts/multi-monitor fullscreen and protocol responsiveness with MONITOR/INPUT/DRAW/DATA owners. No speculative defect claim or parallel geometry authority. The [stage3 audit](verification/RECONCILIATION-2026-10-12-STAGE3.md) independently checks63 advertised atoms, identity/client order/extents and records the reproduced initial empty-work-area gap. The [fixed requirement register](verification/STANDARDS-2026-10-21-PROTOCOL-REQUIREMENTS.md), R01–R12/W01–W12, now separates exact historical checks from unqualified semantics and defines desktop/viewport/work-area acceptance. The bounded [desktop-publication checkpoint](verification/VERIFICATION-2026-10-21-DESKTOP-PUBLICATION.md) qualifies pager admission, count adoption/refusal and matching dependent arrays. Reserved-edge/dock geometry, multi-monitor root authority and broader clauses remain open; stage5 follows interaction/publication contracts. |
| SELECTION-01 | P1 manager handoff locally qualified; broader roles/transfer open | Qualify adopted ICCCM clipboard/selection roles and define participating native XDND targets. Reuse bounded DATA-01 transport and INPUT/PROC ownership; test chunks, timestamps, cancellation, owner death, malformed/flood input and independent clients. CLIPBOARD-01 separately scopes a configurable manager consumer, asynchronous text/image roles and CLIPBOARD_MANAGER/SAVE_TARGETS adoption; this audit alone implies no daemon, generic WM drag interception or dropped-file autorun. The [stage3 role audit](verification/RECONCILIATION-2026-10-12-STAGE3.md) separates core WM_Sn, Script PRIMARY/private messages, Form cut buffers and future desktop roles. The [fixed requirement register](verification/STANDARDS-2026-10-21-PROTOCOL-REQUIREMENTS.md), S01–S16, records owner/target/requestor checks and the source-found missing MANAGER owner-window field. Required Script owner targets, INCR and broader manager ownership/target coverage remain open; the startup handoff subset is locally qualified in [PR145](verification/VERIFICATION-2026-10-21-MANAGER-HANDOFF.md). Remaining bounded role/transfer adoption follows stage5; later desktop services retain their own stages. |
| XDG-01 | P2 / queued interoperability | Exact adopted Desktop Entry Exec/field-code parsing, lookup/visibility/localization/actions, startup notification, base-directory rules, MIME application roles and inherited icon lookup. Reuse launcher/UTIL/PROC/parser contracts and existing providers; preserve legacy FVWM paths. Native independent fixtures, safe literal arguments, absent providers and installed-platform acceptance. Stage 5 after underlying parser/process work; autostart remains SESSION-01-owned. |
| TEXT-01 | P2 / queued native text/input | Define Unicode data/version, grapheme editing, bidi/shaping/font fallback and XIM/IME composition for FVWM-owned controls. Audit existing locale/font helpers first; preserve legacy encodings and client pixels. Coordinate SCRIPT-01/I18N-01, review any dependency contract explicitly, and test real composed input plus pure boundaries. Stage 5 before dependent desktop primitives. |
| SETTINGS-01 | P2 / queued shared preferences | Publish supported CFG-01 preferences through native XSETTINGS/X resources and, when enabled, the selected settings portal path. Single ownership, typed bounded updates, serials, restart and actual toolkit/browser observations; do not claim every toolkit obeys every key. Native bridge in stage 6 after CFG/session contracts; optional bridge in stage 7 with SESSION-02. GTK/Qt theme creation remains deferred. |
| A11Y-01 | P2 / queued native accessibility | Apply [native accessibility guidance](THEME-STANDARDS.md): complete keyboard paths and visible focus with BUTTON-01/02; define accessible names/roles/states/actions/events for selected FVWM UI and optional AT-SPI profiles. Real keyboard and assistive-client evidence, focus return, scaling and unavailable-service tests. Follow TEXT/button/CFG contracts in stage 6; qualify optional bus export in stage 7. No new mandatory GUI toolkit. |
| THEME-01 | P2 / SVG artwork adopted; focused native Linux review passed | Apply [THEME-STANDARDS.md](THEME-STANDARDS.md) to all shipped palettes/modes/states, bootstrap/assets, density/targets, opacity, enlarged text and keyboard paths. Keep Slate layout/local overrides; record numeric contrast separately from native visual/interaction evidence. Earlier focused text/disabled-role review is preserved in the [standards report](verification/VERIFICATION-2026-10-06-STANDARDS-THEME.md). The user-approved richer SVG artwork replaces 377 geometric PNG assets, with PNG retained for screenshots and existing raster inputs. Native Linux ARM64 passes 51 policy checks, the full desktop/two-monitor gate, 377 worker render/install comparisons, 12 scale renders and 48 meaningful-glyph contrast pairs; SVG/PNG-disabled Session fallback also passes. See the [SVG theme report](verification/VERIFICATION-2026-10-06-SLATE-SVG.md) and [decision](decisions/THEME-SVG-ARTWORK-2026-10-06.md). Six-target/enlarged-text/physical-display/assistive/native-CI qualification remains pending; asynchronous core image activation stays IMAGE-01-owned. Full theme acceptance before RELEASE-01. |
| RELEASE-01 | P1 / final release gate, queued until coding is complete and user requests release preparation | Final public-launch review of the accepted release candidate: independent identity/attribution, real README/demo, public contributor/community/security files, owned reporting/support channels, tested install/trial/upgrade, source artifacts/checksums/provenance/licenses, excellent installed manuals with optional lightweight HTML, onboarding and quality gates. Reuse CI-01/REPRO-01/DOCS-01/MAINT-02/VERIFY-01/COMPAT-01 and related evidence. Record disposition of X11 walkthroughs, measured results, opt-in diagnostics, customization recipes and public roadmap. Any new code/defect returns to its owner before repeating release checks. No publication authorized. See [full gate](RELEASE-GATE.md). |

P0 is for reproduced urgent failures; P1 covers security/correctness, release
verification and the most consequential responsiveness work. P2 covers broader
cleanup, optional redesigns and new primitives. Split audit/design labels are
intentional: an exposure review need not authorize a wholesale migration.

## Standards acceptance and build handoff

The [standards policy](STANDARDS.md) defines the common evidence contract; the
[theme policy](THEME-STANDARDS.md) supplies measurable native defaults adapted
from W3C guidance. These are first-release requirements, not current compliance
claims. Use the sequence above, reuse existing owners and fix a reproduced
security/correctness defect by its actual risk. Stage numbers are dependencies,
not permission to run an unrelated broad rewrite.

- **STANDARDS-01:** freeze requirement IDs/versions at each owner batch entry.
  Record every adopted clause as verified for a named profile, unqualified,
  planned, delegated with integration evidence, or not applicable with rationale.
  Link independent tests, current-source fingerprints and actual results. Audit
  advertised capability replies as well as implementation. Close adopted required
  gaps and document optional/delegated limits before the release claims review.
- **EWMH-01:** use independent X clients to test types/counts, timestamps,
  focus/user-time policy, transients, frame extents and advertised supported atoms.
  Inventory partial struts, fullscreen monitor selection, ping and resize-sync
  against the specification before selecting implementations. Test dock removal,
  topology changes, malformed requests, client death and continued input. Preserve
  actual-pixel contracts, command order and async error/barrier policy; share
  MONITOR/INPUT/DRAW fixtures and the relevant six-target qualification.
- **SELECTION-01:** define each module's source/target/selection-owner role.
  For adopted clipboard features check TARGETS, TIMESTAMP, MULTIPLE, INCR,
  UTF-8/legacy encodings, bounded chunks and owner loss/late replies with foreign
  clients. For agreed XDND targets check negotiated version/types/actions,
  status/finish, cancellation and safe literal filenames/URIs; no implicit dropped
  content execution. Retain compatibility and DATA-01's validated transport.
- **XDG-01:** test normative Exec quoting/escaping and field-code placement using
  fixtures authored from the spec, including empty/spaced/untrusted arguments.
  Check search precedence, absolute paths, desktop filters, localized names,
  terminal/application actions and missing capabilities. Startup identifiers
  expire and correlate without focus stealing. MIME/icon resolution uses existing
  installed providers where appropriate; verify override/fallback/scale behavior.
  Share SESSION-01 autostart and CFG-01 app roles rather than implementing twice.
- **TEXT-01:** document UTF-8 validation and chosen Unicode/library versions.
  Test combining characters, emoji sequences, non-Latin/bidi text, preedit/commit,
  caret/selection/deletion, invalid/long input and fallback fonts. Preserve bounded
  operations and existing legacy encoding behavior. Pure boundary tests alone do
  not qualify real IME/font-layout behavior on supported Linux/BSD profiles.
- **SETTINGS-01:** define which keys/preferences and precedence are supported.
  Test existing-owner refusal, per-screen selection, serial/atomic publication,
  hostile record lengths, restart and unavailable providers. Observe actual
  supported toolkit/browser changes and optional portal replies; no unsupported
  dark/motion/DPI claim, forced client-pixel scale or second preference store.
  Optional D-Bus/library selection stays with SESSION-02's dependency contract.
- **A11Y-01:** enumerate included FVWM UI and supported assistive profiles.
  Test all desktop actions from the keyboard, logical focus/order, visible state,
  cancel/return and disabled refusal. Verify actual names/roles/states/events,
  actions and geometry through an independent accessible client/screen reader
  where supported. A color/theme pass is not accessibility-tree evidence; missing
  optional assistive providers cannot break portable login.
- **THEME-01:** freeze the current configuration/assets for a measured review.
  Check all four palettes, light/dark, pressed/disabled/accent roles and bootstrap
  numerically; then actual transparency, icon rendering, target geometry, compact
  mode, enlarged/long text, supported scales and keyboard-only flows. Record
  screenshots from real native UI and remaining runtime blockers separately.
  Keep palette/assets in parity; generator or module fixes return to their owners.

DOCS-01 includes actual semantic HTML/CSS validation and applicable WCAG 2.2 AA
review for public exports, plus excellent primary man output. It does not turn
FVWM into a web renderer. RELEASE-01 assembles the frozen requirement/profile
register and theme/HTML evidence after coding qualification; it does not restart
all audits or authorize publication. Native sanitizer, physical display/toolkit
and optional-service limitations remain explicit in SUPPORT.md.

## Completed checkpoints

These are preserved evidence, not entries ahead of the active queue.

| ID | Status | Scope |
| --- | --- | --- |
| IPC-01 | P0 / complete | MFL newline framing and limits. Preserve split lines; execute only complete commands; reject oversized/NUL input per client. Incremental nonblocking pipe transport, bounded input/output, connection cap, callback budgets and slow-reader timeout. Verify fragmented/coalesced traffic, boundary lengths, healthy-client isolation and backpressure with native unit and real-X tests. |
| GEO-01 | P0 / complete | Fix latest-value configure merging and WM_NORMAL_HINTS barrier; fix valid coordinates being clamped to -32768. Native boundary/ordering tests and real-X bursts with explicit UseGravity/IgnoreGravity policies. |
| MAINT-01 | Candidates implemented; adoption tracked separately | Standalone Python stdlib build/docs/metadata/parser/gettext/inventory/archive tools and isolated checks. The original candidate checkpoint is preserved; build/manual adoption now belongs to MAINT-02 and its tooling report. See [tools guide](../tools/maintenance/README.md). |

## Configuration and module primitive backlog

Recorded at the user's request on 2026-10-05 for the build thread to pick up.
These are queued requirements, not implemented features or a new API contract.
Runtime implementation and further theme work remain with the build thread.
The subsequent cleanup request authorizes additive developer tooling here;
it does not authorize implementing the runtime backlog in this thread. Build-thread work should retain the
existing security priorities; these P2 additions do not displace open hardening.
Suggested sequence: BUTTON-01, BUTTON-02 and CFG-01, then CFG-02 and IPC-02.
Define concrete syntax and compatibility policy before implementation; follow
the existing requirement for a user decision only if a compatibility break is
needed.

### BUTTON-01: keyboard focus and disabled controls

- Keep a button focus target inside the module. Tab/Shift+Tab traverse eligible
  controls; arrows navigate a defined grid order; Enter/Space activate the
  focused button. Show a visible focus indicator and define initial focus.
- Add an explicit disabled state that refuses activation through pointer,
  keyboard and PressButton, while allowing a label/reason to remain visible.
  Define focus behavior when a focused control is disabled or removed.
- Avoid WM grabs for ordinary typing in other applications. Preserve legacy
  Return-under-pointer behavior and existing configurations unless users opt
  into the new navigation behavior. Module UI state is not authorization:
  action handlers must still validate requests and recheck OS capabilities.
- Acceptance: native policy plus real-X traversal, reverse traversal, activation,
  disabled controls, focus transitions and rapid typing in unrelated clients.

### BUTTON-02: complete dynamic updates

- Extend supported updates to enabled/selected states and normal, focus, hover
  and pressed presentation, including ActiveColorset/PressColorset assignments.
  Retain existing title/icon updates and stable button identifiers.
- Support a bounded group of property updates in one module: parse and validate
  the entire group, apply it before repaint, or leave the previous state intact
  on refusal. This is not a whole-WM transaction or arbitrary layout engine.
- Define limits for update count/size and validate IDs, colorsets and resource
  references. Keep legacy ChangeButton semantics compatible.
- Acceptance: invalid/mixed batches cannot partially change buttons; actual-X
  updates preserve focus and action identity and do not require module restart.

### CFG-01: typed settings and configuration center

- Build on the current desktop helper's validated, atomic JSON preferences.
  Register setting IDs, types, defaults, ranges/enumerated choices, descriptions,
  groups, owning component and capability/availability information once.
- Expose query, validated change and scoped reset operations. An existing-module
  configuration-center consumer can present registered settings consistently;
  appearance, bar, scheduling, focus and application preferences are examples,
  not authorization to redesign the theme or implement all settings at once.
- Preferences remain data, never FVWM commands or executable plugins. Map IDs
  to known actions with validated arguments. Use private atomic writes, review
  file ownership/path handling, cap input and define schema migration behavior.
- Separate saved preferences from effective runtime state and report overrides
  by personal FVWM configuration. Do not rewrite arbitrary personal configs.
  Preview/revert applies only to explicitly reversible, owned settings; define
  cancellation/recovery before enabling preview. Reset must be narrowly scoped.
- Acceptance: boundary/type/unknown-setting tests, failed-write recovery,
  persistence/reset/reload and real-X effective-state/preview verification.
- Shared preview scope: owned snapshots/generations, bounded confirm/cancel/
  timeout, consumer-loss/restart recovery and refusal on unsupported recovery or
  conflicting external changes. Qualify current reversible appearance/layout
  settings; future display adapters and arbitrary actions gain no implicit rollback.
  See [shared contracts](decisions/DESKTOP-EXPLORATION-2026-10-06.md).

### CFG-02: source-aware diagnostics and safe checking

- Carry source filename, logical line and nested include context into diagnostics
  for unknown commands, malformed arguments/options and module configuration.
  Keep errors useful across continuation lines and identify the owning parser.
- Add non-executing checking for an explicitly supported declarative subset.
  Dynamic functions, expansions and generated configuration may be unchecked;
  report that limitation rather than claiming the whole file is validated.
- Never run shell commands, PipeRead, modules or configuration actions during a
  check. Bound input, include depth and diagnostic output. Preserve normal
  runtime parsing/ordering; no silent language rewrite or compatibility break.
- Acceptance: exact source locations, nested/continued input, malformed data,
  resource limits, and negative tests proving checks execute no external action.

### IPC-02: structured queries and operation results

- Extend existing JSON event transport additively with versioned queries,
  request IDs and explicit result/error replies. Candidate state includes
  monitors/working areas, configured bindings and registered effective settings.
- Keep authoritative X/window state in the core. Helpers report only state
  they own; do not create a second window-state authority. Clearly distinguish
  request acceptance, applied changes and asynchronous service completion.
- Bound message size, outstanding requests, reply queues and wait times;
  define stale IDs, disconnects, unsupported operations and slow consumers.
  Preserve existing IPC-01 framing, producer ordering and legacy clients.
- Retain private local endpoints; add no network listener or privilege broker.
  Consider an explicit read-only client mode for accidental-authority reduction,
  without presenting it or process isolation as an X11 security sandbox.
- Acceptance: native protocol tests and real-X correlation/error/order checks,
  slow-reader isolation, bounded disconnect handling and legacy compatibility.
- Implemented focused subsets: PR154 requests/core queries/barriers/read-only;
  PR155 owned settings queries/actions; current non-publishing validation, local
  pending snapshots and wait-only cancellation. The current directed topology,
  workspace/cache and global-binding queries plus provider-readiness fence pass
  the shared affected cycle. Merged PR158 adds menu/fallback records, normalized placement masks and
  context/modifier policy. The next provider-observation batch passes focused
  checks for capabilities/configuration state, invalidations and conditional
  snapshots. Current desktop-scan operation admission/results/cancellation,
  retention and invalidations pass the accumulated six-target cycle. Domain
  file/session/clipboard progress/decisions and full provider/core acceptance
  remain open. Core colorset inventory, exact sparse-ID lookup and summary/
  freshness inspection are merged in PR161. Current appearance configuration
  freshness and bounded menu/decor pages pass focused Linux gates, including a
  reproduced copied-opacity repair. Owned snapshots, conditional application
  and conflict-safe CFG-01 preview/recovery remain open.
- Shared service/action scope: bounded provider capability/state records and
  generation-aware typed action envelopes with explicit unsupported/unavailable/
  denied/stale failures. SESSION owns service lifecycle; domain adapters own
  targets and payloads. Preserve scoped protected-content handles and legacy order.
- Shared operation scope: correlated progress/decision/cancellation and terminal
  results, honest unknown totals/partial outcomes, bounded progress coalescing
  and explicit view-disconnect/reconnect policy. Adopt with existing session/file
  consumers; see [scope and acceptance](decisions/DESKTOP-EXPLORATION-2026-10-06.md).

All five items use existing dependencies and process-isolated modules where
appropriate. The later approved LUA-01 scope adds an isolated Go-hosted scripting
consumer over these contracts; concrete engine adoption remains gated. No new
GUI toolkit, built-in compositor or privileged service is in scope. Apply the
existing Linux/FreeBSD/OpenBSD ×
ARM64/AMD64 verification requirements, user-visible manual documentation and
native Linux sanitizer CI requirements appropriate to each implementation.

## File manager primitives and consumer handoff

Added at the user's request on 2026-10-06. The
[file-manager decision](decisions/FILE-MANAGER-PRIMITIVES-2026-10-06.md) expands
scope, complexity, ownership and acceptance; this tracker remains the only queue.
These are P2 requirements, not implemented commands or a new mandatory toolkit.
Existing hardening/responsiveness and shared desktop contracts remain ahead.

1. FILE-VIEW-01 + FILE-DATA-01 + FILE-ACTION-01: qualify a configurable local
   browser with native collection/input/action primitives and a working example.
2. FILE-JOBS-01: reliable asynchronous everyday operations and error recovery.
3. DESKTOP-ICONS-01: optional persistent pinboard sharing the browser model.
4. FILE-INTEGRATION-01: portable folder/reveal/properties role, then the optional
   standard D-Bus adapter after SESSION-02's common bus contract. File clipboard
   and XDND use SELECTION-01; design integration requests at checkpoint 1.
5. FILE-ADVANCED-01: deferred candidates, promoted individually after prerequisites.

At checkpoint 1, publish qualified source/profile evidence, installed man pages,
actual configuration syntax, versioned item/action/result contracts, capability
limits and a working browse example. A separate user-directed config/theme build
thread can then consume the primitives while core work continues. It need not
wait for mutation, desktop icons or advanced features; later checkpoints publish
additional handoffs. The consumer owns presentation/configuration, runtime owners
retain correctness/services, and missing capabilities return to the owning item.
No theme/default/install redesign or new chat is implied by this documentation.

Follow WORKFLOW.md's focused-development/combined-qualification cadence and
SUPPORT.md's relevant native/real-X/six-target/install/manual gates. Do not mark
handoff ready from implementation alone or from a consumer's successful theme
prototype. Scope/priority approval is separate from passing runtime evidence.

## Clipboard primitives and consumer handoff

Added at the user's request on 2026-10-06. The
[clipboard decision](decisions/CLIPBOARD-PRIMITIVES-2026-10-06.md) records approved
primitives-first scope, security boundaries, complexity and acceptance. These P2
requirements follow hardening/responsiveness and qualified shared desktop
contracts in stage 6; SELECTION-01 keeps its stage 3 audit/stage 5 transfer work.
Reuse FILE-VIEW-01 and typed actions rather than building another collection UI.
File mutation, advanced file previews and D-Bus are not prerequisites.

1. CONTENT-01 + SELECTION-01 + RETENTION-01 + STORE-01's memory protection:
   qualify bounded text/image items, transfer, lifetime and protection contracts.
   CLIPBOARD-01 supplies a minimal working consumer, installed man pages, actual
   syntax, versioned item/action/result contracts and protection/limit capabilities.
2. Publish this qualified handoff for a separate user-directed configuration/
   theme thread to build the full manager while further runtime work proceeds.
   The consumer owns presentation/bindings/workflows; runtime owners retain
   transport, lifetime and security correctness. Publish incremental handoffs.
3. STORE-01 encrypted persistence and unlock/relock/recovery form the next
   checkpoint. Optional hidden, expiring secret history remains decision-dependent;
   passwords-on-disk and default persistence are not implied by scope approval.

Keep sensitive payloads out of core commands/general broadcasts, protect derived
previews/indexes and test OS protection failures. Ordinary X11 clipboard access
by other display clients remains outside the confidentiality guarantee. Password
retention, capture/persistence defaults, limits, crypto/backend packaging and
public syntax are open decisions, not adopted recommendations. Apply native
policy/storage, independent real-X interoperability, six-target/install/manual
and native Linux sanitizer/security-review gates; theme prototypes do not qualify
the runtime. This record creates no chat or implemented capability.

## Shared desktop contracts and later exploration

Approved at the user's request on 2026-10-06. The
[desktop exploration decision](decisions/DESKTOP-EXPLORATION-2026-10-06.md)
scopes contracts with multiple uses into current owning work, while further
desktop features wait until the existing program is complete. These are queued
requirements, not implemented APIs, new core services or evidence of passing
runtime qualification. Existing feature decisions are supplemented by this scope.

The subsequent [Go/Lua direction](decisions/RUNTIME-GO-LUA-2026-10-06.md)
adds LUA-01 as a programmable consumer of these same contracts. Scope versioned
script bindings, opaque identities, bounded subscriptions/results and reload
generations into the owning handoffs; Go/native services retain correctness,
authorization and payload protection. Initial primitives remain useful without
Lua, and script source stays separate from persisted data-only preferences.

| Shared contract | Current owners and bounded checkpoint |
| --- | --- |
| Capability and service-state reporting | SESSION-01/02 + IPC-02 + CFG-01: provider identity/generation, supported operations, lifecycle/availability/failure reasons and bounded query/update recovery. Reuse owned/borrowed service policy and missing-provider UI; no arbitrary plugin framework or privilege broker. Stage 6, optional bus adoption in stage 7. |
| Typed actions and target identity | IPC-02 + FILE-ACTION-01, coordinated with CONTENT/PROC: one versioned action/result envelope, literal typed arguments, domain-owned target generations, execution revalidation and explicit stale/denied/unavailable results. File and clipboard handoffs retain separate schemas, payload ownership and privacy. Stage 6. |
| Asynchronous operation progress/cancellation | IPC-02 + FILE-JOBS-01, coordinated with PROC/SESSION: accepted/running/decision/terminal states, known or unknown totals, bounded progress/decisions, best-effort cancellation and explicit partial/disconnect outcomes. Qualify existing file jobs and session operations without promising common rollback or a general task daemon. Stage 6. |
| Collection models and bounded filtering | FILE-VIEW-01, coordinated with CONTENT/TEXT/A11Y: domain adapters, stable IDs/generations, bounded incremental sort/filter, supersession cancellation, paging feedback and selection/focus preservation. Qualify file and clipboard consumers at their existing handoffs; protected content cannot enter general indexes/caches. Stage 6. |
| Owned-setting preview/confirm/revert | CFG-01 + IPC-02, coordinated with SESSION: supported reversible settings only, validated snapshots/generations, bounded deadlines, confirmed persistence, recovery and external-change refusal. Current owned appearance/layout consumers qualify this; later display/input backend safety remains separate. Stage 6. |

Keep blocking backend work in isolated owners and authoritative X/window state
in the core. Concrete APIs and shared implementation extraction follow source
review and actual consumers; no broad rename, replacement toolkit, mandatory
desktop service or hardware adapter is added by this scoping. Follow focused
development and combined qualification with relevant native/real-X/six-target,
installed-manual and sanitizer gates. Preserve existing handoffs and open
clipboard security/default decisions.

DESKTOP-EXPLORE-01 records the later candidates, including interfaces mostly
assembled from qualified primitives and capabilities needing new providers.
It follows existing stages 1–8 (release preparation when requested), is not a
release acceptance requirement and does not advance FILE-ADVANCED-01 or the
deferred native locker. Each candidate needs a bounded adoption decision,
platform/dependency review and acceptance plan before implementation.

## Hardening closeout

Approved priority change, 2026-10-21: concentrate on major safety impact and
maintainable ownership/error paths, then advance features. See the
[decision](decisions/HARDENING-CLOSEOUT-2026-10-21.md). This is the current
selection within stages 2–4, not a new independent backlog or a security
certification. A reproduced serious security/availability failure still moves
first. No runtime change or new passing runtime evidence follows from this
priority update. All three selected pickups below now have source-scoped
qualification/dispositions and are merged through PR143.

| Pickup | Owners and bounded entry points | Contract to review and qualify |
| --- | --- | --- |
| Stream repairs and provenance qualified | FD-01 / PROC-01 / CLEAN-01: Status FIFO/continuation, log replacement/failure and immediate native consumers. | PR141 qualifies the stream contracts. The delayed-module provenance concern is reproduced as wrong metadata in the preserved old core and repaired by fork-time identity plus once-only adoption in [PR142](verification/VERIFICATION-2026-10-21-MODULE-INPUT-OWNERSHIP.md). Broad stream/process owners remain open. |
| Selected external-input review disposed | DATA-01 / FD-01: shared legacy/incremental module readers, GetConfigLine, native Script packet/value/message consumers and root Send_ConfigInfo. | PR142 qualifies terminal malformed/truncated framing, Script token ownership and missing module-recipient refusal. GetConfigLine and Script packet/value sources/consumers remain exact to merged PR141 and already covered. [Entry-point dispositions](verification/VERIFICATION-2026-10-21-MODULE-INPUT-OWNERSHIP.md#selected-input-review-dispositions). |
| Interaction-lifetime review qualified in PR143 | INPUT-01 / DATA-01 / CLEAN-01: WaitForButtonsUp, CMD_Wait, execute_complex_function and directly retained command contexts. | Three reproduced cancellation failures are fixed and qualified: selection shutdown, held-classification shutdown and target loss before conditional dispatch. Existing root-only release waiting, refreshed Wait contexts, owned trigger snapshots, active input references, use_depth and deferred managed-window cleanup have explicit already-covered dispositions. [Report](verification/VERIFICATION-2026-10-21-FUNCTION-LIFETIMES.md). |

These are bounded review pickups, not three mandatory implementation PRs or
assertions that every path is defective. Inventory immediate callees/ownership
and freeze each pickup's exact cases before changing code. Batch three to five
related substantive repairs where supported; combine related findings, record
already-covered paths, and do not manufacture cleanup to fill a batch. Further
broad image/sandbox, dependency, tooling, style/cache and structural proposals
retain their owners and named prerequisites; they do not silently expand this
closeout. A confirmed serious finding in any area still advances immediately.

The selected finite closeout satisfies the following local handoff criteria at
merged PR143. Native-CPU and physical/provider/release
gates stay open with their owners. Follow the approved [feature cutoff](decisions/FEATURE-HANDOFF-2026-10-21.md):
manager targets, then BUTTON/CFG/IPC. Additional general maintenance remains
future-release backlog; this does not close the historical audit or approve release.

Handoff to feature dependencies requires:

- A source-scoped disposition for each selected entry point: fixed and verified,
  already covered with exact evidence, or explicitly deferred with owner/reason
  and whether it blocks release or a named feature. No silent dropped finding.
- No unresolved confirmed high-impact crash, corruption, unbounded-work or
  resource/process lifetime defect in the closeout set. This criterion cannot
  be met merely by marking such a finding deferred.
- Focused normal, boundary, failure/retry and teardown cases for changed behavior;
  applicable policy, real-X, sanitizer, installed-consumer and six-target gates
  at the combined checkpoint. Reuse unaffected source-scoped evidence and keep
  native-CPU/hardware/toolkit/compositor limitations explicit.
- Ownership/error-path documentation and the tracker/GitHub mirror reconciled.
  Do not require the entire historical audit or tuning inventory to be finished.

After those dispositions, follow the approved feature cutoff and its named
feature dependencies; additional general stage 5 work is deferred. Native-CPU sanitizer and physical/toolkit/compositor
acceptance remain release gates; they do not require repeating accepted VM
checks or block unrelated feature development. Release preparation/publication
retains its separate ownership and user decision. Reopen a closed safety
checkpoint for a concrete finding or changed contract, not open-ended polish.

## Cleanup and maintenance handoff

The user requested prioritization, safe housekeeping now, parallel replacement
tooling, then migration/removal after verification. See the
[cleanup and library roadmap](CLEANUP-ROADMAP-2026-10-05.md) for all findings,
estimated effort, acceptance criteria and upstream sources.

User direction, 2026-10-23, now prioritizes source/legacy/dependency maintenance
and pauses new features. The provider boundary is accepted in merged PR187.
Use the maintenance-first register for all remaining owner slices; preserve
accepted work and keep performance-only tuning deferred. General maintenance
is no longer restricted to a feature prerequisite. Keep MAINT-01 candidates and remaining MAINT-02 migrations with
their existing owners; neither all tooling migrations nor the deferred
performance tail is a prerequisite for feature handoff.
CI-01's test/sanitizer work is separate from release publishing ownership.
This is a handoff after an evidence checkpoint, not a claim that SCRIPT-01 or
other open hardening work is complete. Per-item maintenance acceptance is in
the active backlog and cleanup roadmap.

## Cross-cutting work details

The [optimization and architecture roadmap](OPTIMIZATION-ROADMAP-2026-10-05.md)
records the 2026-10-05 source review, queue probe, effort estimates and
per-item acceptance. It expands this tracker rather than creating another
schedule. Queue/output and stacking/style/cache/derived-state items are new;
interaction, measurement, drawing, readiness, XIO and structural boundaries
extend their existing owners. Runtime implementation remains in the build
thread; this handoff does not change current checkpoint evidence.

- INPUT-01: include `WaitForButtonsUp` and menu/paging button-release waits
  in the next appropriate interaction batch. Replace repeated pointer-query
  busy polling with shared bounded readiness/deadline handling, preserving
  grab/release/key/expose semantics and cancellation. Real-X progress and
  lifecycle tests are required. A normal-pointer Escape/held-button control now
  reproduces delayed safe timer/module work and repeated-query CPU use. The
  [button-release servicing proposal](decisions/BUTTON-RELEASE-SERVICE-PROPOSAL.md)
  defines the next bounded batch and its specific ordering/cursor timing decision. LOOP-01 consolidates the resulting contract
  rather than creating an independent wait implementation.
- PERF-01: partially implemented. Overdue timers now share a 64-command budget
  between descriptor polls, including nested waits. Each timer is dequeued
  before dispatch, fixing duplicate execution and a reproduced scheduled-Wait
  crash. Sorted head checks replace full-queue execution scans, and XFlush is
  batched. Native policies and real-X ordering, cancellation, periodic timers
  and module fairness pass all six targets. Remaining performance-only
  elapsed-budget tuning and timer-heap experiments
  move to the next release. Keep lightweight regression observations and
  investigate concrete hangs/unbounded work under their safety owner. A budget
  cannot preempt a single blocking handler.
- CLEAN-01: extract pure protocol/geometry/ownership policies and consolidate
  duplicated cleanup. Cloned command contexts now own their triggering-event
  snapshot; window/module and live last-event references remain borrowed.
  The core input queue now uses intrusive global/per-producer links, removing
  its extra queue-node allocation. Paired current/previous X-event snapshots
  preserve put-back behavior during temporary execution. Avoid mechanical API
  renames and style-only rewrites.
- VERIFY-01: native Linux sanitizer execution remains a release gate, with
  bounded protocol/parser fuzz entry points still to add. Produced native C
  executable hardening is checked in Linux GCC/Clang gates for PIE, RELRO,
  immediate binding and non-executable stacks; optional Go binaries are outside
  that check. Local Podman/QEMU passes do not establish a native ASan/UBSan CI
  result. Preparation can continue while executor/repository access is pending.
- IMAGE-01: evaluate an isolated, resource-bounded image decoder helper while
  keeping authoritative X operations in the core.
- COMPAT-01: representative baseline complete on all six OS/architecture
  targets. Fixtures cover decoration recreation, vector buttons, colorsets,
  styles, menus, real key/mouse bindings, complex-function geometry, quoted
  nested Read and PipeRead ordering, and inherited stream descriptors.
  Synchronous module startup/unlock now has a real-X saturation regression;
  native pipes also verify direct unlock reads at production queue capacity.
  Held-drag regressions now cover active producer continuations, independent
  producer routing, actual style mutation after release, dynamic command names,
  unsafe timer ordering and synchronous message callbacks.
  Real XSMP registration is now covered. Arbitrary third-party theme collections remain untested.

## Verification history

Monitor/HiDPI, shaded/interactive resize, MFL event flags, monotonic scheduling,
and a 16-event X dispatch budget are implemented. Initial completed gates:
Linux AMD64 GCC and ARM64 GCC/Clang each 30 tests; four BSD targets each 21 tests,
with default-X-server geometry and responsiveness checks. These counts describe
the previous baseline, not changes begun below. Native Linux ASan CI is
configured but has not run. No remote is configured.

This implementation batch passes 35-test Linux suites and 26-test BSD suites,
plus affected follow-up tests and default-X BSD checks. See the
[2026-10-04 report](verification/VERIFICATION-2026-10-04.md) for exact coverage and limits.

The subsequent property/descriptor/compatibility batch passes 40-test Linux
suites and 31-test BSD suites, with final affected-test follow-up and default-X
checks on all four BSD targets. See the
[hardening report](verification/VERIFICATION-2026-10-04-HARDENING.md). These are local gates;
native Linux sanitizer CI remains unrun.

The legacy-property/session follow-up passes 41-test Linux suites and 32-test
BSD suites, with final native-policy/startup follow-ups and checks on all four
default BSD X servers. See the [session report](verification/VERIFICATION-2026-10-04-SESSION.md).
These results use an isolated hardening snapshot: concurrently added desktop
configuration/tests are preserved in the checkout and excluded from this batch.

The first interaction pass adds shared cancellation waits and bounded motion
coalescing. Linux gates pass 43 tests and BSD gates pass 34 tests, with affected
follow-ups and all four BSD default servers checked. See the
[interaction report](verification/VERIFICATION-2026-10-04-INTERACTION.md) for scope and harness
follow-ups. At that baseline, timers/module commands still waited during a held drag.

Evidence: [platform support](SUPPORT.md), [BSD harness](BSD.md),
[verification](verification/VERIFICATION-2026-10-03.md), and
[comparative WM review](verification/WM-REVIEW-2026-10-03.md).

The scheduler follow-up verifies the integrated desktop, graphics, interaction
and hardening sources together: Linux AMD64 GCC and ARM64 GCC/Clang each pass
47 tests; all four BSD targets pass 38 tests and default-server scheduler checks.
Final affected checks pass after correcting the configure-test server-grab race.
See the [scheduler report](verification/VERIFICATION-2026-10-04-SCHEDULER.md).

The command-lifetime follow-up gives cloned contexts their own triggering event
and caps the core's pending input at 256 commands per module, pausing reads
without dropping commands. Native ownership and real-pipe tests pass, as do
Linux real-X saturation/synchronous-handshake checks and all four BSD full gates
and default-server compatibility/scheduler checks. See the
[command-lifetime report](verification/VERIFICATION-2026-10-04-COMMAND-LIFETIME.md) for exact
counts, source isolation and remaining lifetime work.

The held-drag servicing follow-up admits audited direct commands and safe timer
heads in bounded passes, preserving producer order and active continuations.
Linux gates pass 51 tests; BSD gates pass 41, followed by final cache checks and
compatibility/servicing/cancellation on all four default servers. See the
[servicing report](verification/VERIFICATION-2026-10-04-INTERACTION-SERVICING.md). At that baseline, synchronous
frame subscribers still retained legacy waits; the subsequent timing change is
approved and implemented in the [frame report](verification/VERIFICATION-2026-10-04-FRAME-SYNC.md).

The synchronous-frame follow-up copies/coalesces packets during Move/Resize,
refreshes authoritative final geometry (including rollback), and delivers only
when the outer command stack and grabs unwind. Native ownership/limits and
fourteen real-X frame cases pass. Linux GCC/Clang gates pass 52 tests and all
four BSD gates pass 42, with final fixture/default-server follow-ups. See the
[frame report](verification/VERIFICATION-2026-10-04-FRAME-SYNC.md). Final synchronous waits
retain their legacy callback/timeout behavior after release.

The module input/descriptor follow-up passes 55 tests on Linux ARM64 GCC,
ARM64 Clang/lld and AMD64 GCC; each BSD target passes 44 tests plus actual
module checks on its default X server. See the
[module input report](verification/VERIFICATION-2026-10-04-MODULE-INPUT.md). These verified
reader results are separate from the subsequent interpreter/file work.

## Decisions and progress

- 2026-10-23: accepted [dependency simplification](decisions/DEPENDENCY-SIMPLIFICATION-2026-10-23.md); updated ten existing issue owners and created #179–184 with native parents, dependencies and Later/P2/Backlog Project fields. FVWM 3.x is the compatibility baseline; mandatory Go, full Perl/Python removal and bounded build/library evaluations are queued. Current feature handoff and completed evidence remain unchanged; no runtime improvement is claimed.

- 2026-10-08: user requested complete language support, translated man pages and
  configuration comments as polish after everything else. Added I18N-02 and
  [scope](decisions/LOCALIZATION-POLISH-2026-10-08.md): eleven current locales,
  shared reviewed catalogs/glossary, proposed Weblate/AI-assisted workflow,
  AsciiDoc-to-man/HTML translation and generated config-comment views with
  command parity. Earlier catalog/text/manual foundations retain their owners;
  no translations, tooling dependencies, services or release gates adopted.

- 2026-10-06: user requested runtime-first language standardization: Perl out,
  Python runtime helpers replaced by Go, Lua/go-lua where useful. Added
  RUNTIME-GO-01/LUA-01 and later TOOLING-GO-01; changed UTIL-01's target from
  Python to Go and COMPAT-02 from optional Perl preservation to explicitly
  approved retirement of Perl-specific interfaces. C core/native modules and
  unrelated FVWM/Script compatibility remain. Scoped Go helper parity, native
  screenshot/protected-content boundaries and isolated generation-aware Lua
  bindings into current primitives; no runtime port/dependency/removal performed.
  Audited current install/source references and upstream go-lua limitations;
  local Go lists all six target combinations, which is not native qualification.
  Documentation references/anchors/owner IDs checked locally. See
  [runtime scope](decisions/RUNTIME-GO-LUA-2026-10-06.md).

- 2026-10-06: user requested that additional desktop features be recorded for
  exploration after all existing work, with primitives useful across features
  scoped into current development. Added deferred DESKTOP-EXPLORE-01 and scoped
  capability/state reporting, typed action identities, asynchronous operation
  feedback/cancellation, collection filtering and owned-setting preview/revert
  under existing CFG/IPC/SESSION/FILE owners. Current order, release scope and
  FILE-ADVANCED/native-locker deferrals remain intact; no runtime, backend,
  dependency or default change and no qualification claim. Documentation links,
  tracker IDs and formatting checked locally. See
  [shared scope and inventory](decisions/DESKTOP-EXPLORATION-2026-10-06.md).

- 2026-10-06: user approved documenting and adding reusable text/image clipboard
  primitives to the backlog, then consuming qualified contracts in a separate
  configuration thread while further runtime work continues. Added CONTENT-01,
  RETENTION-01, STORE-01 and CLIPBOARD-01 as P2 stage 6 work, reusing SELECTION-01,
  FILE-VIEW-01 and shared settings/actions. Memory-protected text/image handoff
  precedes encrypted persistence; password retention/defaults/backend choices
  remain open. Preserve current hardening and file-manager/service owners; no
  runtime/configuration changes or passing security claims. See
  [clipboard scope](decisions/CLIPBOARD-PRIMITIVES-2026-10-06.md).

- 2026-10-06: user approved documenting and ranking reusable file-manager and
  desktop-icon primitives, with a separate config/theme build thread consuming
  qualified primitives while core work continues. Added FILE-VIEW/DATA/ACTION,
  FILE-JOBS, DESKTOP-ICONS and FILE-INTEGRATION owners as P2 stages 6/7;
  FILE-ADVANCED holds deferred candidates. The browser contract/manual/example
  is the first consumer handoff. Preserve existing security/responsiveness,
  session/bus and release order; no runtime/config/theme changes or implemented
  capability claims. See the
  [file-manager scope](decisions/FILE-MANAGER-PRIMITIVES-2026-10-06.md).

- 2026-10-06: after the standards documentation checkpoint, focused THEME-01
  review found light Dune/Grove/Ink disabled Session text below the stricter
  project readability policy, and modeled translucent muted text gaps in Grove/
  Ink. Adjusted only those muted palette tokens, regenerated matching disabled
  icons with the existing tool, and documented contrast/motion limits. Native
  rendering/keyboard/layout qualification remains open; runtime/generator source
  is unchanged. See [review](verification/VERIFICATION-2026-10-06-STANDARDS-THEME.md).

- 2026-10-06: user approved repository-wide standards guidance and properly
  ranked conformance stories, followed by configuration/theme work in this
  thread. Added STANDARDS/EWMH/SELECTION/XDG/TEXT/SETTINGS/A11Y/THEME owners with
  bounded acceptance and shared dependencies. Native UI uses WCAG2ICT-adapted
  policy; web exports target WCAG 2.2 AA. Neither guide asserts current complete
  conformance. Runtime changes remain in the build thread; GTK/Qt theme creation
  and native Wayland remain outside this approval. See [standards](STANDARDS.md)
  and [theme guidance](THEME-STANDARDS.md).

- 2026-10-06: user reaffirmed the full currently approved work for the first
  release; the suggested smaller first-release milestone was not adopted.
  Refine, modernize, secure and improve performance before adding the queued
  desktop features, following the existing execution order. The concern is
  drift/repetitive work, not reducing scope. Use bounded owning checkpoints and
  the existing batched verification cadence; repeat accepted checks only for
  relevant changes, failures or unresolved concerns. Further concrete requests
  may extend scope, but this does not authorize speculative rewrites or revive
  declined/deferred work. RELEASE-01 remains last, with publishing separately
  authorized.
- 2026-10-06: user approved a capable portable session manager as part of the
  default install, with comprehensive D-Bus-specific contrib extending the same
  configuration, settings, session actions and UI. Added SESSION-01/SESSION-02:
  foundation after shared desktop primitives, common optional bus contract
  before notification/modern-tray adoption, combined extension qualification
  afterwards. Missing optional services must not block login; enablement should
  be one documented opt-in rather than replacement startup functions. Scope is
  approved; exact APIs, library selection and runtime/build/install adoption
  remain with the build thread. No runtime/default changes in this checkpoint;
  native locker deferred and release gate still last. See the
  [session contract](decisions/SESSION-INTEGRATION-2026-10-06.md).
- 2026-10-06: user approved opt-in existing-locker and Autorandr contributions.
  Added commented/manual recipes and documentation under
  [contrib](../contrib/README.md): xlock/xlockmore resources, optional OpenBSD
  xidle startup, actual user-captured display profiles and initial-login detection.
  No runtime, baseline startup, dependency or install-target change; no service
  was started. Source checks are separate from unrun hardware/authentication,
  hotplug and suspend/resume acceptance. See the
  [recipe review](verification/VERIFICATION-2026-10-06-CONTRIB-RECIPES.md).
  Deeper session ownership, login1/elogind and lock/sleep coordination remain
  discussion, not newly approved runtime implementation. The native locker
  remains deferred and existing execution order is unchanged.
- 2026-10-06: user approved adding NOTIFY-01 and TRAY-01 to the roadmap;
  the locker remains deferred. Native notifications precede XEmbed then
  StatusNotifier tray work after existing hardening/responsiveness and desktop
  primitives. The optional D-Bus dependency/default policy remains to be selected;
  no new mandatory package or runtime implementation in this configuration thread
  is approved. RELEASE-01 remains the last stage. See the
  [service scope](decisions/NATIVE-DESKTOP-SERVICES-2026-10-06.md).
- 2026-10-05: user declined removing image titlebar/button styles, multipart
  decorations, optional SVG support, or restricting images to wallpapers.
  These exploratory simplifications are **not going to do**, not queued work;
  retain existing FVWM rendering/theme/configuration compatibility and the current
  default. Previously approved image hardening remains in scope. See the
  [rendering decision](decisions/RENDERING-COMPATIBILITY-2026-10-05.md).
- 2026-10-05: user authorized cleanup prioritization and additive developer
  tooling alongside the current tools. Runtime/installed-component items are
  queued above for the next build thread, with library review and migration
  gates in the cleanup roadmap. Safe documentation/housekeeping changes and
  parallel tools are implemented; live build, Meson, CI/release and runtime
  entry points remain unchanged by this maintenance batch.
- 2026-10-05: user requested all five configuration/module primitive proposals
  be added to the main backlog for the build thread: BUTTON-01, BUTTON-02,
  CFG-01, CFG-02 and IPC-02. Scope, security constraints, compatibility and
  acceptance criteria are recorded above. This is documentation-only work;
  this thread does not authorize code implementation or further theme changes.
- 2026-10-04: user confirmed simple internals, FVWM theme/config compatibility,
  responsiveness and security; requested all review items be tracked and begun.
- 2026-10-04: IPC-01 and GEO-01 complete with native and real-X regression
  coverage. Pre-fix binaries fail both new real-X regressions. A follow-up fix
  preserves other subscriptions when an extended event is removed.
- 2026-10-04: CLEAN-01 has protocol and configure-policy extractions with native
  unit tests. Remaining cleanup is still open.
- 2026-10-04: bounded EWMH reads fix the short-strut indexing hazard; legacy
  decoration and MULTIPLE validation fixes additional malformed-record paths.
  Valid four-field Motif and three/five-field Open Look records remain supported.
- 2026-10-04: MFL lock lifetime and direct-signal exit bugs reproduced and fixed;
  duplicate-instance, KillModule restart and direct SIGTERM restart checks pass.
  Pre-fix negative controls fail the new legacy-property and signal regressions.
- 2026-10-04: INPUT-01 has an executable held-pointer baseline and a deliberately
  failing --require-responsive mode. It is diagnostic work, not a passing CI gate.
- 2026-10-04: shared ICCCM/session getters remove duplicate parsing and rejected
  reply leaks. Startup scalar checks, legacy protocol compatibility and ICE
  callback lifetime fixes pass Linux and all BSD default-server regressions.
  Restoring pre-fix ICE lifetimes makes the new disconnect regression fail.
- 2026-10-04: native Linux sanitizer CI now enables the live-X tests as well as
  native tests; it remains configured but unrun. FreeBSD harness dependencies
  explicitly include iceauth. Optional session-disabled build checked separately.
- 2026-10-04: INPUT-01 cancellation foundation implemented. Held-button client
  destruction/withdrawal no longer traps the WM, including titlebar and focus
  cleanup. Termination releases opaque and wireframe grabs. A pre-fix binary
  fails the new cancellation regression. Motion coalescing is shared and bounded;
  release and key barriers remain intact. Default-server checks include OpenBSD
  ARM64 wsfb with the root-sized fallback. No compatibility decision required.
- 2026-10-04: DESKTOP-01 baseline complete: one commented Slate layout with
  four light/dark palettes, a centered launcher, primary top bar, persisted
  appearance/schedule/opacity controls, and stdlib/base-tool desktop services.
  Old shipped default assets and scripts were removed; personal configs and
  module fallback compatibility remain. Native graphics error-handler and
  root-pixmap cleanup bugs discovered by palette switching are fixed with a
  deterministic real-X regression and a failing pre-fix control. Linux AMD64
  build/install, 25 native tests and desktop/compatibility checks pass; native
  desktop and graphics checks pass on both BSD architectures. GTK/Qt, a full
  notification server, bundled authentication locker, and graphical monitor
  editor remain separate work. See [desktop verification](verification/DESKTOP-VERIFICATION-2026-10-04.md).
- 2026-10-04: DESKTOP-01 appearance review: optional Picom shadow/fade profile,
  menu keylines, slimmer focus borders, taller titles, 14-pixel terminal padding
  and configurable 16-pixel desktop insets. Composited screenshots include the
  Composite overlay. Ten policy checks and actual Linux AMD64/Xvfb capture,
  shadow-pixel and maximize/restore checks pass. Review uses Picom 12.5's
  compatibility XRender path; the compositor remains optional. Evidence and
  all palette captures are in `builddir/slate-verification/shadows/`.
- 2026-10-04: PERF-01 scheduler prerequisite complete. A pre-fix Linux ARM64
  binary exits with SIGSEGV when a scheduled function enters Wait; dequeue
  before dispatch fixes it. A stopped-inside-Read fixture establishes an overdue
  256-command backlog and observes independent module input after 64 timers on
  all four BSD default servers. Integrated Linux/BSD gates include the desktop
  work. The configure harness now owns the server before stopping the WM,
  avoiding freezing FVWM while it owns the grab. Initial FreeBSD ARM64 timeout
  evidence and the passing full rerun are retained.
- 2026-10-04: INPUT-01 ownership/backpressure prerequisites implemented. Context
  clones own their triggering event; a failing old-code control confirms the
  previous parent alias. Core polling admits at most 256 queued inputs per
  producer and resumes reads as commands start, retaining pipe bytes and module
  lifetime pins. Seventeen real modules pass an 8,704-command stress fixture
  with ordered markers, independent input and synchronous startup/unlock;
  that fixture uses documented verification-only 16/8 limits. Restoring the old
  event-reading path fails its saturation assertion. The shared FIFO and held
  drag behavior remain unchanged. Window/resource lifetime is still open.
- 2026-10-04: user approved safe independent module commands passing another
  module's deferred command during a held drag. Preserve each module's order,
  including the continuation of its active command; retain the shared FIFO
  outside interactions. This is the specific cross-module ordering exception.
- 2026-10-04: INPUT-01 safe servicing implemented under the approved ordering
  exception. Move/Resize admit eight audited commands per pass with 64-record
  bounded scans, transport polling and safe timer heads. Producer heads and
  active command leases preserve each producer's continuation order. Native
  queue tests, real opaque/wireframe interactions and all platform gates pass.
  Restoring the old drag code fails independent-input servicing; restoring old
  FEvent code fails the previous-event put-back assertion. Full/cache/default-X
  evidence and the corrected startup-binding fixture are saved in the report.
- 2026-10-04: a controlled synchronous frame subscriber reproduces a legacy
  stall: independent input and processing physical release wait for its unlock.
  The user subsequently approved delivering final geometry after the drag,
  subject to secure processing.
- 2026-10-04: user approved deferred synchronous frame notifications subject
  to secure processing. Implementation copies and bounds packet snapshots,
  coalesces by window ID, retains the shared output cap, and waits until the
  outer command stack/grabs unwind before the existing synchronous handshake.
  Final geometry refresh covers no-op rollback. Linux 52-test and BSD 42-test
  gates pass, with final fixture checks on all six targets and four default BSD
  servers. The pre-deferral control delivers an intermediate packet and stalls
  its timer. The corrected content-based fixture barrier, initial failures and
  independent 34-file source fingerprints are retained in the frame report.
- Next: continue widget geometry/value and remaining module FIFO arithmetic
  checks. Continue INPUT-01's target/resource lifetime model
  and FD-01's helper/Perl ownership audit. Arbitrary mutators remain deferred;
  the current pump retains
  lifecycle events and does not run the normal dispatcher inside a drag.
  Other review items retain their acceptance criteria above.
- 2026-10-04: DESKTOP-01 native bar follow-up complete. Separate live metrics,
  anchored quick settings with a native mixer slider, optional embedded
  FvwmIconMan tasks, focused-title support, and persisted widget placement use
  bundled modules and base tools. Fifteen policy tests and actual X interactions
  pass on Linux AMD64 and both FreeBSD/OpenBSD architectures. Tests exercise
  selected appearance repaint, Escape, task restore and narrow/custom layouts;
  Linux additionally exercises slider/mute with a private backend fixture and
  optional Picom shadow captures. Existing transparency fixes were relinked in
  snapshot BSD guests. GTK/Qt, tray and full notifications remain separate work.
  Evidence: `builddir/slate-verification/bar/`.
- 2026-10-04: DESKTOP-01 live launcher complete. Super+Space searches apps,
  windows and desktop actions through native FvwmScript widgets and stdlib
  policy. Favorites, optional recents, desktop-entry actions, fuzzy matching,
  compact layouts and full keyboard navigation are documented in the default.
  Private query records never enter the FVWM command channel; nonblocking
  SendMsgAndGet mode 2 and named-script KeyboardFocus preserve existing modes
  and legacy input routing. Eleven launcher and fifteen desktop policies plus
  actual XTest/clipboard/click gates pass on Linux, FreeBSD and OpenBSD, both
  AMD64 and ARM64. All eight palettes have actual native launcher captures.
  See [launcher verification](verification/LAUNCHER-VERIFICATION-2026-10-04.md).

- 2026-10-04: module-input/descriptor batch verified: Linux 55/55 on all three
  compiler/platform legs, BSD 44/44 on all four targets, plus each default X
  server. Real widgets retain text after invalid replies and recover afterward.
  Linux executable checks verify PIE, RELRO, immediate binding and stack flags;
  source fingerprints and initial failing controls are retained in the report.
- 2026-10-04: SCRIPT-01 file/message maintenance implemented; focused native and
  actual-module checks pass on all four BSD targets and all Linux compiler
  legs, with matching final source fingerprints. The strict output-file rule
  was subsequently approved by the user. No full audit
  or native Linux sanitizer result is claimed.

- 2026-10-04: user chose secure defaults and approved WriteToFile's strict
  output-file checks. Owned, single-link regular files remain supported; scripts
  writing through symlinks/hard links must use an appropriate ordinary file.
  The tested implementation is finalized without changing its runtime code.
  In-place updates are not crash-atomic; storage failures can leave partial
  content. Launch/output/evaluation repairs are recorded in the subsequent entry.

- 2026-10-04: Slate native window controls complete. Commented configuration
  adds reversible halves/quarters, directional focus, monitor transfer,
  center/grow, on-demand tiling/cascading, overview and inspection. Optional
  application rules and a State-31 scratchpad recipe are installed as examples.
  Twenty-six policy checks and native controls pass on all six platform targets;
  both Linux architectures also pass the full desktop regression and two-output
  Xorg controls. The new data files pass staged installation. See
  [window controls verification](verification/WINDOW-CONTROLS-VERIFICATION-2026-10-04.md)
  for the exact snapshots, scope and evidence.

- 2026-10-04: SCRIPT-01 command/startup/output batch verified. Do evaluates each
  argument once; bounded launch assembly supports long filenames and reserves
  child identifiers before ownership appears. Init can launch communicating
  children; startup argument storage retains module name plus 19 user arguments.
  GetOutput uses terminated bounded reads, dynamic extraction and an owned cache
  key, preserving existing `0 0` usage. Linux full gates pass 58/58; four BSD gates
  pass 47/47, plus 18 widget checks on each default server. Final whitespace-only
  formatting has focused follow-up coverage on all targets; 61 fingerprints
  match. Remaining interpreter work and unrun native sanitizer CI are explicit
  in [the report](verification/VERIFICATION-2026-10-04-SCRIPT-INTERPRETER.md).

- 2026-10-04: Slate typography, density and secondary-surface polish complete.
  Comfortable remains default; persistent Compact spacing is selectable through
  Appearance and launcher actions. Commented shared tokens, bold headings,
  pressed/restore states, readable semantic feedback colors and a two-by-two
  pager are installed and documented. Twenty-eight policy checks and native
  window/density checks pass on all six targets. Both Linux architectures pass
  the full desktop and two-output Xorg regression. The optional AMD64 Picom
  preview was subsequently found unstable: XRender artifacts and a late abort
  were missed by the initial probe. Compositor liveness is now checked. See
  [theme polish verification](verification/THEME-POLISH-VERIFICATION-2026-10-04.md).

- 2026-10-04: user chose a five-second default GetOutput deadline, with a
  configurable explicit override for slow theme commands. The deadline covers
  output collection and command completion; timeout output is discarded.
- 2026-10-04: native module startup, widget lookup and parser boundary checks
  implemented, with the approved GetOutput deadline and isolated process-group
  cleanup. Exact boundaries, injected descriptor failures, actual module exec
  closure and timeout recovery pass Linux 62/62 and BSD 51/51 full gates,
  staged installs and 22 actual-widget checks on each BSD default server.
  Seventy-nine source fingerprints match every final target. Results and
  retained initial fixture failures are recorded in
  [the follow-up report](verification/VERIFICATION-2026-10-04-SCRIPT-BOUNDS.md). No native
  sanitizer result or whole-interpreter safety claim is made.

- 2026-10-04: SCRIPT-01 numeric/string repairs implemented: checked arithmetic,
  compatibility-preserving prefix conversion, owned hexadecimal padding,
  full length-framed Parse validation and safe substring extraction. For
  excludes its body pointer from numeric decoding, completes extreme inclusive
  bounds, and tolerates replacement of its variable by the body. Set/Key use
  checked byte append storage without a new ordinary-string cap.
- 2026-10-04: verification exposed frozen-overlay timestamp and executable-mode
  defects, plus Form's stale-output completion assertion. Final runners
  reconfigure, rebuild all overlaid sources, preserve source modes, and wait
  for fresh exact Form output. Historical Form assertions are superseded by
  [the numeric follow-up](verification/VERIFICATION-2026-10-04-SCRIPT-VALUES.md).
- 2026-10-04: user approved ExecutionLimit 10000 work steps per top-level
  handler after discussion of the unmeasured 100000-step proposal, with a
  separate default RecursionLimit of 64. Configurable overrides, shared nested
  budgets, handler-only stop/recovery and retained completed side effects are
  implemented and verified. No wall-time or allocation bound is implied.


- 2026-10-04: final numeric/string gates pass Linux 63/63 (ARM64 GCC/Clang,
  AMD64 GCC), BSD 52/52 on four targets, staged installs, and 24 widget checks
  on every default BSD X server. All final archives match 82 source fingerprints
  and 1134 permission entries; complete overlay rebuild and corrected Form
  completion checks supersede earlier stale-object/output assertions. Native
  Linux sanitizer CI remains unrun. See the numeric follow-up report.

- 2026-10-04: Slate Appearance and live desktop review complete. Four native
  palette cards, saved first-frame selection, day/night and spacing/effect
  controls use FvwmButtons and the existing helper. Twelve original symbols
  yield 200 bundled PNG assets with text fallback and no new runtime library.
  Thirty-one policy checks and actual Appearance/density/window controls pass
  Linux, FreeBSD and OpenBSD on both architectures; both Linux full desktop and
  two-output gates also pass. Final source/asset fingerprints agree and staged
  installs contain the panel and all assets. Picom 12.5 legacy XRender aborts
  on native ARM64 as well as emulated AMD64. Modern XRender passes final
  liveness and supplies current pixels to the user-controlled noVNC review;
  headless GLX overlay capture can retain stale pixels. The optional profile
  uses modern XRender; no compositor is added to the default. The review remains
  live on host localhost:6080. See [the Appearance report](verification/SLATE-APPEARANCE-VERIFICATION-2026-10-04.md)
  for initial input-test failures, component readiness, capture limits and scope.

- 2026-10-04: execution/nesting follow-up passes full Linux 65/65 and BSD 54/54
  gates, fresh installs, seven real-handler outcomes and 24 existing widget
  outcomes on every default BSD server. All seven final archives match 86
  source fingerprints and 1138 permission entries. The
  [execution report](verification/VERIFICATION-2026-10-04-SCRIPT-EXECUTION.md) retains initial
  fixture failures and explains installation metadata comparison.
- 2026-10-05: user approved the [value storage defaults](decisions/SCRIPT-VALUE-LIMITS-PROPOSAL.md):
  1 MiB per interpreted string and 32 MiB of stored constant/variable text,
  including names and terminators, with documented overrides. Implementation
  and final gates pass Linux 67/67 and BSD 56/56. Six storage, seven execution
  and 24 widget outcomes pass on every default BSD X server. All seven archives
  match 92 source fingerprints and 1142 permission entries; fresh installs pass.
  Refusal preserves prior values/accounting and later events can recover. This
  is not a whole-process cap. See the [storage report](verification/VERIFICATION-2026-10-05-SCRIPT-STORAGE.md).
- Next SCRIPT-01 target: widget calculations. Source review finds unchecked
  endpoint subtraction/multiplication, equal-endpoint division in progress
  widgets, unchecked fallback endpoint addition and remaining atoi mutations.
  Reuse checked prefix-compatible parsing and explicit arithmetic policy; retain
  normal theme results. Legacy FIFO and large colorset policies remain open.

- 2026-10-04: expanded Slate's native Session menu with capability-aware
  sleep/hibernate/power controls, desktop maintenance, session information and
  shortcut help. Added action-specific confirmations with Return/Escape/pointer
  cancellation, bounded read-only service discovery and activation rechecks.
  No new desktop dependencies or core C changes. All six target platforms pass
  45 policy checks and the targeted real-X menu/forms gate; power cycles and
  authentication are explicitly outside the exercised scope. The existing live
  desktop has the new menu. See
  [session menu verification](verification/SLATE-SESSION-MENU-VERIFICATION-2026-10-04.md).

- 2026-10-05: replaced Slate Session's cramped rows with a commented native
  FvwmButtons panel: generous margins/gutters, large original icon buttons,
  separate capability captions, live palette/availability updates, guarded
  Alt-letter input and a padded small-screen menu fallback. Existing safe
  confirmations and desktop tools remain available. No new dependency or core
  C change. All six targets pass 48 policy checks and targeted native-X Session
  coverage; the full ARM64 Linux desktop gate passes. AMD64 full retries failed
  in existing Appearance pointer checks; broader AMD64 status remains unverified.
  Actual power/authentication requests were not accepted. Native light/dark
  captures and live desktop review are ready. See
  [Session panel verification](verification/SLATE-SESSION-PANEL-VERIFICATION-2026-10-05.md).

- 2026-10-05: user approved clamping indicator/thumb rendering while retaining
  exact stored values. Equal/inverted runtime ranges render empty/stationary;
  representable legacy startup corrections remain. Widget arithmetic follow-up
  is in progress, including signed-literal tag separation, checked mutations,
  MiniScroll wrap and scrollbar Return/release descriptor waits. Source review
  also finds MiniScroll busy-wait repeat delays and broader geometry/font
  arithmetic; those are not resolved by this batch.

- 2026-10-05: approved progress/scrollbar rendering and arithmetic follow-up is
  verified. Shared math preserves ordinary pointer rounding and stored values;
  private numeric tags carry the full signed range without overlapping function
  IDs. Checked mutation parsing, safe MiniScroll wrap, tiny fills and scrollbar
  Return/release waits are implemented. Full Linux 69/69 and BSD 58/58 gates pass,
  with eleven final widget outcomes on every default BSD X server. The final
  fixture-only assertions are repeated with verified unchanged production code
  and binary hashes; full-suite logs and original fixture artifacts are retained.
  All seven archives match 101 fingerprints and 1146 modes; fresh installs pass.
  Previous-widget controls distinguish the AMD64 equal-range exit from ARM64's
  continued instruction and failing endpoint pixels. See the
  [widget report](verification/VERIFICATION-2026-10-05-SCRIPT-WIDGETS.md).
- Next SCRIPT-01 targets: remaining text/list widget indices and geometry/font
  arithmetic, MiniScroll busy-wait repeat delays and modal module servicing;
  then legacy FIFO processing and explicit large-colorset policy. Existing
  target/resource/FD/subprocess lifetime and native sanitizer items remain open.

- 2026-10-05: replaced MiniScroll and shared List/double-click busy waits with
  sleeping delays, preserving existing timing and callback order. Whole-delay
  monotonic accounting fixes the interruption assertion exposed on both FreeBSD
  architectures. Full final Linux 70/70 and BSD 59/59 gates pass; twelve widget
  outcomes pass on every default BSD X server. All seven archives match 105
  fingerprints/1149 modes and fresh staged code. The isolated prior-delay
  comparison records 0.26 CPU seconds for a short hold; final Linux ARM64 records
  zero scheduler CPU ticks. Failed initial checks/control construction and final
  correction provenance are retained. See the
  [delay report](verification/VERIFICATION-2026-10-05-SCRIPT-DELAYS.md).
- Next compatibility decision: cancel MiniScroll's pending repeat immediately
  on release, potentially removing one SingleClic callback. Keep the first
  action and held-repeat cadence. Remaining widget and modal-service work stays
  open; ordinary event processing during holds is not implemented by this pass.

- 2026-10-05: user approved immediate MiniScroll release cancellation while
  retaining the first action and held-repeat cadence. Implementing shared
  cancellation-aware deadline waits, queued-release checks and held termination
  coverage. Focused delay policy checks pass; the first actual-X termination
  observer queried a disappearing window and is corrected to DestroyNotify.
  Initial failed artifacts are retained; final matrix verification is in progress.
- 2026-10-05: approved MiniScroll release cancellation completed and verified.
  All final Linux 70/70 and BSD 59/59 gates pass; sixteen widget outcomes pass
  under nested X and every default BSD X server. Fresh stages/source modes and
  current binary hashes match the final frozen source. A separate previous
  handler produces the fourth callback after release at three; production
  hashes remain unchanged. Initial BadWindow termination-observer failure is
  retained and corrected to DestroyNotify. See the
  [release report](verification/VERIFICATION-2026-10-05-SCRIPT-RELEASE.md).
- Next compatibility decision: use sparse colorset storage to preserve large
  theme IDs, with a finite count of distinct sets. Recommend 4096 per process,
  including default zero, and a shared build-time override up to 65536. Source
  review confirms dense growth and core X/resource initialization through every
  preceding index. The manual explicitly permits large IDs. The
  [proposal](decisions/COLORSET-STORAGE-PROPOSAL.md) bounds actual count instead of IDs;
  user approved 4096 distinct sets and the proposed override. Migration is
  starting after the release pass closes.
- 2026-10-05: sparse colorset/count candidate implemented after the approved
  4096 decision. Core and bundled modules use checked sparse storage, stable
  record addresses and ordered allocated-ID broadcasts. Native Linux ARM64 GCC
  focused checks pass 7/7, including six actual-X palette/capacity/theme cases.
  Pure storage policy passes Linux GCC/Clang and all four BSD architecture
  targets. Missing declarations and observer/saturation/source-isolation
  fixture errors are corrected; failed evidence is retained. Full candidate
  matrix and native Linux sanitizers remain unrun. See the
  [candidate report](verification/VERIFICATION-2026-10-05-COLORSET-CANDIDATE.md).
- Compatibility decision approved (2026-10-05): a real-X previous-versus-sparse comparison
  demonstrates that an undefined module ID below a higher defined ID changes
  from implicit gray to a copy of set zero. User approved the consistent zero
  fallback for simplicity; themes depending on an implicit gap would define
  that set explicitly. This appearance change is separate from the approved
  count limit; both decisions are now approved. Final full gates are in progress.

- 2026-10-05: follow-up legacy FIFO source review identifies blocking operations
  outside the existing counted waits, incorrect descriptor/error progress and
  missing reply termination at the 32000-byte boundary. The concrete
  [FIFO proposal](decisions/SCRIPT-FIFO-PROPOSAL.md) retains the helper protocol with
  validated endpoints, bounded transport and a configurable overall deadline.
  Default deadline decision is requested before implementation; colorset gates
  continue independently.

- 2026-10-05: sparse colorset migration completed with all seven final local
  Linux/BSD test/install gates (73 Linux / 62 BSD) and four default BSD servers.
  Seven palette outcomes include the user-approved zero clone; prior module
  outcomes remain passing. Actual configured limits 1/65536 pass policy/API
  tests and out-of-range settings are rejected. All final archives match the
  frozen source/mode manifests and fresh installations. Initial observer
  failure and corrected source delta are retained. Isolated snapshot guests
  are stopped; concurrent desktop guests and Linux containers are preserved.
  [Final report](verification/VERIFICATION-2026-10-05-COLORSETS.md). Next implementation is
  legacy FIFO processing, pending its explicit default-deadline decision.

- 2026-10-05: user approved the five-second SendMsgAndGet overall deadline with
  a finite configurable override. Checked nonblocking FIFO transport, owned
  endpoint admission, atomic mode-2 sends and terminated bounded replies are
  implemented. All seven final local gates pass (75 Linux / 64 BSD), including
  ten FIFO outcomes and prior module regressions on every default BSD X server.
  Source/mode manifests and fresh installations match the tested binaries.
  Initial helper-barrier, stale-binary and platform-specific signal-fixture
  failures are preserved and explained in the [FIFO report](verification/VERIFICATION-2026-10-05-SCRIPT-FIFO.md).
- Next SCRIPT-01 focus: TextField/List edits and indices, repeated text scans,
  allocation ownership and geometry/font arithmetic, before broader modal and
  resource lifetime work. Source review identifies direct title reallocations
  in TextField/InsertText and unchecked list geometry calculations; these have
  not been fixed or claimed safe by the FIFO pass. Native Linux sanitizer CI
  remains configured but unrun.

- 2026-10-05: backlog ordering reviewed against security, responsiveness,
  simplicity and FVWM compatibility. Completed checkpoints are separated;
  verification/image/lifetime work is explicit in the active queue. The FIFO
  checkpoint supersedes its stale pending handoff. Maintenance migration is
  P2; CI verification remains P1 with publishing ownership separate. The
  execution table governs the detailed roadmap; no runtime status changed.

- 2026-10-05: user questioned placement of the other thread's tool ports and
  cleanup. The prior blanket MAINT-02 downgrade is corrected: incremental
  candidate verification/adoption is P1 and starts after the next runtime
  checkpoint. Safe cleanup and utility fixtures can start early; parser/public
  API changes and predecessor removal retain their specific gates. No tool
  entry point or runtime code is changed by this scheduling correction.

- 2026-10-05: MAINT-02 adopts the documented build interface and shared manual
  list/extractor after real FVWM and Asciidoctor comparisons. The feature
  matrix repaired BSD ports gettext discovery and made minimal-profile checks
  reflect available features while retaining bounded ICCCM-name coverage.
  The tooling report owns final target evidence and qualification gaps. The
  flat website candidate is implemented/rendered; changing the public
  helper's overwrite contract requires the explicit compatibility decision.
  Other tool ports, metadata/protocol fixes and runtime cleanup remain open.

- 2026-10-05: build/manual adoption checkpoint finalized: all six recorded
  Linux/BSD profile matrices have identical old/new options and installed
  files; 25 tooling checks pass per target, with real manual comparisons and
  automatic manifest reconfiguration. Default BSD X passes 40 FIFO outcomes.
  Six evidence archives match 1,169 source hashes/modes; isolated snapshots
  are released. Linux AMD64 Go emulation/native CI/release limitations remain
  explicit. MAINT-02 stays partial pending the public website decision and
  remaining separately gated migrations.

- 2026-10-05: user deferred public website migration in favor of a complete
  static HTML5/CSS-only redesign and new AI-assisted distribution manuals
  using ASD-STE100. DOCS-01 records the brief for later requests. The old
  output proposal is retained without approval; its decision no longer blocks
  hardening, responsiveness or remaining tool adoption.

- 2026-10-05: TextField/List repairs are verified. Checked bounded locale
  walks and atomic title replacement remove unchecked text movement; selection
  ownership is explicit. Lists use wide viewport/thumb arithmetic, one initial
  option scan followed by sequential rows, omit invalid narrow geometry,
  allocate only exposed rows and release loaded fonts. Previous and candidate
  Linux ARM64 modules pass five ordinary behavior outcomes each. Full gates
  pass Linux ARM64 GCC/Clang and AMD64 GCC 77/77, and all four BSD targets
  66/66. Each staged default BSD server passes eight text/list and sixteen prior
  widget outcomes. Six archives match all 1173 source hashes/modes, and staged
  binaries match the exact expected Meson install transformation. Isolated
  snapshot guests are released. See the
  [text/list report](verification/VERIFICATION-2026-10-05-SCRIPT-TEXT.md).
- Next compatibility choice: extend the existing configurable ValueLimit
  (1 MiB default) to interactive TextField titles. The approved interpreted
  storage scope excludes widgets; this new bound is not enabled. A specific
  user decision has been requested. The
  [proposal](decisions/SCRIPT-TEXT-LIMIT-PROPOSAL.md) specifies whole-edit refusal with
  unchanged text/selection and continued shrinking. Broader widget memory,
  font-width complexity, Unicode and modal/descriptor work remain open.

- 2026-10-05: user approved the interactive ValueLimit extension. Shared checked
  editing now covers typing and both paste paths; refusal retains text, cursor,
  selection and viewport and sends no successful callback. Legacy ordinary
  typing callback order remains unchanged. Native default/override boundary
  checks and Linux real-module checks pass; full gates are running. Actual
  six-target package inventories have also been captured for LIB-02/LIB-07.

- 2026-10-05: interactive ValueLimit final gates pass 495 suite executions and
  128 installed default-BSD-X outcomes. Six archives match all 1173 hashes/modes
  and exact Meson-transformed install bytes. See the
  [limit report](verification/VERIFICATION-2026-10-05-SCRIPT-TEXT-LIMIT.md). Next short package
  review has captured actual packages and available vendor fixes; an ordinary
  truncated PNG reproduces an abort in the unmodified shared loader. A scoped
  error-recovery/output-reset fix passes its Linux real-loader checks and is
  undergoing full gates under IMAGE-01/DATA-01. Snapshot guests remain owned by
  this continuation; no original guest or host package was modified.

- 2026-10-05: PNG error recovery and output reset are verified: seven full
  legs pass 495 executions; default BSD servers pass 188 installed outcomes,
  and three staged Linux cores pass 45 image checks. PNG-disabled Linux ARM64
  passes 45/45. Six archives match 1173 hashes/modes and expected install bytes.
  See the [PNG report](verification/VERIFICATION-2026-10-05-PNG-RECOVERY.md). Signed OpenBSD
  snapshot updates and post-reboot qualification are underway; PNG package
  security qualification is separate from this recovery repair.

- 2026-10-05: dependency triage records exact six-target packages and vendor
  backports. All 30 signed OpenBSD snapshot updates were applied and both
  guests rebooted; ARM64 full/default-X revalidation passes, AMD64 runs next.
  The unapplied PNG security guard passes six isolated Meson cases and awaits
  explicit approval because it would refuse older unverified default packages.
  See the [audit](DEPENDENCY-AUDIT-2026-10-05.md) and
  [proposal](decisions/PNG-SECURITY-BASELINE-PROPOSAL.md).

- PNG follow-up source review identified callback registration before the
  application's jump destination was armed during construction. Registration
  now follows initialization, and native injected read/info allocation failures
  plus an error at callback installation pass on Linux ARM64. The new frozen
  source adds only this setup repair and native regression, excluding concurrent
  Meson/desktop work. Final follow-up gates are running before closure.

- 2026-10-05: the user approved requiring libpng 1.6.59 by default with an explicit verified-vendor-backport assertion. Production Meson and installation documentation apply that decision. Hash-verified private dependency profiles are being prepared; old unverified packages will not receive the assertion.

- 2026-10-05: PNG initialization follow-up is accepted: 502 full-suite executions, 188 BSD default-X outcomes and 45 staged Linux core checks; six archives match all 1174 source hashes/modes and staged install bytes. Both signed OpenBSD refresh profiles also pass (132 full tests, 94 default-X outcomes). Patched PNG policy qualification now runs as a separate frozen checkpoint.

- 2026-10-05: the approved patched PNG default is accepted on six platforms and the Linux Clang leg: 502 full tests, 188 BSD default-X outcomes and 45 staged Linux core checks. Four private upstream builds pass 36/36 each; Debian uses authenticated fixed PNG/XPM backports and OpenBSD AMD64 its existing 1.6.59 package. Exact loaded paths and all 1174 source hashes/modes are checked. See [baseline report](verification/VERIFICATION-2026-10-05-PNG-BASELINE.md). Wrapper/profile cleanup is under separate focused qualification.

- 2026-10-05: PNG wrapper/profile cleanup accepted: 499-line header reduced to 71 lines; all modules compile on seven legs, four focused entries pass per leg, installed default/nested X checks pass and PNG-disabled tests pass 46/46. Six archives match 1175 source hashes/modes. [Cleanup report](verification/VERIFICATION-2026-10-05-PNG-CLEANUP.md). Signed OpenBSD XPM source/release-set comparisons confirm its vendor fix on both architectures without a version-string assumption.

- 2026-10-05: subsequent IMAGE-01 acquisition review reproduces blocking image opening with an ordinary task-owned empty FIFO on the patched staged Linux ARM64 core. A two-second command-barrier timeout and `wait_for_partner` confirm the blocked open; bounded fixture cleanup completed. The [image input proposal](decisions/IMAGE-INPUT-PROPOSAL.md) records the regular-file / 128 MiB source-limit compatibility decision, subsequently approved by the user. Earlier PNG baseline/cleanup gates remain accepted.

- Checkpoint cleanup: all four owned snapshot guests were shut down, only QEMU processes matching their saved exact argv were released, and exit was verified. Accepted source/dependency/runtime evidence is archived. Original guests and concurrent desktop work remain outside this task. The image-source contract is now approved; fresh decoder verification must restore the signed update/private dependency profiles.

- 2026-10-05: the user approved regular-file image inputs, the default 128 MiB
  source cap and a build-time override. The shared immutable reader is implemented
  and passes seven Meson compiler legs on all six targets, plus scoped local
  Linux ARM64 ASan/UBSan. All 1178 frozen source fingerprints match. It is not
  connected to production decoders yet; the original FIFO finding remains open.
  BSD socket errno fixture failures were corrected and retained. Adoption needs
  checked decoder/fallback handoffs and real-X/default-server gates. The separate
  [expanded-XPM/decompression defaults](decisions/IMAGE-COMPRESSED-INPUT-PROPOSAL.md)
  were also approved: 128 MiB expanded XPM, five seconds and finite overrides. Decoder integration is proceeding. [Reader evidence](verification/VERIFICATION-2026-10-05-IMAGE-INPUT-READER.md).

- 2026-10-05: checked primary-input integration now covers PNG, XPM, SVG,
  XBM and native/image cursors. Approved expanded-XPM/decompression defaults
  are 128 MiB and five seconds with finite Meson overrides. Initial gates found
  a BSD SIGCHLD notification issue (fixed) and an existing Xcursor-disabled
  mask cleanup error (fixed). Wide XBM file parsing remains compatible with
  Xlib; server pixmap limits are independent. Final source, full-suite,
  installed default-X and scoped local sanitizer qualification is in progress.

- 2026-10-05: primary-image/compressed-XPM production integration accepted:
  seven full compiler legs pass 537 executions, staged installed Linux passes
  192 image outcomes, and all four default BSD X servers pass 232 image plus
  40 FIFO outcomes. Optional profiles pass 12 focused entries; scoped local
  Clang ASan/UBSan with leak detection passes 4/4. All six archives match 1186
  source hashes/modes and all 98 installed native binaries match expected Meson
  output. Existing cursor ownership, opaque-mask cleanup, XPM storage and
  unsigned color packing are repaired. The final staged SVG secondary-FIFO wait
  remains reproduced and has a separately approved policy proposal. Native release
  CI and the unexplained intermediate FreeBSD interaction timing observation
  remain under VERIFY-01. [Image checkpoint](verification/VERIFICATION-2026-10-05-IMAGE-INPUT.md).

- 2026-10-05: the user approved the SVG worker policy: five-second whole-operation deadline, 1024 MiB worker memory limit and finite overrides. Begin its bounded implementation/qualification batch; primary-image/XPM acceptance remains archived. All four prior task-owned snapshots have exited after evidence archival; original guests were preserved.

- 2026-10-06: user approved the [Script geometry policy](decisions/SCRIPT-GEOMETRY-PROPOSAL.md): reject unrepresentable X11 sizes/coordinates, preserve previous runtime state, retain existing zero/omitted widget startup sizing. Implement after the current SVG qualification checkpoint.

- 2026-10-06: SVG isolation accepted with final 572 full-suite executions, 1199 hashes/modes, 105 installed C identities and all staged Linux/default-BSD-X outcomes. A new full-system Linux AMD64 QEMU profile verifies actual finite memory enforcement; a private init-backed ARM64 container avoids shared-fixture zombie exhaustion. PNG-signature fallback avoids useless worker starts, and the inherited Style * token leak is fixed. Parent synchronous waiting remains P1 responsiveness work. Continue the approved Script geometry contract.

- 2026-10-06: Script geometry and shared font/layout preparation are accepted: 586 full-suite executions, 1205 source hashes/modes, 105 staged C identities, 63 installed geometry outcomes, 224 prior widget/text-list outcomes, seven font pixel comparisons and three scoped local sanitizer checks. All four default BSD X drivers pass. Native Linux release sanitizer CI remains unrun. The next proposed font-shadow work default requires a compatibility decision; its bounded review and consumer ownership gates are recorded separately.

- 2026-10-06: shared font-shadow work accepted after the user approved 4096 positions with adjustability to 65536. Seven compiler legs pass 600 suite executions; 1209 hashes/modes and 105 staged identities match. Installed Linux/default BSD X passes 77 shared consumer, 63 geometry, 224 previous widget/text-list and 21 core font state outcomes. Five scoped local sanitizer and six limit-override checks pass. The next concrete decision is asynchronous core SVG completion; prior/absent-image behavior and owner-safe single-threaded commits are specified in its proposal. Native Linux release CI remains unrun.

- 2026-10-06: the user approved asynchronous core SVG completion, preserving previous usable images and existing absent-image first-load rendering. Start the owned nonblocking transport and audit colorset, decor, menu, icon and cursor owners before adoption. Decoder deadlines/memory bounds remain; no recursive command dispatch or new mandatory libraries.

- 2026-10-06: implemented dormant core image readiness/deadlines, nested and
  interactive transport-only servicing, owned X filters and parent-only shutdown.
  Added absent-picture rendering guards and immutable WindowList mini-icon
  snapshots. An ordinary opaque image confirmed a colorset shape duplicate
  release under ASan; cleanup now releases exactly one acquired reference.
  Eight distinct focused Linux entries, four scoped sanitizer entries and an
  early OpenBSD ARM64 readiness unit pass. Related owner adoption continues;
  production SVG is still synchronous and the extensive matrix stays deferred
  to combined activation. [Focused report](verification/VERIFICATION-2026-10-06-SVG-CORE-ADAPTER.md).

- 2026-10-06: continued the same focused batch with mini-icon preparation before
  old-reference release. Pending replacements retain an immutable backing and
  are not broadcast early. Real module packets confirm three unchanged-style
  reloads keep the same pixmap; normal and scoped sanitizer owner fixtures pass.
  This does not activate asynchronous loading or complete icon callback routing.

- 2026-10-06: the user approved the simpler deferred cursor bitmap fallback. Colorset shape copies were replaced with retained immutable backing; shared masks and delayed cleanup pass 5 focused normal and 5 scoped sanitizer checks. The mixed Xcursor-enabled/XRender-disabled profile passes 2 cursor entries. Actual 24/8-bit Average controls show white for opaque red PNG/blue deferred images and blue for a masked blue image; the [bounded source-RGB proposal](decisions/COLORSET-AVERAGE-PROPOSAL.md) was approved by the user; implementation is in progress. Production activation and the combined matrix remain pending.

- 2026-10-06: approved decoded-RGB image averages are implemented for ordinary and deferred file-backed color pictures. Shared immutable summaries remove file-image Average readback/color-query loops and pass 16 focused normal and 16 scoped local sanitizer checks; [exact checkpoint](verification/VERIFICATION-2026-10-06-COLORSET-AVERAGE.md). Derived tint/background acknowledgement and combined qualification remain pending.

- 2026-10-06: colorset extent storage was corrected after a valid 4096×2 image reproduced a zero-width packet. The [geometry/effect checkpoint](verification/VERIFICATION-2026-10-06-COLORSET-GEOMETRY.md) passes 5 normal/5 scoped sanitizer/5 software-profile checks. A subsequent numeric-wrap regression is fixed by checked parsing before allocation/mutation; [packet checkpoint](verification/VERIFICATION-2026-10-06-COLORSET-PACKET.md) passes 3/3/3 focused checks. Independent tint/alpha controls show backend/palette differences; the [bounded effect preparation proposal](decisions/COLORSET-EFFECTS-PROPOSAL.md) preserves them and is implemented with 62 focused Linux/BSD/optional/sanitizer executions passing. [Effects report](verification/VERIFICATION-2026-10-06-COLORSET-EFFECTS.md). Combined qualification and activation remain pending.

- 2026-10-06: approved colorset effect preparation is implemented in the dormant
  core adapter. The one active worker supports bounded server-RGB16 software
  effects even without SVG; hardware and simple copies use private acknowledged
  resources. Exact source/color references and cancelled-batch revision contexts
  remain valid through actual child/binding retirement. Atomic publication,
  refusal/recovery, first-load absence, cached/shared sources, input/timers and
  Quit pass 62 focused executions across normal/software/SVG-disabled/scoped
  sanitizer Linux and early native FreeBSD/OpenBSD ARM64. [Exact checkpoint](verification/VERIFICATION-2026-10-06-COLORSET-EFFECTS.md).
  Production activation and the combined six-target/install/default-X gates
  remain pending; the extensive matrix was not repeated per story.

- 2026-10-06: normal core startup now enables the image scheduler before defaults and user configuration; unbound owners refuse publication with a diagnostic. A shared colorset predicate preserves depth-one bitmap rendering. Eleven focused Linux checks pass, including normal startup/restart at 24/8 bits and all affected file/menu/decor/colorset owner regressions. The combined 1261-file image activation matrix is running; qualification remains pending. [Activation report](verification/VERIFICATION-2026-10-06-IMAGE-ACTIVATION.md).

- 2026-10-06: the next INPUT-01 baseline confirms that Escape with Mouse 1 still held stalls scheduled safe timer/module Echo until release, while the enclosing function correctly remains gated. A 0.603-second local observation consumed 0.08 WM CPU seconds. The [button-release proposal](decisions/BUTTON-RELEASE-SERVICE-PROPOSAL.md) reuses the existing audited pump and defines 20-ms pointer fallback/wait-cursor timing; the user approved bounded servicing and 20-ms timing. Implementation follows the image checkpoint. No production change is included in the separately frozen image checkpoint.

- 2026-10-06: combined asynchronous image activation is accepted locally: eight compiler legs / 908 qualified entries, 120 staged identities, 52 default-BSD-X driver invocations, 118/101 optional entries and 30 unique scoped sanitizer checks. All 1261 source hashes/modes match. Fixture-only repairs retain failed-run provenance and unchanged production sources. Native release CI and physical coverage remain separate. Approved INPUT-01 common release servicing is implemented and entering focused checks. [Activation evidence](verification/VERIFICATION-2026-10-06-IMAGE-ACTIVATION.md).

- 2026-10-06: approved common button-release batch implemented: checked physical masks, 20-ms monotonic timing, finite shared readiness and bounded event servicing. Focused policy/original interaction/Escape-to-release/common-caller checks pass, including deferred image ownership and shutdown. The 1262-file combined checkpoint is running; qualification remains pending. [Report](verification/VERIFICATION-2026-10-06-BUTTON-RELEASE.md).

- 2026-10-06: a separate read-only INPUT-01 baseline confirms active interactive Scroll stalls a scheduled safe timer and independent module Echo until Escape, while the newly implemented common final release wait services them and retains physical-release continuation order. Bounded active viewport service is proposed in the [viewport decision](decisions/VIEWPORT-INTERACTION-SERVICE-PROPOSAL.md); no production change is included in current qualification.

- 2026-10-06: the user approved bounded audited service during active interactive Scroll, 100-ms finite readiness, 32-event motion coalescing and termination cancellation. This subsequent INPUT-01 batch is separate from the frozen common-release qualification. [Viewport contract](decisions/VIEWPORT-INTERACTION-SERVICE-PROPOSAL.md).

- 2026-10-06: approved active viewport batch implemented: finite shared readiness/cancellation, bounded motion coalescing, audited service and checked button masks shared with Move/Resize. Eight focused Linux entries pass, including real viewport coordinates, keyboard/physical release, deferred actual RandR topology and client mapping, producer/timer barriers and image transport/deadline/shutdown. The separate 1263-file checkpoint is frozen; full qualification pending. [Report](verification/VERIFICATION-2026-10-06-VIEWPORT-SERVICE.md).

- 2026-10-06: read-only STACK-02 reproduction now verifies parent/child/grandchild actual layer values through command expansion and real root stack order. All three change to 6/2/4 with stable relative order; children retain stale ABOVE/BELOW properties because the affected-window loop updates only the outer parent. Baseline source/evidence is under `builddir/codex-2026-10-06/viewport-service/`; production stacking remains unchanged and outside the frozen INPUT-01 checkpoint.

- 2026-10-06: common button-release checkpoint locally accepted: 924 entries across eight compiler profiles, 120 staged code identities, 20 installed default-BSD-X driver invocations, optional 120/102 entries and five scoped Clang ASan/UBSan entries. Seven evidence archives validate exact 1262 source hashes/modes and archive integrity. The OpenBSD ARM64 missing private-runtime-path attempt is preserved and its unchanged-source rerun passes. [Report](verification/VERIFICATION-2026-10-06-BUTTON-RELEASE.md).

- 2026-10-06: the next read-only INPUT-01 baseline confirms active Popup stalls safe timers and independent module commands until Escape; common final release servicing then works. A bounded active-menu proposal includes finite readiness, 32-event motion fairness and monotonic submenu ticks so descriptor wakeups cannot accelerate configured delays. Explicit timing/active-owner decision pending; no menu production change is in the frozen viewport checkpoint. [Proposal](decisions/MENU-INTERACTION-SERVICE-PROPOSAL.md).

- 2026-10-06: the user approved bounded active menu servicing, finite 100-ms readiness, 32-event motion batches and monotonic submenu timing preserving configured delays and physical-release continuation. Implementation belongs to the next INPUT-01 batch, separate from frozen viewport qualification. [Contract](decisions/MENU-INTERACTION-SERVICE-PROPOSAL.md).

- 2026-10-06: active viewport checkpoint locally accepted with exact frozen source and retained OpenBSD affected-rerun provenance. Approved active-menu contract now has eight focused policy/actual-X passes; independent frozen qualification is running. See the viewport report and menu decision.

- 2026-10-06: STACK-02 per-window publication and QUEUE-01 explicit FIFO transfer have focused actual-core/native passes. OUTPUT-01 finite asynchronous bounds were approved and implemented; controlled/real-pipe framing, continuation and death-row checks pass. One combined checkpoint remains pending; see [development report](verification/VERIFICATION-2026-10-06-FIFO-STACK-OUTPUT.md).

- 2026-10-06: independent DATA-01 child-list ownership repair passes focused Linux/FreeBSD/OpenBSD ARM64 and leak-enabled scoped sanitizers; next-batch qualification pending. A bounded actual-core baseline reproduces synchronous ModuleTimeout extension by harmless command activity; OUTPUT-02 proposes one monotonic total handshake deadline with the existing default/override and requires a compatibility decision. Current frozen FIFO/stack/output production scope is unchanged.

- 2026-10-06: QUEUE-01 / STACK-02 / OUTPUT-01 combined checkpoint locally accepted: 1008 entries across eight Linux/BSD compiler profiles, 120 staged binary identities, 24 default-BSD-X drivers, optional 131/111 entries and ten scoped sanitizer checks. All seven archives validate exact 1271 source hashes/modes and integrity. Initial BSD fixture-only SIGCHLD failures and all affected reruns are retained; production code is unchanged between initial/final snapshots. Native release CI remains unrun. Independent tree-query repair has focused coverage; OUTPUT-02’s total synchronous deadline requires the pending compatibility decision. [Report](verification/VERIFICATION-2026-10-06-FIFO-STACK-OUTPUT.md).

- 2026-10-07: OUTPUT-02 total synchronous deadline approved with the existing 30-second default and positive override. Implementation/focused qualification underway; the accepted FIFO/stack/asynchronous-output checkpoint remains historical evidence.

- 2026-10-07: OUTPUT-02/DATA-01 combined checkpoint accepted locally: 1024 entries/eight compiler profiles, 120 staged identities, 28 default-BSD-X driver invocations, optional 133/113 and twelve scoped sanitizer checks. Seven evidence archives validate exact 1274 source hashes/modes and complete payloads. Native release CI remains open. STACK-01 indexed lookup has separate focused coverage; OUTPUT-03 startup default awaits the explicit decision. [Report](verification/VERIFICATION-2026-10-07-MODULE-SYNC-TREE.md).

- 2026-10-07: user approved OUTPUT-03’s finite ModuleSynchronous startup default and asynchronous continuation contract. Implementation and focused verification are in progress for the combined STACK-01 checkpoint.

- 2026-10-07: user approved U+FFFD replacement of invalid UTF-8 bytes at JSON output, preserving valid UTF-8 and original FVWM/native bytes. LIB-01 implementation and combined qualification follow the startup/identity checkpoint.

- 2026-10-07: OUTPUT-03/STACK-01 indexed identity checkpoint accepted locally from the exact 1279-file snapshot: 1040 standard entries, 120 installed identities, 36 default BSD X invocations, optional 135/115 and fourteen scoped sanitizers. OpenBSD server/disk failure retained; affected rerun passed without a WM change. Native release CI remains open. [Report](verification/VERIFICATION-2026-10-07-STARTUP-IDENTITY.md).

- 2026-10-07: LIB-01 vendor/numeric/UTF-8/checked-print checkpoint accepted locally from the exact 1282-file snapshot: 1056 standard entries/eight profiles, 120 installed identities, 40 default BSD X invocations, optional 137/117 and sixteen scoped sanitizers. Original/native text stays unchanged; native release CI remains open. [Report](verification/VERIFICATION-2026-10-07-JSON-BOUNDARY.md).

- 2026-10-07: the user approved bounded synchronous expose delivery and 32-record queued snapshots. The corrected routing/merging and related first-icon WM_STATE publication have focused Linux pixel/core coverage and 21 scoped sanitizer passes. The exact 1291-file combined atom/expose/icon checkpoint is running; qualification remains pending. [Report](verification/VERIFICATION-2026-10-07-ATOM-EXPOSE.md).

- 2026-10-07: a separate finite STACK-01 control confirms Layer parent traversal repeats a two-window cycle until the fixture resets its own hints. A private constant-space detector preserves acyclic lookup counts and passes pure/actual-core checks. Production stacking is unchanged; [fallback proposal](decisions/TRANSIENT-PARENT-CYCLE-PROPOSAL.md) awaits the compatibility choice. Current atom/expose qualification remains independent.

- 2026-10-07: the atom/expose/icon checkpoint is locally accepted: eight compiler profiles/1096 standard entries, 120 installed identities, 52 default BSD X invocations, optional 142/122 and 21 scoped sanitizers. All seven evidence archives validate complete payloads and exact 1291 source hashes/modes. Native release CI remains separate. The subsequent Layer cycle fallback is a private candidate awaiting its decision, outside this qualified source. [Report](verification/VERIFICATION-2026-10-07-ATOM-EXPOSE.md).

- 2026-10-07: user approved STACK-01’s originally invoked window fallback for cyclic Layer parent chains. The reviewed operation-local guard is activated; focused acceptance is in progress, with full qualification deferred to the next coherent stacking/query batch.

- 2026-10-07: user approved invoked-window fallback for cyclic Raise/Lower/RestackTransients redirection. Four related stories are activated: Layer guard, finite hint-cycle rejection, highest-override query reduction and other redirection guards. Eight focused Linux checks pass. The 1301-file combined checkpoint is frozen and running; full qualification remains pending until evidence validation.

- 2026-10-07: STACK-01/XIO-01 parent/query checkpoint is locally qualified: eight profiles/1144 entries, 120 installed identities, 72 default BSD X invocations, optional 148/128 and 27 scoped sanitizers. Seven complete archives match 1301 hashes/modes. OpenBSD AMD64 endpoint-readiness failure and one affected unchanged rerun are retained; native release CI remains separate. [Report](verification/VERIFICATION-2026-10-07-STACK-PARENT-QUERY.md). Subsequent transient rank/layer arithmetic work is outside this snapshot.

- 2026-10-07: next STACK-01/DATA-01 arithmetic pass confirms signed transient-rank overflow in a finite 10,739-record actual-core control and absolute/relative Layer overflow in two real-window controls. Wider internal rank storage passes five focused Linux checks; valid maximum-layer placement is being repaired independently. Relative positive overflow needs an explicit retain-previous-state versus clamp choice. [Proposal](decisions/LAYER-ARITHMETIC-PROPOSAL.md). No next-batch code is covered by the 1301-file qualification.

- 2026-10-07: STACK-01/DATA-01 wider rank and valid maximum-layer insertion pass six focused Linux checks plus the same six scoped Clang ASan/UBSan checks from the exact 1304-file source. Complete archive fingerprints/compiled identities validate. Relative-overflow refusal has passing private real-window checks; production activation awaits the user choice. Full next-batch Linux/BSD/default-server qualification and accumulated rank/reset/traversal work remain open. [Focused report](verification/VERIFICATION-2026-10-07-STACK-ARITHMETIC.md).

- 2026-10-07: user approved relative Layer overflow refusal with previous layer and stack order retained. The private checked-addition candidate and its real-window regression are activated; maximum absolute layer and negative-to-zero behavior stay supported. Production focused checks and combined arithmetic qualification follow.

- 2026-10-07: four-story STACK-01/DATA-01 arithmetic batch is frozen at 1306 files after focused acceptance: wider rank, valid maximum-layer insertion, approved relative-overflow refusal and checked retained-rank sums. The repeated-rank control confirms widening alone was insufficient; shared arithmetic preserves representable values and stable endpoint ties. Combined eight-profile/default-BSD-X/install/optional/scoped-sanitizer qualification is running. Private stable-sort measurements are separate from the frozen source.

- 2026-10-07: STACK-01 stable ring sorting is implemented outside the frozen arithmetic checkpoint. Iterative stable merging preserves exact ties/links with no allocation or recursion and a linear sorted fast path. The 2048-record ascending comparison control falls from 4,190,209 to 11,265. Seven focused normal and seven scoped sanitizer checks, both private old/new 70-ring controls and early FreeBSD/OpenBSD ARM64 policy checks pass. Exact 1309-file/compiled/native evidence is retained; full qualification waits for the coherent traversal batch. [Report](verification/VERIFICATION-2026-10-07-STACK-SORT.md).

- 2026-10-07: STACK-01/DATA-01 arithmetic checkpoint is locally qualified: 1168 entries/eight profiles, 120 installed identities, 80 BSD default-X driver invocations, optional 151/131 and 30 scoped sanitizers. Seven complete evidence archives match 1306 source hashes/modes and payload EOF. No production change or affected rerun was needed in the combined gates. Native release CI remains separate. [Report](verification/VERIFICATION-2026-10-07-STACK-ARITHMETIC.md).

- 2026-10-07: three-story STACK-01/DEAD-01 batch is frozen at 1314 files: stable sort, parent-only worklist closure and seven proven inactive functions. Nine focused normal and nine scoped sanitizer checks pass; early BSD ARM64 pure checks pass. Reverse 2048 closure changes from 2048 full scans to two scans plus measured worklist operations; cached layer/group/failure behavior is covered. The native runner label overstated graph combinations as 640; actual unit loops cover 320 plus allocation refusal/retry. Full combined qualification is running. [Report](verification/VERIFICATION-2026-10-07-STACK-TRAVERSAL.md).

- 2026-10-07: a finite actual-core 4096-parent Raise passes at the default stack but fails at a deliberately constrained 128 KiB; ASan confirms parent-recursion stack overflow. A private constant-stack candidate passes default/constrained Linux normal/scoped sanitizer and native FreeBSD/OpenBSD ARM64 controls, deep cycle/style/ring checks and forced storage refusal for all three operations. Production remains the independently qualified traversal source. The new deeper-storage allocation-failure target needs its separate explicit decision; [proposal](decisions/PARENT-WALK-STORAGE-PROPOSAL.md) and [review](verification/VERIFICATION-2026-10-07-PARENT-DEPTH.md).

- 2026-10-07: three-story STACK-01/DEAD-01 traversal checkpoint is locally qualified: 1192 entries/eight profiles, 120 installed identities, 84 default BSD X driver invocations, optional 154/134 and 33 scoped sanitizers. Seven complete archives match exact 1314 source hashes/modes, registered tests, installed bytes and payload EOF. No production change or affected rerun was needed. Native release CI remains open. The separate private iterative parent candidate awaits its storage-failure decision. [Report](verification/VERIFICATION-2026-10-07-STACK-TRAVERSAL.md).

- 2026-10-07: user approved the invoked-window fallback when deep parent-walk storage cannot be allocated. STACK-01 production adoption now separates operation-local parent preparation from X work, removes recursive ancestry descent, and adds pure ownership/failure coverage. Full qualification remains pending for this source.

- 2026-10-07: user clarified that the intended compatibility requirement is existing FVWM configuration files only. The active contract and COMPAT-01 now preserve syntax and useful configured command/binding/function/theme behavior, without blanket internal API, module ABI, exact rendering or incidental legacy-behavior preservation. Shipped consumers still change together; safety, platform qualification, product scope and the approved Perl exception remain separate. Open compatibility-only work needs bounded owner review before any schedule reduction is claimed. [Decision](decisions/CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).

- 2026-10-07: the next STACK-01 actual-core count control finds iconified-parent Layer overcount: three moved records with one/two parent icon windows submit six/nine slots although only four/five are populated. A deterministic zero-filled allocation control suppresses invalid baseline X requests; no desktop compromise is claimed. Correction and initialized-list ownership checks are the next coherent batch, separate from the frozen parent-continuation gates.

- 2026-10-07: next three-story STACK-01 initialized-list batch is implemented: selected-record icon counts, populated-prefix XRestack submission, authoritative ring resynchronization and shared checked packing. Ten focused normal/scoped sanitizer checks and both native BSD ARM64 pure/36-case actual-core groups pass. The separate 1323-file source awaits combined qualification. [Report](verification/VERIFICATION-2026-10-07-STACK-WINDOW-LIST.md).

- 2026-10-07: approved parent continuation is locally qualified from exact 1319 files: 1208 entries/eight profiles, 120 staged identities, 88 default BSD X invocations, optional 156/136 and 35 scoped sanitizers. Seven complete archives validate hashes/modes, registered tests, installed bytes and EOF. OpenBSD AMD64 fake-command harness passed with a finite 60-second outer allowance after two retained 30-second timeouts; internal deadlines/source stayed unchanged. Native release CI remains open. [Report](verification/VERIFICATION-2026-10-07-PARENT-CONTINUATION.md).

- 2026-10-07: finite Layer restack-storage refusal currently exits the core via its fatal allocator. A private preflight candidate preserves public and scratch state, real-server order and successful retry, while retaining all 36 ordinary stacking controls in normal/scoped sanitizers. Its separate failure contract needs an explicit decision; production stays unchanged. [Proposal](decisions/LAYER-RESTACK-STORAGE-PROPOSAL.md).

- 2026-10-07: user approved Layer restack-storage refusal with existing state preserved and the WM kept running. Explicit operation-local prepare/finish ownership is activated before scratch planning/publication. Seven pure and twelve actual-core tests pass in normal/scoped sanitizers, plus native BSD ARM64 buffer/twelve-case groups. The separate 1328-file production snapshot awaits combined qualification. [Report](verification/VERIFICATION-2026-10-07-LAYER-STORAGE.md).

- 2026-10-07: the initialized-list checkpoint is locally qualified from exactly 1323 files: 1224 entries/eight compiler profiles, 120 staged identities, 92 default BSD X invocations, optional 158/138 and 37 scoped sanitizers. Seven final archives validate hashes/modes, complete payloads and registered tests. The parent man paragraph reflow restores the 101 baseline mandoc diagnostics. Native release CI remains separate; approved Layer storage qualification uses a distinct snapshot. [Report](verification/VERIFICATION-2026-10-07-STACK-WINDOW-LIST.md).

- 2026-10-07: the next STACK-01 storage owner is optional RaiseOverUnmanaged correction. A finite actual-core eight-byte refusal exits the baseline; a private candidate skips only the extra correction, preserves completed managed work/server order and permits retry. Normal/scoped sanitizer and ordinary real-client popup/focus checks pass. Production is unchanged; a separate failure decision is proposed after current Layer qualification. [Proposal](decisions/OVERRIDE-RESTACK-STORAGE-PROPOSAL.md).

- 2026-10-07: Layer qualification’s OpenBSD AMD64 ordinary gate records one existing menu-image release failure (147/148 passed). Original logs/artifacts are retained; three affected-only reruns pass with exact source and original five-second inner deadline unchanged. No production/fixture repair or UI timeout extension is made. Interaction/harness cause remains unestablished and queued for INPUT-01 follow-up; accepted entries must validate the raw replacement. [Layer report](verification/VERIFICATION-2026-10-07-LAYER-STORAGE.md).

- 2026-10-07: Linux AMD64 Clang Layer profile records one harness self-test fake-X display readiness failure (160/161 passed; all WM integration cases passed). Three unchanged affected reruns and staged install identities pass; the initial two-second-deadline failure remains archived. Final validation must check the exact raw replacement. RaiseOverUnmanaged’s separate reviewed failure decision is now pending while Layer evidence validation finishes.

- 2026-10-07: approved Layer storage ownership/refusal is locally qualified from 1328 files: 1240 accepted standard entries/eight profiles, 120 staged identities, 96 default BSD X invocations, optional 160/140 and 39 scoped sanitizers. Seven archives validate compressed EOF/CRC, full payloads, exact hashes/modes, registered tests, installed bytes and two exact affected replacements. The OpenBSD menu-release and Linux fake-X readiness initial failures each have three unchanged affected passes; cause remains unestablished, source/deadlines unchanged. Native release CI/physical checks remain separate. Next concrete decision: optional RaiseOverUnmanaged storage refusal. [Layer report](verification/VERIFICATION-2026-10-07-LAYER-STORAGE.md).

- 2026-10-07: user reiterated that compatibility stops apply only to existing configuration files/format, not internal/runtime behavior. The prior RaiseOverUnmanaged allocation-failure approval hold was overbroad and is withdrawn. Main guides/workflow now make the boundary explicit; the reviewed correction proceeds with affected tests because no configuration edit/migration is required. Historical approvals/reports remain evidence of their original scope.

- 2026-10-07: configuration-only compatibility is applied to RaiseOverUnmanaged storage handling: no configuration names, syntax or arguments change, so no approval hold applies. Shared checked prefix construction and explicit correction-array ownership are implemented. Seven pure/thirteen actual-core tests pass in normal/scoped sanitizers, plus native BSD ARM64 unit/four-case controls; exact 1330-file source awaits the next combined storage checkpoint. [Report](verification/VERIFICATION-2026-10-07-OVERRIDE-STORAGE.md).

- 2026-10-07: STACK-01 storage batch extends qualified Layer through twelve owned paths: checked unmanaged buffer, allocation-free 64-XID ordinary/full-resync batches and shared bottom-sibling selection. A 22-record Lower control reproduces suppressed-icon anchoring above unrelated frames in both old/new pre-fix cores; repaired controls pass. Seven pure/fourteen actual-core checks pass normal/scoped sanitizers, plus both native BSD ARM64 policy/twenty-case controls. Frozen 1332-file combined gates are running; no configuration edit/migration or approval hold applies. [Report](verification/VERIFICATION-2026-10-07-STACK-BATCH.md).

- 2026-10-07: the next STACK-01/DEAD-01 source is separate from frozen storage qualification. Group-aware Iconify/Deiconify without usable hints now shares the existing parent worklist: private reverse-2048 control falls from 4,194,304 to 4096 membership tests, preserving marks/ranks and ordered real-group behavior. Normal/scoped focused 7+14 checks and both native BSD ARM64 policy/core controls pass. Eighteen pure eligibility cases and twenty extra group-aware closure/refusal cases are permanent. Six-target compile/link review establishes the empty template and legacy formatting source are unbuilt and system formatters supply active calls; both sources are removed, with config declarations retained. Exact 1330-file source awaits the next coherent combined checkpoint. [Report](verification/VERIFICATION-2026-10-07-STACK-GROUP-CLEANUP.md).

- 2026-10-07: STACK-01 storage checkpoint is locally qualified from the exact 1332-file source: 1256 standard entries/eight compiler profiles, 120 installed identities, 104 default BSD X driver invocations, optional 162/142 and 41 scoped sanitizers. Seven complete archives validate source hashes/modes, registered tests, installed bytes and payload EOF. No combined-gate production change or affected rerun was needed. Manual rendering retains the preceding 101 baseline diagnostics. Group cleanup and callback ownership remain outside this checkpoint. [Report](verification/VERIFICATION-2026-10-07-STACK-BATCH.md).

- 2026-10-07: STACK-01 callback control reproduces root/wrong records when synchronous delivery runs Lower during a multi-part live-ring broadcast. Owned numeric snapshots preserve original packet records; a final outer-boundary current-ring repair prevents stale tail ordering. Eight pure/eighteen actual-core checks pass normal/scoped sanitizers, plus native BSD ARM64 unit/21-case controls. Frozen 1335-file group/cleanup/publication gates are running, separate from qualified 1332-file storage. No configuration migration or approval hold applies. [Report](verification/VERIFICATION-2026-10-07-RESTACK-PUBLICATION.md).

- 2026-10-07: STACK-01 actual-window lifecycle control confirms a stale target reference after callback retirement. Captured client/record identity guards use the existing authoritative lookup; parent continuations return captured IDs and skip retired frames while allowing survivors to finish. Final normal 8+19 checks and native BSD ARM64 unit/seven-case controls pass; private candidate sanitizer cases pass. Final permanent sanitizer and next coherent combined qualification remain pending. Exact 1337-file source is outside frozen 1335-file publication gates. [Report](verification/VERIFICATION-2026-10-07-STACK-RETIREMENT.md).

- 2026-10-07: the final 1337-file retirement source passes eight pure and nineteen actual-core checks in both normal/scoped sanitizer profiles, plus native BSD ARM64 unit/seven-case controls. Four complete focused/private archives validate hashes/modes/EOF. Full qualification remains deferred to the next related ownership batch.

- 2026-10-07: module output admission now refuses unavailable packet, queue-node or deferred configure storage without exiting the core. Only that producer is retired; unowned reservations are released and pinned prior records survive until cleanup. Final normal/scoped sanitizer profiles each pass 11 pure + 20 actual-core tests; both native BSD ARM64 overlays pass queue units and six empty/populated-owner cases. Exact 1339-file source remains outside the preceding 1335-file qualification. Initial overlay unit linker errors are preserved. [Report](verification/VERIFICATION-2026-10-07-MODULE-ALLOCATION.md).

- 2026-10-07: group/cleanup/callback publication is locally qualified from the exact 1335-file source: 1272 accepted entries/eight profiles, 120 installed identities, 108 default BSD X drivers, optional 164/144 and 43 scoped sanitizers. Seven archives validate hashes/modes, registered identities, installed bytes and complete EOF. OpenBSD AMD64 initial spawn-stall/image results and three unchanged affected reruns are retained and exactly validated; cause remains unestablished. Retirement/module allocation work remains a separate next batch. [Report](verification/VERIFICATION-2026-10-07-RESTACK-PUBLICATION.md).

- 2026-10-07: module retirement now relinks its existing list record with zero allocation, and incoming command storage refusal returns the existing producer error without dropping prior input ownership. Prior-source normal/scoped controls reproduce both exits; repaired 11+20 focused profiles and native BSD ARM64 queue/runtime units plus six core cases pass. The related stacking/module lifecycle batch is frozen from 1339 files through fifteen owned paths from qualified publication; one combined matrix is running. [Report](verification/VERIFICATION-2026-10-07-MODULE-LIFECYCLE.md).

- 2026-10-07: bounded pinned-producer control confirms an iterator can visit a retired future producer and skip a survivor. Cleanup holds and separate death-row links preserve continuation, skip retired members and keep new registrations outside the current traversal; all five early-exit owners finish explicitly. Final normal/scoped 11+21 checks pass, with pure/nested and actual BroadcastPacket/real-pipe controls. Native complete-header overlays are running; initial header-selection compile errors are retained. Exact 1341-file source is outside frozen 1339-file lifecycle gates. [Report](verification/VERIFICATION-2026-10-07-MODULE-ITERATION.md).

- 2026-10-07: final 1341-file module iterator source passes 11 pure + 21 actual-core tests in normal/scoped profiles and native BSD ARM64 runtime/two-case broadcast controls. Four complete archives validate hashes/modes/EOF. All iterator-allocating core owners compile against the new complete header; initial overlay header-selection errors remain archived. This source is still outside the frozen lifecycle matrix.

- 2026-10-07: packet construction now accounts for the existing header/capacity and uses the builder’s actual delivery length. Named notification construction removes four temporary allocation/free pairs per four-sender group and bounds name scanning/copying, retaining fields, padding and truncation. Final normal/scoped 11+22 profiles and native BSD ARM64 runtime/packet controls pass; ordinary-name baseline matches payloads with four allocations instead of zero. Exact 1343-file source is outside frozen lifecycle qualification. Structured configure packet packing remains a separate audit item. [Report](verification/VERIFICATION-2026-10-07-MODULE-PACKETS.md).

- 2026-10-07: structured configure fields now share a pure bounded packer with aligned native-word storage, explicit variadic pointer types and byte-copy scalar/raw handling. Incomplete fields refuse the whole message. Final normal/scoped 12+22 profiles and native BSD ARM64 field/runtime/packet controls pass; three complete archives validate hashes/modes/EOF. Exact 1346-file source remains outside frozen lifecycle gates. Exact consumer-layout review found a separate incorrect configure window identifier; its correction and bounded regression are underway. [Field report](verification/VERIFICATION-2026-10-07-MODULE-FIELDS.md).

- 2026-10-07: configure packet identity now carries the managed record rather than a temporary local address. Direct/broadcast payloads match the shipped consumer structure, including signed geometry, native endpoints, shorts, flags/actions and initialized padding. Prior-source normal/scoped controls isolate the identity difference while all other fields match. Final 12+22 normal/scoped profiles and both native BSD ARM64 field/runtime/expanded-packet overlays pass; four complete archives validate hashes/modes/EOF. Exact 1346-file source will join iterator and packet construction in one combined checkpoint. [Configure report](verification/VERIFICATION-2026-10-07-MODULE-CONFIGURE.md).

- 2026-10-07: combined stacking/module lifecycle is locally qualified from exact 1339 files: 1288 standard entries/eight profiles, 120 installed identities, 116 default BSD X drivers, optional 166/146 and 45 scoped sanitizer checks. Seven complete archives validate hashes/modes, registered identities, installed bytes and EOF; no combined source change or affected rerun was needed. Native release CI/physical checks remain separate. Iterator and packet/structured configure work is a separate next source. [Lifecycle report](verification/VERIFICATION-2026-10-07-MODULE-LIFECYCLE.md).

- 2026-10-07: one combined iterator/packet/configure checkpoint is frozen at 1346 files through fifteen owned paths from qualified lifecycle. Final affected Linux normal/scoped 12+22 and native BSD ARM64 controls pass; combined six-target/compiler/install/default-X gates are running. Expected counts are not pass claims. Separate local preflight runner quote correction is retained; no production edit. Native release CI and physical evidence remain separate. [Combined transport report](verification/VERIFICATION-2026-10-07-MODULE-TRANSPORT.md).

- 2026-10-07: fixed configure records now use the existing consumer structure directly with initialized padding, compile-time capacity and local allowed-action state. The sender drops 122 lines and retires three general packer/header/unit files (202 lines); exact consumer controls remain. Both picture senders share one constructor and now cover nonempty metadata at seven name lengths. Final normal/scoped 11+22 profiles and native BSD ARM64 runtime/expanded-packet overlays pass, as do preceding corrected-builder payload comparisons; four archives validate hashes/modes/EOF. Exact 1343-file/nine-path source stays outside frozen transport qualification. Final unsigned monitor-ID review is the next related focused story. [Typed report](verification/VERIFICATION-2026-10-07-MODULE-TYPED.md).

- 2026-10-07: configure monitor IDs now preserve native unsigned RROutput values rather than widening through signed int. Bounded synthetic upper-ID samples verify exact direct/broadcast consumer bytes; prior-source normal/scoped controls confirm the four differing upper-ID results while other fields match. Final normal/scoped 11+22 profiles and native BSD ARM64 runtime/expanded-packet overlays pass; four complete archives validate hashes/modes/EOF. Exact 1343-file source will join typed configure and shared picture cleanup in one combined batch after frozen transport qualification. [Monitor-ID report](verification/VERIFICATION-2026-10-07-MODULE-MONITOR-ID.md).

- 2026-10-07: failed module forks now close constructor-owned pipes through shared pre-start cleanup. Actual Module/ModuleListenOnly prior-source normal/scoped controls confirm four/two retained descriptors over three refusals each; successful retry restores ownership. Final normal/scoped 11+23 profiles and both native BSD ARM64 runtime/constructor overlays pass; four archives validate hashes/modes/EOF. Exact 1345-file source joins typed configure/picture and unsigned monitor identity in the next combined batch. Initial fixture/runner diagnostics remain recorded; other constructor allocation/process paths remain open. [Constructor report](verification/VERIFICATION-2026-10-07-MODULE-CONSTRUCTOR.md).

- 2026-10-07: combined module iteration and packet construction is locally qualified from exact 1346 files: 1312 standard entries/eight profiles, 120 staged identities, 124 default BSD X drivers, optional 169/149 and 48 scoped sanitizer checks (17 pure/31 core). Seven complete archives validate hashes/modes, registered tests, installed bytes and EOF. No combined source change or affected rerun was needed; established OpenBSD AMD64 verification-profile-only TCG allowance remains. Typed configure/picture, unsigned monitor identity and constructor pipe ownership form the separate next 1345-file/twelve-path batch, including three packer deletions. [Transport report](verification/VERIFICATION-2026-10-07-MODULE-TRANSPORT.md).

- 2026-10-07: module argument vectors now grow geometrically with checked size_t capacity; actual Module/ModuleListenOnly controls preserve eighteen token/order/terminator/refusal cases and real retry. At 256 tokens growth drops from 256 to six calls in both modes; no physical latency claim. Two legacy .c inclusions use existing library headers. Final normal/scoped 11+24 profiles and both native BSD ARM64 runtime/constructor overlays pass; four complete archives validate hashes/modes/EOF. Exact 1345-file/four-path source stays outside frozen schema qualification and joins descriptor-width work next. [Argument report](verification/VERIFICATION-2026-10-07-MODULE-ARGUMENTS.md).

- 2026-10-07: descriptor width now reads one system query and clamps unavailable/nonpositive results to FD_SETSIZE. Prior-source normal/leak-enabled sanitizer units confirm -1/zero propagation and duplicate small-limit queries; no invalid select call is exercised. Final normal/scoped 12+24 profiles and both native BSD ARM64 deterministic/native units pass; four complete archives validate hashes/modes/EOF. Exact 1346-file/three-path source joins argument growth/shared-library cleanup after frozen schema qualification. High-descriptor ownership/readiness remains open. [Width report](verification/VERIFICATION-2026-10-07-FD-WIDTH.md).

- 2026-10-07: ICE listeners/accepted connections and core sessions now validate fd_set range before inheritance protection through shared preparation; ordinary cleanup retains ownership. Borrowed watch readiness/dispatch skips unrepresentable metadata, retaining close-callback cleanup. Native policy and bounded actual watch controls plus ordinary XSMP registration/disconnect/authentication pass final normal/scoped 16+25 and both native BSD ARM64 overlays. Three complete archives validate hashes/modes/EOF; object replacement audit and initial policy-only evidence are retained. Exact 1347-file/eight-path source completes the next four-story descriptor batch, separately frozen as thirteen paths from schema1345. Forced high live admission and broader readiness remain open. [Admission report](verification/VERIFICATION-2026-10-07-FD-SELECT.md).

- 2026-10-07: next descriptor checkpoint is prepared from exact 1347 files/thirteen owned paths across four related stories; no new library/configuration migration. Required totals are 1328 standard entries, 120 staged identities, 132 default BSD X drivers, optional 171/151 and 54 scoped checks (21 pure/33 core). Dispatch waits for preceding schema qualification, preserving build/source serialization and its original failure/rerun evidence. [Descriptor checkpoint](verification/VERIFICATION-2026-10-07-MODULE-DESCRIPTORS.md).

- 2026-10-07: typed module schemas/startup ownership is locally qualified from exact schema1345: 1312 standard entries/eight profiles, 120 staged identities, 128 default BSD X drivers, optional 169/149 and 48 scoped checks (16 pure/32 core). Seven archives pass strict source/mode/identity/install/EOF checks. Initial OpenBSD ARM64 display refusal remains retained with its unestablished cause and three affected-only unchanged passes at original deadlines; the other 157 rows stay intact. [Schema report](verification/VERIFICATION-2026-10-07-MODULE-SCHEMA.md).

- 2026-10-07: repeated ICE handler installation now retains the earlier callback rather than saving itself. Bounded prior-source controls observe callback identity without invoking the old self-reference. Final normal/scoped 17+25 and both native BSD ARM64 unit/library/core/session overlays pass; four archives validate hashes/modes/EOF. Header-search and artifact-path runner errors remain retained and required no production repair/deadline extension. Five-story descriptor1349 is frozen as fifteen paths from qualified schema1345; required totals are 1336 standard entries, 120 staged identities, 132 default BSD X drivers, optional 172/152 and 55 scoped checks (22 pure/33 core). [Retry report](verification/VERIFICATION-2026-10-07-ICE-RETRY.md), [combined checkpoint](verification/VERIFICATION-2026-10-07-MODULE-DESCRIPTORS.md).

- 2026-10-07: five-story descriptor1349 is locally qualified: 1336 standard entries/eight profiles, 120 staged identities, 132 default BSD X drivers, optional 172/152 and 55 scoped checks (22 pure/33 core). Seven final archives validate. Original OpenBSD display and Linux AMD64 Xorg teardown failures remain retained with three affected-only unchanged passes each at original deadlines, preserving 160/172 other rows. No first-pass or source/deadline repair claim. [Descriptor report](verification/VERIFICATION-2026-10-07-MODULE-DESCRIPTORS.md).
- 2026-10-07: next four-story client1351 batch is focused-qualified: repeated terminal authentication cleanup, owned OpenBSD nested displays, interruption/termination-aware module reads and checked declared word counts. Original two-server control reproduces duplicate :0 allocation; explicit server locks and an owned-slot race preserve the other owner. Header-only prior-reader controls avoid body/consumer access. Final normal/scoped 3+3, six-target display checks and five QEMU read-unit overlays pass; complete focused archives validate. Exact 1351-file/ten-path source is prepared for one combined gate after descriptor qualification. [Client checkpoint](verification/VERIFICATION-2026-10-07-MODULE-CLIENTS.md).

- 2026-10-07: native module command construction removes per-command heap storage, validates against the core's existing 1000-byte limit and preserves typed wire order. Normal/scoped 3+3 and both native BSD ARM64 units pass; three archives validate complete hashes/modes/EOF. Exact command1352 is focused-qualified; partial writes remain a separate follow-up. FvwmRearrange cleanup and termination-aware write progress form the next coherent batch while client1351 qualification continues. [Command report](verification/VERIFICATION-2026-10-07-MODULE-COMMAND.md).

- 2026-10-07: three-story command/write/cleanup batch1354 passes final normal/scoped 3+3 and both native BSD ARM64 command-unit/ten-case actual FvwmRearrange controls. Partial progress and nonterminal EINTR preserve frames; incomplete output stops subsequent frames, with borrowed descriptors retained. Common termination cancels full-pipe writes; Rearrange cleanup moves to normal flow, retaining its ten-second alarm. Three complete archives validate hashes/modes/EOF. Combined qualification waits for client1351; ordinary blocking-write backpressure and other module handlers remain open. [Write/cleanup report](verification/VERIFICATION-2026-10-07-MODULE-WRITES.md).

- 2026-10-07: final write/cleanup batch adds removal of Rearrange's unused descriptor query/global/includes, with normal/scoped and both native BSD ARM64 compile/link checks; unchanged functional controls are not repeated. Four-story source1354/eight paths is frozen at 2bbe7ddc; ten combined runners preflight. Required future totals: 1368 standard entries, 120 staged identities, 136 BSD default-X drivers, optional 176/156, 59 scoped (24 pure/35 actual process/core). These are pending requirements. [Batch evidence](verification/VERIFICATION-2026-10-07-MODULE-WRITES.md).

- 2026-10-07: Backer returns from common termination/EOF to normal control flow instead of exiting inside SIGPIPE handling. Normal/scoped actual-module and both native BSD ARM64 eight-case controls pass; probe link-source correction retained, three archives validate. It joins the producer checkpoint before matrix dispatch: five-story producer1356/twelve paths, five new tests/helpers, SHA 7af0f639. Ten runners preflight; required future totals 1376 standard entries, 120 identities, 140 BSD X drivers, optional 177/157 and 60 scoped (24 pure/36 actual process/core). [Producer checkpoint](verification/VERIFICATION-2026-10-07-MODULE-PRODUCERS.md).

- 2026-10-07: client1351 locally qualified: 1352 standard entries/eight profiles, 120 staged identities, 132 BSD default-X drivers, optional 174/154, 57 scoped (23 pure/34 actual process/core). Seven archives pass strict validation. Original GCC topology failure/first affected set and the later three unchanged isolated passes remain retained; cause is unestablished, deadlines/source unchanged. Sanitizer selection preserves 55 original rows plus two omitted checks passing separately. Native release CI/physical checks remain unrun. [Client report](verification/VERIFICATION-2026-10-07-MODULE-CLIENTS.md).

- 2026-10-07: producer1356 is locally qualified: 1376 standard entries/eight profiles, 120 staged identities, 140 BSD default-X drivers, optional 177/157 and 60 scoped checks. Seven archives strictly validate without affected reruns. The next five-story shared text/action1358 batch bounds configuration views/requests, removes comma-segment allocations, checks expanded output and widens geometry arithmetic. Normal/scoped 4+3 and both native BSD ARM64 three-unit overlays pass; one combined matrix is running. Other-chat build targets remain excluded from the frozen checkpoint. [Producer report](verification/VERIFICATION-2026-10-07-MODULE-PRODUCERS.md), [text/action report](verification/VERIFICATION-2026-10-07-MODULE-ACTIONS.md).

- 2026-10-08: the initial text/action checkpoint is not qualified: all six initial suites fail the shipped colorset consumer after mistaken NULL-match refusal. Additional Linux AMD64 colorset/image timeouts and OpenBSD AMD64 image/paint deadline failures remain retained with causes unestablished. Null matches now request common/all configuration; the affected consumer and final focused normal/scoped/native units pass. Corrected source29cc90e5/1358 files is in combined qualification. A separate five-story Event/configuration1360 batch reuses the shared reader/termination state, bounds action payloads, advances short configuration and removes unused quit functions; focused normal/scoped 4+4 and BSD ARM64 ten/five controls pass. No configuration migration or decision is needed. [Action report](verification/VERIFICATION-2026-10-07-MODULE-ACTIONS.md), [Event report](verification/VERIFICATION-2026-10-07-MODULE-EVENTS.md).

- 2026-10-08: corrected action1358 passes both Linux AMD64 compilers and all ARM64 full/install/default-X gates, plus optional 179/159 and 62 scoped checks. Remaining BSD AMD64 gates are in progress; initial failures remain retained and final qualification is pending. Event1360 adds upper-window-ID wire coverage and ten preflighted combined runners, waiting for the corrected predecessor. Auto1361 is a separate three-story focused checkpoint: existing poll cancellation replaces fd_set, checked millisecond conversion preserves decimal-prefix/zero behavior, and short packets are unlocked before body access. Normal/scoped 4+5 and native BSD ARM64 controls pass; descriptor 4096 is exercised on Linux/FreeBSD, while OpenBSD hard-limit coverage is explicitly unavailable. [Auto report](verification/VERIFICATION-2026-10-08-AUTO-READINESS.md).

- 2026-10-08: corrected action1358 now passes both Linux compiler pairs, FreeBSD ARM64/AMD64, OpenBSD ARM64, optional 179/159 and 62 scoped checks. OpenBSD AMD64 remains in progress. Event/configuration and Auto readiness are combined before either separate full matrix dispatch: consumer1361/seven owned paths, eight related module wait/dispatch stories, SHA4ae424a9. Final focused normal/scoped 4+5 and native BSD ARM64 controls pass; ten runners preflight required future 1416 standard entries, 120 identities, 152 BSD installed/default-X drivers, optional 182/162 and 65 scoped checks. The single consumer matrix waits for corrected predecessor qualification. [Combined report](verification/VERIFICATION-2026-10-08-MODULE-CONSUMERS.md).

- 2026-10-08: corrected text/action1358 is locally qualified from source29cc90e5: 1392 standard entries/eight profiles, 120 staged identities, 140 BSD default-X drivers, optional 179/159 and 62 scoped checks (26 pure/36 process/core). Seven final archives strictly validate; no corrected-matrix affected rerun or further source change was required. The failed initial matrix and unestablished additional AMD64 deadline causes remain retained. The combined eight-story consumer1361 matrix can proceed without a configuration decision. [Action qualification](verification/VERIFICATION-2026-10-07-MODULE-ACTIONS.md).

- 2026-10-08: combined consumer1361 standard ARM64 and scoped sanitizer gates are dispatched after corrected action qualification. AMD64 and optional profiles remain scheduled; required totals remain requirements until final archived validation. Ident readiness review proceeds outside the frozen combined source. [Consumer checkpoint](verification/VERIFICATION-2026-10-08-MODULE-CONSUMERS.md).

- 2026-10-08: next three-story Ident1363 checkpoint is focused-qualified outside running consumer1361: common two-descriptor poll/termination, declared native payload admission in both owners, and obsolete declaration removal. Normal/scoped 4 pure + 6 actual-module checks and native BSD ARM64 configuration/popup controls pass. Input descriptor 4096 passes Linux/FreeBSD; OpenBSD inherited-hard-limit exclusion remains explicit. Three complete archives validate; preflight/collection mistakes are retained and no configuration decision applies. Full qualification remains pending. [Ident report](verification/VERIFICATION-2026-10-08-IDENT-TRANSPORT.md).

- 2026-10-08: Ident1363 has ten preflighted combined runners, held for consumer1361 qualification. Required future totals: 1424 standard entries, 120 identities, 156 BSD installed/default-X drivers, optional 183/163 and 66 scoped checks (26 pure/40 process/core). These are requirements, not completed matrix evidence.

- 2026-10-08: consumer1361 passes both Linux ARM64 compilers, FreeBSD/OpenBSD ARM64, FreeBSD AMD64, optional 182/162 and 65 scoped checks. Completed roots are archived with matching source/modes. Linux AMD64 Clang and OpenBSD AMD64 are in progress; combined qualification remains pending. Ident1363 stays separate and ready for its later gate.

- 2026-10-08: consumer1361 is strictly locally qualified from SHA4ae424a9: 1416 standard entries/eight profiles, 120 identities, 152 BSD drivers, optional 182/162 and 65 scoped checks (26 pure/39 process-core). Seven archives validate; no full-matrix failure or affected rerun. Ident-only qualification was not dispatched; Pager poll/configuration/disabled-code cleanup joins it in a six-story popup1363 batch. Final source03f000b9 passes normal/scoped 4+7 and native BSD ARM64 controls. The initial incomplete Pager startup fixture and scoped failure remain retained. One combined popup gate is next. [Consumer](verification/VERIFICATION-2026-10-08-MODULE-CONSUMERS.md), [popup](verification/VERIFICATION-2026-10-08-POPUP-READINESS.md).

- 2026-10-08: popup1363 ARM64 standard and scoped gates are dispatched, followed by Linux/FreeBSD AMD64 (at most two simultaneous AMD64 profiles). Optional Linux ARM64 follows its standard compiler pair; OpenBSD AMD64 remains scheduled. Counts stay requirements until strict final archive validation.

- 2026-10-08: popup1363 passes Linux ARM64 GCC/Clang, both BSD ARM64 targets, FreeBSD AMD64 and 67 scoped checks; Linux AMD64, OpenBSD AMD64 and optional gates remain in progress. IconMan1364 passes normal/scoped 4+8 and native BSD ARM64 actual mapped popup/tooltip controls. Buttons/session1365 passes normal 4+11, including borrowed snapshot/range/capacity checks, pending ICE disconnects and real XSMP registration; scoped/native controls run next. Component fixture/preflight failures remain retained. These coherent successors stay outside frozen popup source and will share one later full matrix. [IconMan](verification/VERIFICATION-2026-10-08-ICONMAN-READINESS.md), [Buttons](verification/VERIFICATION-2026-10-08-BUTTONS-READINESS.md).

- 2026-10-08: Buttons/session1365 now passes final normal/scoped 4+11 and native BSD ARM64 mapped popup/pending ICE/real XSMP controls. Three complete archives validate 65/33/34 payloads; exact sourcecf325049 stays outside frozen popup gates. IconMan and Buttons form one six-story successor before any separate full matrix dispatch. Existing session descriptor admission and Script’s select owner remain explicit boundaries. No configuration migration applies. [Buttons](verification/VERIFICATION-2026-10-08-BUTTONS-READINESS.md).

- 2026-10-08: final Buttons source25 adds an inactive-session fast path; refreshed normal/scoped 4+11 and native BSD ARM64 controls pass with all frozen developer source hashes matching. IconMan and Buttons now share one prepared six-story readiness1365 matrix through ten owned paths. Required future totals are 1456 standard entries, 120 identities, 172 BSD drivers, optional 187/167 and 70 scoped checks (26 pure/44 process-core). No separate component full matrix was dispatched. Dispatch waits for strict popup1363 qualification. [Combined report](verification/VERIFICATION-2026-10-08-MODULE-READINESS.md).

- 2026-10-08: popup1363 is strictly locally qualified from SHA03f000b9: 1432 standard entries/eight profiles, 120 installed identities, 160 BSD default-X drivers, optional 184/164 and 67 scoped checks (26 pure/41 process-core). All seven final archives validate; no full-matrix failure, affected rerun or source change. The prepared six-story IconMan/Buttons gate has not dispatched; Script poll/periodic/selection readiness is being added to save a separate matrix. Its new normal 5+15 controls pass; scoped/native evidence remains pending. No configuration migration is needed. [Popup qualification](verification/VERIFICATION-2026-10-08-POPUP-READINESS.md).

- 2026-10-08: Script readiness plus Swallow placeholder ownership is focused-qualified (final normal/scoped 5+16, native BSD ARM64 actual overlays). Complete 98/75/74-file archives validate final source/runners/records and retain initial failures. It joins IconMan/Buttons as ten related stories, readiness1366/seventeen paths/SHA6439c478. Ten runners preflight future 1472 standard entries, 120 identities, 180 BSD drivers, optional 189/169 and 76 scoped checks; no component-only full gates were dispatched. [Script report](verification/VERIFICATION-2026-10-08-SCRIPT-READINESS.md), [combined checkpoint](verification/VERIFICATION-2026-10-08-MODULE-READINESS.md).

- 2026-10-08: combined readiness1366 ARM64 and Linux/FreeBSD AMD64 standard gates are dispatched; 76 scoped entries pass (27 pure leak checks enabled, 49 actual process/core leak checks disabled). Wider counts remain required until archive validation. Subsequent Script packet-shape and empty-callback cleanup work is isolated outside this frozen source.

- 2026-10-08: next Script batch adds declared-body admission, removes fourteen empty widget packet callbacks, borrows validated Swallow names and refuses missing command/title tokens. Source1369/twenty-three paths/SHAcd6b8e8d stays outside running readiness1366. Earlier normal/scoped 6+17 and native BSD ARM64 controls pass; the final assertion-build flag is being confirmed. Ten wider runners are prepared but not dispatched; initial native compile-template failure is retained. [Report](verification/VERIFICATION-2026-10-08-SCRIPT-PACKET.md).

- 2026-10-08: final Script packet1369/SHAcd6b8e8d focus is qualified: normal/scoped 6+17, native BSD ARM64 policy and real packet/session/Swallow controls. All 1369 developer and 37 native overlay hashes, exact runners and recovery records validate; compiled-symbol checks confirm fourteen empty callbacks are removed. Complete 92/142/142-file archives retain final evidence; prior source and native compile-template failures remain separate. Full gate is prepared but awaits readiness1366 qualification.

- 2026-10-08: readiness1366/SHA6439c478 is strictly locally qualified: 1472 standard entries/eight profiles, 120 staged identities, 180 BSD installed/default-X drivers, optional 189/169 and 76 scoped checks (27 pure/49 process-core). Seven complete archives validate; no combined-gate failure, affected rerun or source change. Native release CI and physical checks remain separate. Script packet cleanup will join Form input/timer work before one successor matrix. [Qualification](verification/VERIFICATION-2026-10-08-MODULE-READINESS.md).

- 2026-10-08: Form ordinary timer baseline confirms input stalls until configured timeout. Common poll and nonblocking libevent servicing now allow commands while counting down. Linux normal 6+18 and FreeBSD ARM64 controls pass the initial source; OpenBSD ARM64 exposes a shared draw-string double free. Exact failing source/log/archive is retained before the explicit borrowed timeout-buffer correction. Focused final checks run before successor qualification; no configuration migration or decision is needed.

- 2026-10-08: final Form1370/SHA9216120d passes normal/scoped 6+18 and native BSD ARM64 controls; complete 127/27/26-file archives strictly validate exact sources/runners and timer/input/EOF/termination records. The initial OpenBSD timeout drawing double free remains retained; explicit borrowed-buffer drawing removes the shared-string free and duplicate allocation. Four Script stories and three Form stories join one twenty-five-path module-ui checkpoint; required future 1496/120/188, optional192/172 and scoped79. ARM64/scoped and Linux/FreeBSD AMD64 full gates are dispatched. [Form report](verification/VERIFICATION-2026-10-08-FORM-READINESS.md), [combined report](verification/VERIFICATION-2026-10-08-MODULE-UI.md).

- 2026-10-08: module-ui1370 qualification retains an OpenBSD ARM64 root-capacity failure after all 181 tests passed. The current evidence tree is hash-verified on /home with the original path retained; only unfinished install/default-X checks resume and pass. FreeBSD AMD64 initial default-X startup assertion is retained; three unchanged affected controls and remaining default-X drivers pass. Linux AMD64 GCC retains five image/interaction failures and 25 orphan test children; generated runtime files/process identities are saved and owned children retired before isolated affected controls. No deadlines or production source change; causes of the runtime failures remain unestablished. Final qualification remains pending.

- 2026-10-08: next four-story Form input1373/SHAa6aa669c is focused-qualified: declared packet admission, shared empty-line trimming, bounded owned string copies and prepare-before-publish timeout arguments. Final normal/scoped 7+19 and native BSD ARM64 pure/actual controls pass; complete 137/45/45-file archives strictly validate exact sources/runners/records. The intermediate legacy translation-pointer compile failures remain retained; the wrapper boundary is corrected without changing shared APIs or suppressing warnings. This source stays outside frozen module-ui1370. [Report](verification/VERIFICATION-2026-10-08-FORM-INPUT.md).


- 2026-10-08: five-story Form/harness successor1374/SHA48ec0ae3 is focused-qualified outside frozen module-ui1370. Linux normal/scoped seven C + two Python + nineteen actual entries and native BSD ARM64 controls pass. Complete 143/55/54-file archives strictly validate; original OpenBSD observer/session-query failure remains retained. Form manual renders with unchanged existing mandoc diagnostics. Ten owned paths await one combined matrix; no configuration migration. [Report](verification/VERIFICATION-2026-10-08-HARNESS-TEARDOWN.md).


- 2026-10-08: module-ui1370/SHA9216120d is strictly locally qualified: 1496 standard entries/eight profiles, 120 identities, 188 BSD default-X driver runs, optional192/172 and scoped79 (28 C/51 process-core). Seven final archives validate; all initial storage/runtime failures and exact unchanged recoveries remain retained. The five-story Form/harness successor1374 begins one combined gate; button argument preparation proceeds separately. [Qualified report](verification/VERIFICATION-2026-10-08-MODULE-UI.md).


- 2026-10-08: three-story Form argument/copy component1374/SHA6a57aca4 is focused-qualified outside running five-story Form/harness1374/SHA48ec0ae3: source-bound button keys, shared terminal-field cursor and field-sized owned copies. Final normal/scoped seven C + two Python + twenty-one actual entries and both native BSD ARM64 overlays pass; complete 167/65/67 payloads strictly validate. Ordinary escaped-label/F12 baseline and intermediate component results remain retained. No component-only matrix; geometry/expansion/growth owners remain open. [Report](verification/VERIFICATION-2026-10-08-FORM-ARGUMENTS.md).


- 2026-10-08: Form data source1374/SHA0792ddae is focused-qualified: prepare/replace UseData owners and preserve full native read requests, with empty data-name cleanup. Final normal/scoped seven C + two Python + twenty-two actual entries and native BSD ARM64 controls pass; complete 173/71/74 payloads validate. It joins the preceding three argument/copy stories for one five-story successor; the prepared three-story matrix was never dispatched. Earlier wider Form/harness qualification is completing final archives. [Data report](verification/VERIFICATION-2026-10-08-FORM-DATA.md).


- 2026-10-08: Form/harness1374/SHA48ec0ae3 is strictly locally qualified: 1520 standard entries/eight profiles, 120 installed identities, 192 BSD default-X runs, optional195/175 and scoped81 (29 C/52 actual). Seven final archives validate; initial OpenBSD capacity and optional menu-release timeout plus exact unchanged recoveries remain retained. Menu-release cause remains unestablished and requires fixture/runtime follow-up. The five argument/data stories now share one frozen1374/seven-path/SHA0375b2fe matrix; required1544/120/204, optional198/178 and scoped84. No component-only matrix. [Qualified predecessor](verification/VERIFICATION-2026-10-08-HARNESS-TEARDOWN.md), [new checkpoint](verification/VERIFICATION-2026-10-08-FORM-CHECKPOINT.md).


- 2026-10-08: three subsequent Form array owners are implemented outside frozen0375: line items, selection choices and button commands share checked geometric capacity, preserve line spare capacity and publish after allocation. Each now passes element count and element size once to fxrealloc, removing the previous extra pointer-size multiplier. Pure boundary/growth and actual multi-line/18-choice/21-command controls are prepared in source1374/SHA32ef2e41; focused validation is running, wider qualification remains pending. Geometry/text/expansion/partial reads remain open.

- 2026-10-08: Form array source1374/SHAd7c55798 is focused-qualified: three owners use checked geometric capacity and pass element count/size once. Final normal/scoped seven C + two Python + twenty-three actual checks and native BSD ARM64 controls pass; complete183/77/81 archives strictly validate. The initial created-before-mapped fixture observation and source32ef are retained before same-deadline mapped-state waiting. Input width/pixel preparation joins this batch; no array-only matrix. [Report](verification/VERIFICATION-2026-10-08-FORM-ARRAYS.md).


2026-10-08: Form width/pixel source1374/SHA140fd7a7 is focused-qualified and joins three counted-array owners. Final normal/scoped seven C + two Python + twenty-four actual and native BSD ARM64 controls pass; complete193/83/88 archives strictly validate. The manual-only combined sourcea1282410/seven paths has ten preflighted runners for required1560/120/212, optional200/180 and scoped86. Dispatch awaits strict argument/data0375 qualification; no component-only matrix. Full layout/expansion/editing/partial reads remain open. [Evidence](verification/VERIFICATION-2026-10-08-FORM-GEOMETRY.md).


- 2026-10-08: argument/data0375 is strictly locally qualified at1544/120/204, optional198/178 and scoped84; seven final archives validate, with no combined failure/rerun. Four command stories are focused-qualified at finalsource3567: affected normal/scoped two C + eight actual and complete native BSD ARM64 overlays; initial broader3814 and bounded prior byte results are retained. These join array/Input preparation as one nine-story1375/nine-path/SHAba7bc249 checkpoint, now dispatched. Required1576/120/216, optional202/182 and scoped88 stay requirements. [Qualified predecessor](verification/VERIFICATION-2026-10-08-FORM-CHECKPOINT.md), [command](verification/VERIFICATION-2026-10-08-FORM-COMMAND.md), [combined](verification/VERIFICATION-2026-10-08-FORM-PROCESSING.md).

- 2026-10-08: four Form history/editing stories are focused-qualified at d93a: bounded per-field ring navigation, shared value storage/counts, clear/truncate bookkeeping and checked insertion growth. Normal/scoped two C + nine actual checks and native BSD ARM64 owners pass; strict 131/110/117-file archives validate. Corrected generated history keys and development attempts remain retained. A fifth related story bounds X dispatch/coalescing and continues buffered queues while servicing commands/timers. The count-only control exposed timer lag; a monotonic five-millisecond limit now joins the 32-event budget. The five-story batch stays outside processingba7b and will use one combined gate. [History](verification/VERIFICATION-2026-10-08-FORM-HISTORY.md), [service](verification/VERIFICATION-2026-10-08-FORM-SERVICE.md).

- 2026-10-08: nine-story processing1375/SHAba7bc249 is strictly locally qualified: 1576 standard entries/eight profiles, 120 installed identities, 216 BSD default-X drivers, optional202/182 and scoped88. All seven final archives validate; no combined failure/rerun/source change. Five related recall/editing/X-service stories are focused-qualified at source3159, normal/scoped two C + ten actual and native BSD ARM64; strict149/120/129 payloads validate. One combined editing gate is prepared at required1592/120/224, optional204/184 and scoped90; required counts are not passing evidence. [Qualified processing](verification/VERIFICATION-2026-10-08-FORM-PROCESSING.md), [service focus](verification/VERIFICATION-2026-10-08-FORM-SERVICE.md).

- 2026-10-08: the five-story Form editing1375/nine-path/source3159 checkpoint is dispatched after strict processing qualification. ARM64 standard/default-X/scoped and Linux/FreeBSD AMD64 gates run; OpenBSD AMD64 follows a slot and optional Linux follows its pair. Required1592/120/224, optional204/184 and scoped90 are still requirements. Source matches the strict focused controls and verified manual. [Current checkpoint](verification/VERIFICATION-2026-10-08-FORM-EDITING.md).

- 2026-10-08: the shared caller-owned runtime packet reader and Form/Script adoption are focused-qualified at1378/nine-path/source49a5. Normal/scoped seven C + eighteen actual checks per profile and native BSD ARM64 controls pass; strict333/206/218-file archives validate sixteen consumed native sources. Partial headers/bodies preserve UI/timer/periodic/session servicing, with native framing and configuration syntax unchanged. This candidate stays outside editing3159. Other consumers are the next related ownership pass; one combined reader gate follows the batch. [Report](verification/VERIFICATION-2026-10-08-MODULE-READER.md).

- 2026-10-08: reader ownership reaches five related stories at1379/sourcec249: Buttons/Pager initialize the shared reader after configuration acquisition; incomplete input permits X and Buttons session service. Linux ordinary-action/partial/session controls pass after retained fixture delivery/geometry/configuration corrections. Normal/scoped seven C + twenty-three actual and native BSD ARM64 twenty-file overlay validation finish before one required1632/120/240 optional209/189 scoped95 checkpoint. Editing3159 is unchanged. Startup, other consumers and X fairness remain open; no configuration migration. [Current reader batch](verification/VERIFICATION-2026-10-08-RUNTIME-READER.md).

- 2026-10-08: five-story runtime-reader1379/sourcec249 focus is strictly qualified: Linux normal/scoped seven C + twenty-three actual per profile and native BSD ARM64 controls pass. Complete389/253/268-file payloads validate twenty consumed native files, exact runners and UI/session/partial completion/EOF/termination outcomes. Initial fixture attempts remain distinct; production/deadlines did not change between them. One combined1632/120/240 optional209/189 scoped95 gate follows strict editing qualification. [Report](verification/VERIFICATION-2026-10-08-RUNTIME-READER.md).

- 2026-10-08: five-story editing1375/source3159 is strictly locally qualified:1592 standard/eight profiles,120 installed identities,224 BSD default-X drivers,optional204/184 and scoped90. All seven complete archives validate; no combined failure, rerun or source change. The next five-story reader sourcec249 is separately focused-qualified and preflighted for one combined1632/120/240 optional209/189 scoped95 gate. [Editing qualification](verification/VERIFICATION-2026-10-08-FORM-EDITING.md), [reader batch](verification/VERIFICATION-2026-10-08-RUNTIME-READER.md).

- 2026-10-08: reader1379/sourcec249 combined checkpoint is dispatched after strict editing3159 qualification. ARM64 standard/compiler/install/default-X, Linux/FreeBSD AMD64 and separate scoped sanitizer gates run; OpenBSD AMD64 follows a slot and optional Linux follows the standard pair. Required1632/120/240 optional209/189 scoped95 are not yet passing full evidence. IconMan/Ident are the next remaining live-UI reader owners, outside this frozen source. [Checkpoint](verification/VERIFICATION-2026-10-08-RUNTIME-READER.md).

- 2026-10-08: remaining live-UI readers form a separate three-story1379/eight-path/sourced21f batch: IconMan operation-local reader, Ident window-list/popup state and owned blank-label rendering while display text is absent. Normal/scoped two C + eight actual and native BSD ARM64 pass; strict108/93/98 payloads validate ten consumed files and the frozen donor. Initial compile and pre-fragment drawing assertion remain retained. Current c249 combined gate is unchanged; next1648/120/248 optional211/191 scoped97 is only a requirement. Target reselection, startup and X fairness remain open; no configuration decision. [Report](verification/VERIFICATION-2026-10-08-REMAINING-UI-READER.md).

- 2026-10-08: runtime-reader1379/sourcec249 is strictly locally qualified:1632 standard entries/eight profiles,120 installed identities,240 BSD default-X drivers,optional209/189 and scoped95. Seven complete archives validate exact scope and identities; no combined failure/rerun/source change. Early Linux ARM64 collection is retained and replaced after optional completion. The remaining UI reader sourced21f three-story gate is dispatched separately. [Qualified checkpoint](verification/VERIFICATION-2026-10-08-RUNTIME-READER.md).

- 2026-10-08: the canonical X-service foundation sourcee968/1382/eight paths is focused-qualified: exact Form32-event/5-ms policy and twelve boundary assertions move to one library helper; Form wrappers retain behavior. Linux normal/scoped two C + one actual and native BSD ARM64 two pure units pass. Strict32/25/25 payloads validate six native consumed files. Initial compile failure retained; four UI adopters remain pending before one combined checkpoint. [Report](verification/VERIFICATION-2026-10-08-MODULE-X-SERVICE.md).

- 2026-10-08: IconMan and Ident adopt canonical32-event/5-ms X service with bounded adjacent coalescing. Reproduced baselines drain257 events before ready pipe work; IconMan now continues all queued keys after earlier command service, Ident handles EOF within budget. Focused Linux normal/scoped2 C +4/8 actual entries pass; strict68/110 payloads validate. Baselines retained; native adopters and combined checkpoint pending. Buttons/Pager are next in the natural five-story batch. [Scope](verification/VERIFICATION-2026-10-08-MODULE-X-SERVICE.md).

- 2026-10-08: the remaining live-UI reader sourced21f gate is running after strict c249 qualification. Linux ARM64/AMD64 GCC/Clang, FreeBSD ARM64/AMD64, OpenBSD ARM64, installed/default-server and scoped gates finish; OpenBSD AMD64 and optional Linux remain in progress. Counts1648/120/248 optional211/191 scoped97 are not yet full passing evidence. The separate five-story UI X-service candidate now includes Pager's bounded adjacent purge and expose-load controls; acceptance roots remain frozen.

- 2026-10-08: canonical X service reaches five focused-qualified stories at1382/sixteen paths/sourcecf44: shared32-event/5-ms policy, IconMan/Ident/Buttons/Pager adoption, bounded adjacent coalescing and Pager purge cleanup. Reproduced257-input starvation baselines are retained; final expose-load controls pass. Normal/scoped Linux2 C +15 actual per profile and native BSD ARM642 C +15 actual pass; strict174/153/165 payloads validate fourteen consumed native files and the exact1379-file donor. Intermediate native runner argument/core-path failures and proven reused outcomes remain retained. One1688/120/264 optional216/196 scoped102 checkpoint follows strict readerd21f qualification. Nested drag/lifecycle waits remain open; no configuration decision. [Report](verification/VERIFICATION-2026-10-08-MODULE-X-SERVICE.md).

- 2026-10-08: a separate four-story Pager packet sourceea35/1385/eight paths is strictly focused-qualified: declared native-body admission, bounded84-entry restack storage, command token ownership and unused monitor-focus subscription/callback removal. Normal/scoped Linux3 C +8 actual and native BSD ARM643 C +8 actual pass; strict80/80/87 payloads validate ten consumed files, exact donor and predecessor binaries. Fifth DesktopSize prepare-before-publish work is implemented at1388/source0e500735, with focused checks running. It retains decimal-prefix and non-positive one-page behavior, refusing incomplete/overflowing dimensions without mutation. One combined batch follows qualified UI service; no configuration migration. [Scope](verification/VERIFICATION-2026-10-08-PAGER-PACKET.md).

- 2026-10-08: final five-story Pager cleanup sourcef641/1388/eleven paths is strictly focused-qualified: native-body admission, bounded restack vectors, token ownership, empty subscription/callback removal and checked DesktopSize preparation. Normal/scoped Linux4 C +8 actual and BSD ARM644 C +8 actual pass; strict80/92/99 payloads validate thirteen consumed native files and predecessor identities. Initial declaration-edit compile failure is retained without test/scoped/native claims; corrected source restores exact preceding definitions and passes all affected checks. One1712/120/268 optional219/199 scoped105 checkpoint will follow qualified UI servicecf44. Monitor-line/DesktopConfiguration numeric parsing and nested drag/lifetime work remain open; configuration files need no migration. [Report](verification/VERIFICATION-2026-10-08-PAGER-PACKET.md).

- 2026-10-08: remaining live-UI reader1379/eight-path/sourced21f is strictly locally qualified:1648 standard entries/eight profiles,120 installed identities,248 BSD default-X drivers,optional211/191 and scoped97. Seven final archives validate; no combined failure/rerun/source change. The separate five-story1382/sourcecf44 X-service checkpoint is dispatched at required1688/120/264 optional216/196 scoped102. Pager monitor parsing, ownership and missing-monitor handling continue outside both frozen archives. [Reader qualification](verification/VERIFICATION-2026-10-08-REMAINING-UI-READER.md), [service checkpoint](verification/VERIFICATION-2026-10-08-MODULE-X-SERVICE.md).

- 2026-10-08: three Pager monitor-owner stories are strictly focused-qualified at1388/source7284: checked DesktopConfiguration/Monitor preparation, token cleanup and absent-monitor/highlight guards. Normal/scoped Linux4 C +8 actual and BSD ARM644 C +8 actual pass; complete80/96/105 payloads validate fourteen native sources, exact1382-file cf44 donor and prior binary identities. The initial root-fallback fixture limitation is retained; no production/deadline change. Fold these into the undispatched five-story Pager candidate for one eight-story owner checkpoint at required1712/120/268 optional219/199 scoped105. No extra extensive f641 matrix. The separate cf44 matrix continues; its scoped startup timing failure has three unchanged affected passes, with original evidence retained and final validation pending. [Pager owner batch](verification/VERIFICATION-2026-10-08-PAGER-MONITOR.md).

- 2026-10-08: three declared UI-packet stories are strictly focused-qualified at1393/fourteen paths/sourcee717: shared bounded-text predicate plus Buttons/IconMan owner admission. IconMan refusal retains two synchronous unlock acknowledgements. Normal/scoped Linux5 C +10 actual and native BSD ARM645 C +10 actual pass; complete92/98/107 payloads validate eleven sources, exact1382-file donor and four prior binaries. Initial ignored-type fixture cases are retained; corrected consumed-type coverage passes unchanged production/deadlines. Required successor1736/120/276 optional222/202 scoped108 follows the pending Pager owner checkpoint. Numeric geometry and nested waits remain open. [UI packet scope](verification/VERIFICATION-2026-10-08-UI-PACKET.md).

- 2026-10-08: canonical text adds Pager/Script/Ident adoption to the undispatched three-story UI packet candidate, sharing one six-story1393/source3c37/seventeen-path checkpoint. Normal/scoped Linux6 C +14 actual and native BSD ARM646 C +14 actual pass; complete152/123/135 payloads validate eight overlay source files, fourteen reused object/executable identities and the frozen1382-file donor. No new failure/correction. Required1736/120/276 optional222/202 scoped108 is prepared behind Pager7284 qualification; no extra full e717 matrix. Module schemas remain owner-specific. [Final text scope](verification/VERIFICATION-2026-10-08-UI-TEXT.md).

- 2026-10-08: shared five-story X-service sourcecf44 is strictly locally qualified:1688 standard entries/eight profiles,120 installed identities,264 BSD default-X invocations,optional216/196 and scoped102. Seven final archives validate, including the original startup timing failure and three unchanged passing replacements; cause remains unestablished. The frozen eight-story Pager7284 checkpoint is dispatched at required1712/120/268 optional219/199 scoped105. Canonical UI text3c37 remains queued. Next monitor work covers bounded quoted producer records, checked Rearrange work areas/token ownership and disabled Pager tracking; no configuration migration is proposed. [Qualified service](verification/VERIFICATION-2026-10-08-MODULE-X-SERVICE.md).

- 2026-10-08: four related monitor stories are implemented at1398/eighteen paths/sourcee761: shared checked numeric/record/area preparation, bounded core quoting, Rearrange token cleanup/atomic work-area publication and disabled Pager tracking. Normal/scoped Linux8 C +17 actual per profile pass;192-file Linux focused archive validates. Native BSD ARM64 uses independent full-source focused builds, still running; combined qualification remains pending. Initial link/header build failures are retained; no configuration migration or deadline change. Pager7284 acceptance and canonical-text3c37 remain separately frozen. [Monitor development](verification/VERIFICATION-2026-10-08-MODULE-MONITOR.md).

- 2026-10-08: monitor sourcee761 is strictly focused-qualified: Linux normal/scoped8 C +17 actual per profile and both native BSD ARM648 C +17 actual pass. Complete192/133/148 payloads validate full1398-file source hashes/modes, exact runners and eleven native compiled binary identities. No native overlay/donor reuse. The four-story checkpoint queues behind canonical text3c37; no configuration migration. [Monitor qualification scope](verification/VERIFICATION-2026-10-08-MODULE-MONITOR.md).

- 2026-10-08: numeric/lookup source30be is strictly focused-qualified at normal/scoped Linux and native BSD ARM648 C +17 actual per profile. Complete193/99/114 payloads validate full1398 source hashes/modes and exact outcomes. A reproduced Pager null-member style-offset UBSan failure is repaired with size_t/offsetof; original failed source and outcomes remain retained. Four new stories join the four undispatched monitor stories for one eight-story1398/twenty-four-path checkpoint at required1760/120/280 optional225/205 scoped111, following text3c37. No separate e761 full matrix. [Final owner scope](verification/VERIFICATION-2026-10-08-UI-NUMERIC.md).

- 2026-10-08: a private bounded Pager desk-range candidate passes normal/scoped pure and ten actual controls; production is unchanged. A default4096 displayed desks, finite build override through65536, would refuse valid existing larger ranges and therefore needs the configuration decision. Initial incomplete fixture evidence is retained; reliable startup geometry remains separate authorized follow-up. [Concrete proposal](decisions/PAGER-DESK-RANGE-LIMIT-PROPOSAL.md).

- 2026-10-08: user approved Pager default4096 displayed desks and finite build override through65536. Four related startup stories are implemented: checked range/configuration allowance, safe initial page geometry, checked extreme desk-index arithmetic and absolute high-ID startup style lookup. The new actual check reproduced the style lookup bug on Linux/BSD ARM64; failed scope is retained before repair. Focused five C +five actual checks are running; full qualification remains pending behind text3c37/numeric30be. [Pager startup](verification/VERIFICATION-2026-10-08-PAGER-STARTUP.md).

- 2026-10-08: four-story Pager startup sourcea653 is strictly focused-qualified: normal/scoped Linux and native BSD ARM64 five C +five actual entries per profile, complete693/65/73 payloads with exact source/modes/runners/outcomes. The65536 build override and restoration pass with identical default Pager bytes. Original high-ID startup style failures remain retained. One1776/120/284 optional227/207 scoped113 checkpoint is prepared behind numeric30be; no extensive per-story runs or new configuration decision. [Startup evidence](verification/VERIFICATION-2026-10-08-PAGER-STARTUP.md).

- 2026-10-08: undispatched monitor/numeric30be and Pager-startupa653 qualification are consolidated into one twelve-story shared construction/lookup checkpoint. Final1402-file/34-owning-path sourcea653 is strictly focused-qualified at normal/scoped Linux and native BSD ARM64 nine C +eighteen actual per profile, complete837/137/157 payloads. Earlier individual focused evidence is retained. One1776/120/284 optional227/207 scoped113 matrix follows canonical text3c37; no separate30be or startup-only matrix. [Combined owner checkpoint](verification/VERIFICATION-2026-10-08-PAGER-STARTUP.md).

- 2026-10-08: a separate three-story shared numeric parsing batch is implemented after the actual old library accepted above-INT_MAX values through four APIs. Integer/suffix/base, rectangle preparation and percent/unit ownership now use checked conversions, preserving representable configuration grammar. Baseline identities/outcomes are retained. Linux normal/scoped10 C +22 actual and native BSD ARM6410 C +20 actual focus is running; full qualification is pending. [Parser scope](verification/VERIFICATION-2026-10-08-PARSE-INTEGER.md).

- 2026-10-08: shared numeric parsing source9f17 is strictly focused-qualified at normal/scoped Linux10 C +22 actual and native BSD ARM6410 C +20 actual per profile. Complete259/119/137 payloads validate1403 source hashes/modes and exact runners/outcomes, with the real old-library baseline retained. The three owning paths preserve representable configuration grammar and add checked integer/suffix/base, rectangle and percentage preparation. Extensive qualification remains pending behind earlier module owner checkpoints. [Parser verification](verification/VERIFICATION-2026-10-08-PARSE-INTEGER.md).

- 2026-10-08: Pager7284 Linux AMD64 GCC/Clang full220-entry suites each have one retained100-ms external-xrandr topology timing failure and three unchanged affected passing runs. Both installed-code finishes pass. Cause remains unestablished; source/deadlines are unchanged. Full combined qualification waits for OpenBSD AMD64 standard/default-X and final archives. Shared parser9f17 is separately prepared at1784/120/284 optional228/208 scoped114 behind combineda653.

- 2026-10-08: Pager source7284 is strictly locally qualified at1712 standard entries/eight profiles,120 installed identities,268 BSD default-X invocations,optional219/199 and scoped105. Both retained AMD64 topology timing failures have three unchanged affected passes; cause is unestablished. Seven complete final archives validate. Canonical text3c37 combined qualification is dispatched at required1736/120/276 optional222/202 scoped108. [Pager evidence](verification/VERIFICATION-2026-10-08-PAGER-MONITOR.md).

- 2026-10-08: Pager grid/scale source6e01 is strictly focused-qualified: normal/scoped Linux11 C +23 actual and native BSD ARM6411 C +21 actual per profile. Complete279/124/143 payloads validate1407 source hashes/modes and exact runners/outcomes. Checked Rows/Columns preparation, displayed-desk shape/reconfigure traversal and positive DesktopScale preparation join the three parser9f17 stories in one six-story/fourteen-path checkpoint at required1800/120/288 optional230/210 scoped116 behind sourcea653. No separate parser matrix. Initial missing-header build failure is retained; remaining derived geometry is next. [Grid evidence](verification/VERIFICATION-2026-10-08-PAGER-GRID.md).

- 2026-10-08: four Pager dimension stories are strictly focused-qualified at sourcece40: normal/scoped Linux11 C +23 actual and BSD ARM6411 C +21 actual per profile. Complete291/125/144 payloads validate1407 source hashes/modes and exact runners/outcomes. A real previous-source label×INT_MAX-row overflow is retained and repaired. Checked window/cell/icon/viewport arithmetic and X request bounds fold into the undispatched parser/grid batch: one ten-story/fourteen-path checkpoint at required1800/120/288 optional230/210 scoped116 behind sourcea653. Window projection, minimum-size inputs, drag waits and resource-area bounds remain open. [Dimensions evidence](verification/VERIFICATION-2026-10-08-PAGER-DIMENSIONS.md).

- 2026-10-08: Pager projection/metric source3074 is strictly focused-qualified at normal/scoped Linux11 C +23 actual and native BSD ARM6411 C +21 actual per profile. Complete305/127/146 payloads validate1407 source hashes/modes and exact runners/outcomes. Wide forward/inverse window projection, typed border/minimum parsing and unused percentage-helper removal join the undispatched numeric/rendering checkpoint:14 related stories/fifteen owning paths, required1800/120/288 optional230/210 scoped116 behind sourcea653. A real preceding-source endpoint arithmetic regression and initial client lookup failure remain retained; no final-source focused failure. Drag lifetime and remaining pointer/resource-area work stay open. [Projection evidence](verification/VERIFICATION-2026-10-08-PAGER-PROJECTION.md).

- 2026-10-08: canonical-text source3c37 is strictly locally qualified:1736 standard entries/eight compiler profiles,120 installed identities,276 default-BSD-X drivers,optional222/202 and scoped108. Seven complete final archives validate1393 hashes/modes and exact runners/outcomes; no combined correction or affected rerun. Successors a653/3074 remain undispatched. [Report](verification/VERIFICATION-2026-10-08-UI-TEXT.md).

- 2026-10-08: MAINT-02/I18N-01 candidate language discovery and complete all-language publication reach the focused maintenance checkpoint. All30 macOS maintenance tests pass; normal Linux/BSD ARM64 gettext matches138 legacy message entries and all11 catalogs in both modes. Three clean catalog-only Meson fixtures regenerate/install33 exact identities without committed GMO inputs. Guest tool/clock/package-manager changes are recorded. Whole-source/AMD64 adoption gates remain; no predecessor or committed translation was removed. [Report](verification/VERIFICATION-2026-10-08-I18N-MAINTENANCE.md).

- 2026-10-08: GEN-01 modern generation is focused-qualified for ordinary shipped configurations on all six native targets. Two generations produce identical3 C/header files plus generator metadata. All13 scripts parse with identical window/widget/variable/case/instruction/block structures against verified native3c37 objects;78 comparisons/156 successful parses and exact source/object identities validate. Runtime parser and legacy helper remain unchanged; diagnostics/execution/adoption gates remain. [Report](verification/VERIFICATION-2026-10-08-PARSER-GENERATION.md).

- 2026-10-08: I18N-01 corrects four empty Language metadata fields only. Linux/BSD ARM64 all24 default/fuzzy comparisons preserve translated messages and clear old header warnings; macOS all11 default catalogs compile without those warnings. Separate six-file metadata snapshot/evidence preserves exact updated source; old34-file checkpoint remains historical. No committed GMO is replaced. [Report](verification/VERIFICATION-2026-10-08-I18N-MAINTENANCE.md).

- 2026-10-08: the four-story core performance batch is focused-qualified. Ordered/equal scheduler insertion becomes linear; profiler workload/identity selection and final child-log collection pass; AutoDetect combines only adjacent size requests from a bounded queued snapshot. Final normal Linux AMD64/ARM64, default BSD ARM64 and scoped core controls pass; regular/scoped Linux also pass seven scale/interaction cases. Source005f freezes1409 files/twelve owning paths after retained3074. Full-source/BSD AMD64/install/optional gates and extensive runner preparation remain; paused matrices stay undispatched. [Report](verification/VERIFICATION-2026-10-08-CORE-PERFORMANCE.md).

- 2026-10-11: the current core responsiveness milestone is strictly locally qualified. Final test-only source01c2 retains unchanged production005f and includes inherited a653/3074: 1816 entries/eight compiler profiles, 120 installed identities, 300 default-BSD-X drivers, optional232/212 and scoped118. Seven archives validate1409 hashes/modes, copied runners, installed bytes and exact affected-test derivations. Original monitor/Pager/desktop fixture failures and preparation recoveries remain retained; no configuration migration. All26 man pages and27 HTML outputs render. Separate predecessor matrices are superseded. Native Linux release sanitizer CI, physical desktop checks and broader backlog remain open. [Combined report](verification/VERIFICATION-2026-10-11-CORE-PERFORMANCE.md).

- 2026-10-11: GEN-01 / MAINT-02 follows the accepted core milestone as one coherent maintenance batch. Modern generated runtime, scanner header ordering, generation provenance and the portable developer wrapper are implemented after strict eight-profile candidate checks. The original TCG colorset timeout remains unexplained; baseline and unchanged affected retry pass. Combined production qualification remains pending. [Adoption report](verification/VERIFICATION-2026-10-11-PARSER-ADOPTION.md).

- 2026-10-11: I18N-01 separately extends focused evidence to AMD64 while parser qualification runs: all15 unit checks pass; Linux real gettext preserves138 message keys, passes22 direct catalog comparisons and11 updates, and stages11 regenerated catalogs without committed GMO inputs. Both BSD AMD64 attempts stop after passing units because gettext developer programs are absent; no package/runtime change or predecessor removal is made. [Report](verification/VERIFICATION-2026-10-11-I18N-AMD64.md).

- 2026-10-11: GEN-01 / MAINT-02 modern parser/runtime and developer-wrapper batch reaches the locally qualified2514 checkpoint:1816 full standard entries across eight compiler profiles,120 verified staged code identities,48 installed-module fixtures on four default BSD servers, optional232/212 suites,15 scoped sanitizers and24 affected tooling passes. Seven production and six tooling archives validate exact source/runner scopes. Wider Git-inventory failures and the original candidate timeout remain retained; no FVWM configuration migration or release/native-CI claim. [Report](verification/VERIFICATION-2026-10-11-PARSER-ADOPTION.md).

- 2026-10-11: I18N-01 AMD64 focused generation/catalog-only checkpoint completes on all three targets:15 units, native138-key parity each,66 direct catalog comparisons,33 updates and33 exact staged catalog identities. Missing developer-tool attempts, subsequent package preparation (including FreeBSD pkg self-update) and affected-only recovery are retained. Whole-source/release/distribution/reference adoption gates remain; no legacy helper/GMO removal or configuration migration. [Report](verification/VERIFICATION-2026-10-11-I18N-AMD64.md).

- 2026-10-11: I18N-01/MAINT-02 translation maintenance is locally adopted after one combined whole-source2663 checkpoint:1743 entries/eight profiles,120 staged code identities,77 catalogs,30 native NLS fixtures and30 retained translation units. All six whole-source extractions match138 keys. Deterministic clean-fixture exports and twelve final archives validate source/modes, runners, registration, complete logs and install bytes. Initial fixture/metadata/upload failures and signed ARM64 package preparation remain retained. Ten unused GMO files and the old helper are retired only after live-owner guards and exact backup. No FVWM configuration migration or public-release/native-CI claim. [Report](verification/VERIFICATION-2026-10-11-I18N-ADOPTION.md).

- 2026-10-11: the user selects remaining font/modal service work. The coherent 3bb3 batch implements shared bounded readiness, active-widget mutation cancellation, optional selection servicing and checked Unicode/font-set conversion. Focused Linux/default nested-X and native BSD ARM64 controls pass; combined qualification is running. Prior harness failures remain retained. [Report](verification/VERIFICATION-2026-10-11-FONT-MODAL.md).

- 2026-10-11: the font/modal checkpoint is strictly locally qualified at final69af, with full baseline3bb3 retained:2229 full/optional entries,150 staged code identities,99 catalogs,52 default-BSD-X and8 scoped checks. The four-path resource-revision correction passes104 standard,23 optional,13 scoped and52 default-BSD-X drivers with11 exact module stages. Six baseline/five companion/six correction archives validate1403 source hashes/modes, executed runner versions, consumer identity, registrations/deadlines and stage transforms. Man/HTML render and owning source/whitespace/reference checks pass. Failed preparation and non-reproducing address-reuse controls remain retained. Configuration migration and new dependencies are unnecessary; TEXT-01, broader resource budgets and native release CI/physical checks remain separate. [Report](verification/VERIFICATION-2026-10-11-FONT-MODAL.md).

- 2026-10-11: the four-story Prompt/protocol/utility-foundation batch is strictly locally qualified at final397c with baselineadc5 retained:2276 full/optional entries,159 staged code identities,99 catalogs,27 final affected entries,four Go race/vet controls,nine Go stages,eight default-BSD-X fixtures and three scoped C sanitizer checks. Twelve acceptance archives validate1414 baseline/1415 final source hashes/modes, executed runners, registered commands/deadlines, installed consumers and complete gzip streams. Fifteen utility parity fixtures and checked metadata generation are adopted; no utility/FvwmCommand port or Perl retirement is claimed. Native Go1.22.2/isolated1.27.0 evidence, initial SDK/cache/fixture/runner failures and affected recoveries remain retained. Prompt man/HTML rendering and owning source/reference checks pass. No configuration migration or new mandatory dependency; native release CI and physical checks remain separate. [Report](verification/VERIFICATION-2026-10-11-PROMPT-METADATA.md).

- 2026-10-12: CI-01/monitor/protocol audit checkpoint `ebd1` qualifies affected
  local checks across Linux/FreeBSD/OpenBSD AMD64/ARM64:30 X invocations and30
  native gate contract cases, with unchanged accepted core/module identities.
  Script snapshots now wait for application-level input acknowledgement; portable
  setup has one owner. OpenBSD AMD64's30-second infrastructure timeout and its
  deadline-only180-second follow-up are retained. A fixed32-row reconciliation
  identifies current required gaps without resetting accepted geometry/transfer
  work. Workflow pins are candidates outside live workflows; asset/native-CI
  and release ownership remain open. See the current checkpoint report/coverage.

- 2026-10-12: the FvwmCommand client batch is locally qualified at composed295e / runtime-controls f274. Six targets pass162 final peers,60 Prompt regressions,18 Go package results,14 real-X invocations/84 cases,12 tagged stages and12 exact legacy help/version comparisons. Linux race/vet and actual race-instrumented clients pass; unchanged C/core/MFL identities reuse accepted coverage. Manual rendering/CLI startup samples and retained fixture/cold-cache/harness failures are recorded. Go-enabled builds adopt the client; disabled-Go Python/default-profile migration, Perl utilities/retirement and native release CI remain separate. No configuration migration or new dependency. [Report](verification/VERIFICATION-2026-10-12-COMMAND-PORT.md).

- 2026-10-12: CI-01 local client-profile/default-X batch is qualified at finald751 with unchanged C/header/Go sources. Six targets pass444 ordinary peers,78 final pure contracts,12 nested-X and8 authenticated default-BSD-X fixtures with4 unauthenticated-client refusals; installed native identities and Linux actual race clients pass. A fresh Linux ARM64 Go gate and13 verified asset downloads supplement candidate structure checks. Socket readiness, Xenocara authority reread and Meson target-selector failures are retained with precise repairs. Candidates stay inactive; provider hash/default-X enforcement, container contents and actual native release CI remain open. Installed utility ports are next. [Report](verification/VERIFICATION-2026-10-12-CI-PROFILES.md).

- 2026-10-12: UTIL-01 first standalone utility port is locally qualified at final9ab4/runtimec019. Go-enabled fvwm-menu-xlock preserves recorded CLI/menu behavior with direct five-second/1 MiB help probing and streaming formatting. Six target/profile/install/X checks, Linux actual race utilities and a fresh Go gate pass; source/identity/driver composition validates. Initial buffer-copy, fixture-cleanup, emulated-suite deadline and default-X readiness failures remain in evidence. Perl fallback remains; fvwm-menu-directory and fvwm-convert-2.6 proceed at separate checkpoints. No configuration migration or new dependency. [Report](verification/VERIFICATION-2026-10-12-XLOCK-PORT.md).

- 2026-10-12: UTIL-01 directory utility port is locally qualified at a7e6. Six target/profile/install/X checks and Linux race utilities pass; exact source, installed identities and unchanged core/MFL identities validate. Go-enabled builds cache metadata/format plans and replace reuse-mode shell state writes with an owned atomic helper. Existing configuration options and plain-text state remain usable; disabled-Go retains Perl. Documentation records limits and the synchronous-filesystem gap. Converter is the next separate checkpoint. [Report](verification/VERIFICATION-2026-10-12-DIRECTORY-PORT.md).

- 2026-10-12: UTIL-01 converter port is locally qualified at finalb6ca. Six targets pass966 ordinary/race conversion/file cases,270 recorded observations,36 native gate controls,36 ordinary/twelve race Go package results and twelve tagged stages. Exact source, runner/log and installed/tested ELF/fallback identities validate. Initial63c9 passes are retained; final review corrected total missing-Read accounting with a ninth unit and affected rerun using owned caches. Private complete outputs refuse replacement; heuristic bytes and runtime configuration syntax stay unchanged. Manual renders and installed/example/contrib classification pass; no new dependency or configuration migration. The installed Script communication example is the next separate fixture/port checkpoint. Full XDG/runtime and Perl retirement remain separate. [Report](verification/VERIFICATION-2026-10-12-CONVERTER-PORT.md), [ownership](verification/RECONCILIATION-2026-10-12-UTILITY-CONSUMERS.md).

- 2026-10-12: UTIL-01 installed Script communication example is locally qualified at final `6fcc`. The stdlib Go helper pins its directory, owns private FIFO entries, bounds connected reads/replies and preserves the borrowed Script process. Paired Go/Perl consumers, 546 normal/race transport controls, thirty selected registered entries, twelve tagged stages and eighteen native-X invocations/90 button requests pass across six targets; four default BSD servers use private authority. Final fixture retries retain 24 unchanged entries and fourteen Linux race package results after explicit source/binary checks. Initial Meson quoting, Script diagnostic syntax, emulated-transfer budgeting and FIFO-probe/status races remain in evidence. Source/install/runner composition and manual renders validate. The Perl file remains installed; no configuration migration or new dependency. Small utility/example checkpoints are complete; full XDG provider/frontend fixtures and later runtime/retirement gates remain separate. [Report](verification/VERIFICATION-2026-10-12-SCRIPT-COM-PORT.md).

- 2026-10-12: Full XDG menu provider/frontend fixture checkpoint is locally qualified at `efe8`. Six targets pass 52 distinct observations each (thirteen CLI/paired Form and thirty-nine real PyXDG/XML), plus six exact Meson registration repeats: 390 observation executions. Source/runner/oracle/log identities validate; existing producers match the preceding Script checkpoint. An isolated checksum-pinned PyXDG 0.28 oracle introduces no installed dependency. Recorded discovery/preference/field-code/visibility/icon defects are not future parity requirements. Local Python 3.14 oracle failure and host Meson-name validation correction are documented. No production code, core/X/install qualification or interpreter retirement is claimed. Next is the coordinated full Go provider/frontend implementation with remaining normative, ownership and installed/native-X gates, separate from the other chat's desktop/runtime work. [Report](verification/VERIFICATION-2026-10-12-XDG-FIXTURES.md).

- 2026-10-12: RUNTIME-GO-01 full XDG provider port input-policy foundation is locally qualified at `d33f`. Three stories provide bounded desktop/localized/list/visibility policy, literal Exec expansion and ordered XML/rule policy. Six targets pass eight Go packages, vet and stdlib dependency checks; both Linux race packages pass. Eleven top-level tests/121 named subtests yield 968 subtest executions across ordinary/race runs. All 480 predecessor Go files are unchanged; no runtime/install/X activation or new dependency. Initial quote-fixture and host dispatch-spacing failures are retained. Full provider discovery/resolution/rendering, paired native settings, installed/native-X gates and Perl retirement remain open. [Report](verification/VERIFICATION-2026-10-12-XDG-POLICY.md).

- 2026-10-12: RUNTIME-GO-01 full XDG port advances through four discovery/preference stories, qualified at `9337`. Explicit XDG paths, deterministic menu precedence, recursive desktop IDs/last-AppDir pools with hidden overrides and legacy whole-tail preference parsing/encoding pass. Six targets qualify eight Go packages, vet/stdlib checks and 20 top-level tests/230 named subtests; both Linux race runs pass, yielding 48 ordinary/two race package outcomes and 1840 subtest executions. All 486 predecessor files and installed Python/Perl producers are unchanged. No new dependency, configuration migration, file writer or runtime/X activation is claimed. Full XML merging/allocation/rendering, direct paired Form save/export, installed/native-X gates and retirement remain open. [Report](verification/VERIFICATION-2026-10-12-XDG-DISCOVERY.md).

- 2026-10-12: RUNTIME-GO-01 full XDG port adds three ordered-allocation stories, qualified at `be8e`. Ordered Include/Exclude, excluded-but-allocated source tracking, child-first two-pass OnlyUnallocated, stable output, immutable inputs, shared category caching and aggregate item/storage/work bounds pass. Six targets qualify eight Go packages and 24 top tests/265 named subtests, plus vet/stdlib checks; both Linux policy race runs pass. Strict source/proof/runner/test-set validation yields 48 ordinary/two race package outcomes and 2120 subtest executions. All 492 predecessor Go files are unchanged. Full merging/plan construction, layout/rendering, direct paired Form save/export, install/native-X and retirement remain open. No production caller, dependency or configuration migration is introduced. [Report](verification/VERIFICATION-2026-10-12-XDG-ALLOCATION.md).

- 2026-10-12: The utility-family/XDG port milestone is locally qualified and adopted at `04c1`, with unchanged runtime `cb13`. Six targets/eight compiler profiles compose 1982 full-suite entries and 134 affected callback checks; 25 archives validate source/modes, actual executed runners, registrations, logs and stage identities. Final audits validate 90 installed native identities and independently captured provider/shim bytes; six actual staged core/Form/provider checks and six installed legacy entry consumers pass. Four authenticated default BSD servers, 60 ordinary/twenty race Go package results (98/421 tests/subtests per run), 36 final gate units, minimal CLI30, scoped17 and six manual profiles pass. Initial harness errors, image/topology timing failures, unchanged retries and the older-stage capture error remain in evidence; causes are not invented. Shared callback type/ctype correction is qualified; disabled-Go predecessors and stage5 Perl retirement remain separate. No configuration migration or new mandatory library. Native release CI, CI asset-provider/workflow adoption, physical support and the other chat's desktop runtime remain separate. [Report](verification/VERIFICATION-2026-10-12-XDG-PORT.md).

- 2026-10-13: CI-01's three-story asset preflight is locally qualified at `4bd0`: strict existing lock selection, verified independent private cache/output and finite subprocess acquisition. Six targets pass23 tests each;13 actual release downloads and13 offline cache hits independently verify3,832,706,906 bytes per set. Native failure logs retain stopped transports, OpenBSD gzip collector refusal and ordinary-worker fixture timeouts; final success-case startup allowances retain the separate one-second stall control. No C/runtime/config/dependency changes; no downloaded inputs execute, decompress or boot. The action downloads another runtime and uses mutable boot caches, so an unconsumed preflight is not execution protection. An external CI-provider choice is proposed before replacement, under AGENTS.md's dependency constraint. Actual native CI/default-X/provider execution and publication remain separate. [Report](verification/VERIFICATION-2026-10-13-CI-ASSETS.md), [provider proposal](decisions/CI-VM-PROVIDER-PROPOSAL-2026-10-13.md).

- 2026-10-13: The user approved CI-01's bounded repository-owned QEMU gate, replacing the inactive vmactions/AnyVM wrapper. Implement checked acquisition, fresh-image extraction, private job authentication, scoped process/SSH ownership and four actual provider/default-X qualification profiles before workflow adoption. Publishing and actual remote CI remain separate. [Approved contract](decisions/CI-VM-PROVIDER-PROPOSAL-2026-10-13.md).


- 2026-10-13: CI-01's approved owned QEMU runner is affected-qualified at source `5200` / policy `9c50`. Six targets pass 294 native units; final same-size cache/extraction/boot/gate/deadline/cancellation controls and a long-script EOF fixture pass. Four actual pinned images boot with owned authentication, and all four authenticated default-X startup setups pass. OpenBSD AMD64's unsupported native GPU trial is retained; its VESA fixture explicitly opts into guest aperture 2 and clean reboot. All 36 owned job records finish with keys removed; previous guests/disks and the 1502-file accepted donor are preserved. Failed geometry/output/transport/package/hardware/fixture attempts remain. Distinct core/Go preparation avoids imposing the SDK/default-server packages on the core recipe. No C/runtime/configuration migration or new mandatory library. Candidates remain inactive pending the external patched-PNG delivery decision and fresh provider build/install/default-X consumers; native remote release CI remains separate. [Report](verification/VERIFICATION-2026-10-13-QEMU-PROVIDER.md), [concrete input proposal](decisions/CI-PNG-INPUT-PROPOSAL-2026-10-13.md).

- 2026-10-13: The user approved the fixed upstream libpng 1.6.59 CI source
  bootstrap. Implement verified finite acquisition, owned builds and actual
  core/SVG transitive-loader qualification on fresh provider images. No
  unverified vendor backport assertion or configuration migration.

- 2026-10-13: The approved CI PNG bootstrap is implemented. A real fixed
  upstream download and 336 final applicable native helper checks pass; private
  builds retain upstream tests and transitive loader identities. Fresh provider
  core/Go/staged/default-X qualification is running. OpenBSD package streaming,
  canonical filenames, IPv4 resolution and arm64/aarch64 mapping corrections
  retain separate failures. Application C/header/Go source is unchanged. See
  the [in-progress bootstrap report](verification/VERIFICATION-2026-10-13-CI-PNG-BOOTSTRAP.md).

- 2026-10-13: Approved CI-01 PNG bootstrap now passes final helper source027890: 336 checks across six native targets. Both Linux architectures pass GCC/Clang upstream36/probe/loader builds; both fresh FreeBSD standard profiles pass233/staged core+worker and four patched PNG consumers. Streamed OpenBSD Meson/Go failures retain logs; complete signed ordinary packages resolve preparation without trust overrides. A Go cache mismatch is repaired in the owning Meson block; affected Go/install/race/default-X checks at source127339 run separately from accepted C matrices. Fresh OpenBSD standard qualification and full provider acceptance remain in progress. [Report](verification/VERIFICATION-2026-10-13-CI-PNG-BOOTSTRAP.md).

- 2026-10-13: CI-01 source d243a918 compiles all ten Go test binaries in the owning build before the unchanged180-second runtime gate. Six native component checks pass with60 binary identities and actual uncached runtime results; all1082 C/header/Go sources remain identical to the accepted donor. Linux both architectures pass Go17/staging/race and two actual PNG consumers each; FreeBSD AMD64 passes its fresh Go17/default-X family and clean runner teardown. FreeBSD ARM64 v11 retains an unresolved xlock I/O-wait failure and test-only SDK compilation timeout; its exact xlock binary passes three unchanged27-case controls on the existing p4 guest, and final fresh v12 qualification runs. OpenBSD standard suites and Go provider jobs continue. [Report](verification/VERIFICATION-2026-10-13-CI-PNG-BOOTSTRAP.md).

- 2026-10-13: CI-01 final focused source23c6 gives the four bounded15-second verification-profile calls a75-second outer allowance; twelve cases pass on six targets with application/inner deadlines unchanged. Both OpenBSD standard runs retain232 passes and the original30-second fixture timeout; affected fresh core staging/PNG gates run without repeating those232 entries. Sourcecec1 synchronizes the staged Script replacement fixture with an open reader and holds its writer through cancellation; eighteen complete native repetitions retain strict diagnostics, neighboring/replacement state and helper hashes. Production C/header/Go sources remain unchanged. Both FreeBSD fresh Go/default-X checkpoints pass, including corrected ARM64 transport41.32 seconds and clean teardown; OpenBSD AMD64 passes Go17/installed7/default-X6/two PNG consumers. OpenBSD ARM64 staged completion remains in progress after its retained fixture race. [Report](verification/VERIFICATION-2026-10-13-CI-PNG-BOOTSTRAP.md).

- 2026-10-13: CI-01 approved PNG/provider checkpoint is locally qualified at finalcec1 with all1082 C/header/Go identities preserved. Helper027890 passes336 on six targets; Linux GCC/Clang PNG builds and both Go17/staged7/race/PNG2 profiles pass. Four BSD standard/install profiles qualify233 each (OpenBSD retained232 plus four affected controls, matching compiler/dependency/configuration, staged PNG4); four BSD Go families pass17/installed7/default-X6/PNG2, with24 total default-X fixtures and20 cookie-less refusals. Final OpenBSD ARM64 transport30.00 seconds, strict staged Script readiness and clean teardown pass. Sixty component test-binary identities, six uncached runtime gates, twelve profile cases and eighteen complete Script controls supplement exact archive/source/consumer audits. Failed trials, subsequent initial FreeBSD Go failures and Linux collection correction stay retained. Candidate-only artifact additions preserve actual loader receipts; active workflows are unchanged. Actual native release CI/workflow adoption remain blocked on an unconfigured repository destination; inherited container contents, physical support and publication remain separate. [Final bootstrap report](verification/VERIFICATION-2026-10-13-CI-PNG-BOOTSTRAP.md).

- 2026-10-13: The maintainer requested private gonz0w/fvwm-x, PR review, roadmap issues/Project and a brief maintained README, referencing DNS/Nomvium conventions. All review/tests/lint/validation run locally; Actions is disabled and inherited workflow/bot/funding metadata is archived. This supersedes remote-CI/workflow-adoption requirements while preserving native CPU, sanitizer, X11/default-server, hardware and release evidence gaps. Signed commits, author DCO, manual tracker/issue/Project reconciliation and coherent review batches are adopted. Repository bootstrap publishes the qualified frozen snapshot, excluding another chat's newer runtime edits; the original dirty checkout, builds and VM evidence are preserved. [Approved policy](decisions/LOCAL-REVIEW-GITHUB-2026-10-13.md).

- 2026-10-13: Private [gonz0w/fvwm-x](https://github.com/gonz0w/fvwm-x) is seeded from qualifiedcec1 with upstream history and all1082 C/header/Go files preserved. Actions is disabled before publication; active PR/signature/linear-history/conversation main rules and immutable v-tags are configured with no bypass/required remote checks. [Project6](https://github.com/users/gonz0w/projects/6) mirrors all81 stable tracker IDs, ten milestones and27 native prerequisites; twelve scoped accepted-component backfills do not close broader release gates. Remaining monitor/protocol and runtime-controller work is correctly assigned to stage5. Local foundation verification passes96 on macOS with5 explicit Meson skips; those5 pass in the isolated Linux ARM64 lab. Legacy imported whitespace and transient Project502 remain retained. Signed [PR #83](https://github.com/gonz0w/fvwm-x/pull/83) is the bounded local-review/roadmap foundation for maintainer review, not an automatic merge/release. [Setup report](verification/VERIFICATION-2026-10-13-GITHUB-FOUNDATION.md).

- 2026-10-13: MAINT-02 source-tool child #84/#85 is one locally qualified review batch: bounded file hashing and disk-spooled development archives, Go directive/manifest provenance and explicit pkg-config absence. 75 affected Linux/BSD passes retain 15 missing-Git skips; macOS portable 111 passes/five native-Meson skips and archive byte parity are recorded. No configuration, dependency or runtime change; source-release adoption stays separate. [Report](verification/VERIFICATION-2026-10-13-SOURCE-TOOLS.md).

- 2026-10-13: The maintainer requested reduced build/PR overhead. MAINT-02 #87 groups scalar-option/source identity reuse, no redundant install build and explicit one-PR checkpoint cadence. Final 72 affected Linux/BSD policy/Meson cases pass, with test-only prerequisites preserved; macOS portable 121 passes/seven native-Meson skips and the earlier OpenBSD timeout/retry remain explicit. Related docs/evidence stay together; existing runtime matrices and qualification cache-refresh contracts are unchanged. [Report](verification/VERIFICATION-2026-10-13-BUILD-EFFICIENCY.md).

- 2026-10-13: MAINT-02 #89 restores the published developer-document/evidence closure: 370 frozen originals, exact receipt data and explicit portable/historical navigation. Current offline file-link checks pass; 96 native Python policy controls and local 137 passes/seven native-Meson skips are recorded. Runtime/source/dependency/default/automation adoption is unchanged; broader source-release and DOCS-01 gates remain. [Report](verification/VERIFICATION-2026-10-13-DOCUMENTATION-EVIDENCE.md).

- 2026-10-14: LIB-03 child #91 combines six-target advisory/source triage, removal of unused Logrus and one offline native checkpoint. All 426 retained third-party vendor payload files and six used pins are unchanged; 42 executables, 60 Go package results, 3114 test/subtest records, vet and 60 Prompt observations pass. The old Go 1.22.2 scan retains conservative findings and reviewed platform/input limits. Patched optional compiler support is a proposed external requirement, not adopted. Documentation child #89 merged in PR #90 and its board state is reconciled. [Report](verification/VERIFICATION-2026-10-14-GO-DEPENDENCIES.md).

- 2026-10-14: The user approves current patched Go unless major issues emerge. LIB-03 #12/#91 completes the optional stable compiler policy and combined six-target SDK checkpoint without a first-party C/Go or configuration-format change. Actual Go 1.26.9 is also tested. The BSD provider recipe selects fixed verified Go 1.27.2 SDKs privately; its source/registration/refusal and four BSD extraction controls pass. Two OpenBSD AMD64 VESA startup faults precede any client, then unchanged old/current client and Script replacement controls pass; cause remains unestablished under LIB-07 #93. The original archives are retained and no WM/full distribution matrix is repeated. [Report](verification/VERIFICATION-2026-10-14-GO-BASELINE.md).

- 2026-10-14: LIB-07 #93 groups server-only repetition, truthful attempt/dependency receipts, cancellation/teardown and signed-updated/rebooted guest comparison in one diagnostic PR. Affected six-target checks pass; original startup failures, preparation lock refusal and extra dispatch controls remain explicit. Cause/repair remains open. Go PR #92 merged; LIB-03/#91 are complete for their bounded compiler/dependency checkpoint. [Report](verification/VERIFICATION-2026-10-14-BSD-X-STARTUP.md).

- 2026-10-14: The user defers LIB-07 #93 until much later and resumes the window-manager backlog. CACHE-01 #33 qualifies measured variant filtering, reload/refusal and retained pixels in one combined checkpoint. A 65-variant lookup now checks one file stamp instead of 65. Eight standard compiler profiles across six targets pass 1936 entries; optional profiles add 423, default BSD servers pass 12 cache/staged-WM fixtures, and instrumented cache checks pass. 150 installed code identities and 99 catalogs validate. Broader profiling and release/hardware gates remain open. [Report](verification/VERIFICATION-2026-10-14-PICTURE-CACHE.md).

- 2026-10-14: PR #95 is merged; the user continues window-manager backlog work. LOOP-01 qualifies native poll/deadlines, shared pinned snapshots and actual-core continuation/order/retirement coverage in one combined checkpoint. Typed native poll snapshots and existing death-row pins now serve ordinary/interactive core waits. Eight standard compiler profiles pass 1944 entries across six targets; optional profiles add 425, default BSD servers pass 16 core/cache/interactive geometry fixtures, and two instrumented checks pass. 150 installed code identities and 99 catalogs validate. Broader descriptor/runtime release gates remain open. [Report](verification/VERIFICATION-2026-10-14-CORE-READINESS.md).

- 2026-10-15: PR #96 merged; the user continues LOOP-01 through the synchronous-wait checkpoint. Unlock, blocked-output and startup waits use native poll with shared absolute-deadline conversion. Retained full coverage passes 1944 standard and 425 optional entries; the final timeout refinement passes 50 affected entries across the same profiles. 150 final installed code identities and 99 catalogs match. Default BSD X passes 20+8 fixtures; three scoped sanitizer entries pass. Broader admission/runtime/release audits remain open. #93 remains deferred. [Report](verification/VERIFICATION-2026-10-15-MODULE-WAITS.md).

- 2026-10-15: PR #98 merged. STYLE-01's three-story actual-core baseline measures distinct-ID simplification, ordered late lookup and bulk-update matching inside the server grab. The affected checkpoint passes ten profiles, four authenticated default BSD servers and scoped sanitizer: 165 workload observations, unchanged update-ordering controls and 150 code/99 catalog identities. Production optimization remains queued; the parent full suite's two timing gaps remain open and #93 stays deferred. [Report](verification/VERIFICATION-2026-10-15-STYLE-PROFILE.md).

- 2026-10-15: PR #99 merged. STYLE-01's distinct-ID filter is reviewable in PR #100: fixed 4-KiB transient storage skips unique-ID backward scans, reducing 2016/8128/32640 measured comparisons to zero. All affected tests pass ten profiles, four default BSD X targets and scoped sanitizer. Full scope retains seven original failures; seven unchanged controls pass three and fail four OpenBSD AMD64 cases, leaving its fresh install/staged geometry unperformed. Nine fresh stages validate 135 code/88 catalog identities; 18 geometry invocations, 330 workloads and 120 update observations pass. Full platform/native-CPU/hardware/release and broader matching/grab/repeated-ID work remain open; #93 stays deferred. [Report](verification/VERIFICATION-2026-10-15-STYLE-FILTER.md).

- 2026-10-15: The user requested faster development through larger implementation batches and less frequent expensive tests. Default to three to five substantive related improvements per combined checkpoint; tests/probes/docs do not inflate that count. Use focused incremental checks between improvements, one required broad qualification pass at the batch boundary, targeted reruns after fixes and documentation-only checks for upkeep. Preserve known failures and final acceptance coverage. [Cadence](WORKFLOW.md#implementation-batches-and-test-cadence).

- 2026-10-15: PR #100 merged. The user approved continued backlog work with larger substantive batches. STYLE-01 now implements prepared literal/universal matching, prefix rejection, compatibility-field deduplication and early WindowID rejection, plus the reproduced Name-before-WindowID parser repair. Focused Linux policy/filter/core/update checks pass; a single combined checkpoint is running on a frozen candidate with at most two guest jobs concurrently. Prior broad failures stay visible; #93 remains deferred. [Evidence](verification/VERIFICATION-2026-10-15-STYLE-MATCHING.md).

- 2026-10-15: STYLE-01 five-item matching batch is reviewable in PR #101. Five related matching improvements share one checkpoint. All four affected tests pass ten profiles; core/update probes pass four default BSD X targets, and four scoped sanitizer checks pass. Original full runs pass 1967/1976 standard and 433/433 optional entries. Controls leave five OpenBSD AMD64 gaps and its fresh-install gap open. Nine fresh stages verify 135 code/88 catalog identities; 18 geometry invocations, 330 workloads and 90 matching-cost rows pass. Original failures and exact source/profile scope remain visible; native-CPU/hardware/release and broader matching/grab owners remain open. One matrix run for five substantive items, no retries of previously known failures and one authenticated default display per BSD target implement the clarified cadence. #93 remains deferred. [Report](verification/VERIFICATION-2026-10-15-STYLE-MATCHING.md).

- 2026-10-15: PR #101 merged. STATE-01 begins a three-item traversal/dispatch batch with focused Linux and OpenBSD ARM64 passes. The single combined checkpoint preserves existing barriers and prior failures; #93 remains deferred. [Report](verification/VERIFICATION-2026-10-15-EWMH-STATE.md).

- 2026-10-15: STATE-01 traversal batch is reviewable in PR #102. Three related improvements share one checkpoint. Eight affected/retained entries pass ten profiles; core probes pass four default BSD X targets, and three scoped sanitizer entries pass. Original full runs pass 1990/1992 standard and 437/437 optional entries. Fresh 8 stages verify 120 code/77 catalog identities; 16 geometry invocations pass. Exact broader failures/install gaps remain in the report. One combined matrix serves all three substantive improvements; exact failures/source/profile scope remain explicit. VESA #93 stays deferred. [Report](verification/VERIFICATION-2026-10-15-EWMH-STATE.md).

- 2026-10-16: PR #102 merged. STYLE-01 implements a three-item wildcard batch with focused Linux checks passed; one combined checkpoint is pending. VESA #93 remains deferred. [Report](verification/VERIFICATION-2026-10-16-STYLE-WILDCARDS.md).

- 2026-10-16: STYLE-01 wildcard checkpoint is locally qualified; Three related wildcard improvements are locally qualified. The original matrix records 1988/1992 standard and 437/437 optional passes; its four BSD counter assertions are resolved by portable outer-call accounting. Final affected scope passes 30 native entries, three scoped sanitizer entries and ten staged client probes / 70 cases; all ten stages verify 150 code/99 catalogs. Four final default BSD displays add eight geometry invocations / 56 cases. One full matrix serves the batch; corrected results and failures retain exact source scopes. [Report](verification/VERIFICATION-2026-10-16-STYLE-WILDCARDS.md). Public PR review proceeds under the [hosting decision](decisions/PUBLIC-HOSTING-2026-10-16.md). VESA #93 remains deferred.

- 2026-10-16: PR #103 merged. STYLE-01 continues with one three-story repeated-ID simplification batch. Native Linux baseline records 65 string comparisons, 65 unrelated prefix visits and three surviving records; focused candidates reduce these to one, one and two with unchanged lookup outcomes. Combined six-target qualification follows the focused checks; VESA #93 remains deferred. [Report](verification/VERIFICATION-2026-10-16-STYLE-SIMPLIFICATION.md).

- 2026-10-16: STYLE-01 three-story repeated-ID checkpoint is locally qualified in PR #104. Three repeated-ID improvements are locally qualified. Original observations pass 1991/1992 standard and 437/437 optional entries across six targets / ten profiles. The one failed startup fixture is corrected and passes on unchanged binaries; its original failure remains visible. 4 scoped sanitizer entries pass. 10 fresh stages bind 150 code/99 catalogs; four default BSD displays and 10 staged real-client invocations pass. One full checkpoint serves the batch; preparation failures and scoped limitations remain explicit. [Report](verification/VERIFICATION-2026-10-16-STYLE-SIMPLIFICATION.md). VESA #93 remains deferred.

- 2026-10-18: PR #104 merged. STYLE-01 continues with three related ordered
  candidate-lookup improvements, retaining original matching and precedence.
  Focused Linux and OpenBSD ARM64 checks pass; one combined checkpoint remains
  pending. VESA #93 remains deferred.
  [Report](verification/VERIFICATION-2026-10-18-STYLE-LOOKUP.md).

- 2026-10-18: STYLE-01 ordered candidate lookup is ready for review in PR #105. Three ordered candidate paths share one checkpoint. Original observations pass 1989/1992 standard and 437/437 optional entries across six targets / ten profiles; 4 scoped sanitizer entries pass. 10 fresh stages bind 150 code/99 catalog identities. 5 authenticated default BSD displays and 10 staged real-client invocations pass. One full matrix serves the batch; hashing/collision/cold-build and release limitations remain explicit. [Report](verification/VERIFICATION-2026-10-18-STYLE-LOOKUP.md). VESA #93 stays deferred.

- 2026-10-18: STYLE-01 #38 subject work completes one three-story checkpoint in
  [PR #106](https://github.com/gonz0w/fvwm-x/pull/106): deferred compatibility
  comparisons, domain-selective index queries and shared hash/length scans.
  Exact source, focused checks, one combined matrix and release limits are in
  the [report](verification/VERIFICATION-2026-10-18-STYLE-SUBJECT.md).

- 2026-10-18: MAINT-02/CLEAN-01 interleaves one three-story development-speed checkpoint after merged PR106. PR107 shares 74 production core objects, reduces operation-only style header fanout to 7 and makes index-layout changes private to 2 owner objects. One affected six-target/ten-profile checkpoint passes 70 entries, preserves allocated sections in 1502 core/probe objects and ten WM executables, and verifies 249 fresh staged identities. Four continuations retain earlier successful commands and finish remaining profiles without rerunning passed phases. Portable 166 passes/11 capability skips and exact source/failed-collection scopes are recorded. Broader runtime/default-X/sanitizer evidence and its original failure/control stay with PR106; native-CPU/hardware/release gaps and VESA93 remain separate. [Report](verification/VERIFICATION-2026-10-18-DEVELOPMENT-BUILD-REUSE.md).

- 2026-10-18: STYLE-01's three unescaped wildcard families complete one combined checkpoint in PR #108 after merged PR #107. Exact-width question rejection, anchored one-star question spans and constant-stack general matching retain the independent legacy predicate and empty-tail/escape behavior. Original runs pass 1992/1992 standard and 437/437 optional entries; four scoped sanitizers, 10 fresh stages, 4 authenticated BSD displays and 630 family rows retain exact source scope. Metadata adds eight bytes per rule on the measured ABI; no universal linear-time/latency claim. Historical failures and release/native-CPU/hardware gaps remain explicit. No full suite is repeated per story/PR/report; VESA #93 stays deferred. [Report](verification/VERIFICATION-2026-10-18-STYLE-CURSORS.md).

- 2026-10-18: PR #108 merged. STYLE-01 batches exact/minimum length, fixed-position and literal-membership disjointness proofs. Three measured actual-core lists shrink from three records to two with unchanged witness results and deletion; focused Linux/OpenBSD ARM pass four affected entries and twenty real-client cases each. One combined checkpoint follows; no update/grab/lifetime change or global glob replacement is implied. [Report](verification/VERIFICATION-2026-10-18-STYLE-OVERLAP.md). VESA #93 stays deferred.

- 2026-10-18: PR #109 is ready for review after merged PR #108. Three selector proofs reduce three selected lists from three records to two, preserving reference/collision/matching/removal owners. Final bounded source passes 40/40 affected entries plus four scoped sanitizers, nine fresh stages and four authenticated BSD displays; original broad source passes 1991/1992 standard and 437/437 optional entries. The unchanged OpenBSD AMD64 SVG startup failure and its fresh-install/staged-geometry gap remain under IMAGE-01. No universal latency claim. [Report](verification/VERIFICATION-2026-10-18-STYLE-OVERLAP.md). One broad matrix serves all three stories; the review bounds correction repeats affected gates only. Proof helpers add no retained metadata/heap storage; existing merge copy/free paths remain. VESA #93 stays deferred.

- 2026-10-18: PR #109 merged. STYLE-01 batches three escaped matching families: decoded fixed widths, anchored one-star spans and constant-stack general cursors. Focused Linux checks pass, including 396396 independent legacy comparisons and 26 real-client cases. Nine measured legacy matcher calls become zero with unchanged results/removal; metadata adds sixteen inline bytes. One combined checkpoint follows; IMAGE-01's SVG startup/install gap remains, and #93 stays deferred. [Report](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED.md).

- 2026-10-18: PR #110 is ready for review after merged PR #109. Three escaped matching families replace recursive entries with decoded fixed widths, anchored one-star spans and constant-stack general cursors. Nine selected legacy calls become zero with unchanged results/removal. 1991/1991 executed standard and 437/437 optional entries pass; one known required OpenBSD AMD64 SVG test stays omitted under IMAGE-01, preserving its install/staged-geometry gap. Four scoped sanitizers, nine fresh stages, four authenticated BSD displays and 810 family rows pass. Metadata adds sixteen inline bytes; no universal latency/linear-time claim. [Report](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED.md). One full matrix serves all three stories; no repeat for a story, PR or documentation update. VESA #93 stays deferred.

- 2026-10-18: PR #110 merged. STYLE-01 batches three escaped selector disjointness proofs using decoded lengths, quoted fixed positions and bounded literal membership. Three selected lists reduce from three records to two while reference ownership/results/removal remain asserted. Combined qualification pending; IMAGE-01's known required SVG omission/install gap and deferred #93 stay separate. [Report](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED-OVERLAP.md).

- 2026-10-18: PR #111 is ready for review after merged PR #110. Three escaped selector proofs use decoded lengths, quoted fixed positions and bounded literal membership. Three selected lists retain two records instead of three, preserving initialized lookup, precedence and removal. Original breadth passes 1991/1991 executed standard and 437/437 optional entries; the final literal-fast-path correction passes 40/40 affected entries plus four scoped sanitizers. Nine final stages, four authenticated BSD displays and 290 final client cases pass. One required OpenBSD AMD64 SVG test stays omitted under IMAGE-01 with its install/staged-geometry gap. No metadata/allocation owner change or universal latency claim. [Report](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED-OVERLAP.md). One broad matrix plus affected-only review fixes serves the three stories; no full rerun for a story, formatting, docs or PR. Six observed detached guest jobs remain capacity-bound, and each correction waits for its guest’s broad archive before source changes. VESA #93 stays deferred.

- 2026-10-19: PR #111 merged. STATE-01 batches three EWMH atom changes; focused Linux checks pass. The combined checkpoint passes in its executed scope, preserving IMAGE-01's known required SVG omission/install gap and deferred VESA #93. [Report](verification/VERIFICATION-2026-10-19-EWMH-ATOMS.md).

- 2026-10-19: STATE-01 atom checkpoint is ready for review. Three EWMH atom improvements pass one combined checkpoint: 1991/1991 executed standard and 437/437 optional entries across six targets/ten profiles, three scoped sanitizers, nine fresh stages, four authenticated default BSD displays and 18 geometry invocations/126 cases. The known required OpenBSD AMD64 SVG omission/install gap remains under IMAGE-01. Warm builds and one matrix serve the three items; no full tests repeat for publication. [Report](verification/VERIFICATION-2026-10-19-EWMH-ATOMS.md).

- 2026-10-19: PR #112 merged. STYLE-01 batches decoded head/tail conflicts and zero-slack interior contradictions; three measured lists reduce from three records to two with unchanged initialized results/removal. Focused checks pass; one combined checkpoint follows. OpenBSD owner storage is byte/mode/mtime-verified and relocated before writes. Known IMAGE-01 required SVG/install gap and deferred VESA #93 remain. [Report](verification/VERIFICATION-2026-10-19-STYLE-ANCHORS.md).

- 2026-10-19: STYLE-01 decoded-anchor checkpoint is ready for review. Three decoded style proofs pass the affected scope of one combined checkpoint: 1990/1991 executed standard (one retained CI-worker timeout) and 437/437 optional entries across six targets/ten profiles, four scoped sanitizers, nine fresh stages and four authenticated default BSD displays. OpenBSD AMD64 retains the unchanged CI-01 two-second worker failure and IMAGE-01 required SVG omission/install gap. One matrix serves three stories; no full tests repeat for publication. [Report](verification/VERIFICATION-2026-10-19-STYLE-ANCHORS.md).

- 2026-10-19: PR #113 merged. STYLE-01 field ordering, cached-length refusal and verified-prefix reuse pass focused Linux checks; one combined checkpoint follows, preserving CI/SVG required gaps without retries. [Report](verification/VERIFICATION-2026-10-19-STYLE-FIELDS.md).

- 2026-10-19: STYLE-01 field-work checkpoint is ready for review. Three style field improvements pass 40/40 affected entries across six targets/ten profiles. One combined checkpoint passes 1990/1990 executed standard and 437/437 optional entries, four scoped sanitizers, nine fresh stages and four authenticated default BSD displays. Two known required OpenBSD AMD64 CI/SVG entries stay omitted, preserving its install/staged-geometry gap without retries. One matrix serves three stories; no full tests repeat for publication. [Report](verification/VERIFICATION-2026-10-19-STYLE-FIELDS.md).

- 2026-10-19: PR #114 merged. STYLE-01 batches shared plain/escaped wildcard cursors and anchored literal suffixes; five focused Linux entries and 41 actual-client cases pass. One combined checkpoint follows; CI/SVG required gaps are retained without retries and VESA #93 stays deferred. [Report](verification/VERIFICATION-2026-10-19-SHARED-WILDCARDS.md).

- 2026-10-19: STYLE-01 shared-wildcard checkpoint is ready for review. Three shared wildcard improvements pass 50/50 affected entries across six targets/ten profiles. One combined checkpoint passes 1998/1998 executed standard and 439/439 optional entries, five scoped sanitizers, nine fresh stages and four authenticated default BSD displays. Two known required OpenBSD AMD64 CI/SVG entries stay omitted, preserving its install/staged-geometry gap without retries. One matrix serves the three families; no full test repeats for final formatting or publication. [Report](verification/VERIFICATION-2026-10-19-SHARED-WILDCARDS.md).

- 2026-10-19: PR #115 merged. STYLE-01 batches border colorset invalidation, global pass scheduling and lazy sticky contexts. Focused checks and one combined checkpoint retain CI/SVG required gaps and deferred VESA #93. [Report](verification/VERIFICATION-2026-10-19-STYLE-UPDATE-WORK.md).

- 2026-10-19: STYLE-01 style-update checkpoint is ready for review. Three style-update improvements pass a composed affected checkpoint: one original broad run (2005/2006 executed standard, 441/441 optional), seventy production-correction checks and eighteen fixture-correction checks. The original image-effects timeout is traced to a discarded early acknowledgement and corrected without changing production deadlines. Six original sanitizer checks, three corrected actual-core checks, nine final fresh stages and four final authenticated BSD displays pass. Two known required OpenBSD AMD64 CI/SVG omissions and its install gap remain without retries. One full run; final correction uses affected-only gates. [Report](verification/VERIFICATION-2026-10-19-STYLE-UPDATE-WORK.md).

- 2026-10-19: PR #116 merged. Three style-command fixes pass a composed checkpoint: one original broad run (2013/2014 executed standard, 443/443 optional), then ten scheduler-fixture correction checks on unchanged production binaries. Fifteen ownership drivers retain zero selected fields and perform zero refused-input grabs. Three scoped sanitizer checks, nine composed fresh stages and four authenticated BSD displays pass. Two known required OpenBSD AMD64 CI/SVG omissions and its install/staged-geometry gap remain without retries. One full source-bound checkpoint serves three substantive fixes; the scheduler correction uses affected-only checks. [Report](verification/VERIFICATION-2026-10-19-STYLE-COMMAND-OWNERSHIP.md).

- 2026-10-19: PR #117 merged. STYLE-01 batches incremental index append, retirement and simplification moves; seven focused entries and 41 real-client cases pass, followed by the extended fixture only. One combined checkpoint follows; known CI/SVG required gaps and deferred VESA #93 remain. [Report](verification/VERIFICATION-2026-10-19-STYLE-INDEX-MAINTENANCE.md).

- 2026-10-19: Incremental style-index checkpoint is ready for review. Three incremental style-index improvements pass a composed checkpoint: one original broad run across six targets/ten profiles (2019/2022 executed standard, 445/445 optional), followed by three unchanged isolated fixture controls. Fifteen actual-core index drivers retain zero selected lookup allocations after append, deletion and all three simplification paths. Three scoped sanitizers, nine fresh stages and four authenticated BSD displays pass. Two known required OpenBSD AMD64 CI/SVG omissions and its install/staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-19-STYLE-INDEX-MAINTENANCE.md). One matrix serves the three items; no full test repeats for report or publication. VESA #93 stays deferred.

- 2026-10-19: After merged PR #118, MAINT-02/CLEAN-01 completes a three-boundary style dependency checkpoint. Three style dependency boundaries pass 70/70 affected native entries across six targets/ten profiles. 5844 object pairs and all production WM/module outputs retain allocated code/data sections; ten matcher-test objects have separately proved assertion-line-only differences. Ten scoped stages verify 249 code/catalog identities. Broader PR118 runtime failures/controls, required OpenBSD CI/SVG omissions and install/staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-19-STYLE-TYPE-BOUNDARIES.md). No full runtime tests repeat for publication; the dependency/indentation amendments retain passing tests with native code/data parity.

- 2026-10-19: After merged PR #119, STYLE-01 completes live-domain accounting, empty-index release and typed category flags. Three related improvements pass a composed affected checkpoint: one original broad run records 2029/2030 executed standard and 447/447 optional entries across six targets/ten profiles, plus one unchanged isolated legacy-property control. Fifteen actual-core retention drivers verify live-domain counts, empty-table release and seven-category precedence; fifteen retained maintenance drivers cover incremental mutation and simplification. Four scoped sanitizers, nine fresh stages and four authenticated BSD displays pass. Two known required OpenBSD AMD64 CI/SVG omissions and its install/staged-geometry gap remain without retries. Selected long-field hashing drops from four fields to one, category records from 96 to 56 bytes, and active indexes add 48 bytes each. [Report](verification/VERIFICATION-2026-10-19-STYLE-INDEX-RETENTION.md).

- 2026-10-19: After merged PR #120, MAINT-02/CLEAN-01 completes three command/update dependency boundaries. Three command/update dependency improvements pass 100/100 affected native entries across six targets/ten profiles. All 5864 object pairs and ten WM executables retain identical allocated code/data sections; 249 installed-output pairs preserve their code/data or non-ELF bytes. Ten scoped stages verify 249 code/catalog identities. Parser, complex-function and update-plan layout edits now compile six, four and four objects instead of 204–209, 71 and 116–121. Broader PR120 runtime evidence/failures and required OpenBSD CI/SVG/install gaps remain without retries. One affected checkpoint serves the batch; no full runtime matrix repeats for publication. [Report](verification/VERIFICATION-2026-10-19-COMMAND-TYPE-BOUNDARIES.md).

- 2026-10-19: After merged PR #121, STATE-01 completes three EWMH publication improvements. Three EWMH publication improvements pass one combined checkpoint: 2029/2029 executed standard and 447/447 optional entries across six targets/ten profiles. Selected strut walks fall from two to one, permission queries from nine to six and converted-name length scans from 189 to 63. Four scoped sanitizers, 9 fresh stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its broader install/staged-geometry gap remain without retries. Original compile/capacity attempts and detached-job recovery are preserved; no completed suites repeat for publication. The user requested an isolated interactive QEMU/VNC desktop left running after PR readiness. [Report](verification/VERIFICATION-2026-10-19-EWMH-PUBLICATION.md).

- 2026-10-19: PR122 merged; user reports a fast, working interactive WM and requests shutdown/continuation. Review-only VM/VNC stopped with disk/logs retained. MAINT-02/CLEAN-01 completes three UI record boundaries. Three font/picture/monitor dependency improvements pass 170/170 affected native entries across six targets/ten profiles (17 focused entries retained by exact source/binary identity, 153 fresh). All 5874 object pairs, ten WM executables and 249 install-output pairs preserve allocated code/data or non-ELF bytes. Ten scoped stages verify 249 code/catalog identities. ARM GCC layout fanout falls from 252/289/226 to 49/66/92 objects. Broader runtime evidence and required OpenBSD gaps remain source-scoped to PR122. One affected checkpoint serves the batch. [Report](verification/VERIFICATION-2026-10-19-UI-TYPE-BOUNDARIES.md).

- 2026-10-19: PR123 merged. STATE-01/CLEAN-01 starts one bounded existing-tray membership batch: reuse insertion and retirement links and the inline snapshot helper. Four focused Linux entries pass, with 141 baseline / 145 candidate cases per mode. Two selected 64-window snapshot requests become zero; source-structural duplicate scans disappear. Combined qualification remains pending; required OpenBSD gaps and deferred VESA remain.

- 2026-10-19: STATE-01/CLEAN-01 existing-tray membership checkpoint is ready for review after PR123. Three existing legacy-tray insertion/retirement/publication improvements pass one combined checkpoint: 2029/2029 standard results, fresh or retained after source/binary verification, and 447/447 optional entries across six targets/ten profiles. Four focused standard entries are retained; 2025 standard entries run fresh. Selected 64-window add/remove snapshots each reduce heap requests from one to zero. Source review shows two insertion/retirement searches become one, and two publication walks become one. Four scoped sanitizers, 9 fresh full stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its broader install/staged-geometry gap remain without retries. One full checkpoint serves the batch; focused GCC results are retained through exact source/binary identity. Original probe failure/correction and all required gaps remain explicit. [Report](verification/VERIFICATION-2026-10-19-EWMH-TRAY.md).

- 2026-10-19: PR124 merged. STYLE-01/CLEAN-01 selects three style-matching operations for one combined checkpoint: selector preparation, universal explicit predicates and ordered candidate merging. Production remains unchanged while independent baseline probes are captured; focused and combined qualification remain pending.

- 2026-10-19: The three STYLE-01/CLEAN-01 matching changes pass eight focused Linux entries. Unchanged-production unit and corrected actual-core baselines are source-bound; the first probe assumption and diagnostic remain. Preparation escape scans fall eight to zero. Linear rule/result equivalence and all nine stream slots pass; one combined checkpoint remains pending.

- 2026-10-19: STYLE-01/CLEAN-01 matching checkpoint is ready for review after merged PR124. Three related style preparation/predicate/candidate-merge improvements pass one combined checkpoint: 2029/2029 standard results (2021 fresh / eight retained by exact source and ten binary identities), plus 447/447 optional entries across six targets/ten profiles. Instrumented escape scans fall eight to zero for twelve preparations. 180 actual-core merge cases and 132 preparation cases pass. Four scoped guest sanitizers, nine fresh full stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/staged-geometry gap remain without retries. Failed/diagnostic/corrected baseline evidence and timing limits remain; one full checkpoint serves the batch. Prior gate-command wording is corrected without rerunning or relabelling evidence. [Report](verification/VERIFICATION-2026-10-19-STYLE-MATCH-WORK.md).

- 2026-10-20: After merged PR127, MAINT-02/CLEAN-01 qualifies three private condition/startup/monitor-adapter record boundaries. Three condition/startup/monitor-adapter dependency improvements pass 149/149 affected native entries across six targets/ten profiles (15 retained focused / 134 fresh). All 5914 object pairs, ten WM executables and 249 install-output pairs preserve allocated code/data or non-ELF bytes. Ten scoped stages verify 249 identities. ARM GCC layout fanout falls 214/214/230 to 2/22/2 objects. Broader PR127 runtime evidence and required OpenBSD gaps remain source-scoped. [Report](verification/VERIFICATION-2026-10-20-PRIVATE-RECORD-BOUNDARIES.md).

- 2026-10-20: After merged PR128, CACHE-01/CLEAN-01 qualifies three cache variant/name/retirement improvements. Three picture-cache variant/name/retirement improvements pass one combined checkpoint: 2045/2045 standard results (2038 fresh / seven retained by exact source and nine binary identities), plus 451/451 optional entries across six targets/ten profiles. Selected pathname comparisons fall 65 to one/two; two predecessor walks fall 16,384/32,385 to zero. Five scoped guest sanitizers, nine fresh full stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-20-PICTURE-CACHE-WORK.md).

- 2026-10-20: After merged PR129, DATA-01/IMAGE-01/CLEAN-01 qualifies three donated-icon record/size/retirement improvements. Three donated-icon record/size/retirement improvements pass one combined checkpoint: 2045/2045 standard results (2037 fresh / eight retained by unchanged production/test/build inputs and nine binary identities), plus 451/451 optional entries across six targets/ten profiles. Selected partial deletion allocations fall three to two. Fifteen actual-core drivers verify 510 icon cases; four scoped guest sanitizers, nine fresh stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-20-EWMH-ICON-RETENTION.md).

- 2026-10-20: After merged PR130, DATA-01/CLEAN-01 qualifies bounded ancestor lookup, pointer lookup and checked complete frame translations. Three ancestor/pointer/frame lookup improvements pass one combined checkpoint: 2061/2061 standard results (2055 fresh / six retained by unchanged runtime/test/build inputs and eight binary identities), plus 455/455 optional entries across six targets/ten profiles. Fifteen real-X drivers verify 45 hierarchy/pointer/reparent observations; ten boundary-policy profiles and three scoped guest sanitizers pass. Nine fresh stages and four authenticated BSD displays pass. Three required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. [Report](verification/VERIFICATION-2026-10-20-WINDOW-TREE-WALKS.md).

- 2026-10-20: PR #132 merged. The next FD-01 / PROC-01 / DATA-01 batch
  repairs three active process/session constructors: checked Exec stdin and
  non-flushing failure exit; complete authentication preparation before
  credentials are published; checked session network/environment publication
  with watch/resource rollback. Focused Linux and FreeBSD constructor,
  compatibility and real session checks pass; combined qualification pending.
  The accepted PR132 performance reference starts at 0/5 subsequent merged
  implementation checkpoints; no regular comparison is due for this batch.

- 2026-10-20: Three process/session constructor repairs pass one composed checkpoint after a single full matrix: 2141 composed standard results (2116 original fresh passes, 24 retained after unchanged-input/eight-binary checks per focused target, and one corrected Exec probe), plus 468 optional results across six targets/ten profiles. Eleven actual-core Exec drivers verify 77 observations; seventy session fault/retry entries and nineteen real session integrations pass. Nine scoped guest ASan/UBSan checks, nine fresh stages and four authenticated default BSD displays pass. Three inherited required OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain without retries. Performance reference PR132 is accepted, at 1/5 subsequent merged implementation checkpoints; no comparison is due. [Process-startup report](verification/VERIFICATION-2026-10-20-PROCESS-STARTUP.md). No runtime or full matrix repeat is triggered by publication.

- 2026-10-20: Three registration/pending-queue/property repairs pass a composed six-target/ten-profile checkpoint after one original full run: 2197 standard and 475 optional results. The composition retains 2516 unaffected original passes and uses 156 affected entries (122 fresh / 34 retained by exact inputs and eleven binaries per focused target). Fifty callback entries, twenty pending-peer entries, nineteen real-session drivers with 57 independent clients and 171 property replies, fifteen scoped guest sanitizer entries, nine fresh stages and four final authenticated default BSD session displays pass. Three inherited required OpenBSD AMD64 omissions and its install/two staged-geometry gap remain without retries. The existing no-automatic-save policy is preserved. Performance reference PR132 remains at 1/5; no comparison is due. [Session-ownership report](verification/VERIFICATION-2026-10-20-SESSION-CALLBACKS.md). Final review withdrew the initial-save proposal and qualified only affected session corrections; no full matrix repeat.

- 2026-10-20: PR134 merged; the three-story core record dependency checkpoint passes. Three module/icon-box/event-filter dependency improvements pass 185 affected native checks across six targets/ten profiles (19 focused retained / 166 fresh). All 5981 object pairs, ten WM executables and 249 install-output pairs preserve allocated code/data or non-ELF bytes. Ten scoped stages verify 249 identities. ARM GCC layout fanout falls 68/216/37 to 30/6/2 objects. Broader PR134 runtime evidence and inherited OpenBSD gaps remain source-scoped. [Report](verification/VERIFICATION-2026-10-20-CORE-RECORD-BOUNDARIES.md). Performance cadence: 2/5; no comparison due.

- 2026-10-21: OpenBSD 8.0 becomes the current qualified OpenBSD release baseline on both architectures. A composed local checkpoint passes 538 standard and 34 optional entries, full installs, 44 authenticated default servers and staged/default-X geometry at 1x/1.5x. One full run per profile is retained; three standard fixture entries and four optional scheduling entries run after scoped corrections. Original failures, 7.9 history and wider release limits remain explicit. [Report](verification/VERIFICATION-2026-10-20-OPENBSD-80.md).

- 2026-10-21: User clarification: follow future OpenBSD releases, with 8.0 the current qualified baseline and 7.9 out of scope. Refresh fixed inputs and both architecture gates before advancing support claims; existing 8.0 evidence is unchanged.

- 2026-10-21: STANDARDS-01 completes three bounded protocol inventory stories with forty stable requirement IDs, pinned references, source/fixture fingerprints, exact evidence limits and owner acceptance cases. Existing E/S rows are mapped; monitor rows and broader standards/runtime acceptance stay open. No unchanged runtime matrix or performance comparison is repeated. [Register](verification/STANDARDS-2026-10-21-PROTOCOL-REQUIREMENTS.md).

- 2026-10-21: PR135 is merged on main; PR137 merged into PR136. Resolve their README/tracker overlap while preserving all three checkpoints, current-release OpenBSD policy and historical evidence. Performance cadence advances to 3/5 for merged PR135; the documentation-only inventory does not increment it. No runtime conflict or new runtime behavior is introduced.

- 2026-10-21: After merged PR136, shared string ownership is locally qualified and PR #138 is ready for review. Three shared first-line, multiline and quoted-copy operations retain existing bytes and ownership while avoiding before-source pointers and removing one quoted strlen rescan. One composed six-target checkpoint passes 2208 standard entries, 28 affected optional entries and one scoped guest sanitizer. Ten installs, staged geometry/styles and four authenticated default BSD displays pass; harness failures and wider release limits remain explicit. [Report](verification/VERIFICATION-2026-10-21-STRING-COPIES.md). Completed guest duplicates are retired after canonical identity checks. Performance cadence stays at 4/5 merged checkpoints; merging PR138 makes the next paired comparison due. No full suite is repeated for publication.

- 2026-10-21: After merged PR138, CI-01/MAINT-02/CLEAN-01 qualifies source modes, atomic live/failure status and separate/repeatable PNG loader proofs. Three preparation/evidence repairs pass 282 native policy cases on six targets, two fresh OpenBSD runner cases and six actual loader invocations; original WM/installed bytes remain exact. Live phases and an expected exit 23 case validate diagnostics/cleanup. Portable 177 passes / 11 skips; PR138 runtime evidence retained. [Report](verification/VERIFICATION-2026-10-21-BSD-PREPARATION.md). Due performance comparison records PR132/138 and resets the merged-checkpoint counter to 0/5 against preserved PR138; this PR is unmerged.

- 2026-10-21: User prioritizes high-impact safety/maintainability and defers
  remaining performance-only tuning to the next release. Define finite stream,
  external-input and interaction-lifetime pickups plus explicit dispositions
  and feature-handoff criteria; broad audit owners remain open. PR139 is merged,
  so lightweight performance tracking is at 1/5 against PR138. This priority
  update changes no runtime code and does not repeat a qualification matrix.

- 2026-10-21: PR146 merged. PR147 batches typed/range-checked legacy strut
  adoption, explicit deletion revocation and current per-desktop root work areas.
  One broad six-target checkpoint passes 2372 standard / 30 selected optional
  entries; the reproduced sticky-page review correction passes 90 affected
  continuation entries, four final scoped sanitizers, ten final installs and
  four final authenticated BSD displays. Failed selector/compiler/storage
  attempts remain explicit; canonical-verified guest duplicates are retired.
  [Composed report](verification/VERIFICATION-2026-10-21-WORKAREA.md).

- 2026-10-21: User approves the finite feature handoff: PR147 merged, one bounded manager-target checkpoint, then BUTTON/CFG/IPC features. Remaining general stage 5 maintenance stays open for a future release; only concrete feature prerequisites or reproduced serious safety defects may interrupt. [Decision](decisions/FEATURE-HANDOFF-2026-10-21.md).

- 2026-10-21: PR148 qualifies the finite manager-target handoff: 2396 standard / 38 selected optional entries, 72 affected follow-ups, nine fixture-continuation groups, four guest sanitizer entries and ten final installs. Original preflight, auxiliary loader and disconnect-observation failures retain explicit composed dispositions. Twenty open general stage 5 owners are in Later; BUTTON-01/02 and CFG-01 follow after review/merge. [Report](verification/VERIFICATION-2026-10-21-MANAGER-TARGETS.md). No full matrix repeats for publication; native-CPU/hardware/release gaps remain.

- 2026-10-22: merged PR148 completes the selected feature prerequisite. BUTTON-01 navigation/focus/disabled controls and related BUTTON-02 live presentation form the first feature batch; focused Linux/FreeBSD ARM64 checks pass, combined qualification pending. Atomic grouped updates remain next.

- 2026-10-22: PR149 qualifies four native Buttons feature improvements: opt-in traversal/activation, independent visible focus, disabled-action refusal/recovery and live enabled/selected/palette updates. 2412 standard / 8 selected optional entries, four scoped guest sanitizers, ten stages and four authenticated default BSD displays pass. All ten WM executables remain byte-identical to PR148; only the Buttons installed consumer changes. Atomic BUTTON-02 groups remain next, then CFG-01. Canonical-verified guest duplicates are retired; inherited manual diagnostics and physical/native-CPU/release gaps remain explicit. Performance cadence is 4/5 until merge. [Report](verification/VERIFICATION-2026-10-22-BUTTONS-KEYBOARD.md).

- 2026-10-22: PR149 merged. BUTTON-02 atomic groups form the next coherent
  contract: bounded whole-request validation, owned resource preparation/rollback,
  and final-state focus repair with one affected-controls repaint. Focused Linux
  checks pass; six-target combined qualification is pending. The initial prefix
  dispatch failure is retained and corrected. CFG-01 is next; no extra general
  maintenance is pulled forward. The due PR144→149 preserved-binary performance
  comparison is recorded and resets the cadence to 0/5 against PR149.
  [Performance observation](verification/PERFORMANCE-2026-10-22-PR149.md).

- 2026-10-22: BUTTON-02 is locally qualified for review. One complete atomic
  resource/state contract covers bounded validation, prepared resource rollback,
  failed-startup filename recovery and final-state focus plus bounded repaint.
  Final source passes 106 fresh affected entries across six targets/ten profiles,
  five leak-enabled guest sanitizers, ten fresh stages and four authenticated
  default BSD displays. 2332 unchanged PR149 standard entries are retained after
  exact input/configuration/consumer checks; composed totals are 2420 standard
  and 18 selected optional. Original fixture/control failures and declined
  failed-copy cleanup remain explicit. BUTTON-02 completes on merge, followed
  by CFG-01. PR149 performance reference remains 0/5; no new tuning is selected.
  [Report](verification/VERIFICATION-2026-10-22-BUTTONS-ATOMIC.md).

- 2026-10-22: PR150 merged. CFG-01 selects three related existing-helper improvements: shared typed registry/query, safe saved/effective change/reset/reload, and registry-generated native settings menus. Preview/recovery remains open and unavailable; runtime Go port stays with its owner. PR149 performance reference advances to 1/5; no comparison due.

- 2026-10-22: CFG-01 first consumer checkpoint is locally qualified: 2420 composed standard / 531 optional entries across six targets/ten profiles, ten fresh installs, four authenticated default BSD displays and three guest sanitizer checks. 223 completed OpenBSD AMD64 entries retain exact source/binary/config/registration bindings; only 73 unfinished registrations run after the outer timeout, followed by install/default-X completion after an artifact-only correction. Original failures remain canonical. Preview/recovery remains open; no full suite repeats for publication. [Report](verification/VERIFICATION-2026-10-22-DESKTOP-SETTINGS.md).

- 2026-10-22: PR151 merged. CFG-01 selects a grouped native Form editor, scoped unsaved Defaults/Cancel and bounded generation-bound consumer callbacks. One save publishes all selected fields; later commands/reload/restart retire older drafts. Live desktop preview stays unavailable because the helper cannot observe arbitrary later Style/Colorset changes and must not blindly roll them back. Screenshots are captured from actual-X runs at the user’s request. Focused Linux checks pass; six-target combined affected qualification is pending. All C/build/protocol inputs are unchanged, so exact native target/foundation identities will be retained. PR149 performance reference advances to 2/5; no comparison due.

- 2026-10-22: CFG-01 grouped native editing is functionally qualified: 20 affected registered passes on six targets/ten profiles, two focused observer follow-ups, ten helper stages and four authenticated default BSD displays. All 150 native targets and 77 catalogs retain PR151 identities; 2931 unaffected entries retain their exact foundation scope. One unchanged Linux AMD64 control resolves its pre-editor timeout; an expected retired-window observer correction resolves OpenBSD AMD64. Original failures remain canonical. The new native Form lifecycle and standalone no-editor control expose existing fontconfig/Xft process-exit allocations; CLEAN-01 keeps that gap in Later, and this is not a clean leak/release qualification. Actual-X screenshots are preserved for user review. Live preview remains unavailable pending observable shared-core generations with IPC-02. PR149 cadence stays 2/5 until merge. [Report](verification/VERIFICATION-2026-10-22-SETTINGS-EDITOR.md).

- 2026-10-22: PR152 merged. CFG-02 selects three related features: bounded non-executing literal desktop-settings/Read checking; source, continuation and include-chain diagnostics with offline text/JSON; and a native paged report from the settings center. Unsupported/dynamic FVWM syntax remains explicitly unchecked. Forty-two policy cases and focused Linux actual-X/CLI checks pass during development; combined six-target qualification is pending. No C/build/protocol inputs change. General live core-parser source diagnostics and whole-language checking remain unimplemented. Visual polish and the existing CLEAN-01 Form exit-cleanup diagnostic stay deferred. PR149 performance cadence advances to 3/5; no comparison due.

- 2026-10-22: CFG-02’s first checker checkpoint is functionally qualified: literal desktop-settings/Read analysis, source spans/include context with offline text/JSON, and a native paged consumer pass 20 affected registered entries across six targets/ten profiles. Ten helper stages and four authenticated default BSD displays pass. All 150 native targets / 77 catalogs retain PR152 identities, preserving 2931 unchanged composed entries (2420 standard / 531 optional total). One original Linux ARM64 software staged palette/reset barrier timeout remains failed; an unchanged isolated staged control and only the unfinished minimal profile pass. Review corrects a checker/runtime whitespace mismatch by sharing the existing option parser; final affected gates pass on all six targets with native foundation retained. No correction follows the final freeze. Forty-two policy cases per profile and actual-X captures verify no actions execute and both report pages work. General core runtime diagnostics and CFG-01 preview remain open; IPC-02 follows. Existing native Form exit cleanup, visual polish and tuning stay deferred. PR149 cadence remains 3/5 until merge. [Report](verification/VERIFICATION-2026-10-22-CONFIG-CHECKER.md).

- 2026-10-22: PR153 merged. IPC-02 selects three related behaviors: versioned correlated requests and explicit bounded errors; core-owned current desktop/viewport/work-area queries and processed-command-stream barriers over existing Send_Reply; and one-way read-only connection admission with connection/capability queries. Acceptance covers strict schema/IDs, duplicate/pending/deadline limits, partial/coalesced framing, authoritative query/order, late/disconnected replies, healthy/slow-reader isolation and legacy text/events. No native packet/Perl metadata change or new UI is selected. General monitor/binding/settings providers, applied domain actions, progress/cancellation and CFG-01 preview remain open. Combined qualification is pending. PR149 performance cadence advances to 4/5; no comparison due.

- 2026-10-22: IPC-02’s focused Linux and early FreeBSD ARM64 native policy/actual-X checks pass, including independent X-property comparison, existing legacy streams, explicit read-only refusal, eight pending slots, timeout/late-reply and disconnect/slow-reader isolation. Pre-freeze review tightens duplicate-ID handling for malformed requests, checks deadlines in the input callback as well as the timer, and preserves allocation-failure ownership. The selected combined checkpoint builds all ten profiles, runs seven IPC/consumer entries per profile plus Linux backpressure/interaction checks, tests ten staged native modules and four authenticated default BSD displays, and adds scoped leak-enabled request policy/module lifecycle sanitizers. Unchanged core/other native targets retain exact identities; broader old MFL-dependent results are historical and will not be counted as freshly/composition-qualified for the changed MFL binary. No new UI is selected; native UI screenshots remain required for later visible consumers.

- 2026-10-22: IPC-02’s first correlated query/barrier/read-only subset is locally qualified: 82 selected registered passes across six targets/ten profiles, ten staged request-X sessions, four authenticated default BSD displays and two leak-enabled scoped guest sanitizer exercises. Only ten MFL executables change; 140 other native ELF targets and 77 catalogs remain exact, with all 927 install files verified. Both original OpenBSD selected tests/complete installs are retained through exact source/cache/compiler/package identity while unfinished staged/default-X gates complete; the overstrict RUNPATH verifier and recovery proof-path failures remain canonical. An ASan target-registration setup failure is preserved; explicit reconfigure and scoped follow-up pass without suppression. Old broad MFL-dependent results remain historical, not current composed passes. No runtime/observer correction follows freeze 5f898d6. Full providers, typed applied actions, progress/cancellation and CFG-01 preview remain open. No new UI; later visible work retains screenshots. PR149 cadence stays 4/5 until merge. [Report](verification/VERIFICATION-2026-10-22-IPC-REQUESTS.md).

- 2026-10-22: user requests substantially faster feature delivery. Adopt focused PR merge gates on one incremental Linux profile, early affected-target checks for sensitive work, one accumulated affected six-target checkpoint per roughly three feature PRs/ten substantive stories, and full milestone/release acceptance. Ordinary PRs do not repeat installs or four BSD default-X sessions. Explicit cycle/deferred-check ledger prevents silent coverage debt; PR154's already-qualified candidate starts the next cycle after merge. This documentation-only cadence update stays in open PR154 and repeats no product tests. [Decision](decisions/FEATURE-TEST-CADENCE-2026-10-22.md).

- 2026-10-22: PR154 merged. IPC-02 settings provider batches registry/value snapshots, generation-checked atomic updates and correlated scoped reset/reload. Six affected registered entries pass on reused Linux ARM64 GCC, including 49 desktop policy cases and actual core/MFL/desktop plus independent partial/malformed producer controls. Two leak-enabled scoped guest sanitizer exercises pass. A Clang test-vector warning is corrected by explicit grouping; only native policy repeats on GCC/ASan, retaining exact product/fixture integration evidence. No new UI, installs, BSD boots or full matrix. Shared cycle is 0/3 merged with this three-story batch in review, 1/3 after merge; deferred gates are explicit. Due PR149→PR154 telemetry uses existing binaries in 31.536 seconds and resets the separate performance reference to PR154 at 0/5. [Feature report](verification/VERIFICATION-2026-10-22-IPC-SETTINGS.md), [performance](verification/PERFORMANCE-2026-10-22-PR154.md).

- 2026-10-22: PR155 merged. IPC-02 batches generation-checked non-publishing validation, connection-local pending snapshots and wait-only cancellation. Six selected Linux GCC entries (51 desktop policy cases) and three scoped leak-enabled guest sanitizer exercises pass. Actual-X proves a cancelled write can still commit, plus read-only/capacity/isolation/partial cleanup and late-result controls. The initial simultaneous-expiry ordering assumption is corrected in the fixture; final checks pass. Cycle 1/3 merged, 2/3 after this merge; deferred affected six-target/alternate-profile/release gates remain explicit. Performance reference PR154 is 1/5, 2/5 on merge; no observation due. No new UI/screenshots or matrix/install/BSD repeats. [Report](verification/VERIFICATION-2026-10-22-IPC-LIFECYCLE.md).

- 2026-10-22: After merged PR156, IPC-02 core topology/workspace/global-binding queries and a reproduced startup-readiness fence qualify the accumulated three-PR cycle: 113 composed selected passes across six targets/nine profiles and five scoped guest sanitizer exercises. The readiness correction retains 74 unchanged entries and runs 39 affected/missing entries; original setup/storage/fixture/provider failures and incomplete AMD64 capture stay explicit. Portable 177 passes/14 skips; no new UI, install/default-X repeats or benchmark. On merge reset the integration reference/count; performance PR154 is 2/5, 3/5 on merge. [Report](verification/VERIFICATION-2026-10-22-IPC-CORE-STATE.md).

- 2026-10-22: PR157 merged as the new integrated reference. IPC-02 finishes binding scope inspection with regular/fallback menu records, normalized placement controls and context/modifier policy. Seven focused Linux GCC entries and three scoped leak-enabled guest sanitizer exercises pass; the frozen gate commands take 23.738 seconds. Original unescaped-variable fixture failures remain recorded; the final escaped literal passes without action execution. No new UI, BSD boots, install/default-X/matrix or benchmark repeats. Cycle 0/3 merged, 1/3 on merge; affected six-target/alternate profiles remain due at shared closure, release acceptance separate. Performance PR154 is 3/5, 4/5 on merge. [Report](verification/VERIFICATION-2026-10-22-IPC-BINDING-SCOPES.md).

- 2026-10-22: PR158 merged. IPC-02 batches desktop-provider capability/configuration state, bounded opt-in identity/generation invalidations and conditional settings snapshots. Seven focused Linux entries (54 desktop cases) and two scoped leak-enabled guest sanitizer exercises pass; frozen commands take 39.252 seconds, with unchanged PR158 core bytes. Actual-X covers multiple owners, filters, read-only/full-capacity admission, malformed notice recovery and absence without cached liveness. Original policy-fixture scope error and formatter gaps stay explicit. Cycle 1/3 merged, 2/3 on merge; no UI, install/default-X/BSD/matrix repeat. Performance PR154 is 4/5, 5/5 on merge; compare preserved binaries next run. [Report](verification/VERIFICATION-2026-10-22-IPC-PROVIDER-OBSERVATION.md).

- 2026-10-22: After merged PR159, five owned-operation capabilities adopt existing applications/status workers: typed admission, state/results, generation-checked cancellation, bounded reconnect retention and coalesced invalidations. One frozen accumulated PR158/159/current checkpoint passes 122 selected entries on six targets/nine profiles, five scoped guest sanitizer exercises and portable 177 passes/14 skips. No runtime/fixture correction follows freeze; old broad suites/install/default-X are not recounted. Canonical payload/mode proof precedes completed guest duplicate retirement. On merge reset the integrated reference/count to 0/3. Due PR154→159 performance comparison takes 31.438 seconds with preserved binaries; ranges mostly overlap and feature-size growth is explicit. PR159 resets the independent reference to 0/5, 1/5 after this merge. [Operation report](verification/VERIFICATION-2026-10-22-IPC-DESKTOP-OPERATIONS.md), [performance](verification/PERFORMANCE-2026-10-22-PR159.md).

- 2026-10-22: PR160 merged as integrated reference, resetting the cycle to 0/3. IPC-02 batches core colorset inventory pages, exact sparse-ID lookup and scalar summary/freshness with published versus pending/refused image state. Seven selected Linux entries and three scoped guest sanitizer exercises pass. Two final unit readability edits repeat policy only; exact product/fixture identities retain actual-X evidence. No new UI, full matrix, install/default-X or benchmark repeat. The feature becomes 1/3 on merge; performance PR159 is 1/5, 2/5 on merge. Style/decor generations and safe preview stay open. [Report](verification/VERIFICATION-2026-10-22-IPC-COLORSET-STATE.md).

- 2026-10-22: PR161 merged; appearance configuration freshness, bounded menu/decor pages and reproduced CopyMenuStyle opacity preservation pass eight selected Linux entries and three scoped guest sanitizer exercises. The existing native desktop consumer passes with captured/inspected launcher screenshots. Header-dependency and source73/copied100 controls remain preserved; no runtime/fixture correction follows freeze. Cycle 1/3 merged, 2/3 on merge; no matrix/install/BSD repeat. Performance PR159 is 2/5, 3/5 on merge. Owned snapshots/conditional application/recovery remain open. [Report](verification/VERIFICATION-2026-10-22-IPC-APPEARANCE-STATE.md).

- 2026-10-22: PR162 merged as `fb510466bb0a034c03014424fc16384b83392777`.
  IPC-02 / CFG-01 now batch an atomic core-owned runtime menu-opacity lease,
  typed connection/view-scoped controls and a native confirmation consumer.
  Focused Linux policy and actual-X controls pass; one stale-start retry remains
  generation-checked. Deadline expiry during menu interaction, disconnect,
  native producer death and conflict refusal are covered. Saved preferences
  remain unchanged. The due accumulated six-target checkpoint passes 140 selected
  entries on nine profiles, seven scoped guest core/MFL sanitizer exercises and
  portable 177 passes/14 skips. No C/helper/fixture change follows freeze;
  generated catalog/docs amendments have separate proof. Original development
  failures and broader install/default-X/release/native-CPU limits remain explicit.
  [Report](verification/VERIFICATION-2026-10-22-RUNTIME-OPACITY-PREVIEW.md).

- 2026-10-22: PR163 merged as the new integrated reference, resetting to 0/3.
  CFG-01 / IPC-02 now batch launcher-opacity integer metadata/schema migration,
  native scoped Keep and save, captured draft transfer/resume and orphan Form
  retirement. Confirmation and persistence outcomes remain separate. Focused
  Linux/native UI passes nine selected entries (74 desktop policy cases) and
  four scoped leak-enabled guest sanitizer exercises. Screenshot review corrects
  the percent label and off-screen notice; the corrected focused checkpoint is
  source-bound. No repeat six-target cycle is due. Performance PR159 is 4/5
  merged, 5/5 after this merge; compare existing binaries next run.
  [Report](verification/VERIFICATION-2026-10-22-SAVED-OPACITY-PREVIEW.md).


- 2026-10-23: PR164 merged. User selects file/clipboard foundations and defers
  further opacity expansion. Stable collection metadata/selection, bounded
  isolated directory loading and validated literal actions now have an optional
  native paged consumer. Focused Linux, early native FreeBSD/OpenBSD 8.0 ARM64,
  race/vet and tagged executable/manual/UI checks pass. The only frozen correction
  orders the new registered Go test flags correctly; its unchanged command-client
  pass is retained. Full scrolling/provider/mutation/protected-content handoffs
  remain open. Cycle 1/3 merged, 2/3 on merge. Due PR159→PR164 performance ranges
  overlap; PR164 resets to 0/5, 1/5 on merge. [Report](verification/VERIFICATION-2026-10-23-FILE-BROWSER-FOUNDATIONS.md).


- 2026-10-23: PR178 merged; close the accumulated PR176–178 platform checkpoint
  before new implementation: 84 native entries on six targets/seven profiles,
  556 Go cases/35 package outcomes and retained source-bound ARM64 themes and
  sanitizers. A bounded FreeBSD AMD64 synthetic-input observer correction repeats
  UI only. Next batch qualifies shared correlated RPC, matching-browser reuse,
  core-owned presentation and optional installed folder entry. Ten native entries,
  251 Go cases/15 outcomes including race, four browser native-X runs, three
  scoped sanitizer exercises, staged entry and three manuals pass. Three themes
  capture 41 images each; new handoff properties/folder views are inspected.
  Storage-only race linking completes after canonical-verified old capture
  retirement, preserving all prior passes; one manual paragraph moves and
  rerenders alone. PR178 starts 0/3, 1/3 on this merge. Due PR174 comparison
  records about 1 ms higher scan median with overlapping ranges and unchanged
  peak RSS; qualified handoff binaries reset the independent reference to 0/5
  on merge. Broad owner/release/default-role/private-content gates stay open.
  [Feature report](verification/VERIFICATION-2026-10-23-BROWSER-CONSUMER-HANDOFF.md),
  [performance](verification/PERFORMANCE-2026-10-23-BROWSER-HANDOFF.md).


- 2026-10-23: The user requests larger similar cleanup batches and fewer all-OS
  runs. Supersede the automatic three-PR/ten-story matrix with one focused Linux
  gate per combined batch and a risk review after about five merged batches or
  a substantial handoff. Actual platform/ABI/build/install/server changes and
  concrete regressions still trigger their affected checks; full milestone/
  release qualification remains required. Enlarge open PR186 with four related
  provider maintainability improvements. No new UI is selected; existing gallery
  provenance stays intact. [Decision](decisions/BATCHED-CLEANUP-CADENCE-2026-10-23.md).


- 2026-10-23: Four related provider cleanup improvements are complete in open
  PR186: consolidate operation capability/admission/routing, share strict typed
  value decoding, separate query/mutation ownership and remove query-result
  interface assertions. One Linux gate passes 85 policy cases, 45 browser race
  cases, vet/build and native provider/handoff integration in 22.399 command
  seconds. No post-freeze correction; native C/client/compiler/configuration
  identities are exact. No BSD/install/full C/theme matrix repeats. Preserve
  old feature and performance receipts as their original candidate evidence;
  the new report binds the final browser source. Retire 62 verified guest
  duplicates (6,321,041 bytes). [Report](verification/VERIFICATION-2026-10-23-BROWSER-PROVIDER-CLEANUP.md).


- 2026-10-23: PR186 merges while the related cleanup is finalized. Publish cleanup
  as its own follow-up, based on merged PR186 and PR185 dependency-planning docs.
  Two documentation-index conflicts retain both entries; all 1504 qualified
  runtime/test/build inputs stay exact. No test repeats for this baseline-only
  change. PR178 remains integrated reference with one merged implementation
  batch, two on cleanup merge; PR186 preserved comparison reference is 0/5,
  1/5 on cleanup merge. The risk-based cadence replaces automatic all-OS runs.


- 2026-10-23: User pauses feature work and selects source cleanup, legacy removal,
  dependency reduction and maintenance. Reconcile all 73 open issues into 34
  maintenance owners/children in M1–M8 plus three bounded decision lanes, 12
  conditional qualification owners, five deferred performance owners and 22
  paused feature/polish owners. Promote 19 selected maintenance issues from
  Later to milestone 05; #179/#181 Ready, #180 still blocked by #179. Keep
  stages/priorities/native relationships and accepted evidence. Project Work
  batch mirrors the queue. PR187 is merged; no runtime/OS tests are needed for
  this documentation/board plan. [Batch register](decisions/MAINTENANCE-FIRST-BATCHES-2026-10-23.md).


- 2026-10-23: Maintenance-first queue is mirrored and read back in all 73 open
  issues and Project cards. Nineteen selected maintenance issues move from Later
  to milestone 05; #179/#181 are Ready at the top, #180 remains blocked by #179.
  Work batch contains the logical queue; original relative order of other cards
  is preserved. All native parents/blockers, stage/priority and accepted evidence
  remain intact. Two GitHub gateway failures are reconciled through actual-state
  readback and smaller idempotent setters; final values pass. PR187 is Done;
  planning PR188 is Review. No runtime/OS tests, removals or dependency changes.
  [Receipt](verification/MAINTENANCE-PLANNING-2026-10-23.md).


- 2026-10-23: Complete M1 as one coherent #179/#180/#181 checkpoint. Require the
  approved offline compiler, retire replaced utility/front-end paths and both
  pre-FVWM3 converter implementations, preserve independent frozen references
  and qualify six targets once for the changed compiler/install contract.
  Thirty utility native-X runs and four BSD default-server settings runs pass;
  fresh source-package build and 27 man/HTML manuals pass. Preserve the two BSD
  obsolete-cache-option failures and scoped continuation receipts. M2 is next
  after review/merge; remaining framework, tooling and broad release owners stay
  open. Broader integration remains PR178 with two merged batches, three on M1
  merge; pre-cleanup PR186 performance reference is 1/5, 2/5 on merge. No new
  performance comparison or rendering/theme/sanitizer matrix is due.
  [M1 report](verification/VERIFICATION-2026-10-23-M1-RUNTIME-BASELINE.md).


- 2026-10-23: PR189 merges; M1 children #179/#180/#181 are Done. Begin M2.1,
  grouping application discovery, Linux/BSD system metrics, audio/power jobs,
  typed private transport, process ownership and staged consumers. One combined
  Linux and targeted FreeBSD gate passes, with six offline cross-builds instead
  of another native/default-X matrix. Keep original receipts and repeat only
  affected bridge/worker/installed operations after selector and quoting review
  corrections.
  Settings/preview/checker and native capture functions retain source hashes.
  #23/#7/#15 stay open for remaining M2 work; no Python-free or full-session
  acceptance claim. Incremental status acquisitions add about 0.945 ms median
  child-startup cost in a seven-pair observation, with no speedup claim. Full
  session/RSS/idle/input measurements stay mandatory at runtime closure.
  Periodic browser comparison reference PR186 is 2/5, 3/5 on this merge; broader
  integration reference stays PR178 (three merged batches, four on this merge).
  [Report](verification/VERIFICATION-2026-10-23-M2-DESKTOP-WORKERS.md).

- 2026-10-23: PR190 merges M2.1. M2.2 batches registry/types/schema, candidate/
  query policy and settings/appearance/editor/schedule/preview UI plans into one
  pure Go package. Direct file commits and live generations/leases stay resident;
  complete-plan validation preserves active drafts on failure. Combined Linux
  policy/race/install/actual-X and three theme profiles pass, with six offline
  cross-builds. A fixture-local capture name correction repeats only UI; disk
  recovery finishes only skipped build/manual/adjacent checks. Canonical failed
  attempts remain; exact-hash guest archive duplicates retire, builds/stages
  stay. Whitespace-only Go readability changes retain equal scanner token/literal
  streams. Final diff review restores count-before-copy/scan ordering with one
  direct-call regression and scoped worker normal/race/build/CLI/adjacent passes;
  settings/UI results stay valid. No native C, new feature, full Python retirement
  or size/speed claim.
  Seven paired prepare/query/editor observations add about 1.93 ms median
  process/JSON cost; full-session comparisons stay at #23 closure. This is fifth
  post-PR178 merge coverage review, not an automatic matrix. PR186 periodic
  comparison is 3/5, 4/5 on merge. Remaining resident/state/lifetime/capture
  work stays in M2 before M3. [Report](verification/VERIFICATION-2026-10-23-M2-SETTINGS-POLICY.md).

- 2026-10-24: PR194 merges M2.3. M2.4 replaces the Python resident with one Go
  state/dispatch/lifetime owner, reusing settings policy and vendored Unix APIs.
  Focused Linux/FreeBSD, installed module peers, native-X services/settings/
  operations and three-theme captures pass; twelve supported-pair builds are
  compile-only. Directed termination, Unicode and build-tree capture regressions
  pass after preserved failures. Full owner/session/capture and broad #23/#7/#15
  remain open. Retire only host-verified guest duplicates. The due preserved-
  binary comparison shows overlapping scan ranges/unchanged RSS; PR194 resets
  performance to 0/5, 1/5 on this merge. No full matrix or resident speed claim.
  [Controller report](verification/VERIFICATION-2026-10-24-M2-RESIDENT-CONTROLLER.md),
  [performance](verification/PERFORMANCE-2026-10-24-PR194.md).

- 2026-10-24: Cam explicitly authorizes PR195 merge/cleanup/continuation. Its
  canonical signed squash commit retains the author DCO and reviewed tree;
  completed branches retire. M2.5 replaces the last installed desktop Python
  helper with native Xlib/libpng capture and updates all shipped consumers.
  Linux/FreeBSD ARM64 RGB/private-file/refusal and installed/build-tree dispatch
  pass; Linux desktop/three-theme captures and scoped sanitizers pass. Lead
  corrects a GCC setjmp warning; installation evidence accounts for Meson's
  runtime-path rewrite. Only affected checks repeat, with failures preserved.
  All commands total 47.299 seconds; no full matrix/performance comparison.
  Broad M2/#23/#7/#15 remain open for inventory, complete installed/session
  measurements and owner/platform acceptance before M3.
  [Report](verification/VERIFICATION-2026-10-24-M2-NATIVE-CAPTURE.md).

- 2026-10-24: Cam explicitly authorizes PR198 merge/cleanup/continuation. Its
  canonical signature/DCO and runtime input identity are verified, preserving
  PR197's policy-only integration. M2.6 batches action ownership, unused policy
  RPC removal and the reproduced self-worker/procfs family. Linux policy/race,
  installed Python-free runtime/utility and locker-survival checks pass, with
  three inspected notice themes and paired session observations. Review fixes
  survival evidence, caller refusal propagation and mapped Slate capture.
  Setup/storage/wrong-reference failures stay explicit; only affected work
  repeats. No BSD/full matrix or C sanitizer repeat. Full M2/platform/session
  acceptance remains before M3; #23/#7/#15 remain open. Independent PR194 scan
  cadence is 2/5, 3/5 on merge. [Report](verification/VERIFICATION-2026-10-24-M2-SESSION-RUNTIME.md),
  [session observation](verification/PERFORMANCE-2026-10-24-M2-SESSION.md).
