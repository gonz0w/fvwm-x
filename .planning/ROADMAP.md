# Roadmap: FVWM-X

**Milestone:** v0.1 Monitor and HiDPI Foundations
**Status:** Planning — not started
**Granularity:** Fine

## Overview

FVWM-X v0.1 proceeds through dependency-ordered technical layers: establish an authoritative live-X monitor topology, define one rational scale policy, freeze geometry and ownership contracts, resolve WM-owned UI resources, centralize cross-monitor transitions, reconcile reload and hotplug, extend the module boundary, and close with the native-Linux release matrix. X11-facing geometry remains in actual pixels throughout.

## Phases

- [ ] **Phase 1: Topology Snapshot and Live-X Harness** - Establish trustworthy committed RandR Monitor facts and reproducible live-X topology scenarios.
- [ ] **Phase 2: Scale Policy and Configuration** - Resolve one deterministic rational scale per monitor from overrides, validated DPI, and safe fallbacks.
- [ ] **Phase 3: Geometry and Ownership Contracts** - Define deterministic ownership, constrained sizing, placement, and pixel-protocol boundaries.
- [ ] **Phase 4: WM-Owned UI Metrics and Resources** - Scale all FVWM-owned visual and interaction resources exactly once.
- [ ] **Phase 5: Logical-Size Transition Coordinator** - Apply coherent, drift-free window transitions between differently scaled monitors.
- [ ] **Phase 6: Hotplug, Reload, and Observability** - Reconcile topology and scale changes safely while keeping recovery decisions inspectable.
- [ ] **Phase 7: Module Scale Protocol and Consumers** - Extend the module contract and make the declared bundled modules scale-aware.
- [ ] **Phase 8: Release Verification Matrix** - Prove the complete v0.1 behavior against nested X servers, representative clients, and compositor variants.

## Phase Details

### Phase 1: Topology Snapshot and Live-X Harness

**Goal**: Users and maintainers can rely on one complete, atomic, reproducible view of the live RandR Monitor topology.
**Depends on**: Nothing (first phase)
**Requirements**: TOP-01, TOP-03, TOP-06
**Success Criteria** (what must be TRUE):

  1. User can inspect a committed monitor snapshot containing stable identity, root-pixel geometry, physical dimensions, primary state, complete output membership, and topology epoch.
  2. A burst of topology events produces one validated atomic commit, and a failed monitor query leaves the last good topology active.
  3. Maintainers can reproduce monitor creation, mutation, and removal with Xvfb smoke scenarios and authoritative Xorg-dummy scenarios.

**Plans**: 3/8 plans executed

Plans:
**Wave 1**

- [x] 01-01-PLAN.md — Trace detached RandR facts through canonical atomic epoch publication

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 01-02-PLAN.md — Make FScreen the failure-safe live-X snapshot publisher

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 01-03-PLAN.md — Define and test bounded two-deadline burst settling
- [ ] 01-05-PLAN.md — Expose and document Status v4 plus PrintInfo Topology

**Wave 4** *(blocked on Wave 3 completion)*

- [ ] 01-04-PLAN.md — Wire complete topology events to one deferred semantic commit

**Wave 5** *(blocked on Wave 4 completion)*

- [ ] 01-06-PLAN.md — Build the isolated live-X harness and prove failed-query injection

**Wave 6** *(blocked on Wave 5 completion)*

- [ ] 01-07-PLAN.md — Add capability-aware mandatory Xvfb smoke coverage

**Wave 7** *(blocked on Wave 6 completion)*

- [ ] 01-08-PLAN.md — Add authoritative Xorg-dummy recovery coverage and native-Linux CI

### Phase 2: Scale Policy and Configuration

**Goal**: Every active monitor has one explainable, safe, and exact effective scale before any geometry or rendering consumer uses it.
**Depends on**: Phase 1
**Requirements**: SCALE-01, SCALE-02, SCALE-03, SCALE-04, SCALE-05
**Success Criteria** (what must be TRUE):

  1. User can assign a rational scale to a uniquely named monitor and see it take precedence over automatic policy.
  2. Without an override, valid pixel and millimetre dimensions produce an automatic scale; invalid data follows the configured global fallback and then safe 1x chain.
  3. Invalid scale input or ambiguous monitor matching produces a visible diagnostic with the chosen fallback rather than silent coercion.
  4. Supported quarter-step factors from 1x through 2x and higher convert deterministically without cumulative rounding drift.

**Plans**: TBD

### Phase 3: Geometry and Ownership Contracts

**Goal**: Window ownership and all scale-sensitive geometry obey deterministic coordinate, constraint, and X11 pixel contracts.
**Depends on**: Phase 2
**Requirements**: TOP-02, GEO-01, GEO-03, GEO-05, GEO-07, TEST-01
**Success Criteria** (what must be TRUE):

  1. Every window receives the same deterministic owner for negative origins, gaps, clones, partial overlaps, rotations, output reorderings, and seam ties, with incumbent preference and hysteresis where applicable.
  2. Initial placement, work areas, struts, and monitor-targeted movement remain correct in literal root-pixel coordinates.
  3. A scaled target size obeys ICCCM minimum, maximum, base, increment, aspect, fixed-size, and gravity constraints.
  4. Clients and modules continue to receive EWMH, ICCCM, configure, frame-extents, work-area, and geometry values in actual X11 pixels.
  5. Maintainers can run deterministic native tests for scale arithmetic, signed rounding, invalid physical data, snapshot diffs, ownership ties, ICCCM constraints, and 1,000 repeated round trips.

**Plans**: TBD

### Phase 4: WM-Owned UI Metrics and Resources

**Goal**: All FVWM-owned visuals and interaction regions remain physically usable and internally consistent at every supported monitor scale.
**Depends on**: Phase 3
**Requirements**: UI-01, UI-02, UI-03, UI-04, UI-05, UI-06, TEST-05
**Success Criteria** (what must be TRUE):

  1. Titlebars, borders, title fonts, and window buttons render at the owning monitor's scale.
  2. Menus, icons, iconified-window UI, and move/resize feedback render at the active monitor's scale.
  3. Cursors, resize handles, title-button targets, invisible interaction regions, pan zones, edge actions, snapping distances, and drag thresholds are appropriately scaled.
  4. Fonts and source images are scaled exactly once with target-aware caches, and fractional chrome has no clipped text, gaps, stale resources, or displaced cursor hot spots.
  5. Maintainers can build the supported profile with Xcursor, XRender, PNG, and SVG acceptance paths explicitly enabled.

**Plans**: TBD
**UI hint**: yes

### Phase 5: Logical-Size Transition Coordinator

**Goal**: Windows cross monitor scale boundaries through one coherent transition that preserves perceived size, interaction anchors, and window state.
**Depends on**: Phase 4
**Requirements**: GEO-02, GEO-04, GEO-06
**Success Criteria** (what must be TRUE):

  1. An eligible window preserves its logical client size across scale changes and repeated fractional round trips without drift.
  2. User can drag a window across scales without losing the grabbed pointer anchor or triggering ownership oscillation.
  3. Maximize, fullscreen, unmaximize, restore, and monitor-transition behavior produces correct destination geometry at every supported scale.

**Plans**: TBD

### Phase 6: Hotplug, Reload, and Observability

**Goal**: Runtime topology and scale changes reconcile once, safely, and explainably without stranding windows or restarting FVWM-X.
**Depends on**: Phase 5
**Requirements**: TOP-04, TOP-05, SCALE-06
**Success Criteria** (what must be TRUE):

  1. Reloading scale configuration updates only affected monitors and windows once, without restarting FVWM-X.
  2. When a monitor is removed, its windows remain reachable and preserve normal, maximized, or fullscreen intent on a valid destination.
  3. User can inspect and simulate topology, scale inputs, fallback reasons, ownership, transition epochs, and constraint outcomes for runtime changes.

**Plans**: TBD

### Phase 7: Module Scale Protocol and Consumers

**Goal**: Bundled modules share the v0.1 scale model while preserving existing pixel geometry semantics across the process boundary.
**Depends on**: Phase 6
**Requirements**: MOD-01, MOD-02, MOD-03
**Success Criteria** (what must be TRUE):

  1. Bundled modules receive additive, versioned monitor identity, topology epoch, and effective-scale metadata while existing geometry fields remain actual pixels.
  2. Every bundled module with UI redraws its fonts, controls, icons, cursors, hit targets, and windows for its owning monitor's scale.
  3. FvwmAnimate, FvwmAuto, FvwmBacker, FvwmButtons, FvwmEvent, FvwmForm, FvwmIconMan, FvwmIdent, FvwmMFL, FvwmPager, FvwmPerl, FvwmRearrange, and FvwmScript consume or safely propagate the scale contract.

**Plans**: TBD
**UI hint**: yes

### Phase 8: Release Verification Matrix

**Goal**: Maintainers can demonstrate that the integrated v0.1 scale and topology contract holds across supported layouts, clients, compositing modes, and event pressure.
**Depends on**: Phase 7
**Requirements**: TEST-02, TEST-03, TEST-04
**Success Criteria** (what must be TRUE):

  1. Maintainers can verify 1x/2x and fractional monitor pairs, negative/overlapping/cloned layouts, invalid dimensions, hotplug, primary changes, and live reload against nested X servers.
  2. Maintainers can verify transitions with fixed-size, increment-constrained, Qt, GTK, terminal, and changing-hints clients.
  3. The same geometry results occur with and without an external compositor, while topology storms remain bounded and the desktop stays responsive.

**Plans**: TBD

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Topology Snapshot and Live-X Harness | 3/8 | In Progress|  |
| 2. Scale Policy and Configuration | 0/TBD | Not started | - |
| 3. Geometry and Ownership Contracts | 0/TBD | Not started | - |
| 4. WM-Owned UI Metrics and Resources | 0/TBD | Not started | - |
| 5. Logical-Size Transition Coordinator | 0/TBD | Not started | - |
| 6. Hotplug, Reload, and Observability | 0/TBD | Not started | - |
| 7. Module Scale Protocol and Consumers | 0/TBD | Not started | - |
| 8. Release Verification Matrix | 0/TBD | Not started | - |
