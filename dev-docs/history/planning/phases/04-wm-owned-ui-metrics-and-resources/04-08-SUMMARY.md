---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 08
subsystem: interaction-chrome
tags: [hidpi, pan-frames, snap, movethreshold, manpage, wm_ui_metrics, c11]

requires:
  - phase: 04-wm-owned-ui-metrics-and-resources
    provides: wm_ui_scale_px, wm_ui_scale_edge_thickness, owning and active monitor factors
provides:
  - Map-time scaled pan frames from stored EdgeThickness source 0-2
  - Owning-monitor snap conversion at use without writing scaled style
  - Pointer-monitor MoveThreshold conversion at drag start
  - User-visible owning versus active chrome scale contract
affects: [04-09-docs-ci, UI-05]

actuals:
  tokens: 4194
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - Convert pan-frame thickness at map with wm_ui_scale_edge_thickness; source 0 stays disabled
    - Convert snap and MoveThreshold into locals at use; never persist scaled style or Scr source
    - Rubber-band outline metrics use the active/pointer factor

key-files:
  created: []
  modified:
    - fvwm/virtual.c
    - fvwm/move_resize.c
    - fvwm/functions.c
    - fvwm/events.c
    - doc/fvwm3_manpage_source.adoc

key-decisions:
  - "initPanFrames and checkPanFrames size pan-frame X windows with wm_ui_scale_edge_thickness; source 0 still creates dummy 2px windows then unmaps."
  - "Snap proximity and grid convert from style source with fw->m->effective_scale.factor at use; edge-resistance snapd converts the same way."
  - "MoveThreshold converts from Scr source with the pointer-monitor factor at drag start; CMD_MoveThreshold still stores source."
  - "Manpage lists owning versus active chrome metrics as actual X11 pixels and states display-wide font DPI is not mutated."

patterns-established:
  - "Pattern: EdgeThickness command language stays 0-2 source; mapped hit-band pixels follow that pan frame's monitor."
  - "Pattern: snap and drag thresholds are local converted ints; style and Scr keep the unscaled source."
  - "Pattern: paging math that uses monitor_get_all_widths is not a scale space."

requirements-completed: [UI-05]

coverage:
  - id: D1
    description: "EdgeThickness keeps the 0-2 source range; mapped pan-frame thickness is wm_ui_scale_edge_thickness of that source; source 0 stays disabled."
    requirement: UI-05
    verification:
      - kind: other
        ref: "grep wm_ui_scale_edge_thickness fvwm/virtual.c"
        status: pass
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_edge_zero_stays_zero"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics monitor-ownership"
        status: pass
    human_judgment: false
  - id: D2
    description: "Snap proximity and grid convert from style source with the owning-monitor factor at use; MoveThreshold converts from Scr source with the pointer-monitor factor at drag start."
    requirement: UI-05
    verification:
      - kind: other
        ref: "grep wm_ui_scale_px fvwm/move_resize.c fvwm/functions.c fvwm/events.c"
        status: pass
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_move_threshold_and_snap_sources"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics"
        status: pass
    human_judgment: false
  - id: D3
    description: "Manpage documents that listed chrome pixel metrics follow owning or active monitor scale, remain actual X11 pixels, and that display-wide font DPI and cursor default size are not mutated."
    requirement: UI-05
    verification:
      - kind: other
        ref: "doc/fvwm3_manpage_source.adoc scale-contract paragraph"
        status: pass
      - kind: other
        ref: "git diff -- libs/vpacket.h fvwm/module_interface.c fvwm/status_json.c (empty)"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics supported-features monitor-scale monitor-ownership icccm-size-hints image-safety"
        status: pass
    human_judgment: false

duration: 8min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 08: Pan, Snap, and Chrome Scale Contract Summary

**Pan frames, snap, and MoveThreshold convert at use from retained source integers; EdgeThickness stays 0-2; manpage records owning versus active chrome as actual X11 pixels.**

## Performance

- **Duration:** 8 min
- **Started:** 2026-08-31T15:30:10Z
- **Completed:** 2026-08-31T15:39:00Z
- **Tasks:** 3
- **Files modified:** 5

## Accomplishments

- `initPanFrames` maps pan-frame windows with `wm_ui_scale_edge_thickness` using that monitor's factor; source 0 stays disabled
- `DoSnapGrid` / `DoSnapMonitor` convert snap grid and proximity into locals from style source with the owning-monitor factor
- `CheckActionType` and `_test_for_motion` compare against a pointer-monitor conversion of `Scr.MoveThreshold`; `CMD_MoveThreshold` still stores source
- Manpage chrome-scale paragraph lists owning versus active metrics as actual X11 pixels and states display-wide font DPI is not mutated

## Task Commits

Each task was committed atomically:

1. **Task 1: Scale mapped pan-frame thickness from source 0-2** - `28ecf6bf8` (feat)
2. **Task 2: Convert snap and MoveThreshold at use, not in stored style** - `da29859b4` (feat)
3. **Task 3: Document owning versus active chrome scale as actual pixels** - `013ab0e87` (docs)

_Note: Converter RED fixtures for edge-zero, MoveThreshold 3 at 1.5x, and snap 8 at 2x shipped in 04-01. This plan is GREEN call-site wiring._

## Files Created/Modified

- `fvwm/virtual.c` - Map-time pan-frame thickness from source 0-2; checkPanFrames resizes with the same mapped pixels
- `fvwm/move_resize.c` - Owning-monitor snap conversion at use; active-factor rubber-band outline metrics
- `fvwm/functions.c` - Pointer-monitor MoveThreshold at click/drag classification
- `fvwm/events.c` - Pointer-monitor MoveThreshold at motion threshold
- `doc/fvwm3_manpage_source.adoc` - Owning versus active chrome scale contract

## Decisions Made

- Keep `CMD_EdgeThickness` 0-2 source storage. Source 0 still creates dummy 2px windows so later XMap is not on None, then `checkPanFrames` unmaps them. Sources 1 and 2 map at `wm_ui_scale_edge_thickness`.
- Convert snap at the owning monitor (`fw->m`, else 1x). Convert MoveThreshold at `FindScreenOfXY` of the drag-start coordinates.
- Rubber-band nest/min/split pixel constants convert with the active/pointer factor at draw.
- Do not rewrite HandlePaging into a scale space; `monitor_get_all_widths` stays paging math.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Scale checkPanFrames window geometry**
- **Found during:** Task 1 (Scale mapped pan-frame thickness)
- **Issue:** `checkPanFrames` resizes pan-frame X windows from stored thickness. Scaling only in `initPanFrames` would be undone on the first thickness-change resize.
- **Fix:** Convert current and last source through `mapped_edge_thickness` for X geometry; keep source 0 as the disable check.
- **Files modified:** `fvwm/virtual.c`
- **Verification:** `grep wm_ui_scale_edge_thickness fvwm/virtual.c`; meson test wm-ui-metrics monitor-ownership
- **Committed in:** `28ecf6bf8` (Task 1 commit)

**2. [Rule 2 - Missing Critical] Convert DoSnapWindow proximity at use**
- **Found during:** Task 2 (Convert snap and MoveThreshold)
- **Issue:** Plan named DoSnapGrid/DoSnapMonitor, but window-to-window snap also reads `fw->snap_attraction.proximity` as a pixel distance.
- **Fix:** Convert proximity into a local with the owning-monitor factor; do not write back onto the style.
- **Files modified:** `fvwm/move_resize.c`
- **Verification:** meson test wm-ui-metrics
- **Committed in:** `da29859b4` (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (2 missing critical)
**Impact on plan:** Both keep mapped/use-time pixels consistent with the converters. No scope creep.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 04-09
- Protocol files (`libs/vpacket.h`, `fvwm/module_interface.c`, `fvwm/status_json.c`) remain untouched
- Native suite used by this plan is green

## Self-Check: PASSED

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*
