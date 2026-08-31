# Requirements: FVWM-X v0.1

**Defined:** 2026-08-29
**Core Value:** Users can depend on a responsive X11 desktop whose windows remain usable and visually consistent across changing monitor layouts and scale factors.

## v0.1 Requirements

### Topology

- [x] **TOP-01**: User can inspect one committed RandR 1.5 Monitor snapshot containing stable identity, root-pixel geometry, physical dimensions, primary state, output membership, and topology epoch.
- [x] **TOP-02**: User gets deterministic monitor identity and ownership behavior for negative origins, gaps, exact clones, partial overlaps, rotations, and output reordering.
- [x] **TOP-03**: User sees topology event bursts coalesced into one validated atomic commit, with the last good topology retained after query failure.
- [ ] **TOP-04**: User's windows remain reachable and retain normal, maximized, or fullscreen intent when their monitor is removed.
- [ ] **TOP-05**: User can inspect and simulate topology, scale inputs, fallback reasons, ownership, transition epochs, and constraint outcomes.
- [x] **TOP-06**: Maintainers can reproduce monitor creation, changes, and removal through Xvfb smoke tests and authoritative Xorg-dummy scenarios.

### Scale Policy

- [x] **SCALE-01**: User can assign an explicit rational scale to a uniquely named monitor.
- [x] **SCALE-02**: User can rely on automatic scale derived from validated pixel and millimetre dimensions when no override exists.
- [x] **SCALE-03**: User gets deterministic precedence: monitor override, validated automatic DPI, configured global fallback, then safe 1x.
- [x] **SCALE-04**: User receives a visible diagnostic when scale input or monitor matching is invalid instead of silent coercion.
- [x] **SCALE-05**: User can use 1x, 1.25x, 1.5x, 1.75x, 2x, and higher supported quarter-step factors without cumulative rounding drift.
- [ ] **SCALE-06**: User can reload scale configuration and have only affected monitors and windows update once without restarting FVWM-X.

### WM-Owned UI

- [ ] **UI-01**: User sees titlebars, borders, title fonts, and window buttons rendered at the owning monitor's scale.
- [ ] **UI-02**: User sees fonts and image assets scaled exactly once from source, with caches separated by target scale, dimensions, and generation.
- [ ] **UI-03**: User sees menus, icons, iconified-window UI, and move/resize feedback rendered at the active monitor's scale.
- [ ] **UI-04**: User gets appropriately sized WM cursors, resize handles, title-button targets, and invisible interaction regions.
- [ ] **UI-05**: User gets scale-aware pan zones, edge actions, snapping distances, and drag thresholds.
- [ ] **UI-06**: User gets seamless fractional-scale chrome without clipped text, titlebar gaps, stale resources, or cursor hot-spot displacement.

### Geometry

- [x] **GEO-01**: User's window has one deterministic owning monitor with incumbent tie-breaking and seam hysteresis.
- [ ] **GEO-02**: User's eligible window preserves logical client size across scale transitions and repeated round trips without drift.
- [ ] **GEO-03**: User's scaled target size obeys ICCCM minimum, maximum, base, increment, aspect, fixed-size, and gravity constraints.
- [ ] **GEO-04**: User can drag a window across scales without losing the grabbed pointer anchor or causing ownership oscillation.
- [ ] **GEO-05**: User gets correct initial placement, work areas, struts, and monitor-targeted movement in root-pixel coordinates.
- [ ] **GEO-06**: User gets correct maximize, fullscreen, unmaximize, restore, and monitor-transition behavior at every supported scale.
- [ ] **GEO-07**: Clients and modules continue receiving EWMH, ICCCM, configure, frame-extents, work-area, and geometry values in actual X11 pixels.

### Modules

- [ ] **MOD-01**: Bundled modules receive additive, versioned monitor identity, topology epoch, and effective-scale metadata without reinterpreting existing pixel geometry.
- [ ] **MOD-02**: Every bundled module with UI redraws its fonts, controls, icons, cursors, hit targets, and windows for its owning monitor's scale.
- [ ] **MOD-03**: `FvwmAnimate`, `FvwmAuto`, `FvwmBacker`, `FvwmButtons`, `FvwmEvent`, `FvwmForm`, `FvwmIconMan`, `FvwmIdent`, `FvwmMFL`, `FvwmPager`, `FvwmPerl`, `FvwmRearrange`, and `FvwmScript` consume or safely propagate the v0.1 scale contract.

### Verification

- [ ] **TEST-01**: Maintainers can verify scale arithmetic, signed rounding, invalid physical data, snapshot diffs, ownership ties, ICCCM constraints, and 1,000 repeated round trips with deterministic native tests.
- [ ] **TEST-02**: Maintainers can verify 1x/2x and fractional pairs, negative/overlapping/cloned layouts, invalid dimensions, hotplug, primary changes, and live reload against nested X servers.
- [ ] **TEST-03**: Maintainers can verify transitions with fixed-size, increment-constrained, Qt, GTK, terminal, and changing-hints clients.
- [ ] **TEST-04**: Maintainers can verify identical geometry with and without an external compositor and responsive bounded processing during topology storms.
- [ ] **TEST-05**: Maintainers can verify the supported build profile explicitly enables Xcursor, XRender, PNG, and SVG paths used by v0.1.

## Future Requirements

### Compatibility and Policy

- **COMPAT-01**: User can opt a pixel-sensitive application out of logical-size preservation while retaining scaled FVWM-owned chrome.
- **COMPAT-02**: User can use arbitrary scale percentages outside the supported quarter-step scale buckets.
- **COMPAT-03**: User can automatically restore windows to a reconnected monitor according to an explicit session-placement policy.
- **COMPAT-04**: User receives richer historical explanations for topology recovery decisions across reconnects and restarts.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Native Wayland backend or compositor | FVWM-X v0.1 remains an X11 window manager. |
| Integrated compositing | External compositors remain the supported model. |
| Arbitrary client-content scaling | A non-compositing X11 window manager does not own client rendering. |
| RandR output transforms as the scale model | Output image transforms blur or reshape the framebuffer and are not logical UI scale. |
| Per-monitor mutation of global Xft DPI, XSETTINGS, or toolkit environment | These are global X-screen or process inputs and cannot provide reliable per-monitor behavior. |
| EWMH, ICCCM, strut, or module geometry in logical units | X11 protocol boundaries remain actual root/client pixels. |
| Configuration-language redesign | v0.1 adds only the minimum scale expression needed by the runtime model. |
| Full FVWM2/FVWM3 configuration and module compatibility | Clean breaks are acceptable when needed for a coherent FVWM-X contract. |
| Pixel-preservation client opt-out | Explicitly deferred until compatibility evidence justifies it. |

## Traceability

Each v0.1 requirement maps to exactly one roadmap phase.

| Requirement | Phase | Status |
|-------------|-------|--------|
| TOP-01 | Phase 1 | Complete |
| TOP-02 | Phase 3 | Complete |
| TOP-03 | Phase 1 | Complete |
| TOP-04 | Phase 6 | Pending |
| TOP-05 | Phase 6 | Pending |
| TOP-06 | Phase 1 | Complete |
| SCALE-01 | Phase 2 | Complete |
| SCALE-02 | Phase 2 | Complete |
| SCALE-03 | Phase 2 | Complete |
| SCALE-04 | Phase 2 | Complete |
| SCALE-05 | Phase 2 | Complete |
| SCALE-06 | Phase 6 | Pending |
| UI-01 | Phase 4 | Pending |
| UI-02 | Phase 4 | Pending |
| UI-03 | Phase 4 | Pending |
| UI-04 | Phase 4 | Pending |
| UI-05 | Phase 4 | Pending |
| UI-06 | Phase 4 | Pending |
| GEO-01 | Phase 3 | Complete |
| GEO-02 | Phase 5 | Pending |
| GEO-03 | Phase 3 | Pending |
| GEO-04 | Phase 5 | Pending |
| GEO-05 | Phase 3 | Pending |
| GEO-06 | Phase 5 | Pending |
| GEO-07 | Phase 3 | Pending |
| MOD-01 | Phase 7 | Pending |
| MOD-02 | Phase 7 | Pending |
| MOD-03 | Phase 7 | Pending |
| TEST-01 | Phase 3 | Pending |
| TEST-02 | Phase 8 | Pending |
| TEST-03 | Phase 8 | Pending |
| TEST-04 | Phase 8 | Pending |
| TEST-05 | Phase 4 | Pending |

**Coverage:**

- v0.1 requirements: 33 total
- Mapped to phases: 33
- Unmapped: 0
- Duplicate mappings: 0
- Coverage: 100%

---
*Requirements defined: 2026-08-29*
*Last updated: 2026-08-29 after roadmap creation*
