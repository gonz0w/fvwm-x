---
phase: 03-geometry-and-ownership-contracts
plan: 01
subsystem: geometry-ownership
tags: [xrandr, ownership, root-pixels, hidpi, c11, meson]

requires:
  - phase: 01-topology-snapshot-and-live-x-harness
    provides: committed monitor snapshot identity and root-pixel rectangles
  - phase: 02-scale-policy-and-configuration
    provides: allocation-free libs/ policy unit shape with checked wide arithmetic
provides:
  - Allocation-free monitor_ownership_resolve over snapshot rectangles
  - Native TOP-02 ownership fixtures except incumbent hysteresis
  - FindScreenOfXY as a 1x1 point query with a logged total fallback
affects: [03-02-incumbent-hysteresis, 03-04-placement-work-area, update_fvwm_monitor]

actuals:
  tokens: 5401
  tasks: 3
  commits: 6

tech-stack:
  added: []
  patterns:
    - allocation-free ownership resolve over snapshot facts
    - checked __int128 overlap area with initialized OVERFLOW outputs
    - point lookup as a 1x1 ownership query

key-files:
  created:
    - libs/monitor_ownership.h
    - libs/monitor_ownership.c
    - tests/test_monitor_ownership.c
  modified:
    - libs/meson.build
    - tests/meson.build
    - libs/FScreen.c

key-decisions:
  - "Ownership compares literal snapshot x,y,width,height and never folds through DisplayWidth."
  - "Equal overlap and clone ties compare name_atom then strcmp(name), independent of array order."
  - "Empty input returns an initialized FALLBACK result without aborting; overlap area that does not fit in int returns MONITOR_OWNERSHIP_OVERFLOW."
  - "FindScreenOfXY is a 1x1 point wrapper with incumbent None and hysteresis 0, then a logged primary or enabled non-global fallback."

patterns-established:
  - "monitor_ownership_resolve is the shared owner contract; callers map name_atom/name onto struct monitor."
  - "Overlap area and gap distance use private __int128 intermediates; outputs are initialized before every return."
  - "Paging helpers remain DisplayWidth/DisplayHeight; ownership uses committed snapshot rectangles."

requirements-completed: [TOP-02, TEST-01]

coverage:
  - id: D1
    description: "A window contained in a negative-origin snapshot rectangle resolves to that monitor identity, not a primary at the origin."
    requirement: TOP-02
    verification:
      - kind: unit
        ref: "tests/test_monitor_ownership.c#test_negative_origin_monitor_owns_contained_window"
        status: pass
    human_judgment: false
  - id: D2
    description: "Clone, partial-overlap, rotation, reorder, gap, empty, and overflow fixtures are order-independent and initialize outputs on failure."
    requirement: TOP-02
    verification:
      - kind: unit
        ref: "tests/test_monitor_ownership.c; meson test -C builddir monitor-ownership"
        status: pass
    human_judgment: false
  - id: D3
    description: "FindScreenOfXY queries committed snapshot rectangles in literal root pixels and maps the owner onto an enabled non-global struct monitor with a logged total fallback."
    requirement: TOP-02
    verification:
      - kind: unit
        ref: "meson test -C builddir monitor-ownership monitor-topology fscreen-topology-adapter"
        status: pass
    human_judgment: true
    rationale: "Point-to-struct-monitor mapping is compiled and related native targets are green, but no dedicated FindScreenOfXY fixture asserts the live monitor_q pointer."

duration: 13min
completed: 2026-08-31
status: complete
---

# Phase 3 Plan 1: Monitor Ownership Resolve Summary

**Allocation-free `monitor_ownership_resolve` maps window rectangles onto snapshot identities in literal root pixels, and `FindScreenOfXY` is now a 1x1 wrapper over that contract.**

## Performance

- **Duration:** 13 min
- **Started:** 2026-08-31T00:47:01Z
- **Completed:** 2026-08-31T00:59:33Z
- **Tasks:** 3
- **Files modified:** 6

## Accomplishments

- Production `libfvwm3` exposes `monitor_ownership_resolve` over committed snapshot `name_atom`, `name`, and signed `x,y,width,height`.
- Native TOP-02 fixtures except incumbent hysteresis are green, including both insertion orders for clones and reorders.
- `FindScreenOfXY` no longer wraps through `DisplayWidth` or `exit(106)`; paging helpers are unchanged.

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: failing negative-origin tracer** - `040e02890` (test)
2. **Task 1 GREEN: implement monitor ownership resolve** - `21400edff` (feat)
3. **Task 2 RED: failing TOP-02 fixtures** - `1f5ea0b81` (test)
4. **Task 2 GREEN: close clone, gap, empty, and overflow policy** - `18da780f4` (feat)
5. **Task 3: FindScreenOfXY point wrapper** - `5d211707e` (feat)

**Plan metadata:** docs commit with this SUMMARY

## Files Created/Modified

- `libs/monitor_ownership.h` - Status/reason/result contract, hysteresis constant, safe-init, resolve prototype
- `libs/monitor_ownership.c` - Literal overlap, identity ties, Manhattan gaps, empty fallback, overflow
- `tests/test_monitor_ownership.c` - Native TOP-02 fixtures including both insertion orders
- `libs/meson.build` - Compile `monitor_ownership.c` into libfvwm3 immediately before `monitor_scale.c`
- `tests/meson.build` - Register `monitor-ownership` with `-UNDEBUG`
- `libs/FScreen.c` - `FindScreenOfXY` calls `FScreenGetTopologySnapshot` and `monitor_ownership_resolve`

## Decisions Made

- Ownership never folds coordinates through the root framebuffer size or chooses an owner by red-black or enumeration order.
- Unique maximum overlap wins; remaining ties and exact clones compare `name_atom` then `strcmp(name)`.
- Zero overlap uses Manhattan distance on snapshot rects from the window origin, then the same identity tie-break.
- `monitor_count == 0` returns `MONITOR_OWNERSHIP_OK` with `MONITOR_OWNERSHIP_REASON_FALLBACK` and `SAFE_INIT` fields; it does not abort.
- Overlap area that does not fit in `int` returns `MONITOR_OWNERSHIP_OVERFLOW` after re-initializing the result.
- `FindScreenOfXY` builds a 1x1 rectangle, uses incumbent `None` and hysteresis 0, maps `result.name` through `monitor_resolve_name`, and logs a primary or enabled non-global fallback.

## Deviations from Plan

None - plan executed exactly as written.

Task 3 carried `tdd="true"` but listed only `libs/FScreen.c`; the existing native ownership suite plus topology adapter tests were the verification, with no separate RED commit for the point-query seam.

---

**Total deviations:** 0 auto-fixed
**Impact on plan:** No scope creep. Task 3 skipped a standalone RED commit because no new test file was in the task files list.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 03-02 incumbent hysteresis on the shared `monitor_ownership_resolve` contract.
- `update_fvwm_monitor` still uses top-left `FindScreenOfXY`; plan 03-02 should pass the frame rectangle and incumbent identity.
- Paging `DisplayWidth` helpers and topology epoch semantics are unchanged.

---
*Phase: 03-geometry-and-ownership-contracts*
*Completed: 2026-08-31*

## Self-Check: PASSED
