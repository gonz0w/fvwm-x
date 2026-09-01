---
phase: 05-logical-size-transition-coordinator
plan: 04
subsystem: geometry-transition
tags: [hidpi, geometry, fullscreen, ewmh, frame-extents, c11]

requires:
  - phase: 05-logical-size-transition-coordinator
    provides: FvwmWindow logical_client and update_fvwm_monitor dest commit
provides:
  - EWMH fullscreen dest monitor si rectangle after ownership
  - Unchanged _NET_FRAME_EXTENTS dest chrome pixel writer
affects: [05-05-configure]

actuals:
  tokens: 370
  tasks: 2
  commits: 1

tech-stack:
  added: []
  patterns:
    - Fullscreen dest is fw->m->si after ownership, not FScreenGetScrRect on the current frame
    - EWMH_SetFrameStrut copies get_window_borders dest pixels with no scale conversion

key-files:
  created: []
  modified:
    - fvwm/ewmh.c

key-decisions:
  - "EWMH_fullscreen fills ResizeMoveMaximize from fw->m->si after apply_decor_change, not a scaled copy of the current fullscreen frame."
  - "EWMH_fullscreen does not assign logical_client or logical_client_valid."
  - "EWMH_SetFrameStrut stays dest chrome pixels from get_window_borders; no extra post-maximize strut write."

patterns-established:
  - "Pattern: Dest-monitor fullscreen uses fw->m after ownership, never a second FScreenGetScrRect that can disagree with fw->m consumers."
  - "Pattern: _NET_FRAME_EXTENTS remains dest chrome pixels; apply_decor_change already refreshes struts before ResizeMoveMaximize."

requirements-completed: [GEO-06]

coverage:
  - id: D1
    description: "EWMH fullscreen fills the dest monitor si rectangle after dest decor change and does not write logical_client."
    requirement: GEO-06
    verification:
      - kind: other
        ref: "python3 source-contract over EWMH_fullscreen fw->m->si, ResizeMoveMaximize, apply_decor_change, no logical_client"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson compile -C builddir"
        status: pass
    human_judgment: false
  - id: D2
    description: "_NET_FRAME_EXTENTS remains dest chrome pixels from get_window_borders after fullscreen dest-monitor wiring."
    requirement: GEO-06
    verification:
      - kind: other
        ref: "python3 source-contract over EWMH_SetFrameStrut get_window_borders val[0], no geometry_transition_plan or monitor_scale_apply"
        status: pass
      - kind: unit
        ref: "tests/test_geometry_transition.c#geometry-transition"
        status: pass
    human_judgment: false

duration: 4min
completed: 2026-09-01
status: complete
---

# Phase 5 Plan 4: Logical-Size Transition Coordinator Summary

**EWMH fullscreen fills the dest monitor si rectangle after ownership and dest decor change; _NET_FRAME_EXTENTS stays dest chrome pixels**

## Performance

- **Duration:** 4 min
- **Started:** 2026-09-01T00:59:41Z
- **Completed:** 2026-09-01T01:03:44Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments

- `EWMH_fullscreen` fills `ResizeMoveMaximize` from `fw->m->si` after `apply_decor_change`, resolving missing ownership through `update_fvwm_monitor`
- Fullscreen dest is the dest monitor rectangle, not a scaled copy of the current fullscreen frame
- `EWMH_fullscreen` does not assign `logical_client` or `logical_client_valid`
- `EWMH_SetFrameStrut` still publishes dest chrome pixels from `get_window_borders` into `_NET_FRAME_EXTENTS` and `_KDE_NET_WM_FRAME_STRUT`

## Task Commits

Each task was committed atomically:

1. **Task 1: Fullscreen dest monitor from fw->m si rectangle** - `04199f529` (feat)
2. **Task 2: Keep _NET_FRAME_EXTENTS on dest chrome pixels** - no code change (existing writer and post-decor strut refresh confirmed)

## Files Created/Modified

- `fvwm/ewmh.c` - Dest-monitor fullscreen command geometry; frame-extents writer unchanged

## Decisions Made

- Dest monitor comes from `fw->m` after ownership, not `FScreenGetScrRect` on the current frame center (PA-07)
- `EWMH_fullscreen` does not write `logical_client`; leave-fullscreen remains `unmaximize_fvwm_window` from plan 05-03
- No extra `EWMH_SetFrameStrut` after `ResizeMoveMaximize`: `apply_decor_change` already refreshes struts via `do_setup_frame`, and `maximize_fvwm_window` calls `update_fvwm_monitor` which refreshes struts on dest change

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 05-05: ConfigureRequest echo comparison against `last_wm_sent_client`. GEO-06 remains shared with 05-03, so REQUIREMENTS.md stays pending until sibling SUMMARYs exist.

## Self-Check: PASSED

---
*Phase: 05-logical-size-transition-coordinator*
*Completed: 2026-09-01*
