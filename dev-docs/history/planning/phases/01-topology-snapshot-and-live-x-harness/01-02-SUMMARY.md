---
phase: 01-topology-snapshot-and-live-x-harness
plan: 02
subsystem: monitor-topology
tags: [xrandr, c11, meson, tdd, atomic-publication]

requires:
  - phase: 01-01
    provides: Detached canonical RandR Monitor snapshots and atomic epoch publication
provides:
  - Sole live-X query adapter that publishes complete immutable monitor snapshots
  - Explicit failed, unchanged, and changed refresh outcomes with last-good retention
  - Allocation-staged legacy monitor reconciliation with stable surviving pointers
  - Complete multi-output lookup through committed topology membership
affects: [01-03-event-settling, 01-04-topology-events, 01-05-topology-inspection, 01-06-live-x-harness]

actuals:
  tokens: 21440
  tasks: 2
  commits: 4

tech-stack:
  added: []
  patterns: [call-scoped X adapter, pre-publication allocation stage, allocation-free compatibility reconciliation]

key-files:
  created:
    - tests/test_fscreen_topology.c
  modified:
    - libs/FScreen.h
    - libs/FScreen.c
    - tests/meson.build

key-decisions:
  - "FScreen performs exactly one active XRRGetMonitors query per refresh and publishes only a fully copied and validated candidate."
  - "All new legacy monitor, screen, and name allocations complete before the committed snapshot pointer changes."
  - "The committed snapshot owns raw primary and complete output facts; legacy synthetic primary and first-output fields remain compatibility-only derivations."

patterns-established:
  - "Last-good live adapter: query, resolve, copy, validate, stage, and only then publish one immutable candidate."
  - "Stable compatibility view: reconcile existing monitor nodes by Atom/name identity without allocating after publication."

requirements-completed: [TOP-01, TOP-03]

coverage:
  - id: D1
    description: "FScreen startup and runtime refreshes publish complete detached RandR Monitor facts with exact generation outcomes and last-good retention."
    requirement: TOP-01
    verification:
      - kind: unit
        ref: "tests/test_fscreen_topology.c#test_failed_empty_unresolved_and_invalid_queries_retain_state"
        status: pass
      - kind: unit
        ref: "tests/test_fscreen_topology.c#test_equal_and_changed_candidates_have_exact_generation_results"
        status: pass
    human_judgment: false
  - id: D2
    description: "Every fallible legacy allocation is staged before publication, and failures preserve the exact committed pointer, epoch, and legacy view."
    requirement: TOP-03
    verification:
      - kind: unit
        ref: "tests/test_fscreen_topology.c#test_every_legacy_staging_allocation_failure_retains_state"
        status: pass
    human_judgment: false
  - id: D3
    description: "Stable legacy nodes derive complete multi-output membership from the committed snapshot and survive change or disable reconciliation."
    requirement: TOP-01
    verification:
      - kind: integration
        ref: "python3 tests/run_in_linux.py -- sh -ec 'meson compile -C builddir && meson test -C builddir --print-errorlogs'"
        status: pass
    human_judgment: false

duration: 18min
completed: 2026-08-30
status: complete
---

# Phase 01 Plan 02: Failure-Safe Live-X Snapshot Publisher Summary

**FScreen now turns one live RandR Monitor query into a complete immutable epoch while staged legacy reconciliation preserves last-good state and stable monitor pointers.**

## Performance

- **Duration:** 18 min
- **Started:** 2026-08-30T12:44:08Z
- **Completed:** 2026-08-30T13:02:18Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- Replaced direct live-X monitor mutation with one query, ownership-safe copy, validation, staging, and publication path exposed through explicit refresh results.
- Preserved the exact committed snapshot pointer and epoch for query, name-resolution, candidate-validation, and every injected legacy-allocation failure.
- Reconciled stable legacy monitor nodes by Atom/name identity only after all allocations succeed, including deterministic disable and compatibility-primary behavior.
- Made output lookup use every sorted output member in the committed snapshot rather than only the legacy first-output field.

## Task Commits

Each TDD task was committed atomically as a failing test followed by its implementation:

1. **Task 1: Query and publish one complete live RandR candidate** - `c3514008d` (test), `d28676961` (feat)
2. **Task 2: Reconcile the stable legacy monitor view without split-brain state** - `f35314800` (test), `43cc870bb` (feat)

## Files Created/Modified

- `tests/test_fscreen_topology.c` - Exercises one-query ownership, failure retention, generation results, exhaustive staging failures, pointer stability, disable behavior, and multi-output lookup.
- `libs/FScreen.h` - Declares the read-only snapshot getter, refresh outcomes, complete-output lookup, and call-scoped test adapters.
- `libs/FScreen.c` - Implements the sole live query path, startup/runtime publication boundaries, detached legacy staging, and allocation-free reconciliation.
- `tests/meson.build` - Registers the assertion-enabled `fscreen-topology-adapter` native test.

## Decisions Made

- Kept all X reply and Atom-name ownership inside one call-scoped adapter invocation so no borrowed X memory reaches committed state.
- Used Monitor Atom plus resolved name as compatibility identity, preserving existing `struct monitor *` addresses for surviving identities.
- Retained complete raw output and primary facts only in the committed snapshot; the legacy view receives the first sorted output and synthetic-primary fallback solely for existing consumers.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- The Linux build image continues to omit `clang-format`; formatting was verified with repository-configured local `xcrun clang-format`, while compilation and all native tests ran through the required Podman Linux wrapper.
- Existing unrelated compiler warnings were observed during the full build; no new warning blocked compilation or the test suite.
- The state progress helper declined an unscoped phase update; completed-plan counts and derived progress metrics were reconciled from the two on-disk summaries.

## Known Stubs

None.

## Threat Flags

None - the X-server input and publication/reconciliation trust boundaries were already registered in the plan threat model and were covered by failure-injection tests.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 01-03 can settle event bursts against explicit failed, unchanged, and changed refresh outcomes without redefining snapshot ownership.
- Plans 01-04 through 01-06 can consume one stable topology epoch and test the real X boundary while legacy consumers retain their existing monitor pointers.
- No blockers remain for the next wave.

## Self-Check: PASSED

- All four implementation and test files exist.
- All four TDD task commits are present in git history.
- The focused adapter test and full five-test native suite pass through the Linux wrapper.

---
*Phase: 01-topology-snapshot-and-live-x-harness*
*Completed: 2026-08-30*
