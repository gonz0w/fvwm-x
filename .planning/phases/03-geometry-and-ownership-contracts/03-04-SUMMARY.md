---
phase: 03-geometry-and-ownership-contracts
plan: 04
subsystem: geometry-ownership
tags: [xrandr, work-area, struts, placement, ewmh, root-pixels, hidpi, c11]

requires:
  - phase: 03-geometry-and-ownership-contracts
    provides: allocation-free monitor_ownership_resolve with incumbent hysteresis
  - phase: 03-geometry-and-ownership-contracts
    provides: integer icccm_constrain_size over pixel sizes
provides:
  - monitor_ownership_work_rect root-pixel strut intersection
  - Stored EWMH work areas and strut strips in monitor root pixels
  - StartsOnScreen-not-honored placement on the current monitor snapshot rect
  - User-visible ownership and actual-pixel protocol contract
affects: [04-wm-owned-ui-metrics, transitions, ewmh-workarea]

actuals:
  tokens: 2757
  tasks: 3
  commits: 5

tech-stack:
  added: []
  patterns:
    - work-area intersection in snapshot root pixels including negative origins
    - published EWMH/ICCCM/module geometry remains actual X11 pixels

key-files:
  created: []
  modified:
    - libs/monitor_ownership.h
    - libs/monitor_ownership.c
    - tests/test_monitor_ownership.c
    - fvwm/ewmh.c
    - fvwm/placement.c
    - doc/fvwm3_manpage_source.adoc

key-decisions:
  - "Work rect is the snapshot monitor rectangle inset by that monitor's strut integers in root pixels, including negative si->x."
  - "Negative remaining width or height, or invalid insets, initialize the output rectangle to zeros and return MONITOR_OWNERSHIP_INVALID_ARGUMENT."
  - "Stored ewmh_working_area is already in root pixels, so EWMH_GetWorkAreaIntersection intersects without adding m->si->x again."
  - "Strut strips use monitor si extents; they do not wrap through DisplayWidth or DisplayHeight."
  - "When StartsOnScreen is set but not honored, screen_g comes from FScreenGetScrRect CURRENT rather than 0,0,DisplayWidth,DisplayHeight."
  - "EWMH, ICCCM, configure, frame extents, work area, and module packets stay actual X11 pixels; Status and module packets were not expanded."

patterns-established:
  - "monitor_ownership_work_rect is the shared root-pixel work-area contract; EWMH stores its output."
  - "Paging still uses monitor_get_all_widths; ownership, work area, placement, and MoveToScreen use snapshot rectangles."

requirements-completed: [GEO-05, GEO-07, TEST-01]

coverage:
  - id: D1
    description: "A monitor at x=-1920 with left/top struts 64/32 yields a work rect at x=-1856, y=32, width=1856, height=1048 without folding through DisplayWidth."
    requirement: GEO-05
    verification:
      - kind: unit
        ref: "tests/test_monitor_ownership.c#test_work_rect_negative_origin_struts"
        status: pass
    human_judgment: false
  - id: D2
    description: "Invalid remaining size initializes the work-rect outputs to zeros and returns MONITOR_OWNERSHIP_INVALID_ARGUMENT."
    requirement: GEO-05
    verification:
      - kind: unit
        ref: "tests/test_monitor_ownership.c#test_work_rect_invalid_insets_initialize_outputs"
        status: pass
    human_judgment: false
  - id: D3
    description: "ewmh_ComputeAndSetWorkArea stores monitor_ownership_work_rect output; strut strips use monitor si extents; unhonored StartsOnScreen uses FScreenGetScrRect CURRENT; MOVE_SCREEN keeps m->si destination geometry."
    requirement: GEO-05
    verification:
      - kind: other
        ref: "meson compile -C builddir && meson test -C builddir monitor-ownership"
        status: pass
    human_judgment: true
    rationale: "Native work-rect fixtures and compilation prove the helper, but EWMH property publication and live StartsOnScreen/MoveToScreen mapping are asserted by source inspection rather than a nested-X fixture."
  - id: D4
    description: "EWMH, ICCCM, configure, frame-extents, work-area, and module geometry fields remain actual X11 pixels; protocol files gained no scaled-unit fields."
    requirement: GEO-07
    verification:
      - kind: other
        ref: "git diff -- fvwm/status_json.c libs/Module.h libs/vpacket.h fvwm/module_interface.c; EWMH_SetFrameStrut border pixels"
        status: pass
    human_judgment: false
  - id: D5
    description: "Native TEST-01 targets monitor-ownership, icccm-size-hints, monitor-scale, and monitor-topology are green together."
    requirement: TEST-01
    verification:
      - kind: unit
        ref: "meson test -C builddir monitor-ownership icccm-size-hints monitor-scale monitor-topology"
        status: pass
    human_judgment: false

duration: 5min
completed: 2026-08-31
status: complete
---

# Phase 3 Plan 4: Work Area, Placement, and Protocol Pixels Summary

**Work areas and strut strips intersect snapshot monitor rectangles in root pixels, unhonored StartsOnScreen placement uses the current monitor rect, and published EWMH/module geometry stays actual X11 pixels.**

## Performance

- **Duration:** 5 min
- **Started:** 2026-08-31T01:32:11Z
- **Completed:** 2026-08-31T01:38:02Z
- **Tasks:** 3
- **Files modified:** 6

## Accomplishments

- `monitor_ownership_work_rect` insets a snapshot rectangle by strut integers with checked subtraction, including negative origins.
- EWMH stores that root-pixel rectangle and builds strut strips from `m->si` extents; `_NET_WORKAREA` shape and `EWMH_SetFrameStrut` border pixels are unchanged.
- Unhonored `StartsOnScreen` uses `FScreenGetScrRect CURRENT`; `MOVE_SCREEN` still copies destination geometry from `m->si`.
- The manpage states root-pixel ownership with 8-pixel incumbent hysteresis and actual X11 pixel protocol fields.

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: failing work-rect strut fixtures** - `f00df07d8` (test)
2. **Task 1 GREEN: implement work-rect and EWMH root-pixel storage** - `7a3a383f3` (feat)
3. **Task 2: place unhonored StartsOnScreen on current monitor rect** - `96a935a90` (feat)
4. **Task 3: document root-pixel ownership and actual-pixel protocol** - `b52ecd956` (docs)

**Plan metadata:** docs commit with this SUMMARY

## Files Created/Modified

- `libs/monitor_ownership.h` - `monitor_ownership_work_rect` prototype
- `libs/monitor_ownership.c` - Checked root-pixel strut intersection
- `tests/test_monitor_ownership.c` - Negative-origin and invalid-inset work-rect fixtures
- `fvwm/ewmh.c` - Store work areas in root pixels; strut strips from monitor extents
- `fvwm/placement.c` - Unhonored StartsOnScreen uses current snapshot rectangle
- `doc/fvwm3_manpage_source.adoc` - MoveToScreen and StartsOnScreen ownership contract

## Decisions Made

- Work rect origin is `monitor_rect.x + strut_left` (same for y); remaining size subtracts both edge insets and must not go negative.
- Overflow of origin or size that does not fit in `int` returns `MONITOR_OWNERSHIP_OVERFLOW` after zeroing the output.
- `EWMH_GetWorkAreaIntersection` intersects the query with the stored root-pixel rect using integer max/min.
- Public `EWMH_GetStrutIntersection` derives left/right/top/bottom from the stored work rect versus `m->si`, not `DisplayWidth`.
- Paging rectangles may still use `monitor_get_all_widths`; MoveToScreen destination `s` remains `m->si`.
- No Status v4 key, module packet field, partial strut, or live-X layout matrix was added.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Public strut inset derivation still used DisplayWidth**
- **Found during:** Task 1 (GREEN)
- **Issue:** After storing `ewmh_working_area` in root pixels, `EWMH_GetStrutIntersection` still computed right/bottom insets from `monitor_get_all_widths/heights`, which would treat a negative-origin `x` as a huge left strut.
- **Fix:** Derive insets from `m->si` extents versus the stored root-pixel work rect.
- **Files modified:** `fvwm/ewmh.c`
- **Verification:** `meson test -C builddir monitor-ownership` green; no remaining `monitor_get_all_widths` calls in `fvwm/ewmh.c`
- **Committed in:** `7a3a383f3` (Task 1 GREEN)

Task 2 carried `tdd="true"` but listed only `fvwm/placement.c` and `fvwm/move_resize.c`; MOVE_SCREEN already matched the contract, so there was no separate RED commit.

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Required for GEO-05 correctness on negative-origin monitors. No scope creep.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 3 geometry and ownership contracts are complete.
- Work areas, placement, and MoveToScreen consume snapshot root-pixel rectangles.
- Protocol seams remain actual pixels; UI resource scaling can proceed in Phase 4 without expanding Status or module packets.
- TEST-02 live-X layout matrix remains Phase 8.

---
*Phase: 03-geometry-and-ownership-contracts*
*Completed: 2026-08-31*

## Self-Check: PASSED
