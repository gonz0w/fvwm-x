---
phase: 02-scale-policy-and-configuration
plan: 04
subsystem: monitor-scale-observability
tags: [xrandr, hidpi, printinfo, diagnostics, asciidoc]

requires:
  - phase: 02-03
    provides: authoritative effective scale results attached to committed monitors
provides:
  - Reason-complete, injection-safe PrintInfo Scale diagnostics
  - Deterministic retained unmatched override reporting
  - Exact user manual contract for initial monitor scaling
affects: [02-05, scale-policy-verification, monitor-diagnostics]

actuals:
  tokens: 5965
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - Read inspection values from the authoritative effective_scale result without re-resolving policy
    - Encode external names with cJSON before emitting single-line diagnostics

key-files:
  created: []
  modified:
    - fvwm/monitor_scale_config.h
    - fvwm/monitor_scale_config.c
    - fvwm/builtins.c
    - tests/test_monitor_scale_config.c
    - doc/fvwm3_manpage_source.adoc

key-decisions:
  - "PrintInfo Scale reads the core-owned monitor_scale_config state and attached effective_scale values rather than creating a parallel policy path."
  - "Status v4 and module packets remain actual-pixel interfaces without scale metadata; PrintInfo Scale is the inspection boundary."

patterns-established:
  - "Diagnostic integrity: encode config/X-controlled names before placing them on one physical output line."
  - "Explanation fidelity: report selected source and reason together with rejected automatic-policy and endpoint state."

requirements-completed: [SCALE-01, SCALE-02, SCALE-03, SCALE-04, SCALE-05]

coverage:
  - id: D1
    description: "PrintInfo Scale reports deterministic, escaped, reason-complete active monitor and retained override evidence."
    requirement: SCALE-04
    verification:
      - kind: unit
        ref: "tests/test_monitor_scale_config.c; meson test -C builddir monitor-scale-config --print-errorlogs"
        status: pass
    human_judgment: false
  - id: D2
    description: "The installed manual defines exact initial scale grammar, precedence, DPI arithmetic, lifecycle, conversion, and observability boundaries."
    requirement: SCALE-01
    verification:
      - kind: integration
        ref: "meson setup builddir --reconfigure -Dmandoc=true -Dgolang=disabled && meson compile -C builddir"
        status: pass
    human_judgment: false
  - id: D3
    description: "Status v4 and module protocol sources remain unchanged while scale diagnostics are added."
    requirement: SCALE-04
    verification:
      - kind: other
        ref: "git diff 1506c6615..HEAD -- fvwm/status_json.c libs/Module.h fvwm/module_interface.c fvwm/module_list.h"
        status: pass
    human_judgment: false

duration: 9min
completed: 2026-08-30
status: complete
---

# Phase 2 Plan 4: Scale Inspection and Documentation Summary

**Injection-safe `PrintInfo Scale` diagnostics now expose authoritative factors, sources, raw facts, rejection reasons, boundaries, ambiguity, and retained overrides, backed by an exact initial-scale manual contract.**

## Performance

- **Duration:** 9 min
- **Started:** 2026-08-30T20:03:00Z
- **Completed:** 2026-08-30T20:12:12Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- Added case-insensitive `PrintInfo Scale` dispatch with deterministic active-monitor and retained-unmatched output.
- Protected diagnostic line structure by cJSON-encoding exact monitor and override names, including control bytes and UTF-8.
- Covered override, automatic, endpoint-bound, fallback, safe 1x, invalid DPI, ambiguity, and unmatched explanations with exact native tests.
- Documented strict initial-only directives, precedence, exact DPI policy, conversion rules, and the unchanged Status/module boundary.

## Task Commits

Each task was committed atomically:

1. **Task 1: Emit escaped, reason-complete PrintInfo Scale output (RED)** - `a9bf3c98b` (test)
2. **Task 1: Emit escaped, reason-complete PrintInfo Scale output (GREEN)** - `03c14fcf7` (feat)
3. **Task 2: Document the exact initial scale and inspection contract** - `31495dde8` (docs)

## Files Created/Modified

- `fvwm/monitor_scale_config.h` - Exposes the no-argument scale inspection entry point.
- `fvwm/monitor_scale_config.c` - Formats authoritative result facts and safely encoded names for diagnostics.
- `fvwm/builtins.c` - Routes the `Scale` PrintInfo subject to scale inspection.
- `tests/test_monitor_scale_config.c` - Asserts stable output and injection-safe encoding across all result classes.
- `doc/fvwm3_manpage_source.adoc` - Defines the exact initial scale and inspection contract.

## Decisions Made

- Used the existing core-owned scale configuration and attached effective result as the only inspection source, preventing explanation drift.
- Kept Status v4 and module packets unchanged; scale details remain a local diagnostic concern in this phase.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- Documentation-enabled Linux compilation emitted pre-existing `FlocaleCharset.c` missing-initializer warnings; compilation and all tests passed.

## TDD Gate Compliance

- RED: `a9bf3c98b` introduced failing exact scale inspection output tests.
- GREEN: `03c14fcf7` implemented the production inspection path and passed the focused suite.

## Known Stubs

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 02-05 can parse `PrintInfo Scale` evidence from the nested-X harness and exercise the complete initial policy matrix.
- No blockers remain; the full 12-test Linux suite passes.

## Self-Check: PASSED

- All five changed implementation, test, and manual files exist.
- Task commits `a9bf3c98b`, `03c14fcf7`, and `31495dde8` exist.
- The canonical summary file exists at the required phase path.

---
*Phase: 02-scale-policy-and-configuration*
*Completed: 2026-08-30*
