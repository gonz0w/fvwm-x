---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 07
subsystem: images-cursors
tags: [hidpi, svg, png, xcursor, hotspot, wm_ui_metrics, c11]

requires:
  - phase: 04-wm-owned-ui-metrics-and-resources
    provides: wm_ui_scale_px, wm_ui_scale_hotspot, PCacheFvwmPictureAtScale
provides:
  - PImageLoadFvwmPictureFromFile threads factor and target pixels
  - SVG/PNG/XPM rasterize once into the destination rectangle
  - WM cursors load at scaled 16px nominal with clamped hot spots
affects: [04-08-feedback-pan-snap, 04-09-docs-ci, UI-04, UI-06]

actuals:
  tokens: 5797
  tasks: 2
  commits: 4

tech-stack:
  added: []
  patterns:
    - PImageLoadFvwmPictureFromFile takes factor plus targets; PImageLoadPixmapFromFile stays 1x 0,0
    - 0,0 destinations convert intrinsic size with wm_ui_scale_px
    - Xcursor loads at explicit nominal size; never XcursorSetDefaultSize

key-files:
  created: []
  modified:
    - libs/Picture.c
    - libs/PictureImageLoader.c
    - libs/PictureImageLoader.h
    - libs/Fcursor.h
    - fvwm/cursor.c
    - tests/test_wm_ui_metrics.c

key-decisions:
  - "PImageLoadFvwmPictureFromFile gained factor, target_width, and target_height; PImageLoadPixmapFromFile still feeds MONITOR_SCALE_QUARTERS_MIN and 0,0."
  - "When both targets are 0, SVG/PNG/XPM convert intrinsic size with wm_ui_scale_px; PNG/XPM decode the original buffer then blit once."
  - "librsvg 2.52+ renders into the destination viewport; cairo does not scale a 1x raster into a 2x surface."
  - "PImageLoadCursorFromFile loads at wm_ui_scale_px(WM_UI_XCURSOR_NOMINAL_SOURCE_PX, factor) and clamps hot spots with wm_ui_scale_hotspot."
  - "CreateCursors caches theme cursors per CRS index, quarters, and WM_UI_RESOURCE_GENERATION_INITIAL using XcursorShapeLoadImages(shape, theme, size)."

patterns-established:
  - "Pattern: cache-miss picture loads thread the request factor and targets into PImageLoadFvwmPictureFromFile."
  - "Pattern: destination size is caller targets when both are >0, else wm_ui_scale_px of intrinsic; FImageSizeIsSafe still gates alloc."
  - "Pattern: cursor nominal size is an explicit integer; display-wide Xcursor default size is not the scale mechanism."

requirements-completed: [UI-04, UI-06]

coverage:
  - id: D1
    description: "SVG render viewport and PNG/XPM destination size equal the scaled target pixels; 0,0 destinations convert intrinsic size with wm_ui_scale_px and FImageSizeIsSafe still gates alloc."
    requirement: UI-06
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_pimage_load_fvwm_picture_from_file_takes_scale_and_targets"
        status: pass
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_zero_targets_convert_intrinsic_with_factor"
        status: pass
      - kind: other
        ref: "meson test wm-ui-metrics image-safety"
        status: pass
    human_judgment: false
  - id: D2
    description: "WM cursors load at wm_ui_scale_px(WM_UI_XCURSOR_NOMINAL_SOURCE_PX, active factor) with hot spots converted then clamped."
    requirement: UI-04
    verification:
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_pimage_load_cursor_from_file_takes_scale_factor"
        status: pass
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_xcursor_nominal_source_scales_at_1x_and_2x"
        status: pass
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_hotspot_scaled_then_clamped"
        status: pass
      - kind: other
        ref: "grep wm_ui_scale_hotspot libs/PictureImageLoader.c && grep wm_ui_scale_px fvwm/cursor.c"
        status: pass
    human_judgment: false
  - id: D3
    description: "Display-global cursor default size is not written; CreateCursors caches per shape, quarters, and generation."
    requirement: UI-04
    verification:
      - kind: other
        ref: "grep XcursorSetDefaultSize fvwm/cursor.c libs/PictureImageLoader.c (absent)"
        status: pass
      - kind: other
        ref: "meson test wm-ui-metrics image-safety"
        status: pass
    human_judgment: false

duration: 14min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 07: Scaled Image Raster and Cursor Loading Summary

**SVG/PNG rasterize once into the scaled target rectangle and WM cursors load at wm_ui_scale_px of 16px with clamped hot spots, never XcursorSetDefaultSize**

## Performance

- **Duration:** 14 min
- **Started:** 2026-08-31T15:12:51Z
- **Completed:** 2026-08-31T15:27:30Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- `PImageLoadFvwmPictureFromFile` takes `monitor_scale_factor`, `target_width`, and `target_height`; `PCacheFvwmPictureAtScale` threads those on cache miss
- SVG `rsvg_handle_render_document` viewport equals destination pixels; 0,0 targets convert intrinsic size with `wm_ui_scale_px`
- PNG/XPM decode the original buffer then blit once into the destination rectangle, still gated by `FImageSizeIsSafe`
- `PImageLoadCursorFromFile` loads at explicit nominal size and clamps hot spots; `CreateCursors` caches theme shapes per CRS index, quarters, and generation

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: Scaled picture load signature** - `4105c8069` (test)
2. **Task 1 GREEN: Raster SVG and PNG once at target pixels** - `7eeb0c9f5` (feat)
3. **Task 2 RED: Scaled cursor load signature** - `d64f6f5de` (test)
4. **Task 2 GREEN: Load WM cursors at scaled nominal size** - `8db4ab7da` (feat)

**Plan metadata:** pending docs commit

_Note: TDD tasks produced RED then GREEN commits; no REFACTOR was needed._

## Files Created/Modified

- `libs/PictureImageLoader.h` - `PImageLoadFvwmPictureFromFile` and `PImageLoadCursorFromFile` take scale extras; `PImageLoadPixmapFromFile` signature unchanged
- `libs/PictureImageLoader.c` - `FIMAGE_CMD_ARGS` threads factor and targets; destination resolve + one blit; SVG viewport; scaled cursor load
- `libs/Picture.c` - cache-miss and `PGetFvwmPicture` pass factor and targets
- `libs/Fcursor.h` - `FcursorShapeLoadImages` and `FcursorGetTheme` wrappers
- `fvwm/cursor.c` - pointer-monitor factor, theme load at nominal size, per-slot cache table
- `tests/test_wm_ui_metrics.c` - signature and 0,0 / 16px nominal fixtures

## Decisions Made

- New parameters on `PImageLoadFvwmPictureFromFile` rather than changing public `PImageLoadPixmapFromFile` (FvwmScript, fvwm-root, and icons.c keep compiling). 1x wrappers pass `MONITOR_SCALE_QUARTERS_MIN` and 0,0.
- When both targets are greater than 0, that rectangle is the destination; when both are 0, convert intrinsic width/height with the threaded factor. Mixed one-zero SVG path options stay the existing render-opts aspect fill.
- librsvg 2.52+ sets `vp.width`/`vp.height` to destination pixels and does not cairo-scale a 1x raster. Older librsvg keeps drawing-op cairo transforms.
- Cursor nominal size is `wm_ui_scale_px(WM_UI_XCURSOR_NOMINAL_SOURCE_PX, factor)` passed to `FcursorFilenameLoadImages` / `XcursorShapeLoadImages`. Overflow keeps 16px and `fvwm_debug`. Theme load uses `XcursorGetTheme`; core `XCreateFontCursor` remains the fallback.
- Cursor table identity is CRS index, `factor.quarters`, and `WM_UI_RESOURCE_GENERATION_INITIAL`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] XcursorShapeLoadImages is three-argument**
- **Found during:** Task 2 (GREEN compile)
- **Issue:** Plan/Fcursor wrapper assumed `(shape, size)`. Installed Xcursor declares `XcursorShapeLoadImages(unsigned int shape, const char *theme, int size)`.
- **Fix:** Wrap the three-argument API and pass `FcursorGetTheme(disp)` as theme (NULL uses the default theme).
- **Files modified:** `libs/Fcursor.h`, `fvwm/cursor.c`
- **Verification:** `meson compile` succeeds
- **Committed in:** `8db4ab7da` (Task 2 GREEN)

**2. [Rule 1 - Bug] FQueryPointer returns void**
- **Found during:** Task 2 (GREEN compile)
- **Issue:** `cursor_active_factor` treated `FQueryPointer` as a boolean, but `libs/FEvent.h` declares it `void`.
- **Fix:** Call `FQueryPointer` then `FindScreenOfXY`, falling back to `monitor_get_current()`.
- **Files modified:** `fvwm/cursor.c`
- **Verification:** `meson compile` succeeds
- **Committed in:** `8db4ab7da` (Task 2 GREEN)

---

**Total deviations:** 2 auto-fixed (2 Rule 1)
**Impact on plan:** Both were compile blockers for the planned Xcursor/pointer-monitor path. No scope creep.

## TDD Gate Compliance

| Task | RED | GREEN | REFACTOR | Status |
|------|-----|-------|----------|--------|
| 1 scaled picture load | `4105c8069` | `7eeb0c9f5` | — | Pass |
| 2 scaled cursor load | `d64f6f5de` | `8db4ab7da` | — | Pass |

RED tests failed before implementation (incompatible `PImageLoadFvwmPictureFromFile` then `PImageLoadCursorFromFile` pointer types). GREEN `meson test wm-ui-metrics image-safety` is green.

## Issues Encountered

None beyond the two compile fixes above.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 04-08 (feedback, pan, snap metrics)
- Display-wide `XcursorSetDefaultSize` was not added
- `libs/vpacket.h` and `fvwm/module_interface.c` were not edited
- No blockers

## Self-Check: PASSED

- `libs/Picture.c` FOUND
- `libs/PictureImageLoader.c` FOUND
- `libs/PictureImageLoader.h` FOUND
- `libs/Fcursor.h` FOUND
- `fvwm/cursor.c` FOUND
- `tests/test_wm_ui_metrics.c` FOUND
- Commits `4105c8069`, `7eeb0c9f5`, `d64f6f5de`, `8db4ab7da` FOUND
- `meson test wm-ui-metrics image-safety` passed in Podman Linux

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*
