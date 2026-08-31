---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 02
subsystem: testing
tags: [meson, ci, xcursor, xrender, png, svg, cairo, config.h]

requires:
  - phase: 04-wm-owned-ui-metrics-and-resources
    provides: wm-ui-metrics native test target and converter
provides:
  - Native supported-features HAVE_* assertions for Xcursor, XRender, PNG, and SVG
  - Explicit -D*=enabled flags on matrix, sanitizer, and live-x Meson setups
  - live-x harness --check-workflow contract kept in exact-once sync
affects: [04-03-picture-cache, 04-07-cursor-loading, TEST-05]

actuals:
  tokens: 973
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - Supported profile is explicit Meson enabled plus HAVE_* == 1, not feature auto
    - cairo is enabled alongside svg because librsvg needs a cairo backend
    - meson.options feature defaults remain auto

key-files:
  created:
    - tests/test_supported_features.c
  modified:
    - tests/meson.build
    - .github/workflows/ccpp.yml
    - tests/live_x/live_x_harness.py

key-decisions:
  - "Explicit enabled plus HAVE_* == 1 is the supported profile; meson.options defaults stay auto."
  - "cairo is enabled alongside svg because librsvg needs a cairo backend."

patterns-established:
  - "Pattern: native supported-features asserts HAVE_XCURSOR, HAVE_XRENDER, HAVE_PNG, and HAVE_RSVG each equal 1 under -UNDEBUG."
  - "Pattern: CI meson setup never treats feature auto as proof; every setup path passes -Dxcursor=enabled -Dxrender=enabled -Dpng=enabled -Dsvg=enabled -Dcairo=enabled."
  - "Pattern: live_x_harness.py --check-workflow matches the live-x meson setup string exactly once."

requirements-completed: [TEST-05]

coverage:
  - id: D1
    description: "The supported-profile native test asserts HAVE_XCURSOR, HAVE_XRENDER, HAVE_PNG, and HAVE_RSVG each equal 1."
    requirement: TEST-05
    verification:
      - kind: unit
        ref: "tests/test_supported_features.c#test_supported_profile_macros"
        status: pass
    human_judgment: false
  - id: D2
    description: "Matrix, sanitizer, and live-x meson setup commands pass -Dxcursor=enabled -Dxrender=enabled -Dpng=enabled -Dsvg=enabled -Dcairo=enabled rather than relying on feature auto."
    requirement: TEST-05
    verification:
      - kind: other
        ref: "python3 tests/live_x/live_x_harness.py --check-workflow .github/workflows/ccpp.yml"
        status: pass
    human_judgment: false
  - id: D3
    description: "meson.options png, svg, xcursor, and xrender defaults remain auto."
    requirement: TEST-05
    verification:
      - kind: other
        ref: "meson.options value: 'auto' for png, svg, xcursor, xrender"
        status: pass
    human_judgment: false

duration: 4min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 02: Supported Feature Profile Summary

**Native `supported-features` asserts HAVE_XCURSOR, HAVE_XRENDER, HAVE_PNG, and HAVE_RSVG are 1, and every CI Meson setup passes explicit enabled flags while meson.options defaults stay auto.**

## Performance

- **Duration:** 4 min
- **Started:** 2026-08-31T14:11:29Z
- **Completed:** 2026-08-31T14:16:05Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- Native `test_supported_profile_macros` fails if any of the four HAVE_* macros is not 1 under the enabled profile
- Matrix, sanitizer, and live-x meson setup commands each pass `-Dxcursor=enabled -Dxrender=enabled -Dpng=enabled -Dsvg=enabled -Dcairo=enabled`
- `tests/live_x/live_x_harness.py --check-workflow` still finds the live-x setup command exactly once
- `meson.options` png, svg, xcursor, and xrender defaults remain `auto`

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: Assert supported-profile HAVE macros** - `e6615caab` (test)
2. **Task 1 GREEN: Register supported-features Meson test** - `c2c22452d` (feat)
3. **Task 2: Pass explicit feature flags on every CI meson setup** - `9f6442749` (feat)

_Note: TDD task 1 produced RED then GREEN commits; no REFACTOR was needed._

## Files Created/Modified

- `tests/test_supported_features.c` - GPL banner, `config.h` first, asserts the four HAVE_* macros
- `tests/meson.build` - Registers slimmer `test-supported-features` with `-UNDEBUG` after wm-ui-metrics
- `.github/workflows/ccpp.yml` - Explicit enabled flags on matrix, sanitizer, and live-x meson setups
- `tests/live_x/live_x_harness.py` - Exact-once live-x setup command string kept in sync

## Decisions Made

- Explicit enabled plus HAVE_* == 1 is the supported profile; meson.options feature defaults stay auto so packagers can still disable a feature, but CI cannot silently drop one via auto.
- cairo is enabled alongside svg because librsvg needs a cairo backend (PA-11).

## Deviations from Plan

None - plan executed exactly as written.

---

**Total deviations:** 0 auto-fixed
**Impact on plan:** None.

## TDD Gate Compliance

| Task | RED | GREEN | REFACTOR | Status |
|------|-----|-------|----------|--------|
| 1 supported-profile HAVE macros | `e6615caab` | `c2c22452d` | — | Pass |

RED `meson test supported-features` failed with unknown test name before Meson registration. GREEN `meson test supported-features` passed after reconfigure with the five enabled flags.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 04-03 (Picture cache keys) and later chrome/cursor plans that assume the supported Xcursor/XRender/PNG/SVG profile
- TEST-05 configure-time contract is asserted natively and in CI setup flags
- No blockers

## Self-Check: PASSED

- `tests/test_supported_features.c` FOUND
- `tests/meson.build` FOUND
- `.github/workflows/ccpp.yml` FOUND
- `tests/live_x/live_x_harness.py` FOUND
- Commits `e6615caab`, `c2c22452d`, `9f6442749` FOUND
- `meson test supported-features wm-ui-metrics` passed in Podman Linux
- `python3 tests/live_x/live_x_harness.py --check-workflow .github/workflows/ccpp.yml` exited 0

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*
