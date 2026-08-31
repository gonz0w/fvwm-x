---
phase: 03-geometry-and-ownership-contracts
plan: 05
subsystem: geometry-ownership
tags: [xrandr, work-area, struts, ewmh, bounding-box, snap, minoverlap, root-pixels, hidpi, c11]

requires:
  - phase: 03-geometry-and-ownership-contracts
    provides: monitor_ownership_work_rect root-pixel strut intersection
  - phase: 03-geometry-and-ownership-contracts
    provides: Stored EWMH work areas and strut strips in monitor root pixels
provides:
  - monitor_ownership_work_insets and monitor_ownership_work_rect_paged
  - Bounding-box, snap, and minoverlap consumers of stored root-pixel work rects
  - $[wa.*]/$[dwa.*] documented as root-pixel work-area rectangle fields
affects: [04-wm-owned-ui-metrics, ewmh-workarea, modules]

actuals:
  tokens: 4018
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - derive work-area insets as stored origin minus si origin
    - page-offset a stored root-pixel work rect without adding si origin again

key-files:
  created: []
  modified:
    - libs/monitor_ownership.h
    - libs/monitor_ownership.c
    - tests/test_monitor_ownership.c
    - fvwm/ewmh.c
    - fvwm/move_resize.c
    - fvwm/placement.c
    - doc/fvwm3_manpage_source.adoc

key-decisions:
  - "Work-area insets are work origin minus si origin; consumers must not add si origin onto stored ewmh_working_area."
  - "Snap copies the stored root-pixel work rect plus page offset only."
  - "$[wa.x]/$[wa.y] remain the stored root-pixel origin and are documented as such, not strut insets."

patterns-established:
  - "monitor_ownership_work_insets is the shared inset derivation for bounding-box, strut intersection, and minoverlap."
  - "monitor_ownership_work_rect_paged is the shared page-offset copy of a stored root-pixel work rect."

requirements-completed: [GEO-05, TEST-01]

coverage:
  - id: D1
    description: "For monitor rect x=-1920,y=0,width=1920,height=1080 and stored work rect x=-1856,y=32,width=1856,height=1048, monitor_ownership_work_insets returns left=64,right=0,top=32,bottom=0."
    requirement: GEO-05
    verification:
      - kind: unit
        ref: "tests/test_monitor_ownership.c#test_work_insets_negative_origin"
        status: pass
    human_judgment: false
  - id: D2
    description: "The same insets hold for monitor x=1920 and work x=1984."
    requirement: GEO-05
    verification:
      - kind: unit
        ref: "tests/test_monitor_ownership.c#test_work_insets_right_hand_origin"
        status: pass
    human_judgment: false
  - id: D3
    description: "monitor_ownership_work_rect_paged on that work rect with page 0,0 returns x=-1856, not -3776; page_x=1920 returns x=64."
    requirement: GEO-05
    verification:
      - kind: unit
        ref: "tests/test_monitor_ownership.c#test_work_rect_paged_negative_origin"
        status: pass
    human_judgment: false
  - id: D4
    description: "Minoverlap working-area candidates use page_p1 plus those insets: page_p1.x=-1920 yields xtest=-1856; page_p1.x=1920 yields xtest=1984."
    requirement: GEO-05
    verification:
      - kind: unit
        ref: "tests/test_monitor_ownership.c#test_minoverlap_edges_use_insets"
        status: pass
    human_judgment: false
  - id: D5
    description: "get_ewmhc_boundingbox and EWMH_GetStrutIntersection derive insets via monitor_ownership_work_insets so modules and move_to_next_monitor receive left=64 on a -1920 monitor with a 64px left strut."
    requirement: GEO-05
    verification:
      - kind: other
        ref: "python identifier check of get_ewmhc_boundingbox and EWMH_GetStrutIntersection; meson test monitor-ownership"
        status: pass
    human_judgment: false
  - id: D6
    description: "shuffle_win_to_closest page-offsets the stored work rect; minoverlap X/Y working-area edges use derived insets plus page_p1."
    requirement: GEO-05
    verification:
      - kind: other
        ref: "python identifier check of shuffle_win_to_closest and _pl_minoverlap_get_next_{x,y}; meson test monitor-ownership"
        status: pass
    human_judgment: false
  - id: D7
    description: "$[wa.x] and $[dwa.x] are documented as root-pixel origin in actual X11 pixels, not strut insets; expansion still reads stored x/y."
    requirement: GEO-05
    verification:
      - kind: other
        ref: "python phrase check of doc/fvwm3_manpage_source.adoc $[wa.x]/$[dwa.x] paragraphs"
        status: pass
    human_judgment: false
  - id: D8
    description: "Native TEST-01 targets monitor-ownership, icccm-size-hints, monitor-scale, and monitor-topology are green together."
    requirement: TEST-01
    verification:
      - kind: unit
        ref: "meson test -C builddir monitor-ownership icccm-size-hints monitor-scale monitor-topology"
        status: pass
    human_judgment: false

duration: 6min
completed: 2026-08-31
status: complete
---

# Phase 3 Plan 5: Work-Area Inset Consumers Summary

**Checked inset and paged-rect helpers convert stored root-pixel work areas so bounding-box, snap, minoverlap, and $[wa.*] stay correct when si origin is not 0.**

## Performance

- **Duration:** 6 min
- **Started:** 2026-08-31T11:16:02Z
- **Completed:** 2026-08-31T11:22:15Z
- **Tasks:** 3
- **Files modified:** 7

## Accomplishments

- `monitor_ownership_work_insets` derives left/right/top/bottom as work origin minus si origin with checked `__int128` arithmetic; negative derived insets are allowed.
- `monitor_ownership_work_rect_paged` copies a stored root-pixel work rect plus page offset only, so page 0,0 keeps x=-1856 instead of double-counting to -3776.
- `get_ewmhc_boundingbox` and `EWMH_GetStrutIntersection` consume those insets; snap page-offsets the stored rect; minoverlap working-area edges add insets to `page_p1`.
- `$[wa.*]`/`$[dwa.*]` remain the stored root-pixel rectangle fields and the manpage states that contract, including the -1920/64px example yielding `$[wa.x]` of -1856.

## Task Commits

Each task was committed atomically:

1. **Task 1: End-to-end work-area insets through bounding-box** - `651af158a` (feat)
2. **Task 2: Snap and minoverlap consume stored root-pixel work rects** - `b76e6d6be` (feat)
3. **Task 3: Document $[wa.*] as root-pixel work-area origin** - `0afd3f97c` (docs)

**Plan metadata:** docs commit with this SUMMARY

## Files Created/Modified

- `libs/monitor_ownership.h` - `monitor_ownership_work_insets` and `monitor_ownership_work_rect_paged` prototypes
- `libs/monitor_ownership.c` - Checked inset derivation and page-offset copy; failure zeros outputs
- `tests/test_monitor_ownership.c` - GEO-05 consumer fixtures for si origin -1920 and 1920
- `fvwm/ewmh.c` - Bounding-box and public strut intersection via `work_insets`
- `fvwm/move_resize.c` - Snap via `work_rect_paged`; keep monitor+page wa when the helper fails
- `fvwm/placement.c` - Minoverlap working-area edges via insets plus `page_p1`
- `doc/fvwm3_manpage_source.adoc` - `$[wa.*]`/`$[dwa.*]` as root-pixel work-area rectangle fields

## Decisions Made

- Inset-only sites derive left/top as work origin minus si origin through `monitor_ownership_work_insets`; they must not treat stored `ewmh_working_area.x/y` as insets.
- Rectangle sites (`shuffle_win_to_closest`) copy the stored root-pixel work rect plus page offset only; they must not add `si->x` onto an already-absolute origin.
- On helper failure, bounding-box and strut intersection return zeros; snap keeps the monitor+page rectangle; minoverlap skips the two working-area edge tests.
- `$[wa.x]`/`$[wa.y]`/`$[dwa.x]`/`$[dwa.y]` stay the stored root-pixel origin (PA-12). Expansion does not subtract si origin.
- Negative derived insets are allowed. Overflow is any wide result outside `INT_MIN..INT_MAX`.
- CR-01 double-counted origins no longer ship.

## Deviations from Plan

None - plan executed exactly as written.

TDD_MODE was false for this run, so Task 1 and Task 2 shipped as one feat commit each (tests plus implementation) rather than separate RED/GREEN commits. Behavior matches the plan.

---

**Total deviations:** 0 auto-fixed
**Impact on plan:** None

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- GEO-05 CR-01 is closed: bounding-box, snap, minoverlap, and `$[wa.*]` are correct for si origin -1920 and 1920.
- Phase 3 geometry and ownership contracts are complete; ready for `/gsd-verify-work` then Phase 4 UI metrics.
- TEST-02 live-X layout matrix remains Phase 8.
- No Status v4 key, module packet field, or live-X case was added.

---
*Phase: 03-geometry-and-ownership-contracts*
*Completed: 2026-08-31*

## Self-Check: PASSED
