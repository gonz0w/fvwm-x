---
phase: 01-topology-snapshot-and-live-x-harness
plan: 06
subsystem: live-x-testing
tags: [xvfb, xrandr, ld-preload, linker-wrap, meson, process-isolation, tdd]

requires:
  - phase: 01-04
    provides: Deferred bounded topology refresh and changed-only commit sequencing
  - phase: 01-05
    provides: Status v4 topology JSON and committed snapshot inspection contract
provides:
  - Dependency-free Python live-X process, readiness, Status, evidence, and teardown primitives
  - Dual-mode one-shot XRRGetMonitors fault shim for preload and linker wrapping
  - Executable Xvfb probe that validates production preload interposition or the test-only wrap fallback
  - Default-disabled Linux-only Meson live-X targets with no installed fault surface
affects: [01-07-xvfb-smoke, 01-08-xorg-dummy, live-x-integration, topology-failure-recovery]

actuals:
  tokens: 9138
  tasks: 2
  commits: 4

tech-stack:
  added: []
  patterns: [monotonic bounded process ownership, displayfd allocation, per-invocation one-shot fault probing, test-only dual-mode C shim]

key-files:
  created:
    - tests/live_x/live_x_harness.py
    - tests/live_x/xrr_query_fault.c
  modified:
    - tests/meson.build
    - meson.options

key-decisions:
  - "Only the test shim reads and atomically unlinks FVWM_TEST_XRR_FAIL_FILE; production source and ordinary FVWM environments expose no fault switch."
  - "Each fault-bearing invocation probes LD_PRELOAD against the actual production FVWM executable first and runs the non-installed linker-wrap executable only when preload is not proven."
  - "Nested X processes use display-fd allocation, disabled TCP, isolated sessions, monotonic deadlines, and bounded reverse-order terminate/wait/kill cleanup."

patterns-established:
  - "Owned live-X lifecycle: every child receives an argv list, private TMPDIR, captured log, fresh session, and bounded teardown responsibility."
  - "Independent fault selection: retained metadata reports one mechanism for the current invocation and is never consumed as durable configuration by another test."

requirements-completed: [TOP-03, TOP-06]

coverage:
  - id: D1
    description: "The dependency-free live-X harness owns display allocation, child sessions, readiness predicates, Status parsing, evidence, and bounded reverse teardown."
    requirement: TOP-06
    verification:
      - kind: unit
        ref: "python3 tests/live_x/live_x_harness.py --self-test"
        status: pass
      - kind: integration
        ref: "python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs"
        status: pass
    human_judgment: false
  - id: D2
    description: "An actual production FVWM process proves one preload XRRGetMonitors failure, with a built non-installed linker-wrap fallback and no ordinary production fault environment."
    requirement: TOP-03
    verification:
      - kind: e2e
        ref: "meson test -C builddir topology-fault-injection-probe --print-errorlogs"
        status: pass
      - kind: integration
        ref: "dual FVWM_XRR_QUERY_FAULT_PRELOAD/FVWM_XRR_QUERY_FAULT_WRAP syntax compilation and Xcode clang-format dry-run"
        status: pass
    human_judgment: false

duration: 17min
completed: 2026-08-30
status: complete
---

# Phase 01 Plan 06: Isolated Live-X Harness and Fault Probe Summary

**A dependency-free, deadline-bounded Xvfb harness now proves one-shot `XRRGetMonitors` failure against actual FVWM through preload, with an executable test-only linker-wrap fallback and no production fault control.**

## Performance

- **Duration:** 17 min
- **Started:** 2026-08-30T13:49:35Z
- **Completed:** 2026-08-30T14:06:02Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- Added argv-only process ownership with private runtime roots, sanitized environments, isolated process sessions, captured logs, monotonic predicates, and bounded reverse terminate/wait/kill teardown.
- Added collision-safe `-displayfd` nested-X startup with `-nolisten tcp`, RandR readiness, nonblocking Status JSON-line collection, strict topology accessors, and pre-cleanup evidence retention.
- Added one C shim source that rejects ambiguous compile modes, atomically consumes one private sentinel, and supports both `LD_PRELOAD` delegation and `__wrap_XRRGetMonitors` delegation.
- Added a default-disabled Linux-only Meson suite that builds no installed shim target, probes actual production FVWM first, automatically retains the wrap fallback, and proves an ordinary production startup receives no fault controls.

## Task Commits

Both tasks followed RED then GREEN:

1. **Task 1 RED: Specify live-X lifecycle contracts** - `6c9095fbc` (test)
2. **Task 1 GREEN: Implement isolated lifecycle primitives and dual-mode shim** - `9702ce3c0` (feat)
3. **Task 2 RED: Specify one-mechanism probe metadata** - `6ba611d97` (test)
4. **Task 2 GREEN: Probe production preload and activate wrap fallback** - `01ec34ee6` (feat)

## Files Created/Modified

- `tests/live_x/live_x_harness.py` - Owns child sessions, display-fd startup, deadlines, xrandr readiness, Status records, fault probing, diagnostics, and cleanup.
- `tests/live_x/xrr_query_fault.c` - Provides mutually exclusive preload and linker-wrap XRR query interception with one atomic sentinel claim.
- `tests/meson.build` - Builds Linux-only non-installed fault targets and registers the nonparallel hard-timeout live-X probe.
- `meson.options` - Adds the default-false `live-x-tests` build gate.

## Decisions Made

- Fault behavior remains entirely in the test shim: the spawned fault process alone receives `FVWM_TEST_XRR_FAIL_FILE`, while environment sanitation removes both that key and inherited `LD_PRELOAD` from ordinary children.
- A missing sentinel is necessary but insufficient proof; the probe also requires FVWM exit 101 and exactly one FScreen failed-query diagnostic before selecting a mechanism.
- The production executable is always probed first with preload. The wrapped executable is eligible only as a same-invocation fallback and is never installed.
- Ordinary production readiness is proven by successful Status FIFO creation plus a live process, avoiding dependence on a second unsolicited Status record after the FIFO reader attaches.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Rebased wrapped FVWM sources for the tests subdirectory**
- **Found during:** Task 2 GREEN (Probe preload against FVWM and activate the fallback)
- **Issue:** Reusing root-relative `fvwm_sources` directly in `tests/meson.build` made Meson resolve entries below `tests/fvwm/` and fail configuration.
- **Fix:** Derived test-subdirectory paths with a `../` prefix before constructing the non-installed wrapped target.
- **Files modified:** `tests/meson.build`
- **Verification:** `meson setup --reconfigure -Dlive-x-tests=true` and the complete wrapped FVWM build passed.
- **Committed in:** `01ec34ee6`

**2. [Rule 1 - Bug] Corrected ordinary FVWM readiness after Status FIFO attachment**
- **Found during:** Task 2 GREEN live-X execution
- **Issue:** The first probe waited for another Status JSON record after opening the already-created FIFO, but startup need not generate a second state change, causing a false timeout despite a healthy FVWM process.
- **Fix:** Treat successful Status FIFO creation and a still-live production process as the ordinary-startup proof; strict Status JSON predicates remain available for scenarios that trigger emissions.
- **Files modified:** `tests/live_x/live_x_harness.py`
- **Verification:** The named probe and complete `live-x` Meson suite passed under routed Xvfb.
- **Committed in:** `01ec34ee6`

---

**Total deviations:** 2 auto-fixed (1 Rule 3 blocking issue, 1 Rule 1 bug)
**Impact on plan:** Both fixes were confined to test build-path correctness and readiness semantics; production code, fault isolation, and planned feature scope were unchanged.

## Issues Encountered

- The Linux build image does not contain `clang-format`. Both C compile modes were verified in Linux, and the repository formatting dry-run passed separately with Xcode's `xcrun clang-format`.
- Existing unrelated compiler warnings remained visible during the full build; the new targets compiled and the live-X suite passed.

## Known Stubs

None.

## Threat Flags

None - process execution, nested-X network isolation, teardown bounds, and the test-to-production fault boundary were all registered in the plan threat model and received their specified mitigations.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 01-07 can reuse the owned Xvfb lifecycle, Status collector, strict topology accessors, xrandr predicates, and retained evidence path.
- Plan 01-08 can add its Xorg-dummy backend while preserving the same display-fd, no-TCP, session, deadline, and cleanup contract.
- Fault-bearing recovery scenarios can independently call the proven per-invocation preload/wrap selector without shared selection state.

## Self-Check: PASSED

- Both created files and both modified build files exist.
- All four Task 1/Task 2 RED and GREEN commits are present in git history.
- Python self-tests, dual-mode C syntax checks, Xcode formatting, the named live-X probe, and the full live-X suite passed.

---
*Phase: 01-topology-snapshot-and-live-x-harness*
*Completed: 2026-08-30*
