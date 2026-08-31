---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 05
subsystem: window-chrome
tags: [hidpi, titlebar, border, font, wm_ui_metrics, c11]

requires:
  - phase: 04-wm-owned-ui-metrics-and-resources
    provides: wm_ui_scale_px, wm_ui_compose_title_thickness, FlocaleLoadFontAtScale
provides:
  - SelectDecor converts clamped BorderWidth/HandleWidth through wm_ui_scale_px
  - setup_window_font and setup_icon_font open faces via FlocaleLoadFontAtScale
  - get_title_font_size_and_offset composes thickness through wm_ui_compose_title_thickness
affects: [04-06-menus-icons, 04-09-miniicon-buttons, UI-01, UI-06]

actuals:
  tokens: 1574
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - Clamp chrome source integers before wm_ui_scale_px; keep source on overflow
    - Owning factor is fw->m->effective_scale.factor else MONITOR_SCALE_QUARTERS_MIN
    - Title thickness is one composed result; font height is not converted a second time

key-files:
  created: []
  modified:
    - fvwm/decorations.c
    - fvwm/add_window.c
    - fvwm/geometry.c

key-decisions:
  - "Clamp BorderWidth/HandleWidth against MAX_BORDER_WIDTH first, then wm_ui_scale_px; keep the clamped source and fvwm_debug on conversion failure or SHRT_MAX overflow."
  - "Use fw->m->effective_scale.factor when fw->m is non-NULL, else MONITOR_SCALE_QUARTERS_MIN."
  - "Pass already-loaded title_font->height as scaled_font_height; EXTRA_TITLE_FONT_HEIGHT is a source argument to wm_ui_compose_title_thickness."
  - "Title and icon fonts both open through FlocaleLoadFontAtScale at generation 1."

patterns-established:
  - "Pattern: chrome setup converts source integers at SelectDecor/setup_window_font/get_title_font_size_and_offset, never in the draw path."
  - "Pattern: overflow of short chrome fields keeps last-good or unscaled source and fvwm_debug; it never truncates into short."
  - "Pattern: title composition is one helper result plus FlocaleGetMinOffset; explicit height below the font is raised by wm_ui_compose_title_thickness."

requirements-completed: [UI-01, UI-06]

coverage:
  - id: D1
    description: "SelectDecor converts clamped BorderWidth and HandleWidth through wm_ui_scale_px using the owning-monitor factor and keeps the source on overflow."
    requirement: UI-01
    verification:
      - kind: other
        ref: "grep wm_ui_scale_px fvwm/decorations.c"
        status: pass
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_source_handle_width_scales_at_1x_and_2x"
        status: pass
    human_judgment: false
  - id: D2
    description: "setup_window_font loads the title font with FlocaleLoadFontAtScale at fw->m->effective_scale.factor and WM_UI_RESOURCE_GENERATION_INITIAL; icon fonts use the same factor."
    requirement: UI-01
    verification:
      - kind: other
        ref: "grep FlocaleLoadFontAtScale fvwm/add_window.c"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics"
        status: pass
    human_judgment: false
  - id: D3
    description: "get_title_font_size_and_offset composes title thickness from one wm_ui_compose_title_thickness result so an explicit title shorter than the font is raised."
    requirement: UI-06
    verification:
      - kind: other
        ref: "grep wm_ui_compose_title_thickness fvwm/geometry.c"
        status: pass
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_explicit_title_below_font_is_raised"
        status: pass
      - kind: unit
        ref: "meson test icccm-size-hints"
        status: pass
    human_judgment: false

duration: 8min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 05: Window Chrome Metrics Summary

**SelectDecor, title/icon font load, and title composition now consume owning-monitor scale so window chrome metrics are actual scaled pixels without clipping the font or converting client size.**

## Performance

- **Duration:** 8 min
- **Started:** 2026-08-31T14:39:07Z
- **Completed:** 2026-08-31T14:48:06Z
- **Tasks:** 3
- **Files modified:** 3

## Accomplishments

- `SelectDecor` clamps `BorderWidth`/`HandleWidth` against `MAX_BORDER_WIDTH`, then converts with `wm_ui_scale_px`
- `setup_window_font` and `setup_icon_font` open faces through `FlocaleLoadFontAtScale` before title geometry
- `get_title_font_size_and_offset` composes thickness via `wm_ui_compose_title_thickness` and never independently rounds font plus extra
- Overflow of `short` chrome fields keeps last-good/unscaled source and `fvwm_debug`
- `g.normal`/`g.frame` client size and `libs/vpacket.h` were not converted

## Task Commits

Each task was committed atomically:

1. **Task 1: Scale BorderWidth and HandleWidth at SelectDecor** - `f5fbe117a` (feat)
2. **Task 2: Load the title font at owning-monitor pixelsize** - `d1fdeff3e` (feat)
3. **Task 3: Compose title thickness from one scaled result** - `1ebe0a345` (feat)

**Plan metadata:** pending docs commit

## Files Created/Modified

- `fvwm/decorations.c` - Owning-monitor `wm_ui_scale_px` after source clamp in `SelectDecor`
- `fvwm/add_window.c` - `FlocaleLoadFontAtScale` for title and icon fonts
- `fvwm/geometry.c` - `wm_ui_compose_title_thickness` in `get_title_font_size_and_offset`

## Decisions Made

- Clamp style source against `MAX_BORDER_WIDTH`/`MAX_HANDLE_WIDTH` first, then convert with `fw->m->effective_scale.factor` when `fw->m` is set, else `MONITOR_SCALE_QUARTERS_MIN`. Conversion failure or a result above `SHRT_MAX` keeps the clamped source and logs with `fvwm_debug`.
- Title and icon fonts open at that same owning factor and `WM_UI_RESOURCE_GENERATION_INITIAL`. Icon load lives in `setup_icon_font` in the same file because that is the existing icon-font seam.
- Pass already-loaded `fw->title_font->height` as `scaled_font_height`. `EXTRA_TITLE_FONT_HEIGHT`, `decor->title_height`, and `decor->min_title_height` are source integers into `wm_ui_compose_title_thickness`. On failure, keep unscaled font-plus-extra and `min_offset`.
- `title_thickness` and `boundary_width` remain `short` actual X11 pixels. Client rectangles and module packets stay unconverted.

## Deviations from Plan

None - plan executed exactly as written.

---

**Total deviations:** 0 auto-fixed
**Impact on plan:** None.

## TDD Gate Compliance

Plan frontmatter is `type: execute` (`tdd_mode` false). Tasks marked `tdd="true"` are core `fvwm3` wiring, not symbols linked by `meson test wm-ui-metrics`. RED commits were not added because a failing unit test of `SelectDecor`/`setup_window_font`/`get_title_font_size_and_offset` would require extracting those functions from the window-manager process (Rule 4). Conversion behavior remains covered by existing 04-01 fixtures; GREEN is the three `feat(04-05)` commits plus Podman `meson test wm-ui-metrics` / `icccm-size-hints`.

| Task | RED | GREEN | REFACTOR | Status |
|------|-----|-------|----------|--------|
| 1 SelectDecor scale | — | `f5fbe117a` | — | Wiring only |
| 2 FlocaleLoadFontAtScale | — | `d1fdeff3e` | — | Wiring only |
| 3 title composition | — | `1ebe0a345` | — | Wiring only |

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 04-06 (menus, icons, and feedback at active-monitor scale)
- `libs/vpacket.h` and `fvwm/module_interface.c` were not edited
- Drawing files were not rescaled; title buttons still size to stored `title_thickness`
- No blockers

## Self-Check: PASSED

- `fvwm/decorations.c` FOUND
- `fvwm/add_window.c` FOUND
- `fvwm/geometry.c` FOUND
- Commits `f5fbe117a`, `d1fdeff3e`, `1ebe0a345` FOUND
- `meson test wm-ui-metrics` and `icccm-size-hints` passed in Podman Linux

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*
