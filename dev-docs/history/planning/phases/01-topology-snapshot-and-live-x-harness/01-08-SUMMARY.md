---
phase: 01-topology-snapshot-and-live-x-harness
plan: 08
subsystem: live-x-testing
tags: [xorg-dummy, xrandr, status-v4, fault-injection, github-actions, meson, tdd]

requires:
  - phase: 01-06
    provides: Isolated nested-X lifecycle, one-shot query fault mechanisms, bounded teardown, and retained evidence
  - phase: 01-07
    provides: Strict Status v4 comparison, XRandR parsing, and mandatory Xvfb smoke coverage
provides:
  - Authoritative Xorg-dummy topology, canonical equality, burst coalescing, and hard-cap progress coverage
  - Per-invocation failed-query retention and recovery proof using preload with link-wrap fallback
  - Dedicated non-matrix native-Linux live-X CI lane with failure-only evidence upload
  - Ruby YAML syntax and semantic source checker for the live-X workflow contract
affects: [monitor-scaling, topology-hotplug, live-x-integration, native-linux-ci]

actuals:
  tokens: 16895
  tasks: 3
  commits: 7

tech-stack:
  added: [Xorg dummy DDX CI package, actions/upload-artifact@v4]
  patterns: [authoritative queried-state comparison, per-invocation fault selection, bounded event-pressure assertions, syntax-before-semantics workflow validation]

key-files:
  created:
    - tests/live_x/xorg-dummy.conf
  modified:
    - tests/live_x/live_x_harness.py
    - tests/meson.build
    - .github/workflows/ccpp.yml

key-decisions:
  - "Xorg-dummy physical-size authority combines root --fbmm mutation with per-output WIDTH_MM and HEIGHT_MM properties because the dummy DDX does not propagate root millimeters into automatic Monitor facts."
  - "Rapid multi-command transitions briefly suspend FVWM event consumption so each researched operation is observed as one settled semantic generation rather than transient intermediate epochs."
  - "The failed-query test selects preload first and uses a fresh wrapped-FVWM run only when that invocation proves preload cannot intercept the real query."
  - "Live-X CI remains a separate non-matrix Debian-container job and uploads one immutable failure artifact containing harness evidence and Meson logs."

patterns-established:
  - "Authoritative nested-X mutation: query exact server facts first, then require byte/semantic-equivalent Status topology at the stated epoch."
  - "Fault recovery: preserve a copied last-good snapshot across exactly one consumed query fault, require no failed-generation emission, then prove one later recovery commit."
  - "Workflow validation: parse YAML with Ruby before enforcing exact job, package, command, and artifact source contracts."

requirements-completed: [TOP-01, TOP-03, TOP-06]

coverage:
  - id: D1
    description: "Xorg-dummy authoritatively proves output, mode, position, physical-size, primary, user Monitor, automatic Monitor, and membership changes against strict Status v4 facts and canonical epochs."
    requirement: TOP-01
    verification:
      - kind: e2e
        ref: "python3 tests/run_in_linux.py --live-x -- meson test -C builddir topology-xorg-dummy --print-errorlogs"
        status: pass
      - kind: e2e
        ref: "python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs"
        status: pass
    human_judgment: false
  - id: D2
    description: "Canonical membership reorder and duplicate notifications preserve epochs, bursts coalesce once, sustained pressure commits by the hard cap, and one failed query retains the exact last-good topology before one recovery commit."
    requirement: TOP-03
    verification:
      - kind: e2e
        ref: "python3 tests/run_in_linux.py --live-x -- meson test -C builddir topology-xorg-dummy-failed-query --print-errorlogs"
        status: pass
      - kind: integration
        ref: "tests/live_x/live_x_harness.py#run_xorg_dummy_topology and run_xorg_dummy_failed_query"
        status: pass
    human_judgment: false
  - id: D3
    description: "One native-Linux non-matrix CI job installs only the four audited X packages, runs the complete live-X suite once, and retains bounded failure evidence with no secrets."
    requirement: TOP-06
    verification:
      - kind: integration
        ref: "python3 tests/live_x/live_x_harness.py --check-workflow .github/workflows/ccpp.yml"
        status: pass
      - kind: e2e
        ref: "python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs"
        status: pass
    human_judgment: false

duration: 46min
completed: 2026-08-30
status: complete
---

# Phase 01 Plan 08: Authoritative Xorg-Dummy and Native Live-X CI Summary

**Deterministic Xorg-dummy scenarios now prove exact topology epochs, bounded coalescing, failed-query recovery, and the complete nested-X contract in a dedicated native-Linux CI lane.**

## Performance

- **Duration:** 46 min
- **Started:** 2026-08-30T14:43:03Z
- **Completed:** 2026-08-30T15:29:01Z
- **Tasks:** 3
- **Files modified:** 4

## Accomplishments

- Added a private Xorg-dummy fixture and mandatory scenario covering exact output, geometry, physical-size, primary, Monitor lifecycle, membership, canonical equality, rapid burst, and sustained-pressure behavior.
- Added a self-contained failed-query scenario that probes preload before link-wrap, requires the exact retained-topology diagnostic and no failed-generation Status emission, and proves one subsequent recovery commit.
- Added a separate native-Linux live-X CI job that installs only the audited packages, runs the exact Meson suite, and uploads deterministic harness and Meson evidence only on failure.
- Added a standalone workflow checker that runs Ruby YAML parsing before exact semantic assertions for job isolation, packages, commands, existing CI anchors, and artifact settings.

## Task Commits

1. **Task 1 RED: Specify authoritative Xorg-dummy contracts** - `63e3f4863` (test)
2. **Task 1 GREEN: Prove authoritative Xorg topology transitions** - `267355a59` (feat)
3. **Task 2 RED: Specify failed-query recovery contract** - `32ff699ef` (test)
4. **Task 2 GREEN: Prove failed topology query recovery** - `6668108f5` (feat)
5. **Task 3: Add dedicated native live-X gate** - `60d2550b6` (ci)
6. **Task 3 fix: Use the required Ruby YAML parser argv** - `80800fbbb` (fix)

_Note: Tasks 1 and 2 followed the required TDD RED/GREEN cycle._

## Files Created/Modified

- `tests/live_x/xorg-dummy.conf` - Defines deterministic DUMMY0/DUMMY1 modes, virtual framebuffer capacity, and private Xorg-dummy authority without a fixed display or TCP listener.
- `tests/live_x/live_x_harness.py` - Implements authoritative topology/coalescing scenarios, per-invocation failed-query recovery, complete retained evidence, and workflow validation.
- `tests/meson.build` - Registers the authoritative topology and failed-query tests as nonparallel hard-timeout members of the opt-in `live-x` suite.
- `.github/workflows/ccpp.yml` - Adds the dedicated native-Linux live-X lane and bounded failure-only artifact upload while preserving build and sanitizer jobs.

## Decisions Made

- The dummy DDX's output `WIDTH_MM` and `HEIGHT_MM` properties are the queried automatic-Monitor physical-size authority; root `--fbmm` remains part of each root-size transition.
- User span Monitors are removed before automatic-output transitions and restored for canonical-membership tests so they cannot suppress the automatic Monitor facts under test.
- FVWM is briefly stopped while tightly grouped XRandR operations run, allowing the event loop to consume one final settled generation without adding sleeps or blocking work to production.
- Sustained pressure uses an output-less synthetic Monitor so notification pressure remains independent of the automatic DUMMY0/DUMMY1 Monitor set.
- CI validates the entire live-X suite in one dedicated job rather than multiplying nested-X work across the compiler/OS matrix or sanitizer job.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Made automatic Monitor millimeters observable on the dummy DDX**
- **Found during:** Task 1 GREEN authoritative physical-size verification
- **Issue:** Root `xrandr --fbmm` changed root dimensions but did not update the automatic DUMMY Monitor millimeter facts exposed by Xorg-dummy.
- **Fix:** Applied output `WIDTH_MM` and `HEIGHT_MM` properties after each root-size transition and asserted both queried and Status values.
- **Files modified:** `tests/live_x/live_x_harness.py`
- **Verification:** The named Xorg-dummy scenario and complete live-X suite passed.
- **Committed in:** `267355a59`

**2. [Rule 1 - Bug] Prevented user Monitors from hiding automatic output facts**
- **Found during:** Task 1 GREEN output mode/position/primary verification
- **Issue:** Retaining the spanning user Monitor while mutating its member outputs changed which automatic Monitors XRandR exposed, invalidating the intended automatic-fact assertions.
- **Fix:** Removed the spanning Monitor before automatic-output transitions and recreated it only for canonical membership-order coverage.
- **Files modified:** `tests/live_x/live_x_harness.py`
- **Verification:** Every queried DUMMY0/DUMMY1 transition matched strict Status facts at its stated epoch.
- **Committed in:** `267355a59`

**3. [Rule 1 - Bug] Eliminated transient epochs during rapid grouped mutations**
- **Found during:** Task 1 GREEN canonical reorder and burst verification
- **Issue:** FVWM could consume a delete notification before the paired recreation, publishing an intermediate topology and extra epoch.
- **Fix:** Briefly suspended FVWM event consumption while issuing each bounded grouped mutation, then resumed it before polling server and Status state.
- **Files modified:** `tests/live_x/live_x_harness.py`
- **Verification:** Duplicate/reordered operations advanced zero epochs and the rapid burst advanced exactly once.
- **Committed in:** `267355a59`

**4. [Rule 1 - Bug] Kept sustained pressure independent of automatic Monitor membership**
- **Found during:** Task 1 GREEN hard-cap verification
- **Issue:** An output-attached pressure Monitor displaced automatic Monitor facts, coupling the pressure source to the semantic state being measured.
- **Fix:** Used an output-less `none` Monitor for repeated delete/recreate notifications while applying a separate DUMMY position change as the required semantic commit.
- **Files modified:** `tests/live_x/live_x_harness.py`
- **Verification:** Pressure gaps stayed below 50 ms for at least 400 ms and the distinct commit appeared by the 250 ms cap plus tolerance.
- **Committed in:** `267355a59`

**5. [Rule 1 - Bug] Matched the plan-required Ruby YAML argv exactly**
- **Found during:** Final Task 3 contract review
- **Issue:** The first checker implementation invoked `Psych.parse_file` directly rather than the specified `require "yaml"; YAML.parse_file(...)` argv contract.
- **Fix:** Switched the syntax subprocess to the exact required Ruby expression while preserving syntax-before-semantics ordering.
- **Files modified:** `tests/live_x/live_x_harness.py`
- **Verification:** Python compilation, harness self-test, workflow check, and diff checks passed.
- **Committed in:** `80800fbbb`

---

**Total deviations:** 5 auto-fixed (5 Rule 1 bugs)
**Impact on plan:** The fixes made the planned authoritative assertions deterministic on the researched Xorg-dummy implementation without changing production behavior or broadening scope.

## Issues Encountered

- Local macOS verification repeatedly provisions the audited X packages in fresh Podman containers. The first complete live-X attempt exceeded the 180-second wrapper timeout during compilation; rerunning with the completed incremental build and a larger bound produced four passing live-X tests.
- The full native build displayed existing unrelated warnings in legacy files. No task-owned warning or test failure remained; all seven native tests and all four live-X tests passed.

## Known Stubs

None.

## Threat Flags

None - nested server isolation, one-shot fault scoping, event-pressure bounds, teardown deadlines, and CI evidence retention were all registered in the plan threat model and received their specified mitigations.

## User Setup Required

None - CI installs the audited nested-X dependencies inside the existing build container.

## Next Phase Readiness

- TOP-01, TOP-03, and TOP-06 now have native policy coverage plus Xvfb smoke and mandatory Xorg-dummy end-to-end authority.
- The complete Phase 1 topology foundation is ready for milestone verification and subsequent monitor-scale policy work.
- No blockers remain.

## Self-Check: PASSED

- All four implementation files and this Summary exist on disk.
- Task commits `63e3f4863`, `267355a59`, `32ff699ef`, `6668108f5`, `60d2550b6`, and `80800fbbb` are present in git history.
- The workflow checker and diff integrity check pass after the final task commit.

---
*Phase: 01-topology-snapshot-and-live-x-harness*
*Completed: 2026-08-30*
