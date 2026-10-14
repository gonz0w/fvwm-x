---
phase: 05-logical-size-transition-coordinator
plan: 08
subsystem: geometry-transition
tags: [hidpi, geometry, logical-size, shaded, last-transition, c11]

requires:
  - phase: 05-logical-size-transition-coordinator
    provides: restore echo gate is in place
  - phase: 05-logical-size-transition-coordinator
    provides: update_fvwm_monitor dest chrome rebind, plan, and frame_setup_window commit
provides:
  - last_transition tuple stored only after OK/INELIGIBLE/NOOP
  - Shaded dest converts fw->g.normal from stored logical_client without frame_setup_window
  - test_unshaded_dest_from_logical_seven_replaces_source_sized_normal
affects: [phase-05-verification]

actuals:
  tokens: 1180
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - last_transition_* is written after geometry_transition_plan success, never on OVERFLOW/INVALID
    - Shaded dest plans GEOMETRY_TRANSITION_NORMAL into g.normal from logical_client
    - CMD_WindowShade is not a planner call site; converted g.normal is the unshade size

key-files:
  created: []
  modified:
    - fvwm/update.c
    - tests/test_geometry_transition.c

key-decisions:
  - "update_fvwm_monitor assigns last_transition_name_atom, last_transition_quarters, and last_transition_kind only after geometry_transition_plan returns OK, INELIGIBLE, or NOOP."
  - "After OK, INELIGIBLE, or NOOP on the IS_SHADED dest g.normal plan, store dest_atom, dest_quarters, and kind the same as the non-shaded success path."
  - "On dest change while IS_SHADED and logical_client_valid, fw->g.normal size becomes monitor_scale_apply(logical_client, dest_factor) plus dest chrome, without frame_setup_window on the shaded strip."
  - "IS_WINDOW_BEING_MOVED_OPAQUE still resolves dest from a 1x1 pointer rectangle; dest-scale drag still uses GrabEm rather than a second XGrabPointer."
  - "CMD_WindowShade is not modified; converted g.normal is the unshade size."

patterns-established:
  - "Pattern: Do not record last-transition on OVERFLOW/INVALID or before the plan; a failed dest must remain retryable."
  - "Pattern: Shaded dest packs src_client from fw->g.normal, not the shaded g.frame strip."
  - "Pattern: Unshade reads converted g.normal through get_unshaded_geometry; do not add a planner call in CMD_WindowShade."

requirements-completed: [GEO-02, GEO-04]

coverage:
  - id: D1
    description: "last_transition_name_atom, last_transition_quarters, and last_transition_kind are assigned only after geometry_transition_plan returns OK, INELIGIBLE, or NOOP on the non-shaded path."
    requirement: GEO-02
    verification:
      - kind: other
        ref: "python3 source-contract over update_fvwm_monitor last_transition_name_atom after geometry_transition_plan, none between setup_icon_font and the plan"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson compile -C builddir"
        status: pass
    human_judgment: false
  - id: D2
    description: "Shaded dest change plans GEOMETRY_TRANSITION_NORMAL from stored logical_client into fw->g.normal, records last-transition after OK/INELIGIBLE/NOOP, and does not call frame_setup_window."
    requirement: GEO-02
    verification:
      - kind: other
        ref: "python3 source-contract over IS_SHADED convert slice src_client from fw->g.normal, fw->g.normal = after plan, last_transition after plan, no frame_setup_window, windowshade.c has no geometry_transition_plan"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson compile -C builddir"
        status: pass
    human_judgment: false
  - id: D3
    description: "test_unshaded_dest_from_logical_seven_replaces_source_sized_normal replaces source-sized dest_frame 7x7 at origin 100,200 with dest 9x9 from logical 7 at quarters 5."
    requirement: GEO-02
    verification:
      - kind: unit
        ref: "tests/test_geometry_transition.c#test_unshaded_dest_from_logical_seven_replaces_source_sized_normal"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson test -C builddir geometry-transition --print-errorlogs"
        status: pass
    human_judgment: false
  - id: D4
    description: "IS_WINDOW_BEING_MOVED_OPAQUE still resolves dest from a 1x1 pointer rectangle; monitor_ownership_resolve remains once in update_fvwm_monitor."
    requirement: GEO-04
    verification:
      - kind: other
        ref: "python3 source-contract IS_WINDOW_BEING_MOVED_OPAQUE and s.count(monitor_ownership_resolve)==1"
        status: pass
    human_judgment: false

duration: 6min
completed: 2026-09-01
status: complete
---

# Phase 5 Plan 8: Shaded Dest Last-Transition Summary

**last_transition recorded only after OK/INELIGIBLE/NOOP; shaded dest change converts unshaded g.normal from stored logical_client without expanding the shaded strip**

## Performance

- **Duration:** 6 min
- **Started:** 2026-09-01T10:08:06Z
- **Completed:** 2026-09-01T10:14:21Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- `update_fvwm_monitor` no longer writes `last_transition_name_atom` / `last_transition_quarters` / `last_transition_kind` before `geometry_transition_plan`; OVERFLOW and INVALID_ARGUMENT keep the last-good frame and leave the tuple unchanged
- Dest change while `IS_SHADED` and `logical_client_valid` plans `GEOMETRY_TRANSITION_NORMAL` into `fw->g.normal` from stored `logical_client` plus dest chrome, without `frame_setup_window` on the shaded strip
- After OK, INELIGIBLE, or NOOP on that shaded plan, dest atom/quarters/kind are stored the same as the non-shaded success path so a later dest change can re-plan
- `test_unshaded_dest_from_logical_seven_replaces_source_sized_normal` asserts source-sized dest_frame 7x7 at origin 100,200 becomes 9x9 with origin unchanged
- Opaque-move dest freeze remains a 1x1 pointer rectangle; `CMD_WindowShade` is unmodified

## Task Commits

Each task was committed atomically:

1. **Task 1: Record last-transition only after OK, INELIGIBLE, or NOOP** - `622e8b39a` (feat)
2. **Task 2: Convert unshaded g.normal from logical_client on shaded dest change** - `b0f44d8a0` (test), `7ab69c998` (feat)

## Files Created/Modified

- `fvwm/update.c` - last-transition after successful plan; shaded dest writes dest-sized `g.normal`
- `tests/test_geometry_transition.c` - `test_unshaded_dest_from_logical_seven_replaces_source_sized_normal`

## Decisions Made

- Record last-transition only after OK/INELIGIBLE/NOOP, never on OVERFLOW/INVALID and never before the plan
- Convert unshaded size in `update_fvwm_monitor` rather than adding a planner call in `CMD_WindowShade`
- Shaded dest always plans `GEOMETRY_TRANSITION_NORMAL` into `g.normal` even when kind is MAXIMIZED or FULLSCREEN; stored `last_transition_kind` remains the original kind
- Pack shaded `src_client` from `fw->g.normal` minus dest chrome, not from the shaded `g.frame` strip
- Do not write `last_wm_sent_client` or `SET_FORCE_NEXT_CR` on the shaded path (no client ConfigureNotify)

## Deviations from Plan

TDD RED for Task 2 did not fail: `test_unshaded_dest_from_logical_seven_replaces_source_sized_normal` documents existing `geometry_transition_plan` dest-size replacement from stored logical 7. Investigated, treated as a contract fixture, and proceeded to the `update.c` shaded wiring (GREEN).

None of Rules 1–3 applied.

---

**Total deviations:** 0 auto-fixed (1 TDD RED-already-green process note)
**Impact on plan:** No scope change. Production wiring matches the plan.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Phase 5 gap-closure plans 05-07 and 05-08 are complete. CR-02 / WR-02 shaded last-transition is closed. Ready for phase verification.

## Self-Check: PASSED
