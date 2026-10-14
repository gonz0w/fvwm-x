---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 11
subsystem: pan-frames
tags: [hidpi, EdgeThickness, HandlePaging, pan-frames, wm_ui_metrics, c11]

requires:
  - phase: 04-wm-owned-ui-metrics-and-resources
    provides: mapped_edge_thickness, wm_ui_scale_edge_thickness, initPanFrames scaled InputOnly windows
provides:
  - HandlePaging proximity and warp tests use mapped_edge_thickness once
  - checkPanFrames writes last_edge_thickness as the 0-2 source integer
  - Native fixture locks EdgeThickness source 2 at 2x as 4px and at 1x as 2px
affects: [UI-05, Phase 5, Phase 8]

actuals:
  tokens: 498
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - HandlePaging converts pan-frame thickness once with mapped_edge_thickness and keeps virtual_scr.edge_thickness as the 0-2 command source
    - checkPanFrames records last_edge_thickness in source space so mapped pixels are never fed back into wm_ui_scale_edge_thickness

key-files:
  created: []
  modified:
    - fvwm/virtual.c
    - tests/test_wm_ui_metrics.c

key-decisions:
  - "HandlePaging local edge_thickness comes from mapped_edge_thickness(m, m->virtual_scr.edge_thickness); the stored EdgeThickness field stays source 0-2."
  - "checkPanFrames writes m->virtual_scr.last_edge_thickness = source_thickness after map/unmap/resize, never mapped pixels."

patterns-established:
  - "Pattern: pan-frame X windows and HandlePaging proximity share mapped_edge_thickness; paging-span math stays monitor_get_all_widths / monitor_get_all_heights."
  - "Pattern: last_edge_thickness remains a 0-2 source integer so EdgeThickness 2 then 1 then 2 resizes existing frames."

requirements-completed: [UI-05]

coverage:
  - id: D1
    description: "HandlePaging converts once with mapped_edge_thickness and uses that local for every proximity and warp test; virtual_scr.edge_thickness stays the 0-2 command source; monitor_get_all_widths remains the paging-span helper."
    requirement: UI-05
    verification:
      - kind: other
        ref: "python source-contract on fvwm/virtual.c HandlePaging mapped_edge_thickness / monitor_get_all_widths / EdgeThickness 0-2"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics"
        status: pass
    human_judgment: false
  - id: D2
    description: "checkPanFrames writes last_edge_thickness as source_thickness, and native tests lock source 2 at 2x as 4 and source 2 at 1x as 2."
    requirement: UI-05
    verification:
      - kind: other
        ref: "python source-contract last_edge_thickness = source_thickness and test_paging_edge_source_two_at_2x_is_four"
        status: pass
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_paging_edge_source_two_at_2x_is_four"
        status: pass
    human_judgment: false

duration: 4min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 11: Mapped Paging Proximity and Source last_edge_thickness Summary

**HandlePaging hit-tests the mapped InputOnly pan-frame band via mapped_edge_thickness, and checkPanFrames records last_edge_thickness as the 0-2 EdgeThickness source so returning to source 2 resizes existing frames.**

## Performance

- **Duration:** 4 min
- **Started:** 2026-08-31T16:56:35Z
- **Completed:** 2026-08-31T17:00:30Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- HandlePaging forward-declares `mapped_edge_thickness` and initializes its local `edge_thickness` from `mapped_edge_thickness(m, m->virtual_scr.edge_thickness)` once; every proximity and warp comparison uses that local
- `m->virtual_scr.edge_thickness` is not written; `CMD_EdgeThickness` still rejects values outside 0-2; `monitor_get_all_widths` / `monitor_get_all_heights` remain the paging-span calls
- `checkPanFrames` writes `m->virtual_scr.last_edge_thickness = source_thickness` after the four-side map/unmap/resize block so mapped pixels are never scaled a second time
- `test_paging_edge_source_two_at_2x_is_four` locks source 2 at quarters 8 as 4 and source 2 at quarters 4 as 2

## Task Commits

Each task was committed atomically:

1. **Task 1: End-to-end paging proximity on mapped pan thickness** - `1a97e42c4` (feat)
2. **Task 2 RED: paging edge source-2 conversion fixture** - `49b5c4e87` (test)
3. **Task 2 GREEN: write last_edge_thickness as EdgeThickness source** - `dfa7a31fa` (feat)

_Note: Task 1 carried `tdd="true"` with no new test symbol because HandlePaging is X-event code. Task 2 RED fixture passed immediately because `wm_ui_scale_edge_thickness` already converts source 2; GREEN is the last_edge_thickness source write-back._

## Files Created/Modified

- `fvwm/virtual.c` - HandlePaging uses mapped pan thickness; checkPanFrames stores source last_edge_thickness
- `tests/test_wm_ui_metrics.c` - `test_paging_edge_source_two_at_2x_is_four`

## Decisions Made

- Convert pan-frame proximity once with `mapped_edge_thickness`; keep stored `edge_thickness` as the 0-2 command source (PA-G14)
- Write `last_edge_thickness` in source space so the next `checkPanFrames` pass does not feed mapped pixels into `wm_ui_scale_edge_thickness` (PA-G15)
- Do not change `FScreen.c` initialization (`last_edge_thickness = 2`) or raise the 0-2 command range

## Deviations from Plan

None - plan executed exactly as written.

---

**Total deviations:** 0 auto-fixed
**Impact on plan:** None.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Paging proximity matches the mapped pan-frame band at 2x (source 2 → 4px)
- EdgeThickness 2 then 1 then 2 resizes existing pan frames because last_edge_thickness stays source
- Stored edge fields remain 0-2 source integers; WR-04 cursor reload and duplicated `scaled_move_threshold` stay out of scope
- Phase 04 plans complete; ready for `/gsd-verify-work` then Phase 5

## Self-Check: PASSED

- FOUND: `fvwm/virtual.c`
- FOUND: `tests/test_wm_ui_metrics.c`
- FOUND: `1a97e42c4`
- FOUND: `49b5c4e87`
- FOUND: `dfa7a31fa`

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*
