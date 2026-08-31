---
phase: 04-wm-owned-ui-metrics-and-resources
reviewed: 2026-08-31T16:03:06Z
depth: standard
files_reviewed: 35
files_reviewed_list:
  - .github/workflows/ccpp.yml
  - doc/fvwm3_manpage_source.adoc
  - fvwm/add_window.c
  - fvwm/borders.c
  - fvwm/cursor.c
  - fvwm/decorations.c
  - fvwm/events.c
  - fvwm/ewmh_icons.c
  - fvwm/functions.c
  - fvwm/geometry.c
  - fvwm/icons.c
  - fvwm/menuitem.c
  - fvwm/menuitem.h
  - fvwm/menuroot.h
  - fvwm/menus.c
  - fvwm/menustyle.h
  - fvwm/move_resize.c
  - fvwm/virtual.c
  - libs/Fcursor.h
  - libs/Fft.c
  - libs/FftInterface.h
  - libs/Flocale.c
  - libs/Flocale.h
  - libs/Picture.c
  - libs/Picture.h
  - libs/PictureBase.h
  - libs/PictureImageLoader.c
  - libs/PictureImageLoader.h
  - libs/meson.build
  - libs/wm_ui_metrics.c
  - libs/wm_ui_metrics.h
  - tests/live_x/live_x_harness.py
  - tests/meson.build
  - tests/test_supported_features.c
  - tests/test_wm_ui_metrics.c
findings:
  critical: 1
  warning: 5
  info: 2
  total: 8
status: issues_found
---

# Phase 04: Code Review Report

**Reviewed:** 2026-08-31T16:03:06Z
**Depth:** standard
**Files Reviewed:** 35
**Status:** issues_found

## Summary

Phase 04 wires `wm_ui_scale_px` / AtScale cache entry points through chrome, fonts, pictures, menus, icons, cursors, and pan frames. The converter itself, Picture/Flocale cache-key identity, and overflow-to-zero policy look sound. Several call sites still drop scale, and one pan-frame state write is a dead store that leaves EdgeThickness updates unable to resize existing frames.

## Narrative Findings (AI reviewer)

## Critical Issues

### CR-01: Pan-frame last thickness never written back

**File:** `fvwm/virtual.c:1003`
**Issue:** `checkPanFrames` copies `m->virtual_scr.last_edge_thickness` into a local, converts both current and last source values, then assigns the mapped pixel result back to that local. The monitor field is never updated. `FScreen.c` initializes the field to source `2`, so a user who changes `EdgeThickness` away from 2 and later back to 2 sees `edge_thickness == last_mapped` and skips `XResizeWindow` / `XMoveResizeWindow`. Pan frames stay at the previous mapped size. The assignment at line 1182 is a dead store.

**Fix:** Keep the field in source space and write it back after mapping:

```c
	edge_thickness = mapped_edge_thickness(m, source_thickness);
	last_mapped = mapped_edge_thickness(
		m, m->virtual_scr.last_edge_thickness);
	/* ... map / unmap / resize using edge_thickness vs last_mapped ... */
	m->virtual_scr.last_edge_thickness = source_thickness;
```

Do not store mapped pixels in `last_edge_thickness`; the next call would pass those pixels through `wm_ui_scale_edge_thickness` again.

## Warnings

### WR-01: Xft scale can apply monitor factor on top of already-DPI-matched pixelsize

**File:** `libs/Fft.c:182-208`
**Issue:** `FftGetFontAtScale` is supposed to scale the unmatched source pixel size. If the font name uses `size=` (points) rather than `pixelsize=`, `src_pat` has no `FC_PIXEL_SIZE` and the code falls back to `load_pat` after `XftFontMatch`. That value already includes Xft/fontconfig DPI (`Xft.dpi`). `wm_ui_scale_px` then multiplies it by the monitor factor, so a 12pt face on a 2x monitor with `Xft.dpi: 192` opens at ~64px instead of ~32px. `FC_SIZE` is also left on `load_pat` after the rewrite.

**Fix:** Prefer unmatched `FC_PIXEL_SIZE`, else convert unmatched `FC_SIZE` at 96 DPI (`px = size * 96 / 72`), then scale. Delete both `FC_SIZE` and `FC_PIXEL_SIZE` before adding the scaled pixel size. Do not use the post-match DPI-adjusted value.

### WR-02: Window and icon fonts fall back to unscaled `Scr.DefaultFont`

**File:** `fvwm/add_window.c:1768-1771`
**Issue:** `setup_window_font` / `setup_icon_font` only call `FlocaleLoadFontAtScale` when the style names an explicit font. Otherwise they share process-global `Scr.DefaultFont` (1x generation 1). Default-config title and icon text stay 1x on a 2x monitor while `SelectDecor` and `wm_ui_compose_title_thickness` scale borders and extra title pixels. Menus already reload the style font name at the popup factor via `bind_popup_font`; window chrome does not.

**Fix:** Load the default face by name at the owning-monitor factor, and do not `FlocaleUnloadFont` `Scr.DefaultFont`:

```c
fw->title_font = FlocaleLoadFontAtScale(
	dpy, Scr.DefaultFont->name, "fvwm", factor,
	WM_UI_RESOURCE_GENERATION_INITIAL);
if (fw->title_font == NULL)
	fw->title_font = Scr.DefaultFont;
```

Same pattern for `setup_icon_font` at `fvwm/add_window.c:1823-1826`.

### WR-03: HandlePaging still treats EdgeThickness source as root pixels

**File:** `fvwm/virtual.c:690-711`
**Issue:** `initPanFrames` / `checkPanFrames` size the InputOnly hit bands with `mapped_edge_thickness`, but `HandlePaging` compares pointer position to `m->virtual_scr.edge_thickness` (command source 0–2). At 2x with source 2 the pan frame is 4px while the paging fast-path treats only the outer 2px as an edge. Motion in the inner half of the mapped band is swallowed by the pan frame and then ignored (`return -1`), so paging dead-zones appear on scaled monitors.

**Fix:** Convert once with the current monitor factor and use that local for the proximity tests (keep the stored field as source):

```c
int edge_thickness = mapped_edge_thickness(
	m, m->virtual_scr.edge_thickness);
```

### WR-04: Theme cursors are sized once at startup and the scale cache is never read

**File:** `fvwm/cursor.c:343-388`
**Issue:** `CreateCursors` loads Xcursor images at the pointer monitor's nominal size and stores `quarters` / `generation` on `cursor_table`, but nothing later reads those fields or reloads when the pointer moves to a different scale. Mixed-DPI sessions keep whatever size was current at `Scr.FvwmCursors = CreateCursors(dpy)`. `CMD_CursorStyle` also snapshots `cursor_active_factor` once.

**Fix:** Before returning `Scr.FvwmCursors[i]`, if `cursor_table[i].quarters` differs from the active factor, reload that slot (shape or file) at the new nominal size, free the previous `Cursor`, and update the table. `CreateCursors` can stay the 1x-or-startup seed.

### WR-05: File-icon cache entries are never released

**File:** `fvwm/icons.c:2166-2187`
**Issue:** `GetIconFromFile` loads through `PCacheFvwmPictureAtScale`, copies pixmap IDs onto `FvwmWindow`, sets `PIXMAP_OURS` false, and discards the `FvwmPicture *` without `PDestroyFvwmPicture`. Cache `count` never drops. Replacing or destroying the icon leaves the scaled pixmaps allocated for process lifetime. `ewmh_icons.c` still owns its stretched pixmaps; this path does not.

**Fix:** Store the `FvwmPicture *` on the window (or destroy immediately after cloning pixmaps with `PIXMAP_OURS` true). On icon teardown call `PDestroyFvwmPicture` so the extra cache weight is dropped.

## Info

### IN-01: `scaled_move_threshold` is duplicated

**File:** `fvwm/functions.c:752-780`
**Issue:** `fvwm/events.c:1226-1254` contains the same pointer-monitor `MoveThreshold` helper. Drift risk if one path gains a fallback the other does not.

**Fix:** Share one helper (header in `misc.h` or similar) used by click detection, focus-raise, and the menu motion path in `fvwm/menus.c:4358`.

### IN-02: Disabled-SVG stub returns success

**File:** `libs/PictureImageLoader.c:577-582`
**Issue:** The `#if USE_SVG` else branch of `PImageLoadSvg` returns `True` without writing `argb_data` / width / height. A `.svg` request in a non-SVG build would look like a successful load of uninitialized data. Supported-profile CI enables SVG, so this is latent.

**Fix:** `return False;`

---

_Reviewed: 2026-08-31T16:03:06Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
