---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 03
subsystem: picture-cache
tags: [hidpi, picture, cache-key, scale, c11]

requires:
  - phase: 04-wm-owned-ui-metrics-and-resources
    provides: wm_ui_cache_key and WM_UI_RESOURCE_GENERATION_INITIAL
provides:
  - FvwmPicture cache identity includes scale_quarters, target size, and generation
  - PCacheFvwmPictureAtScale scaled entry point
  - Compatible PCacheFvwmPicture wrapper at 1x generation 1
affects: [04-04-flocale, 04-07-cursor-loading, UI-02]

actuals:
  tokens: 1656
  tasks: 2
  commits: 4

tech-stack:
  added: []
  patterns:
    - Picture lookup agrees on name, stamp, fpa, scale_quarters, target size, and generation
    - Unscaled PCacheFvwmPicture forwards MONITOR_SCALE_QUARTERS_MIN and WM_UI_RESOURCE_GENERATION_INITIAL

key-files:
  created: []
  modified:
    - libs/PictureBase.h
    - libs/Picture.c
    - libs/Picture.h
    - tests/test_wm_ui_metrics.c

key-decisions:
  - "New PCacheFvwmPictureAtScale rather than breaking PCacheFvwmPicture; missing extras equal quarters 4 and generation 1."
  - "Do not reorder pixmap members on FvwmPicture; append the four unsigned cache-key fields after existing ones."

patterns-established:
  - "Pattern: Picture cache hits require matching scale_quarters, target_width, target_height, and generation in addition to name/stamp/fpa."
  - "Pattern: PCacheFvwmPicture remains the five-arg module-facing wrapper and forwards 1x generation 1 with target 0,0."

requirements-completed: [UI-02]

coverage:
  - id: D1
    description: "FvwmPicture stores scale_quarters, target_width, target_height, and generation after existing pixmap members."
    requirement: UI-02
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_picture_lookup_requires_scale_and_generation"
        status: pass
    human_judgment: false
  - id: D2
    description: "PCacheFvwmPicture lookup hits only when scale extras match in addition to name, stamp, and PICTURE_FPA_AGREE; 1x and 2x cannot share a pixmap."
    requirement: UI-02
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_picture_lookup_requires_scale_and_generation"
        status: pass
      - kind: other
        ref: "grep scale_quarters/generation plus PICTURE_FPA_AGREE in libs/Picture.c"
        status: pass
    human_judgment: false
  - id: D3
    description: "PCacheFvwmPictureAtScale is the scaled entry point; PCacheFvwmPicture still has the original five parameters and forwards MONITOR_SCALE_QUARTERS_MIN, target 0,0, and WM_UI_RESOURCE_GENERATION_INITIAL."
    requirement: UI-02
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_pcache_wrapper_forwards_1x_generation_1"
        status: pass
      - kind: other
        ref: "meson test wm-ui-metrics image-safety"
        status: pass
    human_judgment: false

duration: 6min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 03: Scaled Picture Cache Key Summary

**Picture cache identity now includes scale quarters, target size, and generation, with `PCacheFvwmPictureAtScale` as the scaled entry and the original five-arg `PCacheFvwmPicture` wrapper staying on 1x generation 1.**

## Performance

- **Duration:** 6 min
- **Started:** 2026-08-31T14:18:41Z
- **Completed:** 2026-08-31T14:24:56Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- `FvwmPicture` appends `scale_quarters`, `target_width`, `target_height`, and `generation` without reordering pixmap members
- Cache lookup requires those extras plus name, unchanged stamp, and `PICTURE_FPA_AGREE`
- `PCacheFvwmPictureAtScale` is the scaled entry; unscaled callers still compile against `PCacheFvwmPicture`
- Pixmap-wrapped cache entries store 1x generation-1 extras so later lookups stay consistent

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: Picture cache-key fields** - `54b67b20b` (test)
2. **Task 1 GREEN: Extend Picture cache identity** - `0a2092761` (feat)
3. **Task 2 RED: PCacheFvwmPictureAtScale declaration** - `0093bb2aa` (test)
4. **Task 2 GREEN: 1x generation-1 wrapper** - `0b2b25594` (feat)

**Plan metadata:** pending docs commit

_Note: TDD tasks produced RED then GREEN commits; no REFACTOR was needed._

## Files Created/Modified

- `libs/PictureBase.h` - Four unsigned cache-key fields appended on `FvwmPicture`
- `libs/Picture.c` - Lookup agreement and `PCacheFvwmPictureAtScale` body; 1x wrapper; pixmap extras
- `libs/Picture.h` - `PCacheFvwmPictureAtScale` declaration; original five-arg `PCacheFvwmPicture` kept
- `tests/test_wm_ui_metrics.c` - Native fixtures for extras agreement and the scaled entry point

## Decisions Made

- New `PCacheFvwmPictureAtScale` rather than breaking `PCacheFvwmPicture`. Missing extras equal quarters 4 (`MONITOR_SCALE_QUARTERS_MIN`) and generation 1 (`WM_UI_RESOURCE_GENERATION_INITIAL`). Target 0,0 means intrinsic decoded size after later raster scaling.
- Do not reorder pixmap members on `FvwmPicture`; append the four unsigned fields after existing ones.

## Deviations from Plan

None - plan executed exactly as written.

---

**Total deviations:** 0 auto-fixed
**Impact on plan:** None.

## TDD Gate Compliance

| Task | RED | GREEN | REFACTOR | Status |
|------|-----|-------|----------|--------|
| 1 cache-key fields | `54b67b20b` | `0a2092761` | — | Pass |
| 2 AtScale wrapper | `0093bb2aa` | `0b2b25594` | — | Pass |

RED tests failed before implementation (missing `FvwmPicture` extras, then undeclared `PCacheFvwmPictureAtScale`). GREEN `meson test wm-ui-metrics image-safety` is green.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 04-04 (Flocale cache keys) and later 04-07 raster scaling through `PCacheFvwmPictureAtScale`
- `libs/vpacket.h` and `fvwm/module_interface.c` were not edited
- No blockers

## Self-Check: PASSED

- `libs/PictureBase.h` FOUND
- `libs/Picture.c` FOUND
- `libs/Picture.h` FOUND
- `tests/test_wm_ui_metrics.c` FOUND
- Commits `54b67b20b`, `0a2092761`, `0093bb2aa`, `0b2b25594` FOUND
- `meson test wm-ui-metrics image-safety` passed in Podman Linux

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*
