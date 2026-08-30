---
phase: 01-topology-snapshot-and-live-x-harness
plan: 01
subsystem: monitor-topology
tags: [xrandr, c11, meson, tdd, podman]

requires: []
provides:
  - Detached, owned RandR Monitor snapshot model with canonical ordering
  - Atomic epoch publication with semantic equality and last-good retention
  - Recoverable allocation seam and malformed-fact validation coverage
  - Native-Linux verification router for Linux and Darwin Podman hosts
affects: [01-02-fscreen-adapter, 01-05-topology-inspection, live-x-verification]

actuals:
  tokens: 10412
  tasks: 3
  commits: 7

tech-stack:
  added: [Python standard-library verification router]
  patterns: [detached candidate publication, canonical set semantics, recoverable allocator injection]

key-files:
  created:
    - tests/run_in_linux.py
    - libs/monitor_topology.h
    - libs/monitor_topology.c
    - tests/test_monitor_topology.c
  modified:
    - .gitignore
    - libs/meson.build
    - tests/meson.build

key-decisions:
  - "Canonical snapshots retain RandR Atom/name identity, root-pixel facts, and sorted deduplicated output membership."
  - "Pixel spans that overflow their integer coordinate representation are invalid, while zero or implausibly large physical millimetres remain unmodified facts."
  - "Darwin verification copies read-only source into a case-sensitive Podman volume while preserving the reusable build directory."

patterns-established:
  - "Detached candidate: deep-copy and validate every fact before a single committed-pointer swap."
  - "Failure retention: malformed input, epoch overflow, and every injected allocation failure preserve the previous pointer and epoch."
  - "Native routing: execute directly on Linux and through an amd64 Podman image on Darwin without interpolating caller argv."

requirements-completed: [TOP-01, TOP-03]

coverage:
  - id: D1
    description: "Owned canonical topology snapshots publish initially at epoch 1 and advance exactly once for semantic change."
    requirement: TOP-01
    verification:
      - kind: unit
        ref: "tests/test_monitor_topology.c#test_first_publication_has_epoch_one"
        status: pass
      - kind: unit
        ref: "tests/test_monitor_topology.c#test_equal_publication_preserves_pointer_and_epoch"
        status: pass
    human_judgment: false
  - id: D2
    description: "Malformed facts, unsafe geometry, invalid UTF-8, epoch overflow, and recoverable allocation failures cannot replace last-good topology."
    requirement: TOP-03
    verification:
      - kind: unit
        ref: "tests/test_monitor_topology.c#test_malformed_facts_leave_committed_snapshot_untouched"
        status: pass
      - kind: unit
        ref: "tests/test_monitor_topology.c#test_every_allocator_failure_is_recoverable"
        status: pass
    human_judgment: false
  - id: D3
    description: "The topology model is linked once per production/test target and verified through the native-Linux command router."
    requirement: TOP-01
    verification:
      - kind: integration
        ref: "python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs"
        status: pass
      - kind: unit
        ref: "python3 tests/run_in_linux.py --self-test"
        status: pass
    human_judgment: false

duration: 32min
completed: 2026-08-30
status: complete
---

# Phase 01 Plan 01: Topology Snapshot Domain Tracer Summary

**Immutable RandR Monitor facts now flow through checked ownership, canonical set comparison, and atomic epoch publication with exhaustive native failure coverage.**

## Performance

- **Duration:** 32 min
- **Started:** 2026-08-30T12:03:30Z
- **Completed:** 2026-08-30T12:35:32Z
- **Tasks:** 3
- **Files modified:** 7

## Accomplishments

- Added an owned topology model that deep-copies monitor names and complete output membership, canonicalizes order, and publishes one immutable epoch at a time.
- Enforced structural, UTF-8, identity, geometry, allocation, and epoch-overflow failure boundaries while preserving zero and extreme physical dimensions unchanged.
- Added TDD coverage for all semantic fields, order independence, duplicate output normalization, last-good retention, and every recoverable allocation index.
- Added a Linux/Darwin native verification router and registered the retained model in both production and assertion-enabled test build graphs.

## Task Commits

Each task was committed atomically, with TDD tasks split into test and implementation commits:

1. **Task 1: Trace detached monitor facts to one committed epoch** - `ad9434987` (test), `b3f1aff11` (feat)
2. **Task 2: Close malformed-input, identity, ordering, and allocation edges** - `30d89c1a5` (test), `aa8409ea3` (test), `17ce357c5` (feat)
3. **Task 3: Register the retained tracer in the production build graph** - `56a40d073` (chore)
4. **Post-verification correctness fix** - `69b1618bf` (fix)

## Files Created/Modified

- `tests/run_in_linux.py` - Routes native commands directly on Linux or through a clean case-sensitive amd64 Podman workspace on Darwin.
- `libs/monitor_topology.h` - Declares owned monitor/snapshot facts, allocator injection, publication results, and lifecycle APIs.
- `libs/monitor_topology.c` - Implements checked deep copy, UTF-8 validation, canonical equality, epoch publication, and cleanup.
- `tests/test_monitor_topology.c` - Exercises publication, malformed facts, semantic equality, raw-mm preservation, overflow, and exhaustive allocation failure.
- `tests/meson.build` - Registers the assertion-enabled `monitor-topology` executable.
- `libs/meson.build` - Links `monitor_topology.c` into the production static library exactly once.
- `.gitignore` - Ignores Python bytecode cache output from verification.

## Decisions Made

- Stable identity remains the RandR Monitor Atom plus its resolved UTF-8 name; outputs are sorted membership facts rather than identity.
- Root-pixel coordinate spans must remain representable, but physical millimetre values are preserved without DPI plausibility policy.
- Darwin uses a read-only host mount plus a case-sensitive staging volume because direct case-insensitive mounts misresolve `strings.h` against `libs/Strings.h`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Staged Darwin builds in a case-sensitive Podman volume**
- **Found during:** Task 1 (Trace detached monitor facts to one committed epoch)
- **Issue:** A direct macOS bind-mounted worktree caused case-insensitive `strings.h`/`libs/Strings.h` collisions and blocked the native build.
- **Fix:** Mounted source read-only and copied it into a case-sensitive named volume used as the container working directory.
- **Files modified:** `tests/run_in_linux.py`
- **Verification:** Router self-tests, full Meson compilation, and native tests passed under Podman `linux/amd64`.
- **Committed in:** `ad9434987`

**2. [Rule 1 - Bug] Made partial-allocation destruction tolerate a missing monitor array**
- **Found during:** Task 2 (Close malformed-input, identity, ordering, and allocation edges)
- **Issue:** Failing the monitor-array allocation left a valid snapshot shell whose destructor dereferenced a NULL array.
- **Fix:** Guarded recursive monitor cleanup while still releasing the array and snapshot shell through the selected allocator.
- **Files modified:** `libs/monitor_topology.c`
- **Verification:** Every allocation failure index completes without a crash or leak in `monitor-topology`.
- **Committed in:** `17ce357c5`

**3. [Rule 1 - Bug] Removed stale staged sources before Darwin verification**
- **Found during:** Overall verification
- **Issue:** Reusing the case-sensitive volume could retain a source file deleted from the host and produce a stale build.
- **Fix:** Clear staged source entries before each copy while preserving only `builddir`, and assert cleanup ordering in the router self-test.
- **Files modified:** `tests/run_in_linux.py`
- **Verification:** Router self-tests and the wrapped `monitor-topology` target passed after a fresh staged copy.
- **Committed in:** `69b1618bf`

---

**Total deviations:** 3 auto-fixed (2 Rule 1 bugs, 1 Rule 3 blocking issue)
**Impact on plan:** All fixes were required for reliable ownership cleanup and reproducible native verification; no feature scope was added.

## Issues Encountered

- The amd64 Go toolchain crashed under local QEMU emulation, so the reusable local build directory was configured with `-Dgolang=disabled`; the C production and test graphs compiled successfully.
- The build image does not contain `clang-format`; the same repository formatting gate passed with local `xcrun clang-format`, while compilation and tests remained inside the required Linux route.

## Known Stubs

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 01-02 can adapt one `XRRGetMonitors` reply into this detached candidate API and retain the committed snapshot on query or staging failure.
- Later Status and live-X plans can consume deterministic monitor/output ordering and epoch semantics without redefining topology identity.

## Self-Check: PASSED

- All created implementation and test files exist.
- All seven task and correctness-fix commits are present in git history.

---
*Phase: 01-topology-snapshot-and-live-x-harness*
*Completed: 2026-08-30*
