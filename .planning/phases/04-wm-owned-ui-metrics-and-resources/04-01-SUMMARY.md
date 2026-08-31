---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 01
subsystem: ui-metrics
tags: [hidpi, scale, chrome, cache-key, c11, meson]

requires:
  - phase: 02-scale-policy-and-configuration
    provides: monitor_scale_apply exact-quarter conversion with overflow-to-zero
provides:
  - Allocation-free wm_ui_scale_px wrapper over monitor_scale_apply
  - wm_ui_cache_key identity on quarters, target size, and generation
  - Native UI-01 through UI-06 integer and cache-key fixtures
affects: [04-02-supported-features, 04-03-picture-cache, 04-04-flocale, 04-05-window-chrome]

actuals:
  tokens: 3661
  tasks: 3
  commits: 6

tech-stack:
  added: []
  patterns:
    - wm_ui_scale_px always calls monitor_scale_apply on the retained source integer
    - cache-key equality is unsigned integer field equality
    - title composition uses already-scaled font height plus a fresh extra conversion

key-files:
  created:
    - libs/wm_ui_metrics.h
    - libs/wm_ui_metrics.c
    - tests/test_wm_ui_metrics.c
  modified:
    - libs/meson.build
    - tests/meson.build

key-decisions:
  - "Promote scaled actual pixels as the primary chrome representation; configured 1x integers remain the unscaled source to monitor_scale_apply."
  - "wm_ui_cache_key identity is scale_quarters, target_width, target_height, and generation; name-byte walks stay in Picture/Flocale."
  - "Resource generation starts at WM_UI_RESOURCE_GENERATION_INITIAL and is not incremented in this phase."
  - "Explicit title source below the scaled font raises thickness to the font; surplus above font-plus-extra is centered into offset."

patterns-established:
  - "Pattern: wm_ui_scale_px is the only legal chrome converter and must not reimplement quarter arithmetic or use floating-point."
  - "Pattern: converters initialize outputs to 0 and return OVERFLOW or INVALID_ARGUMENT; they do not log."
  - "Pattern: never convert a previously rounded result; title extra and explicit heights are converted from source integers."

requirements-completed: [UI-01, UI-02, UI-03, UI-04, UI-05, UI-06]

coverage:
  - id: D1
    description: "Source chrome integers 1, 7, and 13 convert at 1x, 1.25x, 1.5x, 2x, and 4x to the same pixel results as monitor_scale_apply."
    requirement: UI-01
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_chrome_sources_match_apply_at_supported_factors"
        status: pass
    human_judgment: false
  - id: D2
    description: "Cache keys that differ only in scale_quarters or only in generation miss; identical quarters, target dimensions, and generation hit."
    requirement: UI-02
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_cache_key_misses_across_quarters_and_generation"
        status: pass
    human_judgment: false
  - id: D3
    description: "Menu border, triangle, and icon wanted sizes convert through wm_ui_scale_px at a caller-supplied factor."
    requirement: UI-03
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_menu_and_icon_wanted_sizes"
        status: pass
    human_judgment: false
  - id: D4
    description: "A source hot-spot scaled at 2x is monitor_scale_apply then clamped into [0, width-1]."
    requirement: UI-04
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_hotspot_scaled_then_clamped"
        status: pass
    human_judgment: false
  - id: D5
    description: "Edge source 0 stays 0 at every factor; edge 1 at 2x is 2; MoveThreshold 3 at 1.5x is 5; snap 8 at 2x is 16."
    requirement: UI-05
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_edge_zero_stays_zero"
        status: pass
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_move_threshold_and_snap_sources"
        status: pass
    human_judgment: false
  - id: D6
    description: "Explicit title height below the scaled font raises the titlebar to the font; overflow initializes outputs to 0."
    requirement: UI-06
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_explicit_title_below_font_is_raised"
        status: pass
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_overflow_initializes_output_zero"
        status: pass
    human_judgment: false

duration: 10min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 01: WM-Owned UI Metrics Converter Summary

**Allocation-free `wm_ui_scale_px` wraps `monitor_scale_apply` for chrome integers, with title composition, hotspot clamp, edge-zero, and four-field cache keys proven by native Meson fixtures.**

## Performance

- **Duration:** 10 min
- **Started:** 2026-08-31T13:57:11Z
- **Completed:** 2026-08-31T14:07:14Z
- **Tasks:** 3
- **Files modified:** 5

## Accomplishments

- Production `libfvwm3` exposes `wm_ui_scale_px` over `monitor_scale_apply` with no floating-point and no reimplemented quarter arithmetic
- Two-scale and generation cache-key inequality is asserted, not inferred
- Title composition never clips the scaled font; overflow and NULL outputs initialize to 0
- Hotspot clamp, disabled edge, MoveThreshold, snap, and wanted-size fixtures are native-tested without X

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: End-to-end source handle width** - `f959a5767` (test)
2. **Task 1 GREEN: Implement scaled chrome metric converter** - `05e69e0f8` (feat)
3. **Task 2 RED: Title composition and overflow fixtures** - `ed48f437d` (test)
4. **Task 2 GREEN: Compose title thickness without clipping** - `9b44f82db` (feat)
5. **Task 3 RED: Hotspot, edge, and wanted-size fixtures** - `5db06a903` (test)
6. **Task 3 GREEN: Scale hotspots, edges, and wanted sizes** - `2f9218c75` (feat)

**Plan metadata:** pending (docs: complete plan)

_Note: TDD tasks produced RED then GREEN commits; no REFACTOR was needed._

## Files Created/Modified

- `libs/wm_ui_metrics.h` - Public converter, cache-key, generation, hotspot, edge, and title-composition contracts
- `libs/wm_ui_metrics.c` - `monitor_scale_apply` wrappers that initialize outputs to 0
- `tests/test_wm_ui_metrics.c` - Native UI-01 through UI-06 integer and cache-key fixtures
- `libs/meson.build` - Registers `wm_ui_metrics.c` once after `monitor_topology.c`
- `tests/meson.build` - Registers `wm-ui-metrics` with `-UNDEBUG`

## Decisions Made

- Scaled actual pixels are the primary chrome representation; configured 1x integers remain the unscaled source input to `monitor_scale_apply` (GEO-07: no parallel logical-unit type).
- `wm_ui_cache_key` identity is the four integer fields (`scale_quarters`, `target_width`, `target_height`, `generation`). Exact C-string name walks stay in Picture/Flocale (plans 04-03 and 04-04).
- `wm_ui_resource_generation_current` returns `WM_UI_RESOURCE_GENERATION_INITIAL` (1) and is not incremented in this phase.
- Title thickness is already-scaled font height plus a fresh conversion of `EXTRA_TITLE_FONT_HEIGHT`. An explicit source that scales below the font raises thickness to the font; surplus above font-plus-extra is centered into offset.

## Deviations from Plan

None - plan executed exactly as written.

---

**Total deviations:** 0 auto-fixed
**Impact on plan:** None.

## TDD Gate Compliance

| Task | RED | GREEN | REFACTOR | Status |
|------|-----|-------|----------|--------|
| 1 tracer | `f959a5767` | `05e69e0f8` | — | Pass |
| 2 title/overflow | `ed48f437d` | `9b44f82db` | — | Pass |
| 3 hotspot/edge | `5db06a903` | `2f9218c75` | — | Pass |

RED tests failed before implementation (missing header, then implicit declarations). GREEN `meson test wm-ui-metrics` and plan-end `monitor-scale` are green.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 04-02 (supported feature profile) and 04-03/04-04 (Picture/Flocale cache keys consuming `wm_ui_cache_key`)
- Window chrome wiring remains plan 04-05; cursor loading 04-07; map-time edge/snap 04-08
- No blockers

## Self-Check: PASSED

- `libs/wm_ui_metrics.h` FOUND
- `libs/wm_ui_metrics.c` FOUND
- `tests/test_wm_ui_metrics.c` FOUND
- Commits `f959a5767`, `05e69e0f8`, `ed48f437d`, `9b44f82db`, `5db06a903`, `2f9218c75` FOUND
- `meson test wm-ui-metrics monitor-scale` passed in Podman Linux

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*
