---
phase: 03-geometry-and-ownership-contracts
plan: 03
subsystem: geometry-icccm
tags: [icccm, size-hints, hidpi, c11, meson]

requires:
  - phase: 02-scale-policy-and-configuration
    provides: allocation-free monitor_scale_apply with half-away-from-zero rounding
provides:
  - Integer icccm_constrain_size over min/max/base/inc/aspect/fixed/gravity
  - Scale-then-constrain native matrix including 1000 source-stable loops
  - constrain_size as a thin X-facing wrapper over the pure unit
affects: [03-04-placement-work-area, resize, transitions]

actuals:
  tokens: 6717
  tasks: 3
  commits: 5

tech-stack:
  added: []
  patterns:
    - allocation-free ICCCM constrain with checked __int128 arithmetic
    - scale-then-constrain from the original source, never a rounded result
    - X-facing constrain_size wrapper keeps style, borders, CS_ROUND_UP, and XSync

key-files:
  created:
    - libs/icccm_size_hints.h
    - libs/icccm_size_hints.c
    - tests/test_icccm_size_hints.c
  modified:
    - libs/meson.build
    - tests/meson.build
    - fvwm/geometry.c

key-decisions:
  - "Increment snap without CS_ROUND_UP uses nearest multiple of inc with halfway rounding away from zero; CS_ROUND_UP ceiling snap stays in constrain_size."
  - "Aspect uses checked __int128 cross-products; PBaseSize is subtracted only when that flag is set and base is not greater than min."
  - "On ICCCM_CONSTRAIN_OVERFLOW or INVALID_ARGUMENT, constrain_size logs and restores the pre-constrain client size rather than inventing a plausible size."

patterns-established:
  - "icccm_constrain_size is the shared pixel-size contract; callers pack sanitized hint integers."
  - "A scaled target is produced by monitor_scale_apply on the exact source before constrain."
  - "Missing PWinGravity is NorthWestGravity; gravity_constrain_size still calls constrain_size then gravity_resize."

requirements-completed: [GEO-03, TEST-01]

coverage:
  - id: D1
    description: "Inclusive ICCCM min/max clamp one unit outside the bound and leave one unit inside unchanged, including far-outside 10-to-20 and 50-to-40 cases and initialized invalid-argument outputs."
    requirement: GEO-03
    verification:
      - kind: unit
        ref: "tests/test_icccm_size_hints.c#test_min_max_clamp"
        status: pass
    human_judgment: false
  - id: D2
    description: "Base/inc grid, aspect with and without PBaseSize, base-greater-than-min, fixed-size, Northwest gravity, zero increment, and overflow fixtures use integer arithmetic."
    requirement: GEO-03
    verification:
      - kind: unit
        ref: "tests/test_icccm_size_hints.c; meson test -C builddir icccm-size-hints"
        status: pass
    human_judgment: false
  - id: D3
    description: "Scale-then-constrain applies monitor_scale_apply to source 7 then icccm_constrain_size, and 1000 loops recompute from that original source; monitor-scale remains green."
    requirement: TEST-01
    verification:
      - kind: unit
        ref: "tests/test_icccm_size_hints.c#test_repeated_constrain_from_original_source"
        status: pass
      - kind: unit
        ref: "meson test -C builddir icccm-size-hints monitor-scale"
        status: pass
    human_judgment: false
  - id: D4
    description: "constrain_size packs fw->hints into icccm_size_hints after border subtraction and keeps style min/max, CS_ROUND_UP, XSync, and max defect in the wrapper."
    requirement: GEO-03
    verification:
      - kind: other
        ref: "meson compile -C builddir && meson test -C builddir icccm-size-hints monitor-scale"
        status: pass
    human_judgment: true
    rationale: "The wrapper seam is compiled and related native targets are green, but no FvwmWindow fixture asserts live constrain_size style/XSync/CS_ROUND_UP behavior."

duration: 14min
completed: 2026-08-31
status: complete
---

# Phase 3 Plan 3: ICCCM Size Constrain Summary

**Integer `icccm_constrain_size` applies min/max/base/inc/aspect/fixed-size/gravity in pixel space, proves scale-then-constrain over 1000 source-stable loops, and leaves `constrain_size` as the X-facing wrapper.**

## Performance

- **Duration:** 14 min
- **Started:** 2026-08-31T01:14:49Z
- **Completed:** 2026-08-31T01:29:23Z
- **Tasks:** 3
- **Files modified:** 6

## Accomplishments

- Production `libfvwm3` exposes `icccm_constrain_size` over integer ICCCM fields with `OK` / `OVERFLOW` / `INVALID_ARGUMENT` status.
- Native GEO-03 matrix is green, including PBaseSize aspect rules, zero increment, overflow initialization, and scale-then-constrain from source 7.
- `constrain_size` no longer uses floating-ratio aspect; style, borders, `CS_ROUND_UP`, `XSync`, and max defect stay in the wrapper.

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: failing min/max clamp** - `6aa1e82fe` (test)
2. **Task 1 GREEN: implement min/max constrain** - `bd14436fe` (feat)
3. **Task 2 RED: failing ICCCM matrix** - `f73c69d46` (test)
4. **Task 2 GREEN: grid, aspect, gravity, overflow** - `069a3a4f7` (feat)
5. **Task 3: wrap constrain_size** - `d526b6806` (feat)

**Plan metadata:** docs commit with this SUMMARY

## Files Created/Modified

- `libs/icccm_size_hints.h` - Status/hint contract and `icccm_constrain_size` prototype
- `libs/icccm_size_hints.c` - Min/max/base/inc/aspect/fixed/gravity without double aspect
- `tests/test_icccm_size_hints.c` - GEO-03 matrix including scale-then-constrain and 1000 repetitions
- `libs/meson.build` - Compile `icccm_size_hints.c` into libfvwm3 immediately after `ImageSafety.c`
- `tests/meson.build` - Register `icccm-size-hints` with `-UNDEBUG`
- `fvwm/geometry.c` - `constrain_size` packs hints and calls `icccm_constrain_size`

## Decisions Made

- Nearest increment snap (halfway away from zero) lives in the pure unit; interactive `CS_ROUND_UP` ceiling snap and MotionNotify edge checks stay in `constrain_size`, which clears `PResizeInc` for that path so the pure unit does not snap twice.
- Aspect compares `min_aspect.x * height` against `min_aspect.y * width` on `__int128`; `PBaseSize` is subtracted only when that flag is set and base is not greater than min; min is never the aspect base.
- Overflow and invalid increment zero outputs in the pure unit; the wrapper logs `fvwm_debug(__func__, ...)` and restores the pre-constrain client size.
- Missing `PWinGravity` is `NorthWestGravity`; `gravity_constrain_size` still calls `constrain_size` then `gravity_resize` with a NULL frame on the pure call.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Gravity fixture used min-only hints so size did not change**
- **Found during:** Task 2 (GREEN)
- **Issue:** `test_gravity_northwest_default` set only `PMinSize` 50x40 on a 100x80 target, so clamp left the size unchanged and could not observe Northwest origin stability.
- **Fix:** Set `PMinSize` and `PMaxSize` equal to 50x40 so constrain actually resizes and `gravity_resize` keeps the Northwest origin.
- **Files modified:** `tests/test_icccm_size_hints.c`
- **Verification:** `meson test -C builddir icccm-size-hints` green
- **Committed in:** `069a3a4f7` (Task 2 GREEN)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Test correction only; production gravity default was already Northwest. No scope creep.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 03-04 placement, work-area, and strut contracts that consume scaled pixel sizes through `icccm_constrain_size`.
- `constrain_size` remains the X-facing wrapper; protocol units stay actual pixels.
- TEST-01 scale 1000-loop and the new constrain 1000-loop are both green.

---
*Phase: 03-geometry-and-ownership-contracts*
*Completed: 2026-08-31*

## Self-Check: PASSED
