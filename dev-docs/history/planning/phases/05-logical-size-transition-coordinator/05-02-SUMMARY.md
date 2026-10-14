---
phase: 05-logical-size-transition-coordinator
plan: 02
subsystem: geometry-transition
tags: [hidpi, geometry, logical-size, fvwm-window, update-fvwm-monitor, c11]

requires:
  - phase: 05-logical-size-transition-coordinator
    provides: allocation-free geometry_transition_plan
provides:
  - FvwmWindow logical_client and last-transition tuple outside window_g
  - AddWindow records 1x-equivalent client size at first constrain
  - update_fvwm_monitor dest chrome rebind, plan, and frame_setup_window commit
affects: [05-03-maximize, 05-04-fullscreen, 05-05-configure]

actuals:
  tokens: 2028
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - Canonical logical client size lives on FvwmWindow beside m/m_prev
    - One ownership-change commit: dest chrome, geometry_transition_plan, frame_setup_window
    - Opaque-move ownership uses a 1x1 pointer rectangle and never re-resolves the post-resize frame

key-files:
  created: []
  modified:
    - fvwm/fvwm.h
    - fvwm/add_window.c
    - fvwm/update.c

key-decisions:
  - "logical_client, logical_client_valid, last-transition tuple, and last_wm_sent_client sit after m_prev, not inside window_g."
  - "AddWindow records 1x-equivalent client size after the first constrained frame via monitor_scale_remove when dest quarters are not 1x."
  - "update_fvwm_monitor is the single dest-change commit: SelectDecor and fonts, then geometry_transition_plan, then frame_setup_window."
  - "IS_WINDOW_BEING_MOVED_OPAQUE resolves ownership from a 1x1 pointer rectangle; IS_SHADED rebinds dest title chrome and skips client resize."

patterns-established:
  - "Pattern: Do not store logical size inside window_g because fullscreen.g copies that struct."
  - "Pattern: OVERFLOW and INVALID_ARGUMENT keep the last-good frame and fvwm_debug; never invent dest pixels."
  - "Pattern: Last-transition name_atom, quarters, and kind make a second identical dest commit a NOOP."

requirements-completed: [GEO-02]

coverage:
  - id: D1
    description: "FvwmWindow stores logical_client outside window_g and AddWindow records 1x-equivalent client size after the first constrained frame."
    requirement: GEO-02
    verification:
      - kind: other
        ref: "python3 source-contract over fvwm/fvwm.h window_g and fvwm/add_window.c logical_client_valid"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson compile -C builddir"
        status: pass
    human_judgment: false
  - id: D2
    description: "update_fvwm_monitor rebinds dest chrome, calls geometry_transition_plan, and commits dest frame pixels on ownership change."
    requirement: GEO-02
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#geometry-transition"
        status: pass
      - kind: other
        ref: "python3 source-contract over update_fvwm_monitor SelectDecor, geometry_transition_plan, frame_setup_window, 1x1 opaque ownership, IS_SHADED skip"
        status: pass
    human_judgment: false

duration: 11min
completed: 2026-09-01
status: complete
---

# Phase 5 Plan 2: Logical-Size Transition Coordinator Summary

**FvwmWindow stores canonical logical client size beside monitor pointers, and update_fvwm_monitor rebinds dest chrome then plans and commits dest frame pixels on ownership change**

## Performance

- **Duration:** 11 min
- **Started:** 2026-09-01T00:26:21Z
- **Completed:** 2026-09-01T00:38:01Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- `FvwmWindow` now keeps `logical_client`, `logical_client_valid`, last-transition tuple, and `last_wm_sent_client` next to `m` / `m_prev`, not inside `window_g`
- `AddWindow` records 1x-equivalent client size after the first constrained frame (`monitor_scale_remove` when dest quarters are not 1x)
- `update_fvwm_monitor` is the single dest-change commit: dest `SelectDecor` and fonts, `geometry_transition_plan`, then `frame_setup_window` before `BroadcastConfig`
- Opaque moves resolve ownership from a 1x1 pointer rectangle; shaded windows rebind dest title chrome without expanding the shaded frame

## Task Commits

Each task was committed atomically:

1. **Task 1: Store logical_client on FvwmWindow and initialize it at first constrain** - `a33669d7c` (feat)
2. **Task 2: Commit dest chrome, plan, and frame from update_fvwm_monitor** - `a7111739f` (feat)

## Files Created/Modified

- `fvwm/fvwm.h` - Logical client, last-transition tuple, and last WM-sent client after `m_prev`
- `fvwm/add_window.c` - `setup_logical_client` after session `constrain_size` and `gravity_constrain_size`
- `fvwm/update.c` - Dest chrome rebind, plan, and frame commit on ownership change

## Decisions Made

- Canonical logical client size lives on `FvwmWindow` beside `m` / `m_prev` so `fullscreen.g` cannot copy logical units
- `last_wm_sent_client` is stored here so plan 05-05 can compare ConfigureRequest client size without a second detector
- Opaque-move ownership uses a 1x1 pointer rectangle with incumbent hysteresis; the post-resize frame is not fed back into `monitor_ownership_resolve` in the same commit
- `IS_SHADED` still rebinds dest title chrome, then skips `geometry_transition_plan` and `frame_setup_window`
- OVERFLOW and INVALID_ARGUMENT keep the last-good frame and log through `fvwm_debug`

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 05-03: maximize/unmaximize dest work-rect wiring. GEO-02 remains shared across later plans, so REQUIREMENTS.md stays pending until sibling SUMMARYs exist. ConfigureRequest echo comparison against `last_wm_sent_client` is plan 05-05.

## Self-Check: PASSED

---
*Phase: 05-logical-size-transition-coordinator*
*Completed: 2026-09-01*
