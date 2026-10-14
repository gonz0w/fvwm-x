---
phase: 05-logical-size-transition-coordinator
plan: 06
subsystem: geometry-transition
tags: [hidpi, geometry, logical-size, manpage, monitor-scale, ewmh, status]

requires:
  - phase: 05-logical-size-transition-coordinator
    provides: allocation-free geometry_transition_plan
provides:
  - User-visible logical-size transition contract beside MonitorScale
  - GEO-07 proof that vpacket, module_interface, and status_json stay actual-pixel
affects: [05-03-maximize, 05-04-fullscreen, 05-05-configure]

actuals:
  tokens: 317
  tasks: 2
  commits: 1

tech-stack:
  added: []
  patterns:
    - Extend the MonitorScale retained-source paragraph rather than starting a second contract
    - Status v4, module packets, and _NET_FRAME_EXTENTS remain actual X11 pixels

key-files:
  created: []
  modified:
    - doc/fvwm3_manpage_source.adoc

key-decisions:
  - "Extend the existing MonitorScale retained-source paragraph; do not replace it or start a second contract."
  - "Grow-to maximize remains undocumented as dest-scale policy (PA-08)."
  - "Protocol writers stay unmodified; the Status actual-pixel sentence lives in AsciiDoc only."

patterns-established:
  - "Pattern: Document dest-scale logical client preservation next to retained exact source."
  - "Pattern: Maximize fills dest work area; fullscreen fills dest monitor; restore uses stored logical plus dest chrome."
  - "Pattern: GEO-07 guardrail is empty git diff on vpacket.h, module_interface.c, and status_json.c."

requirements-completed: [GEO-02, GEO-04, GEO-06]

coverage:
  - id: D1
    description: "MonitorScale documents eligible-window logical client preservation, dest maximize/fullscreen/restore, and dest chrome _NET_FRAME_EXTENTS."
    requirement: GEO-02
    verification:
      - kind: other
        ref: "python3 assert logical client, work area, _NET_FRAME_EXTENTS within 1800 chars of retained exact source"
        status: pass
    human_judgment: false
  - id: D2
    description: "Dragging across scales keeps the pointer grab and grabbed titlebar point."
    requirement: GEO-04
    verification:
      - kind: other
        ref: "python3 assert pointer grab and titlebar point sentences in MonitorScale region"
        status: pass
    human_judgment: false
  - id: D3
    description: "Maximize fills dest work area, fullscreen fills dest monitor, restore uses stored logical client size plus dest chrome."
    requirement: GEO-06
    verification:
      - kind: other
        ref: "python3 assert dest work area, dest monitor, restore stored logical in MonitorScale region"
        status: pass
    human_judgment: false
  - id: D4
    description: "libs/vpacket.h, fvwm/module_interface.c, and fvwm/status_json.c remain unmodified actual-pixel interfaces."
    requirement: GEO-02
    verification:
      - kind: other
        ref: "git diff --exit-code -- libs/vpacket.h fvwm/module_interface.c fvwm/status_json.c"
        status: pass
    human_judgment: false

duration: 3min
completed: 2026-09-01
status: complete
---

# Phase 5 Plan 6: Logical-Size Transition Documentation Summary

**MonitorScale manpage extends retained-source conversion with dest-scale logical client preservation, dest maximize/fullscreen/restore, and actual-pixel protocol units**

## Performance

- **Duration:** 3 min
- **Started:** 2026-09-01T00:42:38Z
- **Completed:** 2026-09-01T00:45:28Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments

- Eligible windows preserve logical client size across monitor scale changes by converting from the retained exact source at the dest factor, then applying ICCCM constraints
- Maximize fills the dest work area; fullscreen fills the dest monitor with no decorations; unmaximize and leave-fullscreen restore stored logical client size plus dest chrome
- `_NET_FRAME_EXTENTS`, Status version 4, and module packets remain actual X11 pixels; vpacket, module_interface, and status_json have an empty git diff

## Task Commits

Each task was committed atomically:

1. **Task 1: Extend MonitorScale with logical-size transition behavior** - `b62fd36cd` (docs)
2. **Task 2: Prove protocol files stay actual-pixel** - no commit (verification-only; protocol files unchanged)

## Files Created/Modified

- `doc/fvwm3_manpage_source.adoc` - MonitorScale continuation paragraph for dest-scale logical client, maximize/fullscreen/restore, and pixel protocol units

## Decisions Made

- Extended the existing MonitorScale retained-source paragraph rather than replacing it or starting a second contract
- Grow-to maximize remains undocumented as dest-scale policy (PA-08); 100 percent maximize, EWMH max, fullscreen, restore, and drag crossings are the documented surface
- Status/module actual-pixel restatement stays in AsciiDoc; GEO-07 guardrail files were not edited

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for remaining Phase 5 wiring plans (05-03 maximize, 05-04 fullscreen, 05-05 configure). GEO-02, GEO-04, and GEO-06 remain shared across sibling plans, so REQUIREMENTS.md stays pending until those SUMMARYs exist. Protocol files stay actual-pixel; no Status v4 or module packet schema work follows from this plan.

## Self-Check: PASSED

---
*Phase: 05-logical-size-transition-coordinator*
*Completed: 2026-09-01*
