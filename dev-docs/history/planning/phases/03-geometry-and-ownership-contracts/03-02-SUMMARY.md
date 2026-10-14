---
phase: 03-geometry-and-ownership-contracts
plan: 02
subsystem: geometry-ownership
tags: [xrandr, ownership, hysteresis, incumbent, hidpi, c11]

requires:
  - phase: 03-geometry-and-ownership-contracts
    provides: allocation-free monitor_ownership_resolve over snapshot rectangles
provides:
  - Incumbent keep/steal using extra_overlap versus MONITOR_OWNERSHIP_HYSTERESIS_PX
  - Seam hysteresis and identity-tie incumbent fixtures
  - Frame-rect ownership with incumbent, without scaling g.normal
affects: [03-03-icccm-constrain, 03-04-placement-work-area, update_fvwm_monitor]

actuals:
  tokens: 2286
  tasks: 2
  commits: 4

tech-stack:
  added: []
  patterns:
    - incumbent extra-overlap hysteresis before unique-max and identity
    - mapped frame rectangle as the ownership reference geometry

key-files:
  created: []
  modified:
    - libs/monitor_ownership.c
    - tests/test_monitor_ownership.c
    - fvwm/update.c

key-decisions:
  - "Keep an overlapping incumbent when extra_overlap is at most MONITOR_OWNERSHIP_HYSTERESIS_PX; steal only when extra_overlap is strictly greater."
  - "An overlapping incumbent wins before name_atom/name identity on equal overlap."
  - "Mapped windows resolve over fw->g.frame; unmapped windows keep the maximized-or-normal rectangle."
  - "Unresolved ownership names fall back to existing fw->m or monitor_by_primary without aborting."

patterns-established:
  - "extra_overlap is challenger overlap area minus incumbent overlap area, computed on __int128 and saturated to int."
  - "update_fvwm_monitor is a single-threaded consumer of monitor_ownership_resolve; it does not apply scale."

requirements-completed: [GEO-01]

coverage:
  - id: D1
    description: "Incumbent remains owner on a seam when challenger extra overlap is at most MONITOR_OWNERSHIP_HYSTERESIS_PX, and the challenger wins only when extra overlap is greater."
    requirement: GEO-01
    verification:
      - kind: unit
        ref: "tests/test_monitor_ownership.c#test_incumbent_kept_on_equal_seam"
        status: pass
      - kind: unit
        ref: "tests/test_monitor_ownership.c#test_incumbent_kept_when_extra_overlap_equals_hysteresis"
        status: pass
      - kind: unit
        ref: "tests/test_monitor_ownership.c#test_challenger_wins_beyond_hysteresis"
        status: pass
      - kind: unit
        ref: "tests/test_monitor_ownership.c#test_incumbent_kept_when_one_pixel_overlap_remains"
        status: pass
    human_judgment: false
  - id: D2
    description: "update_fvwm_monitor assigns fw->m from the ownership resolver over the mapped frame rectangle (else g.max/g.normal) plus incumbent fw->m->si->name_atom."
    requirement: GEO-01
    verification:
      - kind: unit
        ref: "meson test -C builddir monitor-ownership"
        status: pass
    human_judgment: true
    rationale: "Native ownership fixtures prove the resolver contract, but update_fvwm_monitor wiring is asserted by source inspection rather than a FvwmWindow unit test."

duration: 8min
completed: 2026-08-31
status: complete
---

# Phase 3 Plan 2: Incumbent Hysteresis Summary

**Incumbent extra-overlap hysteresis keeps seam owners until the challenger exceeds 8 pixels of extra area, and update_fvwm_monitor assigns fw->m from the mapped frame rectangle plus incumbent identity.**

## Performance

- **Duration:** 8 min
- **Started:** 2026-08-31T01:03:53Z
- **Completed:** 2026-08-31T01:12:15Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- `monitor_ownership_resolve` keeps an overlapping incumbent when extra overlap is at most `MONITOR_OWNERSHIP_HYSTERESIS_PX` and steals only when the challenger exceeds it.
- Native GEO-01 fixtures cover an equal-overlap seam, extra_overlap 8, extra_overlap 9, one-pixel remaining incumbent overlap, and a zero-overlap incumbent.
- `update_fvwm_monitor` consumes the frame rectangle plus incumbent `name_atom` and does not scale window geometry.

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: failing incumbent hysteresis fixtures** - `8127336de` (test)
2. **Task 1 GREEN: keep incumbent within extra-overlap hysteresis** - `58cd548cc` (feat)
3. **Task 2: assign window owner from frame plus incumbent** - `ec221d76b` (feat)

**Plan metadata:** docs commit with this SUMMARY

## Files Created/Modified

- `libs/monitor_ownership.c` - Incumbent keep/steal using extra_overlap versus hysteresis
- `tests/test_monitor_ownership.c` - Seam hysteresis and identity-tie incumbent fixtures
- `fvwm/update.c` - Frame-rect ownership with incumbent, without scaling g.normal

## Decisions Made

- Extra overlap is challenger area minus incumbent area; keep when that difference is at most 8, steal when it is greater.
- An overlapping incumbent wins before identity, so equal-overlap seams do not flicker when the incumbent is the higher `name_atom`.
- Mapped windows use `fw->g.frame`; unmapped windows keep the existing maximized-or-normal choice.
- NULL snapshot or unresolved `result.name` falls back to existing `fw->m` or `monitor_by_primary`; the path does not abort.
- The extra_overlap 8/9 strip is a 100-wide (then 101-wide) height-1 seam window so extra overlap area is exactly 8 and 9.

## Deviations from Plan

Task 2 carried `tdd="true"` but listed only `fvwm/update.c`; the existing native ownership suite was the verification, with no separate RED commit for the `update_fvwm_monitor` wiring.

---

**Total deviations:** 0 auto-fixed
**Impact on plan:** No scope creep. Task 2 skipped a standalone RED commit because no new test file was in the task files list.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 03-03 ICCCM size-hint constraining.
- Placement and work-area consumers can now treat `fw->m` as the hysteresis-stable owner.
- `FindScreenOfXY` remains a no-incumbent 1x1 point wrapper.

---
*Phase: 03-geometry-and-ownership-contracts*
*Completed: 2026-08-31*

## Self-Check: PASSED
