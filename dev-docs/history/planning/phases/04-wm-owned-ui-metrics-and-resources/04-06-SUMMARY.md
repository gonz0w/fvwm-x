---
phase: 04-wm-owned-ui-metrics-and-resources
plan: 06
subsystem: menus-icons
tags: [hidpi, menu, icon, ewmh, wm_ui_metrics, c11]

requires:
  - phase: 04-wm-owned-ui-metrics-and-resources
    provides: wm_ui_scale_px, FlocaleLoadFontAtScale, PCacheFvwmPictureAtScale
provides:
  - Menu popup converts border and triangle source ints onto MenuRoot
  - Popup fonts and pictures load at the FSCREEN_XYPOS factor
  - Icon wanted sizes convert from icon-geometry monitor else pointer
affects: [04-07-cursors-feedback, 04-09-miniicon-buttons, UI-03]

actuals:
  tokens: 5522
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - Convert menu source ints onto MenuRoot popup fields at FSCREEN_XYPOS
    - MST_* layout macros read MenuRoot; ST_* stay MenuStyle 1x source
    - Icon wanted sizes convert once at create; file icons use PCacheFvwmPictureAtScale

key-files:
  created: []
  modified:
    - fvwm/menus.c
    - fvwm/menustyle.h
    - fvwm/menuroot.h
    - fvwm/menuitem.c
    - fvwm/menuitem.h
    - fvwm/ewmh_icons.c
    - fvwm/icons.c

key-decisions:
  - "Convert ST_BORDER_WIDTH and MENU_TRIANGLE_* through wm_ui_scale_px onto MenuRoot ints at FSCREEN_XYPOS; ST_BORDER_WIDTH stays the 0-50 MenuStyle source."
  - "MST_BORDER_WIDTH, MST_PSTDFONT, and MST_PTITLEFONT read MenuRoot popup fields so layout and paint consume active-monitor pixels without further call-site edits."
  - "menuitem_get_size takes MenuRoot *mr; menuitem_paint reads popup triangle ints and fonts via mpip->cb_mr."
  - "Icon wanted sizes and file-icon cache targets use the icon-geometry monitor, else the pointer; PIXMAP_OURS stays false for cache-backed file icons."

patterns-established:
  - "Pattern: popup converts source integers and binds AtScale fonts/pictures on MenuRoot before make_menu."
  - "Pattern: MenuStyle parse-time FlocaleLoadFont and PCacheFvwmPicture remain 1x generation-1 identity."
  - "Pattern: icon create converts MINI_ICON_WANTED_* / ICON_WANTED_* once; FEwmhIconFindBest stays the chooser."

requirements-completed: [UI-03]

coverage:
  - id: D1
    description: "At popup, FindScreenOfXY of FSCREEN_XYPOS converts MENU_TRIANGLE_* and ST_BORDER_WIDTH onto MenuRoot ints; menuitem size/paint consume those ints via mr/cb_mr."
    requirement: UI-03
    verification:
      - kind: other
        ref: "grep wm_ui_scale_px fvwm/menus.c && grep popup_triangle_width fvwm/menuroot.h"
        status: pass
      - kind: unit
        ref: "tests/test_wm_ui_metrics.c#test_menu_and_icon_wanted_sizes"
        status: pass
    human_judgment: false
  - id: D2
    description: "Popup item/title fonts load with FlocaleLoadFontAtScale and sidepic/mini-icons with PCacheFvwmPictureAtScale; MenuStyle keeps 1x source objects."
    requirement: UI-03
    verification:
      - kind: other
        ref: "grep FlocaleLoadFontAtScale fvwm/menus.c && grep PCacheFvwmPictureAtScale fvwm/menus.c"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics"
        status: pass
    human_judgment: false
  - id: D3
    description: "MINI_ICON_WANTED_* and ICON_WANTED_* convert once at icon create from icon-geometry else pointer and pass into FEwmhIconFindBest; file icons load via PCacheFvwmPictureAtScale."
    requirement: UI-03
    verification:
      - kind: other
        ref: "grep wm_ui_scale_px fvwm/ewmh_icons.c && grep PCacheFvwmPictureAtScale fvwm/icons.c"
        status: pass
      - kind: unit
        ref: "meson test wm-ui-metrics image-safety"
        status: pass
    human_judgment: false

duration: 19min
completed: 2026-08-31
status: complete
---

# Phase 4 Plan 06: Menu and Icon Active-Monitor Metrics Summary

**Menu popup converts border/triangle source integers onto MenuRoot and loads fonts/pictures at the FSCREEN_XYPOS factor; icon wanted sizes convert from the icon-geometry monitor else the pointer.**

## Performance

- **Duration:** 19 min
- **Started:** 2026-08-31T14:50:55Z
- **Completed:** 2026-08-31T15:10:14Z
- **Tasks:** 3
- **Files modified:** 7

## Accomplishments

- `update_menu` resolves `FindScreenOfXY` at the existing `FSCREEN_XYPOS` origin and converts `ST_BORDER_WIDTH`, `MENU_TRIANGLE_WIDTH`, and `MENU_TRIANGLE_HEIGHT` onto MenuRoot popup ints with a scaled `MAX_MENU_BORDER_WIDTH` clamp
- `MST_BORDER_WIDTH` reads the MenuRoot pixel so layout, paint, and menubindings hit tests follow the popup scale; `ST_BORDER_WIDTH` stays unsigned char source 0-50
- `menuitem_get_size` takes `MenuRoot *mr` and `menuitem_paint` reads triangle ints via `mpip->cb_mr`; neither function body uses the 1x `MENU_TRIANGLE_*` macros
- Popup item/title fonts load with `FlocaleLoadFontAtScale` and sidepic/mini-icons with `PCacheFvwmPictureAtScale`; `MST_PSTDFONT`/`MST_PTITLEFONT` read MenuRoot
- `EWMH_SetIconFromWMIcon` converts `MINI_ICON_WANTED_*`/`ICON_WANTED_*` and max caps through `wm_ui_scale_px`; `GetIconFromFile` loads via `PCacheFvwmPictureAtScale`
- `Scr.MoveThreshold` converts at the pointer-monitor drag comparison; `DEFAULT_CLICKTIME` is not converted

## Task Commits

Each task was committed atomically:

1. **Task 1: Scale menu border, triangle, and drag threshold at popup** - `d25601434` (feat)
2. **Task 2: Load menu fonts and pictures at popup with the FSCREEN_XYPOS factor** - `6c296e7de` (feat)
3. **Task 3: Scale icon wanted sizes at create from icon-geometry monitor** - `fa1463f03` (feat)

**Plan metadata:** pending docs commit

## Files Created/Modified

- `fvwm/menus.c` - Popup conversion, AtScale font/picture bind, MoveThreshold scale
- `fvwm/menustyle.h` - `MST_BORDER_WIDTH`/`MST_PSTDFONT`/`MST_PTITLEFONT` read MenuRoot
- `fvwm/menuroot.h` - Popup border, triangle, and font fields next to `sidePic`
- `fvwm/menuitem.c` - Size/paint consume MenuRoot triangle ints and popup fonts
- `fvwm/menuitem.h` - `menuitem_get_size` takes `MenuRoot *mr`
- `fvwm/ewmh_icons.c` - Scaled wanted/max sizes into `FEwmhIconFindBest`
- `fvwm/icons.c` - File icons via `PCacheFvwmPictureAtScale` at converted `ICON_WANTED_*`

## Decisions Made

- Convert menu source integers onto MenuRoot at the `FSCREEN_XYPOS` popup site and remake when converted pixels or popup fonts change, including same-size mixed-DPI monitors.
- Keep MenuStyle parse-time fonts and pictures as 1x `FlocaleLoadFont`/`PCacheFvwmPicture`. On AtScale failure, bind the 1x style object and `fvwm_debug`.
- Iconified UI uses the monitor containing icon geometry (`g.x + g.width/2`), else the pointer. Overflow keeps the source wanted size.
- File-icon pixmaps stay cache-owned (`SET_PIXMAP_OURS` false) so `destroy_icon` does not `XFreePixmap` a shared `PCache` entry.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Remake when popup scale changes at equal screen size**
- **Found during:** Task 1 (menu border/triangle popup)
- **Issue:** `has_screen_size_changed` only compared screen x/y/w/h, so two monitors with the same pixel size and different factors would skip `make_menu`.
- **Fix:** Compare converted border/triangle (and later font pointers) and force remake when they differ.
- **Files modified:** `fvwm/menus.c`
- **Verification:** Conversion still runs at the `FSCREEN_XYPOS` site; `meson test wm-ui-metrics` passed
- **Committed in:** `d25601434` (Task 1), extended in `6c296e7de` (Task 2)

**2. [Rule 2 - Missing Critical] Cache-backed file icons must not be XFreePixmap'd**
- **Found during:** Task 3 (icon file load)
- **Issue:** Copying `PCacheFvwmPictureAtScale` pixmap IDs onto `fw->iconPixmap` with `SET_PIXMAP_OURS` would let `destroy_icon` free cache pixmaps.
- **Fix:** Copy picture/mask/alpha/depth fields and leave `SET_PIXMAP_OURS` false.
- **Files modified:** `fvwm/icons.c`
- **Verification:** `meson test wm-ui-metrics image-safety` passed
- **Committed in:** `fa1463f03` (Task 3)

---

**Total deviations:** 2 auto-fixed (2 missing critical)
**Impact on plan:** Both auto-fixes are correctness requirements (mixed-DPI remake; cache pixmap ownership). No scope creep.

## TDD Gate Compliance

Plan frontmatter is `type: execute` (`tdd_mode` false). Tasks marked `tdd="true"` are core `fvwm3` wiring, not symbols linked by `meson test wm-ui-metrics`. RED commits were not added because a failing unit test of `update_menu`/`GetIconFromFile` would require extracting those functions from the window-manager process (Rule 4). Conversion behavior remains covered by existing 04-01 `test_menu_and_icon_wanted_sizes`; GREEN is the three `feat(04-06)` commits plus Podman `meson test wm-ui-metrics` / `image-safety`.

| Task | RED | GREEN | REFACTOR | Status |
|------|-----|-------|----------|--------|
| 1 menu popup metrics | — | `d25601434` | — | Wiring only |
| 2 popup fonts/pictures | — | `6c296e7de` | — | Wiring only |
| 3 icon wanted sizes | — | `fa1463f03` | — | Wiring only |

## Issues Encountered

- `DEFAULT_CLICKTIME` is not present in `fvwm/menus.c` (it lives in `fvwm/fvwm3.c` and `fvwm/builtins.c`). It was not converted.
- Plan verify `grep ST_PSTDFONT` inside `menuitem_paint` also matches `MST_PSTDFONT`. Paint uses `MST_PSTDFONT(mpip->cb_mr)` and does not use the MenuStyle `ST_*` font macros.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for 04-07 (cursors, pan frames, and snap at active-monitor scale)
- MenuStyle BorderWidth stays source 0-50; popup uses MenuRoot border/triangle/fonts
- Mini-icon in `add_window.c` was not edited (plan 04-09)
- No blockers

## Self-Check: PASSED

- `fvwm/menus.c` FOUND
- `fvwm/menustyle.h` FOUND
- `fvwm/menuroot.h` FOUND
- `fvwm/menuitem.c` FOUND
- `fvwm/menuitem.h` FOUND
- `fvwm/ewmh_icons.c` FOUND
- `fvwm/icons.c` FOUND
- Commits `d25601434`, `6c296e7de`, `fa1463f03` FOUND
- `meson test wm-ui-metrics` and `image-safety` passed in Podman Linux

---
*Phase: 04-wm-owned-ui-metrics-and-resources*
*Completed: 2026-08-31*
