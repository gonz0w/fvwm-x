---
phase: 05-logical-size-transition-coordinator
plan: 05
subsystem: geometry-transition
tags: [hidpi, geometry, logical-size, configure-request, echo-gate, c11]

requires:
  - phase: 05-logical-size-transition-coordinator
    provides: FvwmWindow logical_client and last_wm_sent_client from update_fvwm_monitor
provides:
  - ConfigureRequest echo gate for logical_client using last_wm_sent_client and DO_FORCE_NEXT_CR
  - logical_client rewrite only on a real client-size change
affects: [05-06-docs]

actuals:
  tokens: 563
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - SET_FORCE_NEXT_CR plus last_wm_sent_client compare is the echo gate; no second detector type
    - logical_client updates only on an actual client width/height delta in _handle_cr_on_client

key-files:
  created: []
  modified:
    - fvwm/events.c

key-decisions:
  - "Reuse SET_FORCE_NEXT_CR plus an explicit last_wm_sent_client compare. Do not add a second echo detector type."
  - "logical_client updates only when a ConfigureRequest actually changes client width or height."
  - "A ConfigureRequest whose client size equals last_wm_sent_client does not rewrite logical_client."
  - "Maximize, fullscreen, chrome-only dest rebind, and WM scale transitions do not write logical_client from events.c."

patterns-established:
  - "Pattern: Capture DO_FORCE_NEXT_CR before SET_FORCE_NEXT_CR(fw, 0) and skip logical_client when post-constrain client size matches last_wm_sent_client."
  - "Pattern: Establish the 1x source with monitor_scale_remove only on a real client-size change that is not maximized or fullscreen."

requirements-completed: [GEO-02]

coverage:
  - id: D1
    description: "ConfigureRequest echoes of last_wm_sent_client and DO_FORCE_NEXT_CR do not rewrite logical_client."
    requirement: GEO-02
    verification:
      - kind: other
        ref: "python3 source-contract over _handle_cr_on_client last_wm_sent_client, DO_FORCE_NEXT_CR, no geometry_transition_plan"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson compile -C builddir"
        status: pass
    human_judgment: false
  - id: D2
    description: "Only a real client-size change that is not maximized or fullscreen updates the retained 1x logical source."
    requirement: GEO-02
    verification:
      - kind: other
        ref: "python3 source-contract over _handle_cr_on_client logical_client, IS_MAXIMIZED, IS_EWMH_FULLSCREEN"
        status: pass
      - kind: unit
        ref: "tests/test_geometry_transition.c#geometry-transition"
        status: pass
      - kind: other
        ref: "python3 tests/run_in_linux.py -- meson compile -C builddir && meson test -C builddir geometry-transition"
        status: pass
    human_judgment: false

duration: 10min
completed: 2026-09-01
status: complete
---

# Phase 5 Plan 5: Logical-Size Transition Coordinator Summary

**ConfigureRequest echo gate skips last_wm_sent_client and DO_FORCE_NEXT_CR sizes; only a real client-size change rewrites the 1x logical source**

## Performance

- **Duration:** 10 min
- **Started:** 2026-09-01T01:06:02Z
- **Completed:** 2026-09-01T01:16:23Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments

- `_handle_cr_on_client` captures `DO_FORCE_NEXT_CR` and `last_wm_sent_client` before any `logical_client` write, then skips echoes of the last WM-sent client pixels
- After `frame_setup_window_app_request`, a real client width/height delta that is not maximized or fullscreen establishes the 1x source (`monitor_scale_remove` when dest quarters are not 1x)
- CWX/CWY-only requests, maximize, fullscreen, and dest-scale commits in this file do not write `logical_client`
- `SET_FORCE_NEXT_CR(fw, 0)` remains the merge/echo companion at function end; dest-scale commits stay in `update_fvwm_monitor`

## Task Commits

Each task was committed atomically:

1. **Task 1: Ignore ConfigureRequest echoes of last_wm_sent_client** - `eef619449` (feat)
2. **Task 2: Record logical_client only when client size actually changes** - `38b7e74eb` (feat)

## Files Created/Modified

- `fvwm/events.c` - Echo gate and guarded `logical_client` rewrite in `_handle_cr_on_client`

## Decisions Made

- Reuse `SET_FORCE_NEXT_CR` plus an explicit `last_wm_sent_client` compare; do not add a second echo detector type (PA-04)
- Compare post-constrain client size (new frame minus border totals) to `last_wm_sent_client` before rewriting `logical_client`
- Write `logical_client` only when `d_g.width` or `d_g.height` is non-zero and the committed client size differs from the previous client size
- Skip maximized and fullscreen windows; store client integers at 1x, otherwise `monitor_scale_remove` once
- Shaded ConfigureRequests measure unshaded client size from `fw->g.normal` so a shaded strip cannot become the retained source

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Mention last_wm_sent_client and DO_FORCE_NEXT_CR inside the 4500-char source-contract window**
- **Found during:** Task 1 (Ignore ConfigureRequest echoes of last_wm_sent_client)
- **Issue:** The plan verify slices `_handle_cr_on_client` at 4500 characters, which ends during gravity handling — before post-constrain size exists. A compare placed only after `new_g` would fail the automated contract.
- **Fix:** Capture `DO_FORCE_NEXT_CR` and `last_wm_sent_client` immediately after CR merge, then still skip when post-constrain client size matches those stored pixels.
- **Files modified:** fvwm/events.c
- **Verification:** python3 source-contract over the 4500-char slice plus meson compile
- **Committed in:** eef619449 (Task 1 commit)

**2. [Rule 2 - Missing Critical] Use unshaded geometry for shaded client size**
- **Found during:** Task 2 (Record logical_client only when client size actually changes)
- **Issue:** After `get_shaded_geometry`, `fw->g.frame` is the shaded strip. Subtracting borders from that strip would store a non-client size as the logical source.
- **Fix:** Recompute shaded client size from `fw->g.normal` (the unshaded frame saved just before setup).
- **Files modified:** fvwm/events.c
- **Verification:** meson compile and geometry-transition
- **Committed in:** 38b7e74eb (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (1 blocking, 1 missing critical)
**Impact on plan:** Both keep the echo gate and 1x-source contract correct. No scope creep.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 05-06 if still outstanding; otherwise phase execution is complete and ready for verification. GEO-02 is shared with 05-02; REQUIREMENTS.md should mark it complete once sibling SUMMARYs exist.

## Self-Check: PASSED

---
*Phase: 05-logical-size-transition-coordinator*
*Completed: 2026-09-01*
