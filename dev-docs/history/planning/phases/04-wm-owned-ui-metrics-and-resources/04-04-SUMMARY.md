---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 04
subsystem: flocale-fonts
tags: [hidpi, xft, flocale, cache-key, pixelsize, c11]

requires:
  - phase: 04-wm-owned-ui-metrics-and-resources
    provides: wm_ui_scale_px and WM_UI_RESOURCE_GENERATION_INITIAL
provides:
  - Flocale font cache identity includes name bytes, scale_quarters, and generation
  - FlocaleLoadFontAtScale scaled entry point
  - Compatible FlocaleLoadFont wrapper at 1x generation 1
  - FftGetFontAtScale opens Xft at an explicit FC_PIXEL_SIZE
affects: [04-05-window-chrome, UI-02]

actuals:
  tokens: 3661
  tasks: 2
  commits: 5

tech-stack:
  added: []
  patterns:
    - Flocale lookup agrees on name bytes, scale_quarters, and generation
    - Unscaled FlocaleLoadFont forwards MONITOR_SCALE_QUARTERS_MIN and WM_UI_RESOURCE_GENERATION_INITIAL
    - Xft scale is an integer FC_PIXEL_SIZE rewrite after XftFontMatch, never a display-wide DPI write

key-files:
  created: []
  modified:
    - libs/Flocale.h
    - libs/Flocale.c
    - libs/Fft.c
    - libs/FftInterface.h
    - tests/test_wm_ui_metrics.c

key-decisions:
  - "New FlocaleLoadFontAtScale rather than breaking FlocaleLoadFont; missing extras equal quarters 4 and generation 1."
  - "FftGetFontAtScale rewrites FC_PIXEL_SIZE after XftFontMatch from wm_ui_scale_px on the unmatched source pixel size; FftGetFont stays the 1x wrapper."
  - "XLFD pixel-field rewrite is best-effort; parse or load failure keeps 1x and fvwm_debug."
  - "Display-wide fontconfig DPI is not the scale mechanism."

patterns-established:
  - "Pattern: Flocale cache hits require matching scale_quarters and generation in addition to name bytes."
  - "Pattern: FlocaleLoadFont remains the three-arg module-facing wrapper and forwards 1x generation 1."
  - "Pattern: Xft faces open at an explicit integer pixelsize; XftDefaultSet is never used to publish DPI."

requirements-completed: [UI-02]

coverage:
  - id: D1
    description: "FlocaleFont stores scale_quarters and generation after count; same name at 2x or a later generation misses the 1x face."
    requirement: UI-02
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_flocale_lookup_requires_scale_and_generation"
        status: pass
    human_judgment: false
  - id: D2
    description: "FlocaleLoadFontAtScale is the scaled entry point; FlocaleLoadFont still has the original three parameters and forwards MONITOR_SCALE_QUARTERS_MIN and WM_UI_RESOURCE_GENERATION_INITIAL."
    requirement: UI-02
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_flocale_wrapper_forwards_1x_generation_1"
        status: pass
      - kind: other
        ref: "grep FlocaleLoadFontAtScale plus scale_quarters in libs/Flocale.c"
        status: pass
    human_judgment: false
  - id: D3
    description: "Xft opens at an integer FC_PIXEL_SIZE produced by wm_ui_scale_px from the source font pixel size; display-wide fontconfig DPI is not written."
    requirement: UI-02
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_fft_get_font_at_scale_exists"
        status: pass
      - kind: other
        ref: "grep FC_PIXEL_SIZE and wm_ui_scale_px in libs/Fft.c; meson test wm-ui-metrics"
        status: pass
    human_judgment: false

duration: 8min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 04: Scale-Aware Flocale/Xft Font Cache Summary

**Flocale cache identity now includes name, scale quarters, and generation, with `FlocaleLoadFontAtScale` as the scaled entry and Xft faces opened at an explicit `FC_PIXEL_SIZE` rather than a display-wide DPI write.**

## Performance

- **Duration:** 8 min
- **Started:** 2026-08-31T14:27:19Z
- **Completed:** 2026-08-31T14:36:13Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- `_FlocaleFont` stores `scale_quarters` and `generation` after `count`
- Cache lookup hits only when name bytes, quarters, and generation all match
- `FlocaleLoadFontAtScale` is the scaled entry; unscaled callers still compile against `FlocaleLoadFont`
- `FftGetFontAtScale` rewrites matched Xft patterns to an integer `FC_PIXEL_SIZE` from `wm_ui_scale_px`
- XLFD/core opens attempt a pixel-field rewrite and keep 1x with `fvwm_debug` on failure

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: Flocale cache extras** - `a064a6916` (test)
2. **Task 1 GREEN: Key fonts on name, quarters, and generation** - `1ffef3174` (feat)
3. **Task 2 RED: FftGetFontAtScale declaration** - `497a1ad6e` (test)
4. **Task 2 GREEN: Open Xft at converted pixelsize** - `d745018e9` (feat)

**Plan metadata:** pending docs commit

_Note: TDD tasks produced RED then GREEN commits; no REFACTOR was needed._

## Files Created/Modified

- `libs/Flocale.h` - `scale_quarters` and `generation` after count; `FlocaleLoadFontAtScale` declaration
- `libs/Flocale.c` - Cache extras agreement; 1x wrapper; XLFD pixel-field rewrite; factor threaded into Fft
- `libs/FftInterface.h` - `FftGetFontAtScale` declaration; original three-arg `FftGetFont` kept
- `libs/Fft.c` - `FC_PIXEL_SIZE` rewrite via `wm_ui_scale_px` after `XftFontMatch`; 1x wrapper
- `tests/test_wm_ui_metrics.c` - Native fixtures for Flocale extras, the scaled loader, and `FftGetFontAtScale`

## Decisions Made

- New `FlocaleLoadFontAtScale` rather than breaking `FlocaleLoadFont`. Missing extras equal quarters 4 (`MONITOR_SCALE_QUARTERS_MIN`) and generation 1 (`WM_UI_RESOURCE_GENERATION_INITIAL`).
- `FftGetFontAtScale` takes the caller factor and rewrites `FC_PIXEL_SIZE` after a successful `XftFontMatch`, using the unmatched source pixel size or the matched 1x size. `FftGetFont` remains the 1x wrapper.
- XLFD/core font open is best-effort: rewrite the pixel field; on parse or load failure keep the original 1x name and `fvwm_debug`.
- Display-wide fontconfig DPI is not the scale mechanism. `XftDefaultSet` and `XftDefaultSubstitute` are not used to publish a new DPI. Overflow from `wm_ui_scale_px` keeps the matched 1x pixelsize and does not store 0 as a face size.

## Deviations from Plan

None - plan executed exactly as written.

---

**Total deviations:** 0 auto-fixed
**Impact on plan:** None.

## TDD Gate Compliance

| Task | RED | GREEN | REFACTOR | Status |
|------|-----|-------|----------|--------|
| 1 cache extras | `a064a6916` | `1ffef3174` | — | Pass |
| 2 Xft pixelsize | `497a1ad6e` | `d745018e9` | — | Pass |

RED tests failed before implementation (missing `FlocaleFont` extras / `FlocaleLoadFontAtScale`, then undeclared `FftGetFontAtScale`). GREEN `meson test wm-ui-metrics` is green.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 04-05 (window chrome wiring title fonts through `FlocaleLoadFontAtScale`)
- `libs/vpacket.h` and drawing call sites were not edited
- No blockers

## Self-Check: PASSED

- `libs/Flocale.h` FOUND
- `libs/Flocale.c` FOUND
- `libs/Fft.c` FOUND
- `libs/FftInterface.h` FOUND
- `tests/test_wm_ui_metrics.c` FOUND
- Commits `a064a6916`, `1ffef3174`, `497a1ad6e`, `d745018e9` FOUND
- `meson test wm-ui-metrics` passed in Podman Linux

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*
