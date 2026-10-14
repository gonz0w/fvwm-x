---
phase: 02-scale-policy-and-configuration
plan: 01
subsystem: monitor-scale-policy
tags: [xrandr, hidpi, exact-arithmetic, c11, meson]

requires:
  - phase: 01-topology-snapshot-and-live-x-harness
    provides: committed monitor pixel and physical-dimension facts
provides:
  - Exact quarter-step scale domain from 1x through 4x
  - Deterministic automatic DPI policy with explicit fallback reasons
  - Checked drift-free signed pixel conversions
  - Total effective-scale result on every staged runtime monitor
affects: [02-02-scale-configuration, 02-03-scale-recompute, geometry, ui-metrics]

actuals:
  tokens: 7327
  tasks: 3
  commits: 6

tech-stack:
  added: []
  patterns: [exact integer policy arithmetic, total initialized result structs, TDD red-green commits]

key-files:
  created: [libs/monitor_scale.h, libs/monitor_scale.c, tests/test_monitor_scale.c]
  modified: [libs/FScreen.h, libs/FScreen.c, libs/meson.build, tests/test_fscreen_topology.c, tests/meson.build]

key-decisions:
  - "Represent scale as quarter units from 4 through 16 so policy and conversion paths never require floating-point arithmetic."
  - "Resolve automatic DPI only when both axes are 50..500 DPI inclusive and differ by at most 10% relative to their arithmetic mean."
  - "Initialize every staged monitor to an explicit safe 1x result before publication so failure paths retain a total value."

patterns-established:
  - "Scale results carry factor, source, status, and reason together rather than relying on sentinel values."
  - "Pixel conversion starts from the original integer source and rounds exact signed halves away from zero."

requirements-completed: [SCALE-02, SCALE-03, SCALE-05]

coverage:
  - id: D1
    description: "Committed pixel and millimetre facts resolve to a deterministic exact automatic scale with boundary-safe DPI validation."
    requirement: SCALE-02
    verification:
      - kind: unit
        ref: "tests/test_monitor_scale.c; meson test -C builddir monitor-scale"
        status: pass
    human_judgment: false
  - id: D2
    description: "Scale policy remains allocation-free and does not mutate topology identity, client pixels, toolkit DPI, or RandR transforms."
    requirement: SCALE-03
    verification:
      - kind: unit
        ref: "tests/test_monitor_scale.c and tests/test_fscreen_topology.c; full native Meson suite"
        status: pass
    human_judgment: false
  - id: D3
    description: "Every quarter-step factor from 1x through 4x parses and converts with checked exact bounds, signed half-away rounding, and no cumulative drift."
    requirement: SCALE-05
    verification:
      - kind: unit
        ref: "tests/test_monitor_scale.c; meson test -C builddir monitor-scale"
        status: pass
    human_judgment: false

duration: 59min
completed: 2026-08-30
status: complete
---

# Phase 2 Plan 1: Exact Monitor Scale Policy Summary

**An exact quarter-unit scale domain now derives explainable per-monitor results from committed DPI facts and performs checked, drift-free pixel conversion.**

## Performance

- **Duration:** 59 min
- **Started:** 2026-08-30T18:22:53Z
- **Completed:** 2026-08-30T19:21:25Z
- **Tasks:** 3
- **Files modified:** 8

## Accomplishments

- Added canonical parsing and formatting for every quarter-step scale from 1x through 4x.
- Implemented exact automatic DPI validation, nearest-quarter selection with upward midpoint ties, and explicit override/fallback precedence.
- Added overflow-checked signed pixel conversion with initialized failure output and exact halves rounded away from zero.
- Attached a safe total effective-scale result directly to each staged runtime monitor before publication.
- Covered factor grammar, DPI and agreement boundaries, precedence, overflow, drift resistance, and runtime initialization in native tests.

## Task Commits

Each TDD task was committed as an atomic RED/GREEN pair:

1. **Task 1: Trace one valid committed DPI pair to an exact automatic result**
   - `91e1ed181` (test: failing tracer)
   - `4e23c8e01` (feat: exact automatic tracer implementation)
2. **Task 2: Close factor grammar, DPI boundaries, precedence, and checked conversions**
   - `aef51f151` (test: failing boundary coverage)
   - `3416cc7da` (feat: complete exact policy boundaries)
3. **Task 3: Attach a safe total result directly to every staged runtime monitor**
   - `caed7eb27` (test: failing runtime association checks)
   - `2110a2932` (feat: runtime monitor association)

## Files Created/Modified

- `libs/monitor_scale.h` - Defines exact factors, result metadata, parser, resolver, and checked conversion contracts.
- `libs/monitor_scale.c` - Implements allocation-free exact scale arithmetic and automatic DPI policy.
- `libs/FScreen.h` - Associates a total effective-scale result with each runtime monitor.
- `libs/FScreen.c` - Initializes staged monitors to the explicit safe 1x result.
- `libs/meson.build` - Adds the scale policy implementation to the shared static library.
- `tests/test_monitor_scale.c` - Exercises grammar, exact thresholds, precedence, conversion, overflow, and drift behavior.
- `tests/test_fscreen_topology.c` - Verifies runtime monitor initialization and retained safe state.
- `tests/meson.build` - Registers the assertion-enabled monitor-scale test target.

## Decisions Made

- Quarter units are the canonical representation because the supported domain is finite and exact; no floating-point parsing or policy arithmetic is needed.
- Automatic selection requires valid physical facts on both axes, inclusive 50..500 DPI bounds, and at most 10% symmetric disagreement.
- All fallible APIs initialize outputs before validation, and staged runtime monitors begin with a safe 1x result before any publication can occur.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- The first full-suite attempt reused a build configured with live-X tests and consequently entered topology scenarios not required by this native policy plan. Reconfiguring the existing container build with `-Dlive-x-tests=false` produced the intended native verification profile; all 11 tests passed. Existing unrelated compiler warnings were left unchanged.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 02-02 can build transactional exact-name overrides and global fallback configuration on the completed scale domain.
- Plan 02-03 can resolve and publish configured effective scales through the runtime association established here.
- No blockers remain.

## Self-Check: PASSED

All eight implementation/test files and all six task commits were verified.

---
*Phase: 02-scale-policy-and-configuration*
*Completed: 2026-08-30*
