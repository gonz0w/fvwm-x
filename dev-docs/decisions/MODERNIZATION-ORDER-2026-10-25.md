# Modernization scope and dependency order

Owner: Cam (@gonz0w). Accepted direction, 2026-10-25: record the discussed
modernization ideas as issues, reconcile affected owners and order shared work
to avoid reimplementation. **Cleanup and measured optimization remain the current
priority. New feature implementation stays Backlog.**

The [tracker](../PROJECT-TRACKER.md#execution-order) remains authoritative. This
decision supersedes the earlier performance-only deferral in the October 21
handoff and October 23 maintenance plan. It preserves maintenance-first pickup,
accepted component evidence, approved retirement boundaries and the existing
risk-based batch cadence. It approves scope/planning, not runtime implementation,
library/default adoption, a new release prerequisite or release publication.

## Current foundation and finite handoff

Preserve accepted M1 runtime-baseline removals, M2 Go resident/controller work
and M3 target-owned dependency/linkage work. In particular, closed #23, #35,
#47, #49, #179, #180, #181 and #184 stay closed. Existing settings/editor,
configuration-checker, IPC/preview and native collection/browser components
remain accepted in their original source/profile scopes.

Continue the remaining [maintenance batches](MAINTENANCE-FIRST-BATCHES-2026-10-23.md),
starting with the accepted M3 handoff into M4 SVG consolidation (#183). Select
bounded remaining M5 source/ownership, M6 retirement, M7 tools and M8 distribution
slices by their existing owners. Interleave **O1 measured optimization** under
#29, #33, #37, #38 and #39; #34 remains the cleanup owner. #29 and #34 are P1;
all five performance owners return to milestone 04, retaining stage 04.
No issue is In progress or complete merely because it was reprioritized.

FOUNDATION-READY-01 #209 is a finite scheduling gate, not another broad audit:

1. At pickup, freeze the named remaining maintenance outcomes, entry points,
   owner, evidence and explicit exit criteria. Do not restart accepted work.
2. Complete those selected cleanup slices and one measured optimization
   checkpoint. Record exact accepted outcomes and owner-approved dispositions
   of remaining work; unsupported deferral is not a passing result.
3. Capture core plus enabled-module idle wakeups, latency, CPU/RSS, startup,
   redraw/X requests and installed dependency cost where measurable. Preserve
   before/after inputs and mark unavailable native/hardware observations.
4. Record the maintainer-accepted handoff before feature issues move to Ready.
   Do not require every optional audit/experiment or physical/release gate to
   close, and do not implement a new feature framework to finish cleanup.

BUILD-PROFILES-01 #226 follows #53/#55's existing maintenance ownership.
FUZZ-01 #227 is an optional, bounded local-verification child of #5. Neither
automatically expands all feature acceptance or blocks every other owner.

## Shared owners and delivery waves

MODERNIZE-01 #208 is the program index. Existing #60/#61/#70 retain typed
settings, safe checking, structured queries/results and revisions. #54 owns
text/input; evaluate HarfBuzz/Unicode helpers there only with a justified full
cost/qualification result. #56 owns XDG correctness, #8/#25/#33 own image,
drawing and cache lifetimes, and #19 owns committed monitor/scale semantics.
No new transport, settings store, decoder, core state authority or search daemon
is introduced by this plan.

F1 shared theme/icon/binding contracts precede their F2 native UI and F3
workspace consumers. Native layout and scratchpads do not depend on tiling,
overview does not depend on a compositor, and build profiles do not wait for
#182's alternative-backend experiment. F4 services/effects use the same qualified
session/graphics contracts; portable locking does not require optional D-Bus.

| Issue / tracker ID | Work batch | Parent | Native blockers | Owned outcome |
| --- | --- | --- | --- | --- |
| [MODERNIZE-01](https://github.com/gonz0w/fvwm-x/issues/208) | Future features | Program index | Owner-scoped intake; scheduling policy applies | Track the approved lightweight X11 modernization program without duplicating existing owners. This is an index and sequencing owner, not runtime implementation or a new release prerequisite. |
| [FOUNDATION-READY-01](https://github.com/gonz0w/fvwm-x/issues/209) | O1 Measured optimization | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | Owner-scoped intake; scheduling policy applies | Turn the user's cleanup/optimization-first priority into a finite, reviewable scheduling gate. Existing qualified components stay accepted; planning does not reopen them. |
| [GRAPHICS-01](https://github.com/gonz0w/fvwm-x/issues/210) | F2 Native UI | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#209](https://github.com/gonz0w/fvwm-x/issues/209), [#183](https://github.com/gonz0w/fvwm-x/issues/183) | Implement the already approved native graphics baseline using existing X11 drawing/resource owners, before adding another direct renderer. |
| [GRAPHICS-02](https://github.com/gonz0w/fvwm-x/issues/211) | F2 Native UI | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#210](https://github.com/gonz0w/fvwm-x/issues/210) | Compare a bounded direct Cairo module prototype with GRAPHICS-01. Existing SVG/Cairo linkage is not proof of a general UI renderer. |
| [LAYOUT-01](https://github.com/gonz0w/fvwm-x/issues/212) | F2 Native UI | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#209](https://github.com/gonz0w/fvwm-x/issues/209) | Deliver the existing native-layout-first direction for panels and collection consumers; preserve configured legacy geometry. |
| [BIND-01](https://github.com/gonz0w/fvwm-x/issues/213) | F1 Shared contracts | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#209](https://github.com/gonz0w/fvwm-x/issues/209) | Extend accepted CFG/IPC/provider observation into reusable native model/view bindings; reuse existing transport, state and revision authority. |
| [THEME-02](https://github.com/gonz0w/fvwm-x/issues/214) | F1 Shared contracts | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#209](https://github.com/gonz0w/fvwm-x/issues/209) | Extend existing palettes/colorsets and the typed settings registry into a small data-driven theme model without replacing FVWM configuration. |
| [ICON-01](https://github.com/gonz0w/fvwm-x/issues/215) | F1 Shared contracts | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#209](https://github.com/gonz0w/fvwm-x/issues/209), [#183](https://github.com/gonz0w/fvwm-x/issues/183) | Reuse the existing Go XDG icon-theme lookup as a bounded shared resolver for native UI, applications, files and collections. |
| [ICON-02](https://github.com/gonz0w/fvwm-x/issues/216) | F2 Native UI | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#215](https://github.com/gonz0w/fvwm-x/issues/215) | Support configurable font icons alongside SVG/application icons using the existing font stack and named icon contract. |
| [RELOAD-01](https://github.com/gonz0w/fvwm-x/issues/217) | F2 Native UI | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#214](https://github.com/gonz0w/fvwm-x/issues/214) | Build supported live appearance/component authoring on accepted CFG preview/recovery and generation contracts. |
| [INSPECT-01](https://github.com/gonz0w/fvwm-x/issues/218) | F2 Native UI | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#214](https://github.com/gonz0w/fvwm-x/issues/214) | Extend accepted IPC binding/colorset/appearance inspection into a native opt-in configuration authoring tool. |
| [ACTIONS-01](https://github.com/gonz0w/fvwm-x/issues/219) | F3 Workspace tools | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#213](https://github.com/gonz0w/fvwm-x/issues/213), [#212](https://github.com/gonz0w/fvwm-x/issues/212) | Deliver the approved discoverable action palette through existing native collection views, structured queries and command ownership. |
| [WORKSPACE-01](https://github.com/gonz0w/fvwm-x/issues/220) | F3 Workspace tools | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#209](https://github.com/gonz0w/fvwm-x/issues/209) | Package reusable per-desktop floating, columns/master-stack and snapping policies while retaining FVWM command/configuration flexibility. |
| [WORKSPACE-02](https://github.com/gonz0w/fvwm-x/issues/221) | F3 Workspace tools | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#209](https://github.com/gonz0w/fvwm-x/issues/209) | Provide configurable reusable scratchpad behavior for terminals, notes and other chosen applications, independently of tiling or compositing. |
| [WORKSPACE-03](https://github.com/gonz0w/fvwm-x/issues/222) | F3 Workspace tools | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#209](https://github.com/gonz0w/fvwm-x/issues/209) | Build user-owned monitor/workspace/panel placement profiles on existing MONITOR/CFG contracts, without resizing arbitrary client pixels. |
| [WORKSPACE-04](https://github.com/gonz0w/fvwm-x/issues/223) | F3 Workspace tools | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#213](https://github.com/gonz0w/fvwm-x/issues/213), [#212](https://github.com/gonz0w/fvwm-x/issues/212), [#215](https://github.com/gonz0w/fvwm-x/issues/215) | Create a bounded searchable overview using native labels/icons first, with optional thumbnails separately qualified. |
| [EFFECTS-01](https://github.com/gonz0w/fvwm-x/issues/224) | F4 Services and effects | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#210](https://github.com/gonz0w/fvwm-x/issues/210) | Evaluate a cooperative external X11 compositor path for coherent workspace transitions, shadows/translucency and optional effects. |
| [LOCK-01](https://github.com/gonz0w/fvwm-x/issues/225) | F4 Services and effects | [#208](https://github.com/gonz0w/fvwm-x/issues/208) | [#209](https://github.com/gonz0w/fvwm-x/issues/209), [#72](https://github.com/gonz0w/fvwm-x/issues/72) | Improve locking through qualified per-OS external providers and the portable session lifecycle; authentication stays outside the WM core. |
| [BUILD-PROFILES-01](https://github.com/gonz0w/fvwm-x/issues/226) | M7 Owned tooling | [#53](https://github.com/gonz0w/fvwm-x/issues/53) | Owner-scoped intake; scheduling policy applies | Make minimal, desktop, graphics and development builds reproducible and understandable using the authoritative backend and target-owned dependencies. |
| [FUZZ-01](https://github.com/gonz0w/fvwm-x/issues/227) | Qualification | [#5](https://github.com/gonz0w/fvwm-x/issues/5) | Owner-scoped intake; scheduling policy applies | Extend existing local verification with focused optional fuzz targets for project-owned external-data boundaries. |

Rows with no additional native blocker still obey the scheduling policy. New
features reach #209 directly or through a named prerequisite. Existing feature
roots #57/#60/#61/#63/#65/#66/#69/#70/#75/#80/#81 also depend on #209;
their descendants inherit that order through existing links. Parent links are
organizational and are not blocked-by links. Broad owner references above mean
reuse an accepted scoped contract, not wait for the owner's entire optional
backlog. The remaining maintenance owners do not depend on new feature delivery.

## Observable acceptance and boundaries

- Themes: versioned named palette/state, typography, spacing/density, geometry,
  icon and motion roles; independent choices and deterministic scoped overrides;
  existing FVWM configuration and colorsets remain usable. #75 keeps shipped
  theme qualification, without making all of #214 a current release requirement.
- Icons: share the existing Go XDG resolver and qualified isolated SVG worker;
  inherited/scaled themes, missing-icon fallback, bounded caches and explicit
  icon-font runs with labels. Lucide, Papirus and Font Awesome are candidates,
  not selected packages/defaults. Do not reintroduce retired PyXDG/converters.
- Graphics/layout: actual XRender resource/latency baseline first; a bounded
  direct Cairo prototype compares unmet vector/effect needs and complete cost.
  Reuse prepared layout for drawing/hit testing and virtualize collections.
- Authoring: reuse accepted CFG/IPC/source queries; supported reload prepares
  before publication and retains the last working state. Inspect provenance,
  effective rules, bindings and capabilities. Dynamic configuration remains
  explicitly unchecked; arbitrary shell actions have no rollback promise.
- Workspace tools: typed/revalidated window/app/action palette, configurable
  hybrid layouts/snapping, independent scratchpads, owned docking profiles and
  keyboard overview. Preserve desk/page/sticky/fullscreen/transient semantics,
  monitor generations, client pixels and command-engine authority.
- Locking: #72 owns the generic portable session foundation; #225 owns selected
  per-OS locker readiness/idle/display-power/suspend coordination and failures.
  #77 extends the same lifecycle with optional bus/inhibitor adapters. Keep
  authentication separate, qualify hotplug/resume/provider death and report
  limitations. A themed presentation must preserve the qualified locker boundary.
- Effects: prototype a named supported external compositor and real workspace
  semantics before interface adoption. Keep compositor-free operation, reduced
  motion, interruption and input latency. No built-in compositor, native Wayland,
  mandatory GPU/blur/toolkit or broad XCB migration is selected.
- Build/verification: backend-neutral minimal/desktop/graphics/development
  manifests, effective feature/dependency cost and reproducible offline archives;
  Meson stays authoritative pending #182's separate adoption. LTO/PGO is measured
  under #29 before defaults change. Optional local fuzzing reuses existing pure
  boundaries, independent oracles, limits and minimized reproducers.

The new native graphics/authoring issue names also capture the earlier discussion
of reusable polish. Other unselected notes, layout-engine, media/protocol and
storage candidates retain their original owners/dispositions; this plan does
not silently add their implementation or reopen previously skipped libraries.

## Verification and reconciliation

Planning changes require source/owner review, local documentation checks and
readback of issue bodies, labels/milestones, Project fields, native parents and
the complete dependency graph. Preserve existing parents/blockers, status,
accepted checklists, PR cards and historical evidence. Verify zero dependency
cycles and that all future features reach #209. Keep exactly one status,
priority and stage label; add stable ID mappings to both roadmap indexes.

No C/Go/Meson/runtime behavior changes in this checkpoint, so no WM build,
benchmark or platform matrix is repeated. Later implementations follow their
actual risk/contract and final milestone/release gates. All verification remains
local; Actions/status publishers stay disabled. See the
[planning report](../verification/MODERNIZATION-PLANNING-2026-10-25.md).
