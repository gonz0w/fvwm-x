---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 12
subsystem: window-chrome
tags: [hidpi, addwindow, defaultfont, SelectDecor, flocale, wm_ui_metrics, c11]

requires:
  - phase: 04-wm-owned-ui-metrics-and-resources
    provides: bind_default_font_at_scale, SelectDecor owning-factor fallback, update_fvwm_monitor
provides:
  - Post-ownership SelectDecor plus True font rebind in AddWindow
  - DefaultFont cache-hit count drop inside bind_default_font_at_scale
affects: [UI-01, UI-02, UI-04, UI-06, Phase 5]

actuals:
  tokens: 311
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - AddWindow re-runs SelectDecor and setup_*_font(..., True) after update_fvwm_monitor so chrome uses fw->m->effective_scale.factor
    - bind_default_font_at_scale unloads the extra FlocaleFont count when AtScale returns the fallback pointer and still never frees Scr.DefaultFont from a window slot

key-files:
  created: []
  modified:
    - fvwm/add_window.c

key-decisions:
  - "Always call setup_window_font and setup_icon_font with True after ownership, not only when USING_DEFAULT_* is set, so explicit WindowFont/IconFont also reopen at the owning factor."
  - "Re-run SelectDecor before the True font setup so setup_title_geometry consumes the owning-factor boundary_width; re-apply IS_TRANSIENT and wShaped follow-ups; do not call setup_style_and_decor again."
  - "When bind_default_font_at_scale cache-hits the fallback pointer, call FlocaleUnloadFont on that pointer and still return fallback so FlocaleFont->count is not leaked."

patterns-established:
  - "Pattern: keep the pre-ownership False font bind and first SelectDecor for placement, then a second chrome pass after fw->m is set."
  - "Pattern: cache-hit UnloadFont inside bind_default_font_at_scale only drops the extra count; destroy_window_font / destroy_icon_font still skip Scr.DefaultFont."

requirements-completed: [UI-01, UI-02, UI-04, UI-06]

coverage:
  - id: D1
    description: "After update_fvwm_monitor(fw) in AddWindow, SelectDecor then setup_window_font/setup_icon_font with True rebind title/icon faces and border/handle/title thickness at the owning-monitor factor."
    requirement: UI-01
    verification:
      - kind: other
        ref: "python source-contract on fvwm/add_window.c AddWindow span after update_fvwm_monitor before BroadcastConfig(M_ADD_WINDOW"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics"
        status: pass
    human_judgment: false
  - id: D2
    description: "bind_default_font_at_scale drops the extra FlocaleFont count when AtScale returns the fallback pointer; destroy_window_font and destroy_icon_font still skip FlocaleUnloadFont when the slot is Scr.DefaultFont."
    requirement: UI-06
    verification:
      - kind: other
        ref: "python source-contract on bind_default_font_at_scale FlocaleLoadFontAtScale/FlocaleUnloadFont and destroy_*_font != Scr.DefaultFont"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics"
        status: pass
    human_judgment: false

duration: 5min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 12: AddWindow Ownership Chrome Rebind Summary

**AddWindow rebinds title/icon fonts and recomputes SelectDecor after fw->m is set, and bind_default_font_at_scale drops the extra FlocaleFont count when a 1x AtScale cache-hit returns Scr.DefaultFont.**

## Performance

- **Duration:** 5 min
- **Started:** 2026-08-31T19:41:51Z
- **Completed:** 2026-08-31T19:46:59Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments

- After `update_fvwm_monitor(fw)` and before `BroadcastConfig(M_ADD_WINDOW, fw)`, `AddWindow` re-runs `SelectDecor(fw, &style, &buttons)` so border/handle thickness use `fw->m->effective_scale.factor` instead of `MONITOR_SCALE_QUARTERS_MIN`
- The same span re-applies the `IS_TRANSIENT` `!do_decorate_transient` and `FHaveShapeExtension` `wShaped` follow-ups from `setup_style_and_decor`, then calls `setup_window_font(fw, &style, True)` and `setup_icon_font(fw, &style, True)`
- Default-config and explicit `WindowFont`/`IconFont` map paths therefore destroy and rebind at the owning factor; `setup_title_geometry` inside the True font setup consumes the owning-factor border
- `bind_default_font_at_scale` still loads `fallback->name` via `FlocaleLoadFontAtScale`; NULL fallback still returns NULL; NULL loaded still returns fallback; when loaded equals fallback it calls `FlocaleUnloadFont(dpy, loaded)` and returns fallback
- `destroy_window_font` and `destroy_icon_font` still skip `FlocaleUnloadFont` when the slot pointer is `Scr.DefaultFont`

## Task Commits

Each task was committed atomically:

1. **Task 1: End-to-end chrome bind after ownership — one path only** - `96b6991ab` (feat)
2. **Task 2: Drop DefaultFont cache-hit count on AtScale fallback** - `40429438e` (feat)

_Note: Tasks carried `tdd="true"` but the plan forbids new test symbols because `FlocaleLoadFontAtScale` requires a Display. Proof is the source-contract python in each `<verify>` plus existing `meson test wm-ui-metrics`._

## Files Created/Modified

- `fvwm/add_window.c` - Post-ownership SelectDecor plus True font rebind; DefaultFont cache-hit count drop

## Decisions Made

- Always call `setup_window_font` and `setup_icon_font` with True after ownership, not only when `USING_DEFAULT_*` is set, so explicit WindowFont/IconFont also reopen at the owning factor
- Re-run `SelectDecor` before the True font setup so `setup_title_geometry` consumes the owning-factor `boundary_width`; re-apply transient and shaped follow-ups; do not call `setup_style_and_decor` again
- Keep the pre-ownership False font bind and first SelectDecor; this plan adds a second pass after `update_fvwm_monitor` rather than moving the first calls
- When AtScale returns the fallback pointer, drop the extra count with `FlocaleUnloadFont` and still return fallback; window slots never free `Scr.DefaultFont`

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

- Default-config and explicit-font map paths bind title/icon faces and border/handle/title thickness at the owning-monitor factor after `fw->m` is set
- A 1x DefaultFont cache-hit does not leak `FlocaleFont->count`; `Scr.DefaultFont` is not freed from a window slot
- Phase 4 gap-closure plans are complete; ready for `/gsd-verify-work 04`
- Frame size is still the pre-ownership estimate (`frame_move_resize` is not re-run); converting `g.normal` / `g.frame` remains Phase 5

## Self-Check: PASSED

- FOUND: `fvwm/add_window.c`
- FOUND: `96b6991ab`
- FOUND: `40429438e`
- FOUND: AddWindow post-ownership SelectDecor and True font rebind source-contract
- FOUND: bind_default_font_at_scale cache-hit UnloadFont source-contract
- FOUND: `meson test wm-ui-metrics` green

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*
