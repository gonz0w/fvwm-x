---
phase: 01-topology-snapshot-and-live-x-harness
plan: 03
subsystem: event-loop-policy
tags: [c11, meson, tdd, monotonic-time, debounce]

requires:
  - phase: 01-02
    provides: Failure-safe live RandR topology refresh outcomes and last-good retention
provides:
  - Pure injected-time topology refresh policy with quiet and hard-cap deadlines
  - Saturating deadline arithmetic and positive timeout ceiling semantics
  - Deterministic completion reset and later-generation rearming
  - Production and assertion-enabled focused Meson linkage
affects: [01-04-topology-events, topology-event-loop, live-x-storm-verification]

actuals:
  tokens: 2398
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns: [injected monotonic time, two-deadline burst settling, saturating timeout arithmetic]

key-files:
  created:
    - fvwm/topology_refresh_policy.h
    - fvwm/topology_refresh_policy.c
    - tests/test_topology_refresh_policy.c
  modified:
    - fvwm/meson.build
    - tests/meson.build

key-decisions:
  - "Topology refresh due time is the earlier of a trailing 50 ms quiet deadline and a first-event 250 ms hard cap."
  - "Clean policy state returns no-timeout sentinel -1, due state returns 0, and every positive fractional millisecond rounds up."
  - "Refresh completion resets the entire generation regardless of changed, unchanged, or failed refresh outcome."

patterns-established:
  - "Pure deadline policy: callers inject monotonic nanoseconds and retain all clock, X11, select, and logging work in the integration layer."
  - "Bounded settling: repeated marks update only the trailing timestamp while preserving the first-event starvation cap."

requirements-completed: [TOP-03]

coverage:
  - id: D1
    description: "One dirty generation extends its quiet deadline under repeated notifications but can never move past the 250 ms first-event cap."
    requirement: TOP-03
    verification:
      - kind: unit
        ref: "tests/test_topology_refresh_policy.c#test_hard_cap_wins_during_continuous_events"
        status: pass
      - kind: unit
        ref: "tests/test_topology_refresh_policy.c#test_quiet_deadline_extends_with_marks"
        status: pass
    human_judgment: false
  - id: D2
    description: "Deadline saturation, exact due boundaries, positive timeout ceiling, completion reset, and later rearming are deterministic under injected time."
    requirement: TOP-03
    verification:
      - kind: unit
        ref: "tests/test_topology_refresh_policy.c#test_exact_boundaries_and_timeout_ceiling"
        status: pass
      - kind: unit
        ref: "tests/test_topology_refresh_policy.c#test_saturating_deadline_addition"
        status: pass
      - kind: unit
        ref: "tests/test_topology_refresh_policy.c#test_completion_resets_and_later_marks_rearm"
        status: pass
    human_judgment: false
  - id: D3
    description: "The same policy implementation is compiled exactly once into fvwm3 and exactly once into its assertion-enabled focused test target."
    requirement: TOP-03
    verification:
      - kind: integration
        ref: "python3 tests/run_in_linux.py -- meson test -C builddir monitor-topology fscreen-topology-adapter topology-refresh-policy --print-errorlogs"
        status: pass
      - kind: integration
        ref: "python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs"
        status: pass
    human_judgment: false

duration: 6min
completed: 2026-08-30
status: complete
---

# Phase 01 Plan 03: Bounded Topology Burst Settling Summary

**A pure injected-time policy now coalesces topology notifications behind a 50 ms quiet deadline while a saturating 250 ms hard cap prevents storm starvation and zero-time polling.**

## Performance

- **Duration:** 6 min
- **Started:** 2026-08-30T13:06:53Z
- **Completed:** 2026-08-30T13:12:53Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- Added a caller-owned refresh state whose first notification starts one generation and whose later marks extend only the trailing quiet deadline.
- Enforced a saturating first-event hard cap, exact due boundaries, and ceiling conversion so positive remaining time never becomes a false zero timeout.
- Added deterministic completion/reset and rearming coverage for changed, unchanged, and failed refresh outcomes without clocks, sleeps, threads, X calls, or hidden globals.
- Linked the policy exactly once into production and retained a direct, assertion-enabled focused test target in the native quick and full suites.

## Task Commits

Task 1 followed RED then GREEN; Task 2 was committed atomically after production-link verification:

1. **Task 1 RED: Specify injected-time refresh behavior** - `91db12296` (test)
2. **Task 1 GREEN: Implement bounded topology refresh deadlines** - `83263812e` (feat)
3. **Task 2: Link the production policy and lock quick sampling** - `c4c9f9d87` (chore)

## Files Created/Modified

- `fvwm/topology_refresh_policy.h` - Declares tuneable quiet/hard-cap constants, explicit state, initializer, and injected-time operations.
- `fvwm/topology_refresh_policy.c` - Implements first/last timestamp tracking, saturating deadline selection, timeout ceiling, and completion reset.
- `tests/test_topology_refresh_policy.c` - Covers first/last marks, quiet extension, sustained storms, exact boundaries, saturation, clean state, reset, and later generations.
- `fvwm/meson.build` - Links the refresh policy into the production `fvwm3` source graph exactly once.
- `tests/meson.build` - Registers the direct assertion-enabled `topology-refresh-policy` target.

## Decisions Made

- Used `min(last_dirty + 50 ms, first_dirty + 250 ms)` with saturating addition so burst settling remains both responsive and starvation-bounded near timestamp limits.
- Returned `-1` for clean/no-deadline state, `0` only when due, and ceil-divided every positive nanosecond remainder to at least 1 ms.
- Kept refresh outcomes outside the pure policy: the event-loop coordinator will call the same completion reset after changed, unchanged, or failed refreshes.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- The Linux build image does not contain `clang-format`; repository formatting was applied and dry-run verified with local `xcrun clang-format`, while compilation and all native tests ran through the required Podman Linux router.
- Existing unrelated compiler warnings remained visible during full production compilation; no warning was introduced by the refresh policy and all six native tests passed.

## Known Stubs

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 01-04 can inject monotonic timestamps at the event-loop boundary, merge the policy timeout with scheduled work, and complete one generation after each explicit refresh result.
- The live-X storm scenarios can assert the named 50 ms quiet and 250 ms maximum-latency contract without adding sleeps or policy-only test hooks.
- No blockers remain for dependent topology event integration.

## Self-Check: PASSED

- All three created policy/test files exist.
- RED, GREEN, and production-link commits are present in git history.
- The focused target, three-test topology quick sample, six-test native suite, GPL checks, source-purity scan, and formatting gate passed.

---
*Phase: 01-topology-snapshot-and-live-x-harness*
*Completed: 2026-08-30*
