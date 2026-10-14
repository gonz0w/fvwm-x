---
phase: 02-scale-policy-and-configuration
plan: 02
subsystem: monitor-scale-configuration
tags: [xrandr, hidpi, configuration, transactional-store, c11, meson]

requires:
  - phase: 02-scale-policy-and-configuration
    plan: 01
    provides: exact quarter-step scale factors and committed monitor topology facts
provides:
  - Transactional exact-name monitor scale overrides
  - Independent global scale fallback configuration
  - Strict MonitorScale and MonitorScaleFallback command callbacks
  - Safe diagnostics and production lifecycle ownership
affects: [02-03-scale-recompute, 02-04-scale-observability, configuration, monitor-policy]

actuals:
  tokens: 6929
  tasks: 3
  commits: 6

tech-stack:
  added: []
  patterns: [transactional candidate replacement, injected allocator and diagnostic seams, exact owned-token parsing, TDD red-green commits]

key-files:
  created: [fvwm/monitor_scale_config.h, fvwm/monitor_scale_config.c, tests/test_monitor_scale_config.c]
  modified: [fvwm/commands.h, fvwm/functable.c, fvwm/fvwm3.c, fvwm/meson.build, tests/meson.build]

key-decisions:
  - "Treat RandR Monitor names as exact case-sensitive bytes; numeric and g/c/p tokens receive no legacy alias interpretation."
  - "Stage complete replacement entries before mutating the authoritative store so invalid input and allocation failure retain prior policy."
  - "Encode control and backslash bytes before diagnostics cross the logging seam, while retaining the original exact name in policy storage."

patterns-established:
  - "Configuration callbacks acquire the sole file-private store through monitor_scale_config_get() and delegate mutation to transactional helpers."
  - "User-controlled command tokens are fully acquired, exact-count validated, and released on every success or rejection path."

requirements-completed: [SCALE-01, SCALE-04]

coverage:
  - id: D1
    description: "MonitorScale stores last-valid canonical factors under exact case-sensitive RandR Monitor names and reports none, unique, or ambiguous active matches."
    requirement: SCALE-01
    verification:
      - kind: unit
        ref: "tests/test_monitor_scale_config.c; meson test -C builddir monitor-scale-config"
        status: pass
    human_judgment: false
  - id: D2
    description: "MonitorScaleFallback stores one independent canonical global factor while malformed input and allocation failures preserve previous policy bytes."
    requirement: SCALE-04
    verification:
      - kind: unit
        ref: "tests/test_monitor_scale_config.c; full native Meson suite"
        status: pass
    human_judgment: false
  - id: D3
    description: "Both directives are dispatched by fvwm3 with strict arity, finite factor grammar, owned-token cleanup, and safely encoded rejection diagnostics."
    requirement: SCALE-01
    verification:
      - kind: integration
        ref: "explicit ./fvwm3:executable compile plus full native Meson suite"
        status: pass
    human_judgment: false

duration: 15min
completed: 2026-08-30
status: complete
---

# Phase 2 Plan 2: Transactional Monitor Scale Configuration Summary

**Strict MonitorScale directives now retain exact-name overrides and an independent fallback transactionally, with safe diagnostics and singular production linkage.**

## Performance

- **Duration:** 15 min
- **Started:** 2026-08-30T19:27:56Z
- **Completed:** 2026-08-30T19:43:00Z
- **Tasks:** 3
- **Files modified:** 8

## Accomplishments

- Added one core-owned configuration store with exact case-sensitive override replacement, independent fallback state, and explicit zero/unique/ambiguous active-match outcomes.
- Added strict `MonitorScale` and `MonitorScaleFallback` command parsing using the shared canonical quarter-factor parser and full token cleanup.
- Preserved all last-valid policy state after malformed, unsupported, extra, overlong, or allocation-failed declarations.
- Safely encoded control bytes in monitor identities before diagnostics reach production logging.
- Linked the configuration unit exactly once into `fvwm3` and initialized its lifecycle before startup commands run.

## Task Commits

Each TDD task was committed atomically:

1. **Task 1: Build the transactional exact-name override and fallback store**
   - `c00e45895` (test: failing transactional store coverage)
   - `41a13bb6c` (feat: transactional configuration store)
2. **Task 2: Parse and dispatch MonitorScale directives with strict argument counts**
   - `6bf6b737b` (test: failing callback and parsing coverage)
   - `a87c30557` (feat: strict callbacks, command entries, and safe diagnostics)
3. **Task 3: Link the configuration unit once into the production fvwm3 target**
   - `61b209399` (chore: singular production source linkage)

Additional correctness commit:

- `7ed146ba0` (fix: initialize and shut down the production configuration lifecycle)

## Files Created/Modified

- `fvwm/monitor_scale_config.h` - Defines configuration ownership, transactional declarations, matching, and lifecycle contracts.
- `fvwm/monitor_scale_config.c` - Implements the sole production store, strict callbacks, exact matching, retention diagnostics, and safe name encoding.
- `fvwm/commands.h` - Declares both command callbacks with `F_CMD_ARGS`.
- `fvwm/functable.c` - Dispatches the two lowercase directives in sorted order.
- `fvwm/fvwm3.c` - Initializes the scale store before startup commands and releases it during shutdown or restart.
- `fvwm/meson.build` - Links the configuration implementation exactly once into the production executable.
- `tests/test_monitor_scale_config.c` - Covers sequential replacement, allocation failure, exact identity, match counts, callback arity, malformed factors, controls, and long tokens.
- `tests/meson.build` - Registers the assertion-enabled native configuration test.

## Decisions Made

- Exact names remain byte-for-byte and case-sensitive; legacy numeric and `g`/`c`/`p` monitor aliases are deliberately not resolved.
- Candidate name and factor storage is complete before list mutation, allowing every failure path to preserve prior semantic state.
- Diagnostics receive escaped monitor identity while configuration storage retains the original bytes needed for exact matching.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical Functionality] Wired the production configuration lifecycle**
- **Found during:** Task 3 production linkage verification
- **Issue:** The new store was linked but production never initialized its allocator before startup directives ran, so valid directives would have failed without mutation or a diagnostic.
- **Fix:** Initialized the store before startup command processing and shut it down on normal exit or restart.
- **Files modified:** `fvwm/fvwm3.c`
- **Verification:** Explicit `./fvwm3:executable` compilation and all 12 native Meson tests passed.
- **Committed in:** `7ed146ba0`

---

**Total deviations:** 1 auto-fixed (1 Rule 2)
**Impact on plan:** The lifecycle wiring was required for the planned directives to function in production; no additional feature scope was introduced.

## Issues Encountered

- The Task 2 RED test initially included `commands.h` before the execution-context type was available. The include grouping was corrected, after which RED failed only on the intended missing callback symbols.
- Existing unrelated compiler warnings appeared during the production rebuild and were left unchanged.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 02-03 can finalize the retained initial policy and recompute effective monitor scales from exact overrides, automatic DPI, fallback, and safe 1x precedence.
- The authoritative store, strict command surface, and production lifecycle are ready; no blockers remain.

## Self-Check: PASSED

All eight implementation/test files and all six plan commits were verified.

---
*Phase: 02-scale-policy-and-configuration*
*Completed: 2026-08-30*
