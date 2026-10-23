# Maintenance-first development batches

Accepted user direction, 2026-10-23: pause new features and organize source
cleanup, legacy retirement, dependency reduction and maintenance before resuming
feature completion. This supersedes the earlier feature-first cutoff and the
Later placement in the dependency-simplification plan. It does not undo accepted
component work or authorize deleting supported behavior by age/name alone.

The tracker remains the execution authority. This register reconciles all **73
open issues** in the captured GitHub backlog: **34 maintenance owners/children**
in **eight implementation batches and three bounded decision lanes**, **12
conditional qualification owners**, **five deferred performance owners** and
**22 paused feature/polish owners**. Every open issue has one primary placement;
cross-references below do not create extra work or acceptance claims.

## Development queue

Use one coherent PR per completed owner slice, usually combining six to ten
related improvements where useful. The rows are development groups, not a
promise of one giant PR per entire broad audit. Freeze the named entry points
and remaining acceptance at pickup; implementation/tests/docs are one item.
Finish each related group before its combined focused gate. Main order is
M1 → M2 → M3 → M4 → M5 → M6 → M7 → M8; the dependency column permits an
independent lane when useful without requiring concurrent agents or VM jobs.

| Batch | Primary owners/children | Prerequisites | Reviewable outcome |
| --- | --- | --- | --- |
| M1 — Supported runtime baseline and replaced entry points | [#35](https://github.com/gonz0w/fvwm-x/issues/35), [#40](https://github.com/gonz0w/fvwm-x/issues/40), [#179](https://github.com/gonz0w/fvwm-x/issues/179), [#180](https://github.com/gonz0w/fvwm-x/issues/180), [#181](https://github.com/gonz0w/fvwm-x/issues/181) | None; scope intake first | Require the approved offline Go compiler; retire disabled-Go utility paths, replaced XDG/converter implementations and the pre-FVWM3 converter as one coherent install-contract batch. |
| M2 — Remaining first-party runtime controllers | [#7](https://github.com/gonz0w/fvwm-x/issues/7), [#15](https://github.com/gonz0w/fvwm-x/issues/15), [#23](https://github.com/gonz0w/fvwm-x/issues/23) | M1 | Port remaining desktop/launcher/session/screenshot controllers in native-UI/process ownership groups; remove each predecessor after installed parity. Keep explicit arbitrary shell execution and native capture boundaries. |
| M3 — Target-owned linkage and dependency closure | [#11](https://github.com/gonz0w/fvwm-x/issues/11), [#13](https://github.com/gonz0w/fvwm-x/issues/13), [#47](https://github.com/gonz0w/fvwm-x/issues/47), [#49](https://github.com/gonz0w/fvwm-x/issues/49), [#184](https://github.com/gonz0w/fvwm-x/issues/184) | M1 | Map real symbols/static transitives and replace broad dependency bags by core/module/worker target groups. Remove only unused edges, with package/license/installed closure measurements. |
| M4 — Obsolete SVG backend removal | [#8](https://github.com/gonz0w/fvwm-x/issues/8), [#46](https://github.com/gonz0w/fvwm-x/issues/46), [#183](https://github.com/gonz0w/fvwm-x/issues/183) | M3 | Consolidate the maintained librsvg/Cairo path and remove obsolete backend detection/options/branches; retain SVG-disabled, raster/XPM, scale/alpha and bounded worker behavior. |
| M5 — Core/module source and ownership cleanup | [#4](https://github.com/gonz0w/fvwm-x/issues/4), [#6](https://github.com/gonz0w/fvwm-x/issues/6), [#9](https://github.com/gonz0w/fvwm-x/issues/9), [#25](https://github.com/gonz0w/fvwm-x/issues/25), [#31](https://github.com/gonz0w/fvwm-x/issues/31), [#34](https://github.com/gonz0w/fvwm-x/issues/34), [#36](https://github.com/gonz0w/fvwm-x/issues/36) | M1 | Batch named ownership/error/unwind families and shared readiness consumers. Preserve accepted fixes and single-thread dispatch/order; no global rewrite or performance-only restructuring. |
| M6 — Perl framework retirement and scripting disposition | [#42](https://github.com/gonz0w/fvwm-x/issues/42), [#50](https://github.com/gonz0w/fvwm-x/issues/50) | M1, M2 | Qualify the bounded scripting migration floor, then retire FvwmPerl/perllib/fvwm-perllib and Perl-only build/templates. Keep unrelated FVWM3/FvwmScript behavior and user files. |
| M7 — Owned build/generator/test tool retirement | [#14](https://github.com/gonz0w/fvwm-x/issues/14), [#17](https://github.com/gonz0w/fvwm-x/issues/17), [#53](https://github.com/gonz0w/fvwm-x/issues/53), [#55](https://github.com/gonz0w/fvwm-x/issues/55) | M2, M6 | Port first-party tools in build/source, generation/docs and independent test/VM-driver groups; remove predecessors only after parity, failure capture and offline/reproducibility checks. |
| M8 — Distribution, portability and documentation closure | [#43](https://github.com/gonz0w/fvwm-x/issues/43), [#52](https://github.com/gonz0w/fvwm-x/issues/52) | M1, M2, M3, M4, M5, M6, M7 | Reconcile install/source archives, manuals/examples, package/license references and platform branches; complete accumulated required maintenance qualification once, with release/hardware gaps explicit. |
| D1 — Build backend evaluation | [#182](https://github.com/gonz0w/fvwm-x/issues/182) | None; scope intake first | Bounded Muon/CMake prototype and adopt/defer/reject decision. Meson/Ninja stays authoritative until a separately qualified adoption; no repository-wide build rewrite by inference. |
| D2 — JSON dependency evaluation | [#48](https://github.com/gonz0w/fvwm-x/issues/48) | M3 | Compare maintained cJSON versus yyjson on existing bounded workloads and emit a decision. Do not replace the parser or add a mandatory dependency automatically. |
| D3 — Helper sandbox scope evaluation | [#41](https://github.com/gonz0w/fvwm-x/issues/41) | None; scope intake first | Reconcile the misleading geometry title with the existing helper-sandbox scope. Assess narrow helper isolation and platform access requirements; no whole-core sandbox adoption. |

## First development batch: M1

#179 and #181 are Ready. #180 stays Backlog/blocked by #179; perform its
implementation in the same combined candidate once the required compiler and
install contract is established. Do not merge an intermediate broken fallback
removal. The broader #42/#50 scripting dependency does not block the already
replaced utility subset in #180; full Perl framework retirement remains M6.

1. Reconcile the accepted utility/Go baseline with actual Meson options and
   `bin/meson.build`, including installed names/arguments and offline inputs.
2. Require the approved compiler without implicit downloads. Remove supported
   disabled-Go utility install branches and misleading feature summaries.
3. Remove replaced Python/PyXDG and original Perl menu paths, converter fallbacks
   and their now-unnecessary runtime dependency recommendations.
4. Retire both language implementations of `fvwm-convert-2.6`, its tests/install/
   man targets and pre-3.x references; classify other shipped examples individually.
5. Replace only active retired-oracle requirements with independent fixed
   contracts. Preserve the remaining test coverage, provenance and failure cases.
6. Update manuals, migration notices, package/source manifests and exact removed
   file/dependency inventory together. Keep modern menus, native FvwmScript,
   XPM and required X11/ICCCM/Linux/BSD behavior.
7. Run one combined affected checkpoint for the compiler/install/profile change,
   rather than rebuilding each OS for every deletion or option edit. Any
   six-target acceptance explicitly required by #179/#180 runs once for this
   complete contract, reusing valid scope and incremental builds.

This plan makes no source deletion or mandatory-Go build switch itself. M1's
manifest/reference checks select exact removals during implementation. #35/#40
retain inactive-code and predecessor ownership; accepted old removals are not
reimplemented. Preserve user configurations, Git/verification history and other
owners' workspaces, builds and VM disks.

## Bounds on later batches

M2 inventories the actual remaining runtime controllers before porting them;
accepted Go utility/provider and browser implementations are reused. Group
settings/desktop control, launcher/session coordination and screenshot/native
capture by shared behavior. Tests establish parity and explicit ownership;
replacing Python does not justify a toolkit or a new core state owner.

M3 maps target dependencies before removing an edge; static/transitive use is
real use. M4 follows that map and retains the qualified raster/native fallback
and SVG-disabled profile. The separate LunaSVG alternative remains a bounded
LIB-04 evaluation, not an automatic renderer replacement or a prerequisite to
removing the obsolete libsvg-cairo branch. No cJSON/yyjson adoption follows D2
without an explicit decision.

M5 first reconciles already accepted source families and named remaining
findings. Use two bounded slices: protocol/stream/callback/resource ownership
(#4/#6/#7/#15/#31/#34), then shared readiness/interaction/unwind consumers
(#9/#25/#36/#34). Move checked policy and consolidate proven duplication while
preserving per-producer continuations, native barriers, partial packets and
death-row lifetime. Do not manufacture a defect or rewrite a whole event loop
to make an audit issue look complete. Newly reproduced serious failures take
priority with their concrete regression; performance-only tail work stays deferred.

M6 requires a qualified minimum scripting migration/disposition before claiming
Perl retirement complete. #50 owns the pinned isolated Lua engine evaluation and
useful migration examples; new programmable-UI features remain paused. An
unresolved engine/adoption gate blocks this retirement slice specifically, not
all other maintenance. Arbitrary user Perl is not translated or deleted.

M7 separates build/source helpers, metadata/docs/catalog generation, and native
integration/VM/maintenance tests into parity groups. Retain independent ABI/X11/
fault oracles rather than deleting tests to remove an interpreter. Accepted
parser/catalog/tool migrations are reused. D1 can complete as adopt/defer/reject;
a full build-backend migration needs its own qualified decision. Distinguish
interpreter-free installed runtime, no first-party Python/Perl tools, and no
Python required by the complete build/test workflow. Meson's own Python does
not disappear by porting project scripts.

M8 closes active install, source-distribution, manual, example, package and
license references, plus actual portability branches. Keep optional supported
features and target-native libraries where used. Web redesign, new languages,
new UI and unrelated compatibility expansion are outside this cleanup. Reuse
accepted compiler/platform/profile evidence and execute missing accumulated
contract checks; publication/credentials/upstream notifications remain separately owned.

## Conditional qualification owners

These issues stay open with their existing owner/status/milestone. They are
consulted when the selected maintenance contract touches their behavior, rather
than reopened wholesale as another audit or feature implementation queue.

| Owners | Maintenance trigger |
| --- | --- |
| [#3](https://github.com/gonz0w/fvwm-x/issues/3), [#5](https://github.com/gonz0w/fvwm-x/issues/5), [#16](https://github.com/gonz0w/fvwm-x/issues/16) | Configuration, standards and exact qualification evidence for changed contracts; no blanket conformance claim. |
| [#18](https://github.com/gonz0w/fvwm-x/issues/18), [#19](https://github.com/gonz0w/fvwm-x/issues/19), [#22](https://github.com/gonz0w/fvwm-x/issues/22) | Existing EWMH, monitor and selection behavior affected by cleanup; new protocol/topology/clipboard capabilities stay paused. |
| [#45](https://github.com/gonz0w/fvwm-x/issues/45), [#51](https://github.com/gonz0w/fvwm-x/issues/51), [#54](https://github.com/gonz0w/fvwm-x/issues/54) | Preserve existing catalogs, module scale and text/input behavior when retiring a consumer; new translation/shaping/UI expansion stays paused. |
| [#56](https://github.com/gonz0w/fvwm-x/issues/56) | Existing menu/Desktop Entry/interoperability parity during runtime retirement; no new FileManager1/default-role feature. |
| [#79](https://github.com/gonz0w/fvwm-x/issues/79) | Final release gate remains separately owned; this planning work does not publish a release. |
| [#93](https://github.com/gonz0w/fvwm-x/issues/93) | Existing OpenBSD AMD64 VESA investigation retains original profile/evidence. Use current supported releases; do not resume retired 7.9 experiments or block portable cleanup without a current failure. |

## Parked work

Performance-only owners remain deferred to a later release: [#29](https://github.com/gonz0w/fvwm-x/issues/29), [#33](https://github.com/gonz0w/fvwm-x/issues/33), [#37](https://github.com/gonz0w/fvwm-x/issues/37), [#38](https://github.com/gonz0w/fvwm-x/issues/38), [#39](https://github.com/gonz0w/fvwm-x/issues/39).
Preserve existing improvements and periodic comparisons; no tuning or new
benchmark build is pulled into this maintenance sweep by an issue title.

New feature/polish pickup is paused: [#57](https://github.com/gonz0w/fvwm-x/issues/57), [#60](https://github.com/gonz0w/fvwm-x/issues/60), [#61](https://github.com/gonz0w/fvwm-x/issues/61), [#62](https://github.com/gonz0w/fvwm-x/issues/62), [#63](https://github.com/gonz0w/fvwm-x/issues/63), [#64](https://github.com/gonz0w/fvwm-x/issues/64), [#65](https://github.com/gonz0w/fvwm-x/issues/65), [#66](https://github.com/gonz0w/fvwm-x/issues/66), [#67](https://github.com/gonz0w/fvwm-x/issues/67), [#68](https://github.com/gonz0w/fvwm-x/issues/68), [#69](https://github.com/gonz0w/fvwm-x/issues/69), [#70](https://github.com/gonz0w/fvwm-x/issues/70), [#71](https://github.com/gonz0w/fvwm-x/issues/71), [#72](https://github.com/gonz0w/fvwm-x/issues/72), [#73](https://github.com/gonz0w/fvwm-x/issues/73), [#74](https://github.com/gonz0w/fvwm-x/issues/74), [#75](https://github.com/gonz0w/fvwm-x/issues/75), [#76](https://github.com/gonz0w/fvwm-x/issues/76), [#77](https://github.com/gonz0w/fvwm-x/issues/77), [#78](https://github.com/gonz0w/fvwm-x/issues/78), [#80](https://github.com/gonz0w/fvwm-x/issues/80), [#81](https://github.com/gonz0w/fvwm-x/issues/81).
Accepted components, screenshots and tests remain accepted under their actual
sources. Only existing-behavior parity needed by a named removal is in scope;
no new browser/clipboard/jobs/session/settings/theme/service feature is selected.
Keep stages and feature milestones intact, set remaining work to Backlog, and
record the pause instead of closing these issues. No feature PR was open when
this plan was prepared. User direction is required to resume feature pickup.

## Testing and completion

Follow the [risk-based cadence](BATCHED-CLEANUP-CADENCE-2026-10-23.md): one
incremental Linux profile and affected tests after a combined portable batch;
scoped race/sanitizers/X checks for affected ownership/concurrency/visible state.
Use other OSs, architectures, compiler/install/default-X profiles when their
contract changes or a concrete failure needs them. No automatic all-OS job at a
PR counter. Review accumulated risk after about five merged implementation
batches; full required milestone/release qualification still runs once with
valid evidence reuse. Three-theme/screenshots apply to changed rendering/UI,
not a source move or documentation cleanup.

An issue closes only when its own acceptance is met. Finishing a child does not
close its broad parent, and moving a card is not implementation or qualification.
Decision lanes finish with a specific adopt/defer/reject outcome and bounded
remaining gaps. At each pickup declare finite files/behaviors and dispositions
for remaining criteria; keep unresolved platform/hardware/toolkit/release gaps
explicit. Do not reopen accepted audits or treat the plan as an unlimited rewrite.

The Project's **Work batch** field mirrors this register; existing stage/priority
ownership remains. Promote maintenance issues previously in Later to milestone
05; keep older maintenance milestones with their owners. #179/#181 are Ready;
other maintenance work is Backlog until pickup. Preserve real native blocked-by
and parent relations. Snapshot and readback belong to the
[planning receipt](../verification/MAINTENANCE-PLANNING-2026-10-23.md).
