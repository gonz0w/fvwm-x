---
phase: 05-logical-size-transition-coordinator
plan: 01
subsystem: geometry-transition
tags: [hidpi, geometry, logical-size, icccm, monitor-scale, c11, meson]

requires:
  - phase: 02-scale-policy-and-configuration
    provides: monitor_scale_apply exact-quarter conversion with overflow-to-zero
  - phase: 03-geometry-and-ownership-contracts
    provides: icccm_constrain_size and monitor_ownership_resolve with hysteresis
provides:
  - Allocation-free geometry_transition_plan over stored logical client size
  - Native GEO-02 trip, increment, overflow, and factor-bound fixtures
  - Native GEO-04 grab-offset and pointer 1x1 freeze fixtures
  - Native GEO-06 maximize, fullscreen, and restore dest-kind fixtures
affects: [05-02-update-fvwm-monitor, 05-03-maximize, 05-04-fullscreen, 05-05-configure]

actuals:
  tokens: 5836
  tasks: 3
  commits: 6

tech-stack:
  added: []
  patterns:
    - Dest client pixels come from monitor_scale_apply on stored logical integers
    - Maximize and fullscreen recompute from dest work/monitor rects
    - Overflow and invalid dest outputs initialize width and height to 0

key-files:
  created:
    - libs/geometry_transition.h
    - libs/geometry_transition.c
    - tests/test_geometry_transition.c
  modified:
    - libs/meson.build
    - tests/meson.build

key-decisions:
  - "Canonical logical size is a size_rect plus eligibility bool on the planner API, not a field inside window_g."
  - "Extra status enumerators are GEOMETRY_TRANSITION_INELIGIBLE and GEOMETRY_TRANSITION_NOOP."
  - "Maximize dest equals the supplied dest work rect; fullscreen dest copies the dest monitor rect."
  - "Pointer dest freeze reuses monitor_ownership_resolve on a 1x1 rectangle; the planner does not reimplement ownership."

patterns-established:
  - "Pattern: NORMAL and RESTORE dest client pixels come from monitor_scale_apply on stored logical width and height, never dest_frame input size."
  - "Pattern: On OVERFLOW or INVALID_ARGUMENT, zero dest_frame width and height; never clamp to INT_MAX."
  - "Pattern: Ineligible windows keep src_client pixels and still add dest_chrome total_size."
  - "Pattern: Derived maximize/fullscreen kinds do not scale g.max or a derived fullscreen frame."

requirements-completed: [GEO-02, GEO-04, GEO-06]

coverage:
  - id: D1
    description: "A NORMAL plan of stored logical 7x7 at dest_factor.quarters 5 with zero dest chrome writes dest_frame width 9 and height 9."
    requirement: GEO-02
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_normal_plan_logical_seven_at_five_quarters_is_nine"
        status: pass
    human_judgment: false
  - id: D2
    description: "1000 iterations through dest_factor.quarters 5, 6, 7, and 8 pass stored logical 7 every time; dest width equals one monitor_scale_apply(7, factor)."
    requirement: GEO-02
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_repeated_plan_from_stored_logical_seven"
        status: pass
    human_judgment: false
  - id: D3
    description: "PResizeInc may snap dest client off 9; the next plan still passes stored logical 7."
    requirement: GEO-02
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_increment_snap_does_not_rewrite_logical"
        status: pass
    human_judgment: false
  - id: D4
    description: "INT_MAX logical at quarters 5 returns OVERFLOW with dest_frame size 0; quarters 3 and 17 return INVALID_ARGUMENT with dest outputs 0."
    requirement: GEO-02
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_overflow_zeros_dest_frame"
        status: pass
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_factor_outside_quarters_is_invalid"
        status: pass
    human_judgment: false
  - id: D5
    description: "eligible false keeps src_client 80x60 as dest client and adds dest_chrome totals 8x16 to dest_frame 88x76."
    requirement: GEO-02
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_ineligible_keeps_src_client_and_adds_dest_chrome"
        status: pass
    human_judgment: false
  - id: D6
    description: "MAXIMIZED dest_frame equals the supplied dest work rect at x=-1920 y=0 width=1920 height=1080 when constrain is a no-op."
    requirement: GEO-06
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_maximized_dest_equals_work_rect"
        status: pass
    human_judgment: false
  - id: D7
    description: "FULLSCREEN dest_frame equals the supplied monitor rect without adding dest chrome; RESTORE of logical 7 at quarters 5 with chrome 10x20 writes dest_frame 19x29."
    requirement: GEO-06
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_fullscreen_dest_equals_monitor_rect"
        status: pass
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_restore_uses_stored_logical_plus_dest_chrome"
        status: pass
    human_judgment: false
  - id: D8
    description: "Grab offset converts stored logical 8 at dest_factor.quarters 5 to 10 so origin 100,50 plus dest offset is 110,60."
    requirement: GEO-04
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_grab_offset_apply_from_source"
        status: pass
    human_judgment: false
  - id: D9
    description: "A 1x1 pointer rectangle keeps the incumbent; extra_overlap 8 keeps name_atom 1 while extra_overlap 9 steals; a larger post-resize frame is not the ownership input."
    requirement: GEO-04
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_pointer_one_by_one_freeze_ignores_post_resize_frame"
        status: pass
    human_judgment: false

duration: 11min
completed: 2026-09-01
status: complete
---

# Phase 5 Plan 1: Logical-Size Transition Coordinator Summary

**Allocation-free geometry_transition_plan converts stored logical client 7 to dest pixels 9 at 5/4, with native GEO-02 trip, GEO-04 offset/freeze, and GEO-06 derived-kind fixtures**

## Performance

- **Duration:** 11 min
- **Started:** 2026-09-01T00:10:28Z
- **Completed:** 2026-09-01T00:22:09Z
- **Tasks:** 3
- **Files modified:** 5

## Accomplishments

- Production `geometry_transition_plan` converts stored logical client size through `monitor_scale_apply`, then `icccm_constrain_size`, then dest chrome `total_size`
- GEO-02 1000-trip, increment-snap, overflow, ineligible, and quarter-bound fixtures are green from stored logical 7
- GEO-04 grab-offset apply-from-source and pointer 1x1 dest freeze reuse existing scale and ownership units
- GEO-06 maximize dest equals dest work rect, fullscreen dest copies dest monitor rect, restore uses stored logical plus dest chrome

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: failing tracer for logical 7 to dest 9** - `81bdca921` (test)
2. **Task 1 GREEN: implement geometry_transition_plan** - `ba6f76ade` (feat)
3. **Task 2 RED: failing GEO-02 trip and bound fixtures** - `15fbe1904` (test)
4. **Task 2 GREEN: compose dest frame from stored logical** - `77498ea48` (feat)
5. **Task 3 RED: failing maximize, restore, and freeze fixtures** - `8790afc45` (test)
6. **Task 3 GREEN: plan maximize, fullscreen, and restore dest frames** - `cfc139edc` (feat)

## Files Created/Modified

- `libs/geometry_transition.h` - Status/kind enums and `geometry_transition_plan` contract
- `libs/geometry_transition.c` - Apply-from-logical, constrain, dest-chrome compose, eligibility, overflow init-0
- `tests/test_geometry_transition.c` - GEO-02 trips, GEO-04 freeze/offset, GEO-06 derived-kind fixtures
- `libs/meson.build` - `geometry_transition.c` inserted once before `icccm_size_hints.c`
- `tests/meson.build` - `geometry-transition` meson test with `-UNDEBUG`

## Decisions Made

- Canonical logical size is a `size_rect` plus eligibility bool on the planner API, not a field inside `window_g`
- Extra status enumerators are `GEOMETRY_TRANSITION_INELIGIBLE` and `GEOMETRY_TRANSITION_NOOP`
- Maximize dest equals the supplied dest work rect; fullscreen dest copies the dest monitor rect without adding chrome
- Pointer dest freeze reuses `monitor_ownership_resolve` on a 1x1 rectangle; the planner does not reimplement ownership
- Grab offset converts stored logical integers with `monitor_scale_apply`; origin is pointer plus dest offset

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 05-02: wire `geometry_transition_plan` at `update_fvwm_monitor` after dest chrome rebind. GEO-02, GEO-04, and GEO-06 remain shared across later plans, so REQUIREMENTS.md stays pending until sibling SUMMARYs exist. Core `FvwmWindow` fields and live X wiring are out of this plan.

## Self-Check: PASSED

---
*Phase: 05-logical-size-transition-coordinator*
*Completed: 2026-09-01*
