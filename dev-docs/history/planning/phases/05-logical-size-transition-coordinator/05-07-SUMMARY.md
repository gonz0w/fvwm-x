---
phase: 05-logical-size-transition-coordinator
plan: 07
subsystem: geometry-transition
tags: [hidpi, geometry, logical-size, restore, echo-gate, configure-request, c11]

requires:
  - phase: 05-logical-size-transition-coordinator
    provides: unmaximize RESTORE from stored logical_client plus dest chrome
  - phase: 05-logical-size-transition-coordinator
    provides: ConfigureRequest echo gate using last_wm_sent_client
provides:
  - Restore echo arm: last_wm_sent_client plus SET_FORCE_NEXT_CR before frame_setup_window
  - Echo skip from last-sent size match and maximized/fullscreen, not blanket DO_FORCE_NEXT_CR
  - test_restore_echo_keeps_stored_logical_seven dest-versus-7 fixture
affects: [05-08-shaded-commit]

actuals:
  tokens: 786
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - Successful RESTORE arms last_wm_sent_client then SET_FORCE_NEXT_CR before frame_setup_window
    - skip_logical_client initializes from IS_MAXIMIZED or IS_EWMH_FULLSCREEN only
    - A dest-sized ConfigureRequest equal to last_wm_sent_client still skips logical rewrite

key-files:
  created: []
  modified:
    - fvwm/move_resize.c
    - fvwm/events.c
    - tests/test_geometry_transition.c

key-decisions:
  - "After successful GEOMETRY_TRANSITION_RESTORE, last_wm_sent_client holds dest client pixels (dest_frame minus dest_chrome.total_size, floored at 0) and SET_FORCE_NEXT_CR(fw, 1) runs before frame_setup_window."
  - "_handle_cr_on_client initializes skip_logical_client from IS_MAXIMIZED or IS_EWMH_FULLSCREEN only; a later last_wm_sent_client width/height match still sets skip_logical_client."
  - "OVERFLOW and INVALID_ARGUMENT restore keep get_relative_geometry and do not arm the echo gate."

patterns-established:
  - "Pattern: Restore echo arm matches update_fvwm_monitor non-fullscreen last_wm_sent_client arithmetic; do not copy the fullscreen dest_frame-as-client branch."
  - "Pattern: Blanket DO_FORCE_NEXT_CR skip is not the logical-rewrite gate; last-sent size match is."

requirements-completed: [GEO-02, GEO-06]

coverage:
  - id: D1
    description: "Successful unmaximize/leave-fullscreen restore writes dest client pixels into last_wm_sent_client and arms SET_FORCE_NEXT_CR before frame_setup_window without rewriting logical_client."
    requirement: GEO-02
    verification:
      - kind: other
        ref: "python3 source-contract over unmaximize_fvwm_window last_wm_sent_client then SET_FORCE_NEXT_CR before frame_setup_window, no logical_client assignment"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson compile -C builddir"
        status: pass
    human_judgment: false
  - id: D2
    description: "_handle_cr_on_client initializes skip_logical_client from maximized or fullscreen only; a dest-sized CR equal to last_wm_sent_client still skips; SET_FORCE_NEXT_CR(fw, 0) remains at function end."
    requirement: GEO-02
    verification:
      - kind: other
        ref: "python3 source-contract over _handle_cr_on_client init skip (IS_MAXIMIZED/IS_EWMH_FULLSCREEN, no DO_FORCE_NEXT_CR) and new_client_w == last_sent_w"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson compile -C builddir"
        status: pass
    human_judgment: false
  - id: D3
    description: "test_restore_echo_keeps_stored_logical_seven documents dest-versus-7 inequality after increment-snapped RESTORE; stored logical 7 is unchanged."
    requirement: GEO-06
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_restore_echo_keeps_stored_logical_seven"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson test -C builddir geometry-transition --print-errorlogs"
        status: pass
    human_judgment: false

duration: 4min
completed: 2026-09-01
status: complete
---

# Phase 5 Plan 7: Restore Echo Gate Summary

**Successful unmaximize/leave-fullscreen restore arms last_wm_sent_client and SET_FORCE_NEXT_CR before ConfigureNotify so dest-sized echoes cannot rebuild logical_client**

## Performance

- **Duration:** 4 min
- **Started:** 2026-09-01T10:00:00Z
- **Completed:** 2026-09-01T10:03:39Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- `unmaximize_fvwm_window` successful RESTORE writes dest client pixels (`dest_frame` minus `dest_chrome.total_size`, floored at 0) into `last_wm_sent_client` and calls `SET_FORCE_NEXT_CR(fw, 1)` before `frame_setup_window`
- OVERFLOW and INVALID_ARGUMENT restore keep `get_relative_geometry` and do not arm the echo gate; stored `logical_client` is not rewritten on this path
- `_handle_cr_on_client` initializes `skip_logical_client` from `IS_MAXIMIZED` or `IS_EWMH_FULLSCREEN` only; a later `new_client_w/h == last_sent_w/h` match still skips; `SET_FORCE_NEXT_CR(fw, 0)` remains at function end
- `test_restore_echo_keeps_stored_logical_seven` documents increment-snapped dest-versus-7 inequality; echo-gate production coverage is the events.c last-sent match

## Task Commits

Each task was committed atomically:

1. **Task 1: End-to-end restore echo arm — last_wm_sent_client then SET_FORCE_NEXT_CR before frame_setup_window** - `27a52edd0` (feat)
2. **Task 2: Skip logical rewrite on last-sent size match, not blanket DO_FORCE_NEXT_CR** - `c15e7130f` (feat)

## Files Created/Modified

- `fvwm/move_resize.c` - Restore echo arm on successful RESTORE before `frame_setup_window`
- `fvwm/events.c` - Narrowed `_handle_cr_on_client` skip init; last-sent compare kept
- `tests/test_geometry_transition.c` - `test_restore_echo_keeps_stored_logical_seven`

## Decisions Made

- Copy the non-fullscreen `last_wm_sent_client` arithmetic from `update_fvwm_monitor`; do not copy the fullscreen dest_frame-as-client branch
- Do not arm the echo gate on OVERFLOW or INVALID_ARGUMENT
- Remove blanket `DO_FORCE_NEXT_CR` skip at init (WR-01); keep `SET_FORCE_NEXT_CR(fw, 0)` at function end
- Do not add a second echo-detector type; reuse `last_wm_sent_client` plus `SET_FORCE_NEXT_CR`

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 05-08 (shaded last-transition / unshade conversion). CR-01 restore echo is closed; CR-02 shaded commit remains.

## Self-Check: PASSED
