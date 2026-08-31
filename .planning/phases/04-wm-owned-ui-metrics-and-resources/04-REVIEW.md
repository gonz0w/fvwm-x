---
phase: 04-wm-owned-ui-metrics-and-resources
reviewed: 2026-08-31T19:56:58Z
depth: standard
files_reviewed: 38
files_reviewed_list:
  - .github/workflows/ccpp.yml
  - doc/fvwm3_manpage_source.adoc
  - fvwm/add_window.c
  - fvwm/add_window.h
  - fvwm/borders.c
  - fvwm/builtins.c
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
  - fvwm/menustyle.c
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
  critical: 0
  warning: 5
  info: 2
  total: 7
status: issues
---

# Phase 04: Code Review Report

**Reviewed:** 2026-08-31T19:56:58Z
**Depth:** standard
**Files Reviewed:** 38
**Status:** issues

## Summary

Re-reviewed current Phase 04 sources after gap-closure 04-10 / 04-11 / 04-12. Prior CR-01 (AddWindow default-font bind before `fw->m`), WR-01 (1x `Scr.DefaultFont` cache-hit count leak), and WR-03 (HandlePaging / `last_edge_thickness` source pixels) are closed in the current tree:

- `AddWindow` re-runs `SelectDecor` then `setup_window_font` / `setup_icon_font(..., True)` after `update_fvwm_monitor` (`fvwm/add_window.c:2548-2565`).
- `bind_default_font_at_scale` drops the extra `FlocaleFont->count` when AtScale returns the fallback pointer (`fvwm/add_window.c:1760-1763`).
- `HandlePaging` converts once via `mapped_edge_thickness`; `checkPanFrames` writes `last_edge_thickness = source_thickness` (`fvwm/virtual.c:692-693`, `1185`).

Remaining defects are resource-lifetime and bind-order holes in menus, mini-icons, file icons, and cursors. Frame geometry is still the pre-ownership estimate (explicit Phase 5 handoff) and is not re-filed as a Phase 04 blocker.

## Narrative Findings (AI reviewer)

## Warnings

### WR-01: Menu popup rebind inflates font and picture cache counts on every show

**File:** `fvwm/menus.c:2000-2026`, `fvwm/menus.c:2029-2046`
**Issue:** `update_menu` always calls `apply_popup_menu_metrics`, which calls `FlocaleLoadFontAtScale` / `PCacheFvwmPictureAtScale` for popup fonts, sidepics, item pictures, and mini-icons. Both cache helpers increment `count` on a hit. `bind_popup_font` does not unload when `loaded == *slot` (or `loaded == fallback`). `reload_picture_at_scale` does not `PDestroyFvwmPicture` when `p == old`. Every subsequent menu show therefore leaks one cache weight per font and picture. `menuitem_free` / a later unload then cannot reach count 0, so scaled faces and pixmaps survive until process exit.
**Fix:** Mirror `bind_default_font_at_scale`: drop the extra weight on a cache hit of the already-bound object, and drop it when AtScale returns the 1x fallback pointer.

```c
	loaded = FlocaleLoadFontAtScale(
		dpy, name, "fvwm", factor,
		WM_UI_RESOURCE_GENERATION_INITIAL);
	if (loaded == NULL)
	{
		fvwm_debug(__func__,
			"Couldn't load scaled menu font '%s'; keeping 1x",
			(name != NULL) ? name : "(default)");
		*slot = fallback;
		return;
	}
	if (loaded == *slot || loaded == fallback ||
	    loaded == Scr.DefaultFont)
	{
		if (loaded != *slot)
		{
			FlocaleUnloadFont(dpy, loaded);
		}
		else
		{
			FlocaleUnloadFont(dpy, loaded);
		}
		if (*slot == NULL)
		{
			*slot = fallback;
		}
		return;
	}
	if (*slot != NULL && *slot != fallback &&
	    *slot != Scr.DefaultFont)
	{
		FlocaleUnloadFont(dpy, *slot);
	}
	*slot = loaded;
```

And in `reload_picture_at_scale`:

```c
	if (p == old)
	{
		PDestroyFvwmPicture(dpy, p);
		return old;
	}
```

### WR-02: `DestroyMenu` never unloads popup-scaled fonts

**File:** `fvwm/menus.c:6481-6501`
**Issue:** `MenuRootStatic` holds `popup_std_font` / `popup_title_font` that `bind_popup_font` may replace with AtScale faces distinct from `ST_PSTDFONT` / `Scr.DefaultFont`. `DestroyMenu` frees the sidepic and then `memset`/`free`s `mr->s` without `FlocaleUnloadFont`. Even with WR-01 fixed (count == 1), destroying or recreating a menu leaks the scaled face. Style 1x fonts must not be unloaded.
**Fix:** Before freeing `mr->s` when `MR_COPIES(mr) == 0`:

```c
		if (mr->s->popup_std_font != NULL &&
		    mr->s->popup_std_font != ST_PSTDFONT(MR_STYLE(mr)) &&
		    mr->s->popup_std_font != Scr.DefaultFont)
		{
			FlocaleUnloadFont(dpy, mr->s->popup_std_font);
		}
		if (mr->s->popup_title_font != NULL &&
		    mr->s->popup_title_font != mr->s->popup_std_font &&
		    mr->s->popup_title_font != ST_PTITLEFONT(MR_STYLE(mr)) &&
		    mr->s->popup_title_font != ST_PSTDFONT(MR_STYLE(mr)) &&
		    mr->s->popup_title_font != Scr.DefaultFont)
		{
			FlocaleUnloadFont(dpy, mr->s->popup_title_font);
		}
```

### WR-03: Mini-icons still bind at 1x because `setup_mini_icon` runs before ownership

**File:** `fvwm/add_window.c:2516-2565`
**Issue:** `setup_mini_icon` uses `fw->m->effective_scale.factor` else `MONITOR_SCALE_QUARTERS_MIN` (`add_window.c:619-626`) and is called at 2519, before `update_fvwm_monitor` at 2548. 04-12 rebinds title/icon fonts and `SelectDecor` after ownership but does not re-run `setup_mini_icon` / `change_mini_icon`. Default-config title-button mini-icons therefore stay the 1x 16px raster on a 2x owner. `broadcast_mini_icon` then publishes that 1x picture.
**Fix:** After the post-ownership font rebind, destroy and reload the mini-icon now that `fw->m` is set:

```c
	setup_window_font(fw, &style, True);
	setup_icon_font(fw, &style, True);
	if (FMiniIconsSupported)
	{
		change_mini_icon(fw, &style);
	}
```

Keep the pre-ownership `setup_mini_icon` only if something between 2519 and 2548 truly needs the pointer; otherwise move the first load to after ownership.

### WR-04: File-icon `PCacheFvwmPictureAtScale` result is discarded, so cache weights never drop

**File:** `fvwm/icons.c:2166-2186`
**Issue:** `GetIconFromFile` copies pixmap IDs out of the cached `FvwmPicture` and sets `SET_PIXMAP_OURS(fw, 0)` so `destroy_icon` will not `XFreePixmap`. The `FvwmPicture *` itself is not stored on `FvwmWindow`. `clear_icon` / window destroy therefore never call `PDestroyFvwmPicture`. Each `GetIconFromFile` (including icon replacement) permanently increments the cache weight. Unique `(path, scale, target)` entries then live until process exit, and a later `SetIconPixmapSize` on a shared cached pixmap can mutate other windows' icons.
**Fix:** Store the cache handle on the window and release it in `clear_icon` / icon replacement. Do not copy cache-owned pixmap IDs into a path that may resize them in place.

```c
	pic = PCacheFvwmPictureAtScale(...);
	if (pic == NULL)
	{
		fvwm_debug(__func__, "Failed to load %s", path);
		free(path);
		return;
	}
	if (fw->icon_picture != NULL)
	{
		PDestroyFvwmPicture(dpy, fw->icon_picture);
	}
	fw->icon_picture = pic;
	fw->iconPixmap = pic->picture;
	/* ... */
	SET_PIXMAP_OURS(fw, 0);
```

If icon-size limits require a private raster, clone or reload into a window-owned pixmap instead of resizing the cache entry.

### WR-05: Cursor slots record scale identity but are never reloaded for the pointer monitor

**File:** `fvwm/cursor.c:343-389`
**Issue:** `CreateCursors` loads every glyph once from `cursor_active_factor` (pointer monitor at startup) and `cursor_store_slot` writes `cursor_table[i].quarters` / `.generation`. Nothing later reads `cursor_table` to decide a reload. `CMD_CursorStyle` also stores the pointer factor at command time only. Mixed-DPI pointer motion therefore keeps the startup size, which contradicts the man-page contract that cursors follow the active/pointer monitor (`doc/fvwm3_manpage_source.adoc:7513-7515`).
**Fix:** On pointer-monitor scale change (or before use when `cursor_table[i].quarters` mismatches the active factor), free the old `Cursor` and reload through the existing `cursor_nominal_px` / `FcursorShapeLoadImages` / `PImageLoadCursorFromFile` path, then `cursor_store_slot`. Keep generation at `WM_UI_RESOURCE_GENERATION_INITIAL` until a real generation bump exists.

## Info

### IN-01: `scaled_move_threshold` is duplicated in two translation units

**File:** `fvwm/events.c:1227-1254`, `fvwm/functions.c:752-780`
**Issue:** Two identical helpers convert `Scr.MoveThreshold` with the same NULL-monitor fallback and debug string. A third copy of the conversion sits inline in `fvwm/menus.c:4358-4368`. Drift is already a maintenance hazard (04-11 called this out of scope).
**Fix:** One shared helper, e.g. in `libs/wm_ui_metrics.c` or a small `fvwm/` UI-metrics wrapper that takes `(x, y)` and returns the scaled threshold.

### IN-02: XBM fallback in `PImageLoadFvwmPictureFromFile` ignores the scale factor

**File:** `libs/PictureImageLoader.c:1196-1204`
**Issue:** PNG/SVG/XPM go through `pimage_resolve_destination` / `pimage_blit_argb_once`. The `XReadBitmapFile` fallback assigns the intrinsic 1-bit size and never scales. File icons that only exist as XBM stay 1x on a 2x monitor.
**Fix:** After a successful bitmap read, resolve destination size with `factor` / targets and blit (or fail closed to the 1x bitmap with a debug line, matching other last-good paths).

---

_Reviewed: 2026-08-31T19:56:58Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
