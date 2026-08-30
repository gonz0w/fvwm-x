---
phase: 01-topology-snapshot-and-live-x-harness
plan: 04
subsystem: topology-event-loop
tags: [xrandr, c11, meson, tdd, monotonic-time, event-loop]

requires:
  - phase: 01-02
    provides: Failure-safe changed, unchanged, and failed topology refresh outcomes
  - phase: 01-03
    provides: Bounded monotonic quiet and hard-cap refresh policy
provides:
  - Pure classification and dispatch seam for root and topology-relevant RandR events
  - Complete root and RandR topology notification subscriptions with deferred dirty marking
  - Top-of-loop due refresh with scheduler deadline merging and changed-only downstream effects
affects: [01-06-live-x-harness, 01-07-xvfb-smoke, 01-08-xorg-dummy, topology-hotplug]

actuals:
  tokens: 46120
  tasks: 3
  commits: 6

tech-stack:
  added: []
  patterns: [pure event adapter, constant-time dirty marking, top-of-loop semantic commit, minimum deadline merge]

key-files:
  created:
    - fvwm/topology_event_adapter.h
    - fvwm/topology_event_adapter.c
    - tests/test_topology_event_adapter.c
  modified:
    - libs/FScreen.c
    - fvwm/meson.build
    - fvwm/events.c
    - fvwm/eventmask.h
    - tests/meson.build

key-decisions:
  - "Only root ConfigureNotify plus RandR screen, CRTC, output, output-property, and resource events arm topology refreshes; provider and lease events remain unrelated."
  - "A due topology generation is serviced immediately after deferred signals at the top of HandleEvents(), before another queued X event is consumed."
  - "Changed refreshes alone run the existing EWMH, broadcast, pan-frame, and Status sequence after FScreen has completed publication and legacy reconciliation."

patterns-established:
  - "Narrow event seam: protocol classification may call only update-configuration followed by one dirty mark, never refresh or downstream work."
  - "Bounded event-loop commit: merge topology and scheduler deadlines, then execute one due refresh before additional queued work."

requirements-completed: [TOP-03]

coverage:
  - id: D1
    description: "Real core and RandR constants drive a pure event adapter with exact accepted/rejected classes, update-before-mark ordering, timeout selection, and changed-result decisions."
    requirement: TOP-03
    verification:
      - kind: unit
        ref: "tests/test_topology_event_adapter.c#test_classifies_topology_events"
        status: pass
      - kind: unit
        ref: "tests/test_topology_event_adapter.c#test_dispatch_orders_update_before_mark"
        status: pass
      - kind: unit
        ref: "tests/test_topology_event_adapter.c#test_only_changed_requests_downstream_work"
        status: pass
    human_judgment: false
  - id: D2
    description: "Root Monitor-object and complete topology-relevant RandR notifications perform only update-before-mark or one constant-time dirty mark, with the adapter linked into production exactly once."
    requirement: TOP-03
    verification:
      - kind: integration
        ref: "python3 tests/run_in_linux.py -- sh -ec 'meson compile -C builddir ./fvwm3:executable && meson test -C builddir monitor-topology fscreen-topology-adapter topology-refresh-policy topology-event-adapter --print-errorlogs'"
        status: pass
      - kind: unit
        ref: "tests/test_topology_event_adapter.c#test_dispatch_marks_only_accepted_events"
        status: pass
    human_judgment: false
  - id: D3
    description: "Each settled generation refreshes before more queued X work, shares the minimum wait with scheduled commands, and invokes downstream effects exactly once only for a semantic change."
    requirement: TOP-03
    verification:
      - kind: unit
        ref: "tests/test_topology_event_adapter.c#test_changed_orders_each_downstream_helper_once"
        status: pass
      - kind: integration
        ref: "python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs"
        status: pass
      - kind: other
        ref: "source-order acceptance check for topology_refresh_if_due before My_XNextEvent and exactly-once changed helpers"
        status: pass
    human_judgment: false

duration: 12min
completed: 2026-08-30
status: complete
---

# Phase 01 Plan 04: Deferred Topology Event Commit Summary

**Complete RandR and root topology notifications now coalesce through monotonic bounded deadlines into one failure-safe FScreen refresh and changed-only downstream commit.**

## Performance

- **Duration:** 12 min
- **Started:** 2026-08-30T13:30:28Z
- **Completed:** 2026-08-30T13:42:12Z
- **Tasks:** 3
- **Files modified:** 8

## Accomplishments

- Added a pure adapter over actual X11/RandR constants that rejects unrelated roots, provider notifications, and lease notifications while proving screen update-before-mark ordering.
- Subscribed the root and RandR extension to every required topology signal and replaced immediate per-event queries and effects with one constant-time dirty generation.
- Merged the policy's monotonic timeout with scheduled-command waits and serviced due generations before another queued X event can postpone the hard cap.
- Routed only changed FScreen results through the existing EWMH, module broadcast, pan-frame, and Status helpers once; unchanged and failed attempts complete without downstream effects or epoch movement.

## Task Commits

Each TDD task was committed as its behavior contract followed by implementation:

1. **Task 1 RED: Specify topology event and result decisions** - `e8b126df2` (test)
2. **Task 1 GREEN: Implement pure topology adapter decisions** - `058f572b2` (feat)
3. **Task 2 RED: Expand accepted and rejected deferred dispatch coverage** - `65fbc7eae` (test)
4. **Task 2 GREEN: Wire complete topology notifications to dirty marking** - `1bf0f55d8` (feat)
5. **Task 3 RED: Specify changed-only downstream sequencing** - `3f4998754` (test)
6. **Task 3 GREEN: Run and commit one due settled refresh** - `7e3b3909e` (feat)

## Files Created/Modified

- `fvwm/topology_event_adapter.h` - Declares the exact pure input, class, callback, timeout, and refresh-result decision contract.
- `fvwm/topology_event_adapter.c` - Classifies real topology events, dispatches update-before-mark, and selects timeout/result behavior without clocks, globals, or X queues.
- `tests/test_topology_event_adapter.c` - Asserts every accepted/rejected protocol class, callback ordering, no immediate refresh path, timeout minimums, and changed-only once effects.
- `libs/FScreen.c` - Selects screen, CRTC, output, output-property, and resource-change RandR notifications.
- `fvwm/eventmask.h` - Selects root `StructureNotifyMask` for RandR Monitor create/delete `ConfigureNotify` events.
- `fvwm/events.c` - Marks topology dirty, merges monotonic deadlines, services due work at the loop boundary, and gates downstream effects.
- `fvwm/meson.build` - Links the event adapter into `fvwm3` exactly once beside the refresh policy.
- `tests/meson.build` - Registers the assertion-enabled `topology-event-adapter` target.

## Decisions Made

- Required every accepted event's selected window to be the root, preventing unrelated child or provider activity from arming the topology state.
- Treated monotonic-clock failure as immediately due using `UINT64_MAX`, preserving bounded progress rather than leaving an armed generation indefinitely pending.
- Kept publication and stable legacy reconciliation inside `FScreenRefreshTopology()`; the event coordinator begins downstream work only after its explicit changed result returns.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- Meson's short target name `fvwm3` is ambiguous because both the executable and static library share it; production linkage was verified with the equivalent explicit target `./fvwm3:executable`.
- The Linux build image does not provide `clang-format`. Native compilation and all tests ran through `tests/run_in_linux.py`, while repository formatting was applied and dry-run verified with local `xcrun clang-format`.
- Existing unrelated compiler warnings remained visible during full builds; all seven native tests passed and the completed event-loop integration introduced no unresolved warnings.

## Known Stubs

None.

## Threat Flags

None - topology event pressure, timeout starvation, and changed-result tampering are the registered T-01-10 through T-01-12 surfaces and are covered by the adapter, deadline, source-order, production-link, and full-suite checks.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 01-06 can drive real nested-X events through one production-linked deferred commit path and inject one failed FScreen query without adding a runtime seam.
- Plans 01-07 and 01-08 can assert rapid-burst coalescing, sustained-storm hard-cap progress, changed-only Status emission, and last-good recovery against nested servers.
- No blockers remain for the live-X harness wave.

## Self-Check: PASSED

- All three created adapter/test files and five modified integration/build files exist.
- All six RED/GREEN task commits are present in git history in the required order.
- Production symbol linkage, source-order/mask acceptance checks, focused quick tests, and the full seven-test native suite pass.
- No new stubs, skipped tests, unrun verification, or unregistered threat surface remains.

---
*Phase: 01-topology-snapshot-and-live-x-harness*
*Completed: 2026-08-30*
