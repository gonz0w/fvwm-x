---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 09
subsystem: window-chrome
tags: [hidpi, miniicon, titlestyle, buttonstyle, pcache, wm_ui_metrics, c11]

requires:
  - phase: 04-wm-owned-ui-metrics-and-resources
    provides: PCacheFvwmPictureAtScale, wm_ui_scale_px, FlocaleLoadFontAtScale
provides:
  - Owning-monitor MiniIcon cache load via PCacheFvwmPictureAtScale
  - Owning-monitor TitleStyle/ButtonStyle pixmap blit without sharing one scaled DecorFace
affects: [UI-01, UI-02]

actuals:
  tokens: 3009
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - Style MiniIcon loads at owning-monitor factor with converted 16-pixel wanted size
    - TitleStyle/ButtonStyle keep 1x PCacheFvwmPicture on DecorFace; draw resolves AtScale locally

key-files:
  created: []
  modified:
    - fvwm/add_window.c
    - fvwm/borders.c

key-decisions:
  - "setup_mini_icon calls PCacheFvwmPictureAtScale with fw->m->effective_scale.factor else MONITOR_SCALE_QUARTERS_MIN, converted 16-pixel wanted size, and generation 1; overflow passes target 0,0."
  - "TitleStyle/ButtonStyle command-time loads stay PCacheFvwmPicture 1x source identity; borders.c resolves PCacheFvwmPictureAtScale at draw with targets 0,0 and does not write the local back onto df->u.p."

patterns-established:
  - "Pattern: MiniIcon is a scale-once raster at setup; overflow uses intrinsic 0,0 rather than PCacheFvwmPicture."
  - "Pattern: process-global DecorFace pixmap stays 1x; each window blits a local AtScale picture and PDestroyFvwmPicture drops the extra cache weight."

requirements-completed: [UI-01, UI-02]

coverage:
  - id: D1
    description: "Style MiniIcon loads through PCacheFvwmPictureAtScale with the owning-monitor factor and converted 16-pixel wanted size."
    requirement: UI-01
    verification:
      - kind: other
        ref: "grep PCacheFvwmPictureAtScale fvwm/add_window.c"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics image-safety"
        status: pass
    human_judgment: false
  - id: D2
    description: "TitleStyle and ButtonStyle pixmap faces stay 1x source on DecorFace; borders.c draw/setup loads PCacheFvwmPictureAtScale with the owning-monitor factor so mixed-DPI windows do not share one scaled raster."
    requirement: UI-02
    verification:
      - kind: other
        ref: "grep PCacheFvwmPictureAtScale fvwm/borders.c && grep PCacheFvwmPicture fvwm/builtins.c"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics image-safety"
        status: pass
    human_judgment: false

duration: 10min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 09: MiniIcon and Title-Button Scale-Once Rasters Summary

**Style MiniIcon and TitleStyle/ButtonStyle pixmap faces load through PCacheFvwmPictureAtScale at the owning-monitor factor so chrome buttons do not share one 1x cached raster.**

## Performance

- **Duration:** 10 min
- **Started:** 2026-08-31T15:42:32Z
- **Completed:** 2026-08-31T15:52:39Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- `setup_mini_icon` loads Style MiniIcon through `PCacheFvwmPictureAtScale` using `fw->m->effective_scale.factor` (else `MONITOR_SCALE_QUARTERS_MIN`), converted source 16 as the wanted size, and `WM_UI_RESOURCE_GENERATION_INITIAL`
- Conversion overflow logs with `fvwm_debug` and still uses the AtScale entry with target 0,0 rather than `PCacheFvwmPicture`
- TitleStyle/ButtonStyle command-time loads in `fvwm/builtins.c` remain 1x `PCacheFvwmPicture` on the process-global DecorFace
- `border_draw_decor_to_pixmap` and MultiPixmap blit resolve a local via `PCacheFvwmPictureAtScale` with targets 0,0, use it for picture/mask/alpha/depth/width/height, and `PDestroyFvwmPicture` the extra cache weight without writing back onto `df->u.p`

## Task Commits

Each task was committed atomically:

1. **Task 1: Load Style MiniIcon at owning-monitor scale** - `9a6e50728` (feat)
2. **Task 2: Load TitleStyle and ButtonStyle pixmaps at owning-monitor draw** - `abd34131c` (feat)

## Files Created/Modified

- `fvwm/add_window.c` - `setup_mini_icon` loads MiniIcon at owning-monitor scale; 04-05 `FlocaleLoadFontAtScale` title/icon font loads kept
- `fvwm/borders.c` - draw-time AtScale blit for Pixmap/Shrunk/Stretched/Tiled/Adjusted and MultiPixmap faces

## Decisions Made

- MiniIcon wanted size is the integer 16 converted through `wm_ui_scale_px`; overflow uses intrinsic 0,0 so the AtScale cache key still includes quarters and generation
- Command-time DecorFace pixmap stays 1x so mixed-DPI windows do not share one scaled raster; each draw resolves a local AtScale picture
- `border_picture_at_owning_scale` centralizes factor, `PCacheFvwmPictureAtScale`, and load-failure `fvwm_debug` for the blit sites

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Scaled UseTitleStyle MultiPixmap pixmap blit in `border_draw_decor_to_pixmap`**
- **Found during:** Task 2 (TitleStyle/ButtonStyle pixmap blit)
- **Issue:** The plan named `border_mp_render_into_pixmap` (~2306) and the `df->u.p` sites, but the same 1x `pm[part]` blit also lives in `border_draw_decor_to_pixmap` for UseTitleStyle buttons
- **Fix:** Resolve those `pm[part]` pixmap faces through the same AtScale local and destroy the extra cache weight
- **Files modified:** fvwm/borders.c
- **Verification:** meson test wm-ui-metrics image-safety green; builtins.c still calls PCacheFvwmPicture
- **Committed in:** abd34131c (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** Same UI-02 pixmap-blit contract; no scope creep and no builtins.c edits.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 04 plan 09 is the last plan; MiniIcon and title-button pixmap faces now follow the owning-monitor scale-once cache
- Ready for phase verification / next step
- No blockers

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*

## Self-Check: PASSED
