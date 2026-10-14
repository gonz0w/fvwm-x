---
phase: 02-scale-policy-and-configuration
plan: 05
subsystem: live-x-scale-verification
tags: [xrandr, hidpi, live-x, xorg-dummy, printinfo, argv]

requires:
  - phase: 02-04
    provides: reason-complete PrintInfo Scale diagnostics and exact initial-scale contract
provides:
  - Argv-only extra startup commands and scheduled post-finalization PrintInfo Scale evidence
  - Authoritative Xorg-dummy initial scale-policy matrix
  - Nonparallel bounded scale-policy-xorg-dummy live-X target
affects: [phase-03, monitor-ownership, scale-verification]

actuals:
  tokens: 7109
  tasks: 2
  commits: 4

tech-stack:
  added: []
  patterns:
    - Append initial directives as isolated argv -c pairs with no shell interpretation
    - Wait for scheduled post-finalization PrintInfo Scale before factor assertions
    - Query Xorg-dummy Monitor/output physical facts before policy comparison

key-files:
  created: []
  modified:
    - tests/live_x/live_x_harness.py
    - tests/meson.build
    - tests/run_in_linux.py

key-decisions:
  - "Scenario-owned startup directives execute as separate argv -c command pairs in exact order, including names with spaces and quotes."
  - "add_post_finalize_scale_inspection() appends a separate -c, Schedule 0 PrintInfo Scale pair so evidence is emitted after initial finalization."
  - "Status v4 and module packets remain actual-pixel interfaces; live-X scale proof reads PrintInfo Scale and queried Xorg-dummy facts only."

patterns-established:
  - "Live-X scale cases use a fresh owned FVWM invocation per initial-config case rather than runtime reload."
  - "Escaped unmatched override names are compared with json.dumps() semantics and cannot forge evidence lines."

requirements-completed: [SCALE-01, SCALE-02, SCALE-03, SCALE-04, SCALE-05]

coverage:
  - id: D1
    description: "FVWM startup extra commands are argv pairs only, and PrintInfo Scale evidence is parsed after a scheduled post-finalization inspection."
    requirement: SCALE-04
    verification:
      - kind: unit
        ref: "tests/live_x/live_x_harness.py#test_fvwm_arguments_extra_commands,test_post_finalize_scale_inspection_argv,test_print_info_scale_parser"
        status: pass
    human_judgment: false
  - id: D2
    description: "An isolated Xorg-dummy matrix proves override, automatic DPI, fallback, safe 1x, invalid-replacement retention, endpoint bounds, and retained unmatched overrides against queried facts."
    requirement: SCALE-03
    verification:
      - kind: e2e
        ref: "meson test -C builddir scale-policy-xorg-dummy --print-errorlogs"
        status: pass
    human_judgment: false
  - id: D3
    description: "The live-X scale scenario does not invoke a shell, use a fixed display, use fixed sleeps, or create a second harness."
    requirement: SCALE-04
    verification:
      - kind: unit
        ref: "tests/live_x/live_x_harness.py#fvwm_arguments,add_post_finalize_scale_inspection,run_xorg_dummy_scale_policy"
        status: pass
    human_judgment: false
  - id: D4
    description: "Status v4 fields and module packets remain unchanged while scale-policy live-X coverage is added."
    requirement: SCALE-04
    verification:
      - kind: other
        ref: "git diff fac1afdee^..dbc68d7fe -- fvwm/status_json.c libs/Module.h fvwm/module_interface.c fvwm/module_list.h"
        status: pass
    human_judgment: false

duration: 21min
completed: 2026-08-30
status: complete
---

# Phase 2 Plan 5: Authoritative Scale Policy Matrix Summary

**Isolated live-X Xorg-dummy cases now prove exact named override, automatic DPI, fallback, safe 1x, invalid-replacement retention, endpoint-bound automatic results, and escaped unmatched overrides through argv-only startup and scheduled `PrintInfo Scale` evidence.**

## Performance

- **Duration:** 21 min
- **Started:** 2026-08-30T20:17:22Z
- **Completed:** 2026-08-30T20:37:55Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- Extended `fvwm_arguments()` with immutable `extra_commands` appended as exact `-c`, command argv pairs, including spaces and quotes.
- Scheduled `PrintInfo Scale` after initial finalization and parsed escaped active/unmatched records with bounded polling.
- Registered nonparallel `scale-policy-xorg-dummy` in the existing live-X suite and `LIVE_X_TEST_NAMES` manifest.
- Proved the complete initial policy matrix against queried Xorg-dummy pixel/mm facts without Status v4 or module-packet changes.

## Task Commits

Each task was committed atomically:

1. **Task 1: Extend argv-only FVWM startup and parse PrintInfo Scale evidence (RED)** - `fac1afdee` (test)
2. **Task 1: Extend argv-only FVWM startup and parse PrintInfo Scale evidence (GREEN)** - `6b1e07d0c` (feat)
3. **Task 2: Prove the complete initial policy matrix on Xorg-dummy (RED)** - `144c0cbde` (test)
4. **Task 2: Prove the complete initial policy matrix on Xorg-dummy (GREEN)** - `dbc68d7fe` (feat)

## Files Created/Modified

- `tests/live_x/live_x_harness.py` - Argv extra commands, scheduled scale inspection, strict parser, and Xorg-dummy policy matrix.
- `tests/meson.build` - Nonparallel bounded `scale-policy-xorg-dummy` live-X target.
- `tests/run_in_linux.py` - Includes `scale-policy-xorg-dummy` in the complete live-X manifest.

## Decisions Made

- Used separate argv `-c` pairs for every startup directive so quotes and spaces never pass through a shell.
- Waited for the scheduled post-finalization `PrintInfo Scale` record before asserting factor, source, and reason.
- Kept Status v4 and module packets unchanged; live-X proof stays on PrintInfo Scale plus queried X facts.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- Executor returned after production commits without `02-05-SUMMARY.md`. This summary closes that illegal partial-plan state from the existing TDD commits rather than re-dispatching.

## TDD Gate Compliance

- RED: `fac1afdee` and `144c0cbde` introduced failing argv/parser and matrix tests.
- GREEN: `6b1e07d0c` and `dbc68d7fe` implemented the harness path and authoritative cases.

## Known Stubs

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 02 plans are complete; ready for phase verification and then monitor ownership/geometry work.
- Live-X scale proof remains Linux/Podman-only, matching the Phase 01 harness contract.

## Self-Check: PASSED

- All three changed harness, Meson, and manifest files exist.
- Task commits `fac1afdee`, `6b1e07d0c`, `144c0cbde`, and `dbc68d7fe` exist.
- The canonical summary file exists at the required phase path.

---
*Phase: 02-scale-policy-and-configuration*
*Completed: 2026-08-30*
