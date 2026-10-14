---
phase: 01-topology-snapshot-and-live-x-harness
plan: 07
subsystem: live-x-testing
tags: [xvfb, xrandr, status-v4, meson, capability-detection, tdd]

requires:
  - phase: 01-06
    provides: Isolated live-X process ownership, Status collection, evidence capture, and bounded teardown
  - phase: 01-05
    provides: Status v4 topology JSON and PrintInfo Topology inspection contract
provides:
  - Mandatory Xvfb baseline proving RandR 1.5+, strict Status epoch 1, PrintInfo Topology, and no false epoch
  - Observed-state XRandR Monitor create, mutate, and remove coverage with exact Status epoch/fact matching
  - Explicit unsupported capability result that retains all mandatory baseline and unchanged-state assertions
  - Nonparallel hard-timeout topology-xvfb Meson integration test
affects: [01-08-xorg-dummy, live-x-integration, topology-hotplug, monitor-reconciliation]

actuals:
  tokens: 7311
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns: [queried-state capability detection, strict Status-to-RandR comparison, bounded transition polling, transient-event coalescing]

key-files:
  created: []
  modified:
    - tests/live_x/live_x_harness.py
    - tests/meson.build

key-decisions:
  - "Xvfb Monitor mutation support is established only when repeated xrandr --listmonitors queries expose the requested Monitor; command exit status is retained solely as evidence."
  - "The literal delete/recreate mutation is presented to FVWM as one settled server transition by briefly stopping event consumption until both argv operations complete."
  - "An unsupported Xvfb still receives every literal create/delete/recreate/delete command and must retain the exact baseline Status record and epoch."

patterns-established:
  - "Observed-state gate: issue an argv-only mutation, poll the authoritative nested-X state, then assert the corresponding Status epoch and facts."
  - "Capability narrowing: only absent creation may report unsupported; every missing transition after observed creation is a hard failure."

requirements-completed: [TOP-01, TOP-03, TOP-06]

coverage:
  - id: D1
    description: "The always-running Xvfb scenario proves isolated startup, RandR 1.5+, strict Status v4 epoch 1, PrintInfo Topology, no false epoch, retained diagnostics, and bounded cleanup."
    requirement: TOP-01
    verification:
      - kind: e2e
        ref: "python3 tests/run_in_linux.py --live-x -- meson test -C builddir topology-xvfb --print-errorlogs"
        status: pass
    human_judgment: false
  - id: D2
    description: "Literal XRandR Monitor create, delete/recreate mutation, and removal are classified from queried state and matched to exact Status facts and epochs when supported."
    requirement: TOP-03
    verification:
      - kind: integration
        ref: "tests/live_x/live_x_harness.py#run_xvfb_topology supported mutation path"
        status: pass
      - kind: e2e
        ref: "python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs"
        status: pass
    human_judgment: false
  - id: D3
    description: "Mutation capability never trusts xrandr exit status, and the unsupported branch attempts every operation while requiring the full baseline Status record to remain unchanged."
    requirement: TOP-06
    verification:
      - kind: unit
        ref: "python3 tests/live_x/live_x_harness.py --self-test"
        status: pass
      - kind: integration
        ref: "tests/live_x/live_x_harness.py#poll_xrandr_monitors and unsupported branch assertions"
        status: pass
    human_judgment: false

duration: 21min
completed: 2026-08-30
status: complete
---

# Phase 01 Plan 07: Capability-Aware Xvfb Topology Smoke Summary

**A mandatory Xvfb scenario now proves strict live Status/PrintInfo topology behavior and validates Monitor create, settled mutation, and removal solely from queried RandR state.**

## Performance

- **Duration:** 21 min
- **Started:** 2026-08-30T14:13:46Z
- **Completed:** 2026-08-30T14:34:31Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Added strict RandR version, Monitor, and output-XID parsers that compare the queried Xvfb baseline directly with Status v4 epoch 1 and `PrintInfo Topology` output.
- Added literal Monitor create, delete/recreate mutation, and removal operations with two-second monotonic predicates and exact supported-path Status epochs 2, 3, and 4.
- Added the narrow unsupported branch: all operation argv vectors still run, queried state and the complete Status record must remain at baseline, and exactly one `UNSUPPORTED: xvfb-monitor-mutation` record is emitted.
- Registered `topology-xvfb` as a nonparallel, 45-second, build-tree-artifact test in the opt-in `live-x` suite.

## Task Commits

1. **Task 1 RED: Specify mandatory Xvfb baseline contract** - `653b250d6` (test)
2. **Task 1 GREEN: Prove and register mandatory Xvfb baseline** - `2e8c584dc` (feat)
3. **Task 2: Add observed-state create, mutate, and remove coverage** - `96c16759a` (feat)

_Note: Task 1 followed the required TDD RED/GREEN cycle._

## Files Created/Modified

- `tests/live_x/live_x_harness.py` - Parses authoritative RandR state, proves the strict Xvfb baseline, classifies mutation capability, validates exact Status transitions, and retains bounded evidence.
- `tests/meson.build` - Registers the nonparallel `topology-xvfb` live-X scenario with a hard timeout and deterministic artifact directory.

## Decisions Made

- Capability is true only after a repeated `xrandr --listmonitors` predicate observes `fvwm-xvfb-probe`; return codes are recorded but never used as proof.
- Once creation is observed, mutation or removal failure cannot downgrade to unsupported and instead fails the scenario with retained diagnostics.
- FVWM is briefly stopped while the researched delete/recreate pair runs so it consumes the final settled server topology as one mutation generation rather than committing the transient deletion.
- Unsupported behavior preserves the complete baseline Status JSON record, not only its topology subsection, preventing unrelated Status changes from passing unnoticed.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Deferred initial Status activation until the FIFO reader was ready**
- **Found during:** Task 1 GREEN live-X execution
- **Issue:** Immediate startup `Status On` could emit epoch 1 before the harness attached its FIFO reader, causing a false baseline timeout.
- **Fix:** Started Status through `Schedule 0 Status On` while retaining `PrintInfo Topology` in the startup command path.
- **Files modified:** `tests/live_x/live_x_harness.py`
- **Verification:** The named Xvfb scenario and full live-X suite received and validated strict epoch 1.
- **Committed in:** `2e8c584dc`

**2. [Rule 1 - Bug] Prevented the delete/recreate pair from committing a transient extra epoch**
- **Found during:** Task 2 supported mutation verification
- **Issue:** FVWM sometimes consumed the deletion before recreation, publishing a transient epoch and shifting the final mutation beyond the required `E+2` contract.
- **Fix:** Briefly sent FVWM `SIGSTOP`, issued both literal xrandr argv operations, and sent `SIGCONT` before polling the settled query and Status state.
- **Files modified:** `tests/live_x/live_x_harness.py`
- **Verification:** The named test passed twice through the wrapped named/suite runs, and all nine configured Meson tests passed.
- **Committed in:** `96c16759a`

**3. [Rule 1 - Bug] Removed stale retained artifacts before each scenario invocation**
- **Found during:** Task 2 repeated live-X verification
- **Issue:** A successful rerun could leave an earlier `failure.json` or copied log in the deterministic artifact directory, making current evidence ambiguous.
- **Fix:** Removed only the scenario-owned retained artifact names before creating a new isolated runtime.
- **Files modified:** `tests/live_x/live_x_harness.py`
- **Verification:** Repeated named and suite executions completed with current `result.json` evidence.
- **Committed in:** `96c16759a`

---

**Total deviations:** 3 auto-fixed (3 Rule 1 bugs)
**Impact on plan:** All fixes enforce the specified deterministic evidence and epoch contract without changing production code or broadening the live-X scope.

## Issues Encountered

- XRandR sends separate notifications for the literal delete and recreate commands. Allowing FVWM to consume the intermediate notification produced an extra committed epoch; bounded event-consumption suspension made the required settled-operation semantics deterministic.
- The routed Linux build displayed existing unrelated compiler warnings. No new compile or test failures remained; all nine configured Meson tests passed.

## Known Stubs

None.

## Threat Flags

None - all process execution, observed-state capability, nested-X network isolation, and bounded-cleanup surfaces were registered in the plan threat model and received their specified mitigations.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 01-08 can reuse the strict RandR parsers, exact Status comparison helpers, mutation polling, evidence lifecycle, and nonparallel nested-X registration pattern for Xorg-dummy.
- The Xvfb baseline now runs on every enabled live-X suite invocation and provides a stable regression floor independent of optional Monitor mutation support.
- No blockers remain for the next plan.

## Self-Check: PASSED

- Both modified files and this Summary exist on disk.
- Task commits `653b250d6`, `2e8c584dc`, and `96c16759a` are present in git history.
- Python compilation/self-tests, the named Xvfb test, the full live-X suite, and all nine configured Meson tests passed.

---
*Phase: 01-topology-snapshot-and-live-x-harness*
*Completed: 2026-08-30*
