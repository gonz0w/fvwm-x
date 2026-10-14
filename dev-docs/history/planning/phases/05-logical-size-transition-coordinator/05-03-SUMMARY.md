---
phase: 05-logical-size-transition-coordinator
plan: 03
subsystem: geometry-transition
tags: [hidpi, geometry, grab-offset, maximize, restore, c11]

requires:
  - phase: 05-logical-size-transition-coordinator
    provides: FvwmWindow logical_client and update_fvwm_monitor dest commit
provides:
  - Pointer-frozen drag offset conversion from a once-established logical source
  - Dest work-area maximize through update_fvwm_monitor
  - Unmaximize RESTORE from stored logical_client plus dest chrome
affects: [05-04-fullscreen, 05-05-configure]

actuals:
  tokens: 1579
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - Grab offset is established once as logical source then monitor_scale_apply at dest
    - Dest-scale drag keeps GrabEm and swaps the cursor through XChangeActivePointerGrab
    - Maximize dest ownership goes through update_fvwm_monitor, never fw->m =
    - Unmaximize RESTORE overflow keeps get_relative_geometry as last-good

key-files:
  created: []
  modified:
    - fvwm/move_resize.c

key-decisions:
  - "InteractiveMove stores logical grab offset in file-static state so monitor_scale_remove runs before any later update_fvwm_monitor without changing the move_loop signature used by placement.c."
  - "Dest quarters change keeps the original GrabEm and calls a local change_grab_cursor (XChangeActivePointerGrab), never a second XGrabPointer."
  - "CMD_Maximize FindScreenOfXY dest assignment routes through update_fvwm_monitor."
  - "unmaximize_fvwm_window plans GEOMETRY_TRANSITION_RESTORE when logical_client_valid; OVERFLOW and INVALID_ARGUMENT keep get_relative_geometry."

patterns-established:
  - "Pattern: Convert grab offset from the retained logical source; never remove(apply(dest))."
  - "Pattern: Maximize fills dest work area via EWMH_GetWorkAreaIntersection after coordinator dest commit."
  - "Pattern: Restore dest pixels from stored logical_client plus dest chrome; do not scale g.max."

requirements-completed: [GEO-04, GEO-06]

coverage:
  - id: D1
    description: "Drag keeps GrabEm and converts grab offset from a once-established logical source after dest size changes."
    requirement: GEO-04
    verification:
      - kind: other
        ref: "python3 source-contract over move_loop GrabEm, change_grab_cursor, monitor_scale_remove before update_fvwm_monitor, monitor_scale_apply after it"
        status: pass
      - kind: unit
        ref: "tests/test_geometry_transition.c#geometry-transition"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson compile -C builddir"
        status: pass
    human_judgment: false
  - id: D2
    description: "Maximize fills dest work area through update_fvwm_monitor and does not write logical_client."
    requirement: GEO-06
    verification:
      - kind: other
        ref: "python3 source-contract over fw->m = possibly_new_monitor removed, CS_UPDATE_MAX_DEFECT, EWMH_GetWorkAreaIntersection, no logical_client in maximize_fvwm_window"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson compile -C builddir"
        status: pass
    human_judgment: false
  - id: D3
    description: "Unmaximize and leave-fullscreen restore dest pixels from stored logical client size plus dest chrome."
    requirement: GEO-06
    verification:
      - kind: other
        ref: "python3 source-contract over unmaximize_fvwm_window GEOMETRY_TRANSITION_RESTORE and logical_client_valid"
        status: pass
      - kind: unit
        ref: "tests/test_geometry_transition.c#geometry-transition"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson compile -C builddir"
        status: pass
    human_judgment: false

duration: 8min
completed: 2026-09-01
status: complete
---

# Phase 5 Plan 3: Logical-Size Transition Coordinator Summary

**Drag keeps GrabEm and converts offset from a once-established logical source; maximize fills dest work area; unmaximize restores from stored logical client plus dest chrome**

## Performance

- **Duration:** 8 min
- **Started:** 2026-09-01T00:48:37Z
- **Completed:** 2026-09-01T00:57:14Z
- **Tasks:** 3
- **Files modified:** 1

## Accomplishments

- Interactive move establishes a logical grab offset once from the src factor (`monitor_scale_remove` when src quarters are not 1x)
- `move_loop` applies that stored offset at dest after `update_fvwm_monitor`, updates rubber-band/opaque `sz` from dest frame, and keeps the original `GrabEm(cursor, GRAB_NORMAL)`
- `CMD_Maximize` no longer writes `fw->m` from `FindScreenOfXY`; dest ownership and dest work area go through `update_fvwm_monitor`
- `unmaximize_fvwm_window` restores through `geometry_transition_plan(GEOMETRY_TRANSITION_RESTORE)` when `logical_client_valid`, and keeps `get_relative_geometry` as last-good on overflow

## Task Commits

Each task was committed atomically:

1. **Task 1: Convert grab offset from a retained source and keep GrabEm** - `931a132d2` (feat)
2. **Task 2: Maximize onto dest work area without writing logical_client** - `47af842a2` (feat)
3. **Task 3: Restore unmaximize from stored logical client plus dest chrome** - `8ca75fa0a` (feat)

## Files Created/Modified

- `fvwm/move_resize.c` - Logical grab-offset conversion, dest work-area maximize coordinator, RESTORE unmaximize

## Decisions Made

- Logical grab offset is stored in file-static state so `monitor_scale_remove` runs in `InteractiveMove` (before any later `update_fvwm_monitor` in the file) without changing the `move_loop` signature used by manual placement
- Dest-scale cursor swap uses a local `change_grab_cursor` (`XChangeActivePointerGrab`) so the existing `GrabEm` stays held
- `CMD_Maximize` opportunistic dest assignment calls `update_fvwm_monitor` instead of `fw->m = possibly_new_monitor`
- RESTORE overflow and invalid argument keep the `get_relative_geometry` rectangle; `logical_client` is never written on maximize or unmaximize

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 05-04: fullscreen dest-monitor wiring. GEO-04 and GEO-06 remain shared across later plans, so REQUIREMENTS.md stays pending until sibling SUMMARYs exist.

## Self-Check: PASSED

---
*Phase: 05-logical-size-transition-coordinator*
*Completed: 2026-09-01*
