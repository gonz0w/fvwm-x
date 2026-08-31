---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 10
subsystem: window-chrome
tags: [hidpi, defaultfont, title-font, icon-font, flocale, wm_ui_metrics, c11]

requires:
  - phase: 04-wm-owned-ui-metrics-and-resources
    provides: FlocaleLoadFontAtScale, wm_ui_compose_title_thickness, bind_popup_font analog
provides:
  - bind_default_font_at_scale opens DefaultFont->name at owning-monitor factor
  - Default title and icon slots use AtScale faces before title geometry
  - CMD_DefaultFont rebinds USING_DEFAULT_* windows at each owning factor
affects: [UI-01, UI-06, 04-11]

actuals:
  tokens: 1071
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - Default chrome fonts open DefaultFont->name via FlocaleLoadFontAtScale; Scr.DefaultFont stays the 1x fallback
    - Destroy and DefaultFont rebind never FlocaleUnloadFont the process-global 1x DefaultFont pointer

key-files:
  created: []
  modified:
    - fvwm/add_window.h
    - fvwm/add_window.c
    - fvwm/builtins.c

key-decisions:
  - "bind_default_font_at_scale result becomes fw->title_font and fw->icon_font; Scr.DefaultFont remains the 1x fallback only when AtScale returns NULL."
  - "Keep SET_USING_DEFAULT_WINDOW_FONT / SET_USING_DEFAULT_ICON_FONT at 1 on the default-name path so CMD_DefaultFont and has_default_font_changed still select those windows."
  - "CMD_DefaultFont still loads the process-global face with FlocaleLoadFont and rebinds USING_DEFAULT_* windows before unloading the previous 1x face."

patterns-established:
  - "Pattern: default title/icon fonts match bind_popup_font — load by fallback->name at owning factor, keep fallback on NULL, never unload Scr.DefaultFont from a slot."
  - "Pattern: USING_DEFAULT_* remains set on the default-name path even when the slot holds an AtScale face, so DefaultFont command and style-update still rebind those windows."

requirements-completed: [UI-01, UI-06]

coverage:
  - id: D1
    description: "Windows with no Style WindowFont open Scr.DefaultFont->name through FlocaleLoadFontAtScale at the owning-monitor factor before setup_title_geometry, and destroy never unloads the process-global 1x DefaultFont pointer."
    requirement: UI-01
    verification:
      - kind: other
        ref: "python source-contract on fvwm/add_window.c bind_default_font_at_scale / setup_window_font / destroy_window_font"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics"
        status: pass
    human_judgment: false
  - id: D2
    description: "Default icon fonts and CMD_DefaultFont rebinds use owning-monitor AtScale faces; Scr.DefaultFont stays a FlocaleLoadFont 1x identity used only as fallback."
    requirement: UI-06
    verification:
      - kind: other
        ref: "python source-contract on setup_icon_font / destroy_icon_font / CMD_DefaultFont"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics"
        status: pass
    human_judgment: false

duration: 4min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 10: Default Title and Icon Fonts at Owning Scale Summary

**Default-config title and icon chrome open Scr.DefaultFont->name through FlocaleLoadFontAtScale at the owning-monitor factor so compose sees a scaled font height instead of mixing 1x DefaultFont with scaled extra pixels.**

## Performance

- **Duration:** 4 min
- **Started:** 2026-08-31T16:49:00Z
- **Completed:** 2026-08-31T16:52:56Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- Added `bind_default_font_at_scale(fallback, m)` which loads `fallback->name` via `FlocaleLoadFontAtScale` at `m->effective_scale.factor` (else `MONITOR_SCALE_QUARTERS_MIN`) and generation 1; NULL load keeps fallback and never calls `FlocaleUnloadFont`
- `setup_window_font` default-name path binds that AtScale face then calls `setup_title_geometry`, so `wm_ui_compose_title_thickness` receives a scaled font height
- `destroy_window_font` unloads a loaded title face when the pointer is not `Scr.DefaultFont`, dropping the `USING_DEFAULT_WINDOW_FONT` conjunct so AtScale default faces are released
- `setup_icon_font` default-name path uses the same helper; `destroy_icon_font` mirrors the title unload rule
- `CMD_DefaultFont` still loads the process-global face with `FlocaleLoadFont`, assigns `Scr.DefaultFont = new_font`, rebinds `USING_DEFAULT_*` windows through `bind_default_font_at_scale` at each window's owning factor, then unloads the previous 1x face

## Task Commits

Each task was committed atomically:

1. **Task 1: End-to-end default title font at owning scale** - `2c4acfc67` (feat)
2. **Task 2: Scale default icon fonts and DefaultFont rebind** - `654dacc9b` (feat)

**Plan metadata:** (pending docs commit)

_Note: Tasks carried `tdd="true"` but the plan forbids new test symbols because `FlocaleLoadFontAtScale` requires a Display. Proof is the source-contract python in each `<verify>` plus existing `meson test wm-ui-metrics`._

## Files Created/Modified

- `fvwm/add_window.h` - Declares `bind_default_font_at_scale`
- `fvwm/add_window.c` - Helper, default title/icon AtScale bind, destroy skips `Scr.DefaultFont`
- `fvwm/builtins.c` - `CMD_DefaultFont` rebinds `USING_DEFAULT_*` slots at owning scale before unloading the previous 1x face

## Decisions Made

- Promote PA-G10: the AtScale face is the window title/icon font; `Scr.DefaultFont` stays the process-global 1x generation-1 fallback only when AtScale returns NULL
- Keep `SET_USING_DEFAULT_WINDOW_FONT` / `SET_USING_DEFAULT_ICON_FONT` at 1 on the default-name path so `CMD_DefaultFont` and `has_default_font_changed` still select those windows
- Rebind `USING_DEFAULT_*` windows before unloading the previous process-global face; do not call `FlocaleLoadFontAtScale` for `Scr.DefaultFont` itself

## Deviations from Plan

None - plan executed exactly as written.

---

**Total deviations:** 0 auto-fixed
**Impact on plan:** None.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Default-config title and icon fonts follow the owning monitor's scale; compose sees a scaled default-font height
- `Scr.DefaultFont` remains the 1x fallback pointer and is not unloaded from a window slot
- Ready for 04-11 (remaining gap: pan-frame hit-testing / last-thickness bookkeeping)

## Self-Check: PASSED

- FOUND: `fvwm/add_window.h`
- FOUND: `fvwm/add_window.c`
- FOUND: `fvwm/builtins.c`
- FOUND: `2c4acfc67`
- FOUND: `654dacc9b`

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*
