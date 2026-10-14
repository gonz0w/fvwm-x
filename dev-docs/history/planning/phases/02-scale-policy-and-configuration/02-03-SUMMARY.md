---
phase: 02-scale-policy-and-configuration
plan: 03
subsystem: monitor-scale-lifecycle
tags: [xrandr, hidpi, event-loop, c11, meson, tdd]

requires:
  - phase: 02-scale-policy-and-configuration
    plan: 01
    provides: Total quarter-step scale resolution policy and committed topology facts
  - phase: 02-scale-policy-and-configuration
    plan: 02
    provides: Transactional exact-name scale overrides, fallback policy, and the core-owned configuration store
provides:
  - Allocation-free association of every committed topology fact with one total runtime monitor scale result
  - Initial scale-policy finalization with retained unmatched diagnostics and post-startup mutation rejection
  - Changed-topology coordination that recomputes scale before EWMH, broadcast, pan-frame, and Status effects
affects: [02-04-scale-observability, geometry-policy, ui-scaling, topology-hotplug]

actuals:
  tokens: 7060
  tasks: 3
  commits: 6

tech-stack:
  added: []
  patterns: [allocation-free committed-snapshot pass, sole lifecycle-owned configuration object, injected ordered downstream coordinator, TDD red-green commits]

key-files:
  created: []
  modified: [fvwm/monitor_scale_config.h, fvwm/monitor_scale_config.c, fvwm/fvwm3.c, fvwm/events.c, fvwm/topology_event_adapter.h, fvwm/topology_event_adapter.c, tests/test_monitor_scale_config.c, tests/test_topology_event_adapter.c, tests/meson.build]

key-decisions:
  - "Associate a topology fact with a runtime monitor by exact name and RandR Atom when available, rejecting missing or duplicate runtime associations."
  - "Finalize the sole monitor-scale configuration object after startup directives and reject all later direct declarations until the planned reload phase."
  - "Dispatch changed refreshes through one tested callback coordinator so scale recomputation always precedes every existing downstream topology effect."

patterns-established:
  - "Committed-snapshot association: resolve in one bounded in-memory pass and write a total effective_scale directly to the stable runtime monitor."
  - "Scale-first topology commit: refresh completion precedes a changed-only callback sequence of recompute, EWMH, broadcast, pan frames, and Status."

requirements-completed: [SCALE-01, SCALE-02, SCALE-03, SCALE-04]

coverage:
  - id: D1
    description: "Every committed active monitor resolves through exact override, valid automatic DPI, configured fallback, or safe 1x into a direct total runtime result without allocation or topology mutation."
    requirement: SCALE-03
    verification:
      - kind: unit
        ref: "tests/test_monitor_scale_config.c#test_recompute_is_allocation_free_total_and_idempotent"
        status: pass
      - kind: integration
        ref: "meson test -C builddir monitor-scale monitor-scale-config fscreen-topology-adapter"
        status: pass
    human_judgment: false
  - id: D2
    description: "One core-owned configuration object spans startup declarations, initial recompute and validation, inspection, late-mutation rejection, shutdown, and deterministic reinitialization."
    requirement: SCALE-01
    verification:
      - kind: unit
        ref: "tests/test_monitor_scale_config.c#test_finalize_owns_one_store_and_rejects_late_mutation"
        status: pass
      - kind: integration
        ref: "explicit ./fvwm3:executable compile"
        status: pass
    human_judgment: false
  - id: D3
    description: "A changed topology refresh recomputes scales exactly once before EWMH, broadcast, pan-frame, and Status effects, while unchanged and failed refreshes invoke none."
    requirement: SCALE-03
    verification:
      - kind: unit
        ref: "tests/test_topology_event_adapter.c#test_changed_orders_each_downstream_helper_once"
        status: pass
      - kind: integration
        ref: "production topology_refresh_if_due coordinator linkage plus full 12-test native Meson suite"
        status: pass
    human_judgment: false
  - id: D4
    description: "Ambiguous and unmatched exact overrides preserve lower-priority resolution metadata, and unmatched declarations remain retained for later topology changes."
    requirement: SCALE-02
    verification:
      - kind: unit
        ref: "tests/test_monitor_scale_config.c#test_association_rejects_ambiguous_and_retains_unmatched_overrides"
        status: pass
    human_judgment: false

duration: 14min
completed: 2026-08-30
status: complete
---

# Phase 2 Plan 3: Runtime Scale Finalization and Hotplug Ordering Summary

**Committed topology facts now populate total per-monitor scales in one allocation-free pass, with startup finalization and scale-first changed-refresh ordering.**

## Performance

- **Duration:** 14 min
- **Started:** 2026-08-30T19:46:00Z
- **Completed:** 2026-08-30T20:00:00Z
- **Tasks:** 3
- **Files modified:** 9

## Accomplishments

- Associated every active committed topology fact with exactly one runtime monitor and resolved complete override, automatic DPI, fallback, or safe-default result metadata.
- Finalized the sole production scale configuration after startup directives, diagnosed retained unmatched overrides, rejected post-finalization mutation, and released the store before X teardown.
- Replaced duplicated hotplug effect ordering with a production-used callback coordinator that proves changed refreshes recompute scales exactly once before all downstream effects.
- Preserved committed topology pointer/epoch identity and kept recomputation free of allocation, X queries, file access, subprocesses, sleeps, and publication.

## Task Commits

Each TDD task was committed as its behavior contract followed by implementation:

1. **Task 1: Resolve the committed snapshot into direct runtime monitor results**
   - `4065e2c8d` (test: failing scale association and recompute coverage)
   - `3de710add` (feat: runtime monitor scale resolution)
2. **Task 2: Own one scale-config instance across bootstrap, finalization, inspection, and shutdown**
   - `4060e0dd6` (test: failing scale lifecycle coverage)
   - `18d384f48` (feat: initial finalization and lifecycle placement)
3. **Task 3: Dispatch changed topology through an executable scale-first coordinator**
   - `bf6b5b353` (test: failing scale-first topology dispatch coverage)
   - `50091c2f2` (feat: production scale-first downstream coordination)

## Files Created/Modified

- `fvwm/monitor_scale_config.h` - Declares association, recompute, validation, finalization, and test-injection contracts.
- `fvwm/monitor_scale_config.c` - Implements total direct monitor association, bounded recompute, lifecycle finalization, and retained unmatched diagnostics.
- `fvwm/fvwm3.c` - Initializes the store after core variables, finalizes it after startup directives, and destroys it before X teardown.
- `fvwm/events.c` - Supplies production callbacks and dispatches each completed refresh through the tested scale-first coordinator.
- `fvwm/topology_event_adapter.h` - Defines the exact five-callback downstream contract.
- `fvwm/topology_event_adapter.c` - Executes changed-only downstream callbacks once in required order.
- `tests/test_monitor_scale_config.c` - Covers precedence, ambiguity, unmatched retention, automatic boundaries, idempotence, lifecycle identity, and mutation rejection.
- `tests/test_topology_event_adapter.c` - Proves `rebps` ordering and zero effects for unchanged or failed refreshes.
- `tests/meson.build` - Enables the private scale-config test seam for the native unit target.

## Decisions Made

- Runtime association uses exact monitor names and also requires matching RandR Atom identity when the legacy runtime monitor has one; duplicate or absent associations fail the pass rather than silently writing the wrong monitor.
- Initial finalization succeeds only after recomputation and range validation of every active result, then locks direct policy mutation until the Phase 6 reload contract exists.
- The pure topology adapter owns downstream callback order, while `events.c` supplies thin wrappers over existing production effects and the no-argument scale recompute entry point.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- Meson has no standalone compile target named `topology-event-adapter`; RED was confirmed by compiling the configured build, and focused GREEN verification used the registered Meson test name.
- Whole-file clang-format checking is not applicable to legacy `fvwm3.c`, which contains extensive pre-existing historical formatting. All newly created compact C units and tests were formatted, the realized diff passed `git diff --check`, and native compilation passed.

## Known Stubs

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 02-04 can expose finalized effective scale and reason metadata through diagnostics and live-X observability without inventing another policy store or recompute path.
- Geometry, UI resource, and hotplug phases can consume `struct monitor.effective_scale` knowing it is initialized before normal processing and refreshed before topology effects.
- No blockers remain; all 12 native Meson tests pass.

## Self-Check: PASSED

- All nine implementation, integration, test, and build files exist.
- All six RED/GREEN task commits are present in git history in the required order.
- Explicit production compilation, focused scale/event tests, and the full 12-test native Meson suite pass.
- No new stubs, skipped tests, unrun verification, or unregistered threat surface remains.

---
*Phase: 02-scale-policy-and-configuration*
*Completed: 2026-08-30*
