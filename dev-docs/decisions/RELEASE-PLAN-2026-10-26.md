# Three-release desktop roadmap

Accepted user direction, 2026-10-26. Owner: Cam (@gonz0w). The product target is
an extensively configurable, fast, low-memory X11 desktop lite environment.
All currently planned issue-defined features target R1. Existing evaluations
finish in R1 and implement accepted choices; evaluating a candidate does not
force its adoption. Future feature requests enter R2/R3 intake unless the owner
explicitly changes frozen R1 scope.

[PROJECT-TRACKER.md](../PROJECT-TRACKER.md#execution-order) owns live execution.
This decision and the [finite issue contracts](RELEASE-CONTRACTS-2026-10-26.json)
supersede cleanup-first scheduling in the October 23/25 plans. Accepted M1-M4,
settings/IPC/browser and other source-bound components remain accepted. PR256
is merged as `b263edf8fdf744fe545fa9af6d9685cf2b705375`, followed by this
roadmap in PR261 (`1748e2f41199d996c97f47f84e047249fdec3394`, closing #208).
The next finite #209/#29/#226 checkpoint freezes
[reference profiles](R1-REFERENCE-PROFILES-2026-10-26.md). These merges do not
claim a whole-M5 audit. No release publication, tag, credential or upstream notification is
selected by this roadmap.

## Release targets and work classification

Milestones identify releases; Stage labels/Project field identify technical
ownership. Work size is independent of priority and elapsed time. Major means a
substantive capability, cross-component/native contract, migration or broad
qualification. Minor means bounded cleanup, documentation, polish or a local
profile/test task. Minor R1 work still belongs to release acceptance.

| Release | Target | Major | Minor | Issues |
| --- | --- | ---: | ---: | ---: |
| R1 | Configurable Desktop Lite | 60 | 5 | 65 |
| R2 | Maintenance and Runtime Simplification | 18 | 4 | 22 |
| R3 | Developer Tooling and Deferred Work | 4 | 0 | 4 |

These are 87 pre-existing open contracts plus two explicitly parked children and
R2/R3 acceptance gates. Counts are issue owners, not independent defects or PRs;
parent/child contracts can share one implementation checkpoint. The old ten
stage/Later milestones are retired from scheduling while retaining completed
history. Calendar dates and the public release version remain unset; milestones
are scope targets, not measured delivery estimates.

## R1 scope and implementation order

1. **Foundation/profile decisions:** accept this finite roadmap and PR256;
   freeze concrete missing prerequisites and reference profiles/budgets. Reuse
   accepted components. Run existing library/backend evaluations early enough
   to qualify any adopted integration before affected feature/profile freeze.
2. **Shared contracts:** text/input, structured actions/state/bindings, native
   graphics/layout, theme/icons, selection/content, file data/actions, existing
   XDG/monitor/protocol/scale contracts and isolated Lua.
3. **Desktop consumers:** settings/checker/inspector/reload, browser and file jobs,
   clipboard/history/protection, pinboard, portable session/preference bridges,
   optional notifications/bus/tray, and accepted file-manager candidates.
4. **Workspace capabilities:** palette, layouts/snapping, scratchpads, docking
   profiles, overview, external locker coordination and the accepted optional
   external-compositor result. Parallel independent contracts need not wait for
   the entire wave; native blocked-by links remain the actual prerequisites.
5. **RC acceptance:** complete shipped-theme/keyboard/accessibility/localization,
   manuals/install/offline/source/profile/dependency/binary evidence and the
   frozen performance budgets, then record #79's maintainer acceptance.

All existing features, including eleven-language polish and optional services,
are in R1. New features remain optional where their original contract says so.
The minimal WM and lightweight desktop must not autostart every optional service.
No new mandatory GUI toolkit, database, compositor or bus follows merely from
release placement. Basic browser/palette operation stays database-independent.
Keep the C core/X authority single-threaded and services isolated; preserve the
configuration-file contract and actual-pixel protocol/geometry.

## Speed and memory acceptance

#29 and #226 must produce a checked-in numeric budget/cost manifest for minimal
WM, shipped desktop and enabled optional-service profiles. Freeze workloads,
reference source/binaries/platforms, startup-to-usable, input/focus/move/resize
latency, idle CPU/wakeups, steady/peak RSS plus PSS or an available equivalent,
redraw/X requests, burst behavior and installed direct/transitive bytes.
Per-process and whole-session results must identify each enabled service.

Freeze numeric regression envelopes before accepting a new default resident
service or mandatory dependency. #29 enforces final RC budgets; #209 does not
wait for every optimization hypothesis or R2 audit. Use existing exact evidence
where applicable, record variance and unavailable/native/emulated distinctions,
and preserve failures. This planning change supplies no new measurement or
passing performance claim. A reproduced blocking regression returns to its
finite owner; budgets cannot be silently relaxed to obtain a pass.

## What waits for R2 and R3

R2 owns the remaining snapshot-bounded source/input/descriptor/process/image and
interaction reviews, dirty-state/cache/style/state/X-query experiments, existing
tool adoption, predecessor and Perl runtime/framework retirement, optional fuzz/
allocator qualification and OS-specific image-worker containment. Each issue
freezes a finite source/workload/consumer manifest and publishes its results;
it does not grow whenever unrelated code is discovered.

R3 owns #55's remaining first-party developer-tool language migration and the
separate parked pathname/Backer checkpoints. R2 #6/#34 explicitly exclude those
children and can close without claiming them fixed. The parked issues stay
blocked on a supported review route; administrative restructuring grants no
inspection, implementation, retry or alternate-executor workaround. Deferred
Form final font cleanup remains explicitly within R2 #34.

R2/R3 release gates freeze their selected manifests, preserve R1 capabilities and
require actual candidate evidence plus maintainer acceptance. An explicit
release exclusion is a documented deferral, never a passing test. Future feature
intake receives its own finite contract and target; it does not silently extend
R1 or perpetually reopen a closed audit.

## Status dispositions

The new R2 remainder for #4/#6/#7/#15/#34 is Backlog. Their former Review/In
progress labels described older component work: PR238, PR241 and PR254 are
merged and remain accepted. PR256 and PR261 are now merged/Done; #209/#29/#226
are In progress for the finite reference checkpoint, while
#34 describes unimplemented remaining R2 work. Each of the five contracts
records its specific reason. No issue is closed, reopened or claimed completed
by this status change. Other existing statuses are retained; #208 records the
completed planning handoff.

## Completion and verification rules

Every issue below has one target release, Major/Minor class, concrete outcome,
acceptance checks, preserved accepted work and explicit exclusions in the JSON
contract and GitHub body. Coordination references do not automatically become
blocked-by relationships. Later-release parents/children do not block an earlier
release's explicitly accepted subset. #208 closes when this plan/mirror is
accepted; it does not claim all child runtime work delivered. #209's finite
foundation handoff unblocks actual feature contracts, not an M4-M8 catch-all.

Keep the faster cadence: coherent related implementation batches, affected
local checks, early checks for changed platform APIs and one combined required
qualification at the appropriate checkpoint. Reuse unaffected passes and run
only corrected/unfinished paths after a failure. New UI gets actual screenshots
and the representative three-theme smoke set at shared UI checkpoints. Required
support/native/sanitizer/hardware/release evidence remains exact and separate.
No hosted Actions or success-status publisher is introduced.

The complete release manifest follows. Detailed checklist/results boundaries
are in the issue bodies and versioned contract JSON. Historical bodies remain
folded in GitHub; the original near-limit CLEAN issue is preserved in
[this byte-exact snapshot](../history/issue-snapshots/2026-10-26-CLEAN-01.txt).

## R1 — Configurable Desktop Lite manifest

| Issue | Work | End result |
| --- | --- | --- |
| [#3](https://github.com/gonz0w/fvwm-x/issues/3) | Major | Qualify the R1 release candidate against a frozen representative FVWM 3.x configuration corpus. |
| [#5](https://github.com/gonz0w/fvwm-x/issues/5) | Major | Complete the finite R1 local native and release verification register for the exact release candidate. |
| [#11](https://github.com/gonz0w/fvwm-x/issues/11) | Major | Qualify the exact image/transitive dependency closure shipped by R1. |
| [#13](https://github.com/gonz0w/fvwm-x/issues/13) | Major | Qualify the R1 X11/server/font dependency and platform support manifest. |
| [#16](https://github.com/gonz0w/fvwm-x/issues/16) | Major | Close the finite R1 adopted-protocol requirement register with truthful advertised capabilities. |
| [#17](https://github.com/gonz0w/fvwm-x/issues/17) | Minor | Leave one usable local, source-bound verification and repository review recipe for the R1 candidate. |
| [#18](https://github.com/gonz0w/fvwm-x/issues/18) | Major | Finish the adopted R1 ICCCM/EWMH correctness rows and advertise only supported behavior. |
| [#19](https://github.com/gonz0w/fvwm-x/issues/19) | Major | Deliver the reconciled monitor/HiDPI runtime behavior promised by the R1 desktop. |
| [#22](https://github.com/gonz0w/fvwm-x/issues/22) | Major | Complete the finite selection transfer contracts required by native R1 clipboard/desktop consumers. |
| [#29](https://github.com/gonz0w/fvwm-x/issues/29) | Major | Freeze and enforce a finite release performance/resource budget for minimal, desktop and opt-in extras profiles. |
| [#43](https://github.com/gonz0w/fvwm-x/issues/43) | Major | Ship accurate installed R1 manuals, examples and package documentation. |
| [#45](https://github.com/gonz0w/fvwm-x/issues/45) | Minor | Complete a finite localization/UI coverage pass for the R1 shipped interface manifest. |
| [#46](https://github.com/gonz0w/fvwm-x/issues/46) | Major | Complete the bounded LunaSVG comparison and qualify an adopted R1 choice, or retain the baseline with an explicit defer/reject decision. |
| [#48](https://github.com/gonz0w/fvwm-x/issues/48) | Minor | Complete the bounded status-serializer comparison and qualify an adopted R1 choice, or retain patched cJSON with explicit defer/reject. |
| [#50](https://github.com/gonz0w/fvwm-x/issues/50) | Major | Ship an isolated, bounded Lua scripting host with working event automation, configurable policy and a Perl-to-Lua migration guide. |
| [#51](https://github.com/gonz0w/fvwm-x/issues/51) | Major | Qualify native module scale consumers promised by the R1 desktop. |
| [#52](https://github.com/gonz0w/fvwm-x/issues/52) | Major | Ship a finite qualified Linux/BSD source/install package set for R1. |
| [#53](https://github.com/gonz0w/fvwm-x/issues/53) | Major | Make the frozen R1 source and documented development profiles reproducible and offline-capable. |
| [#54](https://github.com/gonz0w/fvwm-x/issues/54) | Major | Deliver one qualified shared text/input path for FVWM-owned labels and editable controls, including graphemes, shaping/bidi, fallback and composed input. |
| [#56](https://github.com/gonz0w/fvwm-x/issues/56) | Major | Provide correct installed desktop-entry, launch, MIME-role and icon interoperability while retaining legacy FVWM paths. |
| [#57](https://github.com/gonz0w/fvwm-x/issues/57) | Major | Make the selected native desktop controls fully keyboard operable and expose truthful accessible names, roles, states and actions. |
| [#60](https://github.com/gonz0w/fvwm-x/issues/60) | Major | Complete the native typed settings center with safe preview, confirmation and saved preferences for a frozen set of reversible appearance/layout settings. |
| [#61](https://github.com/gonz0w/fvwm-x/issues/61) | Major | Add accurate source-aware runtime diagnostics and a finite non-executing declarative checker that clearly reports unchecked constructs. |
| [#62](https://github.com/gonz0w/fvwm-x/issues/62) | Major | Deliver a documented configurable text/image clipboard consumer over protected content and retention primitives, with a working in-memory handoff and qualified optional persistence. |
| [#63](https://github.com/gonz0w/fvwm-x/issues/63) | Major | Provide bounded typed content items and safe text/image/URI representation transfer with explicit identity, leases and sensitivity. |
| [#64](https://github.com/gonz0w/fvwm-x/issues/64) | Major | Ship an opt-in persistent desktop pinboard with accessible movable references and safe monitor-aware placement. |
| [#65](https://github.com/gonz0w/fvwm-x/issues/65) | Major | Complete typed file/collection navigation and launch actions with correlated final results and a reusable installed browser handoff. |
| [#66](https://github.com/gonz0w/fvwm-x/issues/66) | Major | Complete bounded byte-preserving directory data/watch contracts and decide the finite metadata/indexed-search backend comparison, implementing accepted outcomes. |
| [#67](https://github.com/gonz0w/fvwm-x/issues/67) | Major | Provide installed portable folder/reveal/properties interoperability and an opt-in FileManager1 adapter using the same typed browser operations. |
| [#68](https://github.com/gonz0w/fvwm-x/issues/68) | Major | Deliver reliable bounded file-operation jobs with explicit progress, conflict, cancellation and partial outcomes. |
| [#69](https://github.com/gonz0w/fvwm-x/issues/69) | Major | Complete a reusable configurable native list/grid collection view and qualify the installed browser handoff. |
| [#70](https://github.com/gonz0w/fvwm-x/issues/70) | Major | Finish one bounded versioned state/action/operation contract for R1 settings, files, clipboard and session consumers over the accepted transport. |
| [#71](https://github.com/gonz0w/fvwm-x/issues/71) | Major | Ship configurable bounded clipboard capture/history policy with explicit secret, expiry, clear and lock behavior. |
| [#72](https://github.com/gonz0w/fvwm-x/issues/72) | Major | Ship the portable default session manager with bounded lifecycle, controlled autostart, honest service capabilities and installed native configuration. |
| [#73](https://github.com/gonz0w/fvwm-x/issues/73) | Major | Expose supported desktop preferences through one native XSETTINGS/X-resource bridge and a qualified optional portal path. |
| [#74](https://github.com/gonz0w/fvwm-x/issues/74) | Major | Deliver protected clipboard memory and an evidence-based optional authenticated-persistence contract with explicit failure and key/default policies. |
| [#75](https://github.com/gonz0w/fvwm-x/issues/75) | Major | Qualify all shipped theme palettes, modes and interaction states for R1, including the completed SVG artwork and new native UI consumers. |
| [#76](https://github.com/gonz0w/fvwm-x/issues/76) | Major | Ship opt-in isolated native freedesktop notifications with bounded history/actions and shared live desktop settings. |
| [#77](https://github.com/gonz0w/fvwm-x/issues/77) | Major | Ship a single opt-in D-Bus extension of the portable session with qualified shared adapters and truthful no-bus behavior. |
| [#78](https://github.com/gonz0w/fvwm-x/issues/78) | Major | Deliver configurable native XEmbed and opt-in StatusNotifier/application-menu tray integration. |
| [#79](https://github.com/gonz0w/fvwm-x/issues/79) | Major | Record a concrete final acceptance decision for the completed R1 candidate when release preparation is requested. |
| [#80](https://github.com/gonz0w/fvwm-x/issues/80) | Major | Complete reviewed offline localization of R1 first-party UI, manuals and shipped configuration comments for all eleven existing catalog languages. |
| [#81](https://github.com/gonz0w/fvwm-x/issues/81) | Major | Complete finite decisions for the existing advanced file-manager candidates and implement every accepted R1 choice using qualified primitives. |
| [#182](https://github.com/gonz0w/fvwm-x/issues/182) | Major | Decide the bounded Muon/CMake comparison and complete an adopted R1 backend integration, or explicitly retain/defer the authoritative backend. |
| [#208](https://github.com/gonz0w/fvwm-x/issues/208) | Minor | Close the finite R1 program index once all existing planned feature contracts, release placement and dependency waves are mapped and accepted. |
| [#209](https://github.com/gonz0w/fvwm-x/issues/209) | Major | Accept a finite R1 foundation handoff after the currently owned PR256 checkpoint, with scope and numeric performance/resource budgets recorded before RC. |
| [#210](https://github.com/gonz0w/fvwm-x/issues/210) | Major | Publish qualified reusable native XRender drawing/resource operations and fallback behavior for representative R1 module consumers. |
| [#211](https://github.com/gonz0w/fvwm-x/issues/211) | Major | Finish the direct Cairo native-module comparison and implement the accepted bounded drawing/effects outcome. |
| [#212](https://github.com/gonz0w/fvwm-x/issues/212) | Major | Ship reusable responsive native layout for two representative panel/collection consumers and complete the bounded optional Clay comparison. |
| [#213](https://github.com/gonz0w/fvwm-x/issues/213) | Major | Deliver reusable bounded event-driven native model/view bindings for two independent existing consumers. |
| [#214](https://github.com/gonz0w/fvwm-x/issues/214) | Major | Provide a composable semantic theme model with independent appearance choices and deterministic scoped overrides using existing native configuration. |
| [#215](https://github.com/gonz0w/fvwm-x/issues/215) | Major | Expose one bounded named icon resolver to at least two native consumers with scalable assets and reliable fallback. |
| [#216](https://github.com/gonz0w/fvwm-x/issues/216) | Major | Support configurable named icon-font runs alongside SVG/application icons with shared text measurement and dependable accessible fallback. |
| [#217](https://github.com/gonz0w/fvwm-x/issues/217) | Major | Reload a finite set of supported theme/native components transactionally while retaining the last working state and surviving UI identity. |
| [#218](https://github.com/gonz0w/fvwm-x/issues/218) | Major | Ship an opt-in read-only native inspector for effective settings, theme/rule provenance, bindings and layout bounds. |
| [#219](https://github.com/gonz0w/fvwm-x/issues/219) | Major | Deliver a keyboard-first shared native command palette for windows, applications, desktops and registered actions. |
| [#220](https://github.com/gonz0w/fvwm-x/issues/220) | Major | Provide configurable per-desktop floating, columns/master-stack and snapping policies through the existing FVWM command engine. |
| [#221](https://github.com/gonz0w/fvwm-x/issues/221) | Major | Ship named configurable scratchpads with predictable launch, summon, placement and focus return independently of tiling/compositing. |
| [#222](https://github.com/gonz0w/fvwm-x/issues/222) | Major | Restore user-owned monitor-role docking profiles without overwriting newer changes or making windows unreachable. |
| [#223](https://github.com/gonz0w/fvwm-x/issues/223) | Major | Ship a bounded searchable keyboard-accessible workspace/window overview that works without a compositor. |
| [#224](https://github.com/gonz0w/fvwm-x/issues/224) | Major | Complete the cooperative external-compositor workspace-transition evaluation and implement the explicitly accepted optional outcome. |
| [#225](https://github.com/gonz0w/fvwm-x/issues/225) | Major | Integrate qualified per-OS external locker readiness, manual/idle locking and suspend coordination without moving authentication into the core. |
| [#226](https://github.com/gonz0w/fvwm-x/issues/226) | Major | Ship portable effective-feature and complete-cost manifests for R1 build profiles. |
| [#231](https://github.com/gonz0w/fvwm-x/issues/231) | Minor | Publish a truthful finite hardening requirement/evidence profile for the R1 candidate. |
| [#235](https://github.com/gonz0w/fvwm-x/issues/235) | Major | Verify platform-appropriate protections on the exact installed R1 C and Go artifacts. |

## R2 — Maintenance and Runtime Simplification manifest

| Issue | Work | End result |
| --- | --- | --- |
| [#4](https://github.com/gonz0w/fvwm-x/issues/4) | Major | Finish one finite remaining FvwmScript ownership review and disposition checkpoint. |
| [#6](https://github.com/gonz0w/fvwm-x/issues/6) | Major | Close a one-pass checked-data ownership register for the remaining named input families. |
| [#7](https://github.com/gonz0w/fvwm-x/issues/7) | Major | Finish a snapshot-bounded descriptor ownership and admission pass for remaining constructors. |
| [#8](https://github.com/gonz0w/fvwm-x/issues/8) | Major | Disposition the remaining asynchronous image boundary risks in one frozen image-worker review. |
| [#9](https://github.com/gonz0w/fvwm-x/issues/9) | Major | Close one finite interaction lifetime and mutation review for existing interaction modes. |
| [#14](https://github.com/gonz0w/fvwm-x/issues/14) | Major | Complete the finite R2 maintenance-tool adoption manifest without expanding into the R3 full developer-tool rewrite. |
| [#15](https://github.com/gonz0w/fvwm-x/issues/15) | Major | Complete a one-pass process construction and cleanup audit for an enumerated helper set. |
| [#25](https://github.com/gonz0w/fvwm-x/issues/25) | Major | Complete one measured dirty-state optimization experiment for a frozen redraw workload set. |
| [#33](https://github.com/gonz0w/fvwm-x/issues/33) | Minor | Finish one bounded cache-cost experiment with an adopt/defer/no-change result. |
| [#34](https://github.com/gonz0w/fvwm-x/issues/34) | Major | Finish a one-pass finite remaining core/module cleanup register with selected ownership repairs. |
| [#37](https://github.com/gonz0w/fvwm-x/issues/37) | Major | Resolve one measured state-burst processing hypothesis against a frozen observer contract. |
| [#38](https://github.com/gonz0w/fvwm-x/issues/38) | Major | Resolve one remaining style-application cost hypothesis with preserved configuration precedence. |
| [#39](https://github.com/gonz0w/fvwm-x/issues/39) | Major | Finish one measured X-query/barrier reduction decision without changing Xlib event ownership. |
| [#40](https://github.com/gonz0w/fvwm-x/issues/40) | Minor | Retire the finite R2 predecessor list whose replacements are already qualified. |
| [#41](https://github.com/gonz0w/fvwm-x/issues/41) | Major | Accept a finite per-mode worker capability policy and platform feasibility decision. |
| [#42](https://github.com/gonz0w/fvwm-x/issues/42) | Major | Complete the approved finite Perl framework retirement and migration floor. |
| [#227](https://github.com/gonz0w/fvwm-x/issues/227) | Minor | Deliver a finite optional local fuzz target/corpus set for selected owned boundaries. |
| [#232](https://github.com/gonz0w/fvwm-x/issues/232) | Major | Implement and qualify the accepted finite OpenBSD image-worker pledge/unveil policies. |
| [#233](https://github.com/gonz0w/fvwm-x/issues/233) | Major | Implement and qualify finite FreeBSD image-worker Capsicum descriptor policies. |
| [#234](https://github.com/gonz0w/fvwm-x/issues/234) | Major | Implement and qualify finite detected Linux worker restrictions with truthful support reporting. |
| [#236](https://github.com/gonz0w/fvwm-x/issues/236) | Minor | Complete a bounded OpenBSD allocator-audit qualification profile. |
| [#259](https://github.com/gonz0w/fvwm-x/issues/259) | Major | Record a finite acceptance decision for the R2 candidate and its frozen maintenance manifest, preserving the shipped R1 desktop features. |

## R3 — Developer Tooling and Deferred Work manifest

| Issue | Work | End result |
| --- | --- | --- |
| [#55](https://github.com/gonz0w/fvwm-x/issues/55) | Major | Finish the finite first-party build/docs/generator/test/VM tooling language-retirement manifest after the R2 runtime retirement. |
| [#257](https://github.com/gonz0w/fvwm-x/issues/257) | Major | Resolve the preserved, bounded pathname-construction proposal with supported review access, evidence-backed changes and matching caller tests, or an explicit evidenced no-change disposition. |
| [#258](https://github.com/gonz0w/fvwm-x/issues/258) | Major | Resolve the preserved finite Backer admission/ownership checkpoint and its failing combined valid-input control after a supported review route is available. |
| [#260](https://github.com/gonz0w/fvwm-x/issues/260) | Major | Record a finite acceptance decision for the R3 developer-tooling/deferred-work candidate without reopening accepted desktop features. |
