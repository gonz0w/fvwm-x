---
phase: 01-topology-snapshot-and-live-x-harness
plan: 05
subsystem: topology-inspection
tags: [xrandr, cjson, status-v4, printinfo, asciidoc, tdd]

requires:
  - phase: 01-01
    provides: Canonical immutable topology snapshots, UTF-8 validation, and epoch publication
  - phase: 01-02
    provides: FScreen committed topology snapshot getter and failure-safe live-X publication
provides:
  - Canonical allocation-safe Status v4 topology JSON from the committed snapshot
  - PrintInfo Topology human view from the same immutable epoch and monitor facts
  - Exact schema, order, strict parsing, permutation, and allocation-failure tests
  - User manual contract for topology identity, raw facts, ordering, epochs, and transport
affects: [01-06-live-x-harness, 01-07-xvfb-smoke, 01-08-xorg-dummy, topology-observability]

actuals:
  tokens: 34735
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns: [detached cJSON construction, one-snapshot machine-and-human views, strict versioned inspection contract]

key-files:
  created: []
  modified:
    - libs/monitor_topology.h
    - libs/monitor_topology.c
    - fvwm/builtins.c
    - tests/test_monitor_topology.c
    - tests/meson.build
    - doc/fvwm3_manpage_source.adoc

key-decisions:
  - "Build the complete topology cJSON tree detached and transfer it to the Status parent only after every allocation succeeds."
  - "Status v4 and PrintInfo Topology both read FScreenGetTopologySnapshot(), while existing screen fields and FVWM_STATUS_PIPE transport remain unchanged."
  - "PrintInfo quotes monitor names through cJSON escaping so valid UTF-8 control and quote bytes cannot forge additional log lines."

patterns-established:
  - "Atomic serialization view: failure deletes only the detached topology tree and leaves the caller-owned cJSON parent unchanged."
  - "Canonical inspection: machine JSON and concise human text iterate the already sorted immutable snapshot without consulting monitor_q for topology facts."

requirements-completed: [TOP-01]

coverage:
  - id: D1
    description: "Status v4 exposes exact deterministic topology keys, types, values, sorted monitors/outputs, strict UTF-8 JSON, and allocation-safe ownership."
    requirement: TOP-01
    verification:
      - kind: unit
        ref: "tests/test_monitor_topology.c#test_status_topology_json_schema"
        status: pass
      - kind: unit
        ref: "tests/test_monitor_topology.c#test_topology_json_allocation_failure_is_atomic"
        status: pass
      - kind: unit
        ref: "tests/test_monitor_topology.c#test_topology_json_is_canonical_for_input_permutations"
        status: pass
    human_judgment: false
  - id: D2
    description: "Status and PrintInfo Topology consume one committed snapshot while preserving existing Status fields, FIFO, and newline transport."
    requirement: TOP-01
    verification:
      - kind: integration
        ref: "python3 tests/run_in_linux.py -- sh -ec 'meson compile -C builddir && meson test -C builddir monitor-topology --print-errorlogs'"
        status: pass
      - kind: other
        ref: "source acceptance check for version 4, two FScreenGetTopologySnapshot() calls, topology builder, PrintInfo subject, and FVWM_STATUS_PIPE"
        status: pass
    human_judgment: false
  - id: D3
    description: "The FVWM manual defines the complete Status v4 topology schema, epoch/order semantics, raw pixel/mm facts, and PrintInfo Topology behavior."
    requirement: TOP-01
    verification:
      - kind: integration
        ref: "python3 tests/run_in_linux.py -- sh -ec 'meson setup builddir --reconfigure -Dmandoc=true && meson compile -C builddir && meson test -C builddir monitor-topology --print-errorlogs'"
        status: pass
      - kind: other
        ref: "manual source acceptance check for every schema key and required semantic phrase"
        status: pass
    human_judgment: false

duration: 8min
completed: 2026-08-30
status: complete
---

# Phase 01 Plan 05: Status v4 and Topology Inspection Summary

**Status now emits allocation-safe canonical topology JSON at version 4, while `PrintInfo Topology` reports the same committed epoch and monitor facts through the existing local inspection paths.**

## Performance

- **Duration:** 8 min
- **Started:** 2026-08-30T13:16:48Z
- **Completed:** 2026-08-30T13:24:43Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- Added a deterministic cJSON view builder with exact field order, canonical monitor/output order, detached ownership, and exhaustive allocation-failure containment.
- Upgraded Status to version 4 with `topology.epoch` and complete monitor facts while retaining `current_screen`, `desktop_mode`, `screens`, `FVWM_STATUS_PIPE`, and newline-delimited output.
- Added `PrintInfo Topology` with one escaped epoch header and one deterministic line per monitor from the same committed snapshot getter.
- Documented every schema key plus stable identity, raw server primary, actual root pixels, uncoerced millimetres, complete outputs, ordering, and semantic epoch movement.

## Task Commits

Task 1 followed RED then GREEN, and Task 2 was committed after documentation-enabled verification:

1. **Task 1 RED: Specify Status topology schema and failure ownership** - `fc05ffe8a` (test)
2. **Task 1 GREEN: Publish Status v4 and PrintInfo Topology** - `62bdcdc96` (feat)
3. **Task 2: Document Status v4 and PrintInfo Topology semantics** - `0ffb5f196` (docs)

## Files Created/Modified

- `libs/monitor_topology.h` - Declares the narrow cJSON topology builder over immutable snapshots.
- `libs/monitor_topology.c` - Builds exact nested topology objects off-parent and transfers ownership only on success.
- `fvwm/builtins.c` - Emits Status v4 and escaped human topology lines from `FScreenGetTopologySnapshot()`.
- `tests/test_monitor_topology.c` - Asserts schema/type/key order, canonical bytes, strict parsing, permutation equality, and every serializer allocation failure.
- `tests/meson.build` - Links the vendored cJSON implementation into the direct focused topology test target.
- `doc/fvwm3_manpage_source.adoc` - Defines the public Status v4 and `PrintInfo Topology` contract.

## Decisions Made

- The topology builder constructs a detached tree and adds it to the caller only after all children are complete, making failure atomic without changing cJSON globally.
- Human monitor names use cJSON string escaping before log formatting so readable UTF-8 remains safe on one physical log line.
- The expensive public version transition is limited to Status version 4; no listener, socket, daemon, or module-packet contract was added.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Linked cJSON into the direct monitor-topology test target**
- **Found during:** Task 1 RED (Specify Status topology schema and failure ownership)
- **Issue:** The focused target compiled `monitor_topology.c` directly and did not contain cJSON, so the planned serializer tests and implementation could not link.
- **Fix:** Added the existing vendored `libs/cJSON.c` to that assertion-enabled target exactly once; no dependency or production build change was introduced.
- **Files modified:** `tests/meson.build`
- **Verification:** The RED build reached the expected missing-API failure, then the GREEN target and full six-test suite linked and passed.
- **Committed in:** `fc05ffe8a`

---

**Total deviations:** 1 auto-fixed (1 Rule 3 blocking issue)
**Impact on plan:** The change only completed the existing focused test linkage required by the planned serializer API; feature scope and production dependencies were unchanged.

## Issues Encountered

- The Linux build image does not contain `clang-format`. Native compilation, documentation generation, focused tests, and the full suite ran through `tests/run_in_linux.py`; the repository formatting dry-run passed separately with local `xcrun clang-format`.
- Existing unrelated compiler warnings remained visible during full compilation; no new warning blocked the build or test suite.

## Known Stubs

None.

## Threat Flags

None - server-controlled names remain behind snapshot UTF-8 validation and cJSON escaping, both inspection views expose the same epoch, and no unregistered trust boundary was added.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plans 01-06 through 01-08 can parse real newline-delimited Status v4 records and compare canonical topology facts against nested X server state.
- The live-X harness can invoke `PrintInfo Topology` without adding a test-only production command or changing the module protocol.
- No blockers remain for live topology inspection verification.

## Self-Check: PASSED

- All six modified implementation, test, build, and documentation files exist.
- All three RED, GREEN, and documentation task commits are present in git history.
- Focused, documentation-enabled, full-suite, formatting, source-contract, prohibition, and stub checks passed.

---
*Phase: 01-topology-snapshot-and-live-x-harness*
*Completed: 2026-08-30*
