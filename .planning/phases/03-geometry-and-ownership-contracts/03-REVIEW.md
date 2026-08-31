---
phase: 03-geometry-and-ownership-contracts
reviewed: 2026-08-31T01:48:40Z
depth: standard
files_reviewed: 15
files_reviewed_list:
  - libs/monitor_ownership.h
  - libs/monitor_ownership.c
  - tests/test_monitor_ownership.c
  - libs/FScreen.c
  - fvwm/update.c
  - libs/icccm_size_hints.h
  - libs/icccm_size_hints.c
  - tests/test_icccm_size_hints.c
  - fvwm/geometry.c
  - fvwm/ewmh.c
  - fvwm/placement.c
  - fvwm/move_resize.c
  - doc/fvwm3_manpage_source.adoc
  - libs/meson.build
  - tests/meson.build
findings:
  critical: 1
  warning: 3
  info: 2
  total: 6
status: issues_found
---

# Phase 03: Code Review Report

**Reviewed:** 2026-08-31T01:48:40Z
**Depth:** standard
**Files Reviewed:** 15
**Status:** issues_found

## Narrative Findings (AI reviewer)

## Summary

Phase 03 extracts allocation-free ownership and ICCCM constrain units, wraps `FindScreenOfXY` / `constrain_size`, and stores work areas in snapshot root pixels. The pure helpers are careful about `__int128` arithmetic and output initialization. The production seam that stores `ewmh_working_area` as an absolute root-pixel rectangle does not update inset-style consumers, which is a GEO-05 correctness break on any monitor whose origin is not `(0,0)`. Additional defects: hysteresis is area not edge pixels, unsatisfiable aspect is misclassified as overflow (skipping min/max), and `FindScreenOfXY` can now return NULL into unchecked callers.

## Critical Issues

### CR-01: Work-area storage changed to root pixels; inset consumers still add `si->x`

**File:** `fvwm/ewmh.c:1043-1068`
**Issue:** Before this phase, `ewmh_ComputeAndSetWorkArea` stored monitor-relative insets (`x = left`, `width = si->w - left - right`). `EWMH_GetWorkAreaIntersection` then re-added `m->si->x`. The new path stores `monitor_ownership_work_rect` output, so `ewmh_working_area.x` is a root-pixel origin (e.g. `-1856` on a monitor at `x=-1920` with a 64px left strut). Intersection and public strut derivation were updated. These consumers were not:

1. `get_ewmhc_boundingbox` (`fvwm/ewmh.c:2108-2113`) still treats `working_area.x/y` as left/top insets:
   ```c
   left = m->Desktops->ewmh_working_area.x;
   right = m->si->w - left - m->Desktops->ewmh_working_area.width;
   ```
   On a monitor at `x=-1920` with work rect `(-1856, 32, 1856, 1048)` this yields `left=-1856` and `right=1920`. Those values are published to modules (`fvwm/module_interface.c:494-500`) and used as insets by `move_to_next_monitor` (`fvwm/move_resize.c:319`: `win_r->x = m->si->x + r.left`).

2. Snap-to-working-area (`fvwm/move_resize.c:426-436`) still does `wa.x = si->x + page.x` then `wa.x += working_area.x`. For any monitor with `si->x != 0` this double-counts the origin (`-1920 + -1856 = -3776`; right-hand monitor `1920 + 1984 = 3904`). Primary-at-origin layouts accidentally still work.

3. Minoverlap placement (`fvwm/placement.c:702-710`, `840-847`) still adds `page_p1` / `screen_g` to `working_area.x/y` as if they were insets. The Y path (`screen_g.y + working_area.y + height`) double-counts `si->y` once Y is absolute.

4. `$[wa.x]` / `$[wa.y]` in `fvwm/expand.c:789-811` (not in this file list, same store) change from strut insets to absolute root coordinates.

This is the GEO-05 case the phase claimed to close. Native work-rect fixtures pass because they never exercise these consumers.

**Fix:** Keep the stored rectangle in root pixels. Derive insets at inset-only call sites the same way `EWMH_GetStrutIntersection` already does, and assign the stored rect (plus page offset) where a rectangle is needed:

```c
boundingbox
get_ewmhc_boundingbox(struct monitor *m)
{
	boundingbox r = {0};
	rectangle *wa;

	EWMH_UpdateWorkArea(m);
	wa = &m->Desktops->ewmh_working_area;
	r.left = wa->x - m->si->x;
	r.right = (m->si->x + m->si->w) - (wa->x + wa->width);
	r.top = wa->y - m->si->y;
	r.bottom = (m->si->y + m->si->h) - (wa->y + wa->height);
	return r;
}
```

```c
/* fvwm/move_resize.c snap/move working area */
wa = fw->m->Desktops->ewmh_working_area;
wa.x += page.x;
wa.y += page.y;
```

Do not add `si->x` on top of an already-absolute `wa.x`. Update placement minoverlap to use the stored rect as absolute (or convert to insets once, then add). Add a native fixture with `si->x != 0` through bounding-box / snap, not only `monitor_ownership_work_rect`.

## Warnings

### WR-01: 8px incumbent hysteresis is extra overlap *area*, so real windows steal after 1px

**File:** `libs/monitor_ownership.c:226-228`
**Issue:** GEO-01 / RESEARCH lock hysteresis as "8 root pixels of extra overlap **along the shared edge**". The manpage repeats "hysteresis of 8 pixels". The implementation compares challenger-minus-incumbent **area** to `MONITOR_OWNERSHIP_HYSTERESIS_PX` (8). Extra area grows as `2 * height * pixels_past_midpoint`. For a 200×100 window, one pixel past an equal seam yields `extra_overlap = 200`, which is already `> 8`, so the challenger steals. Equal-overlap incumbent keep still works; 1px jitter around a seam does not. Tests hide this by using `height = 1` strips so area equals width (`tests/test_monitor_ownership.c:453-457`). The plan's own 200×100 example at `x=891` expects a steal (`extra_overlap == 1800`), which matches area but not an 8-pixel edge band.

**Fix:** Compare extra overlap *along the shared axis* (width of extra overlap for a vertical seam, height for a horizontal seam), or scale the area threshold by the overlap extent. Keep the equal-overlap incumbent rule. Add a 200×100 window fixture that stays incumbent one pixel past the midpoint and steals at 9 pixels.

### WR-02: Unsatisfiable aspect is reported as OVERFLOW, so `constrain_size` skips min/max

**File:** `libs/icccm_size_hints.c:168-171`
**Issue:** `adjust_aspect` returns `ICCCM_CONSTRAIN_OVERFLOW` not only when the wide result does not fit in `int`, but also when the alternate-axis candidate fails `within_max`. `icccm_constrain_size` then zeros outputs. `constrain_size` (`fvwm/geometry.c:870-876`) restores the **pre-constrain** client size (`client_width` captured before the pure call), so min/max/style clamps already computed inside `icccm_constrain_size` are thrown away.

Concrete path: `PMinSize` 100×100, `PMaxSize` 200×200, `PAspect` min 16:1, requested 10×10. Clamp yields 100×100; aspect cannot be met within max; OVERFLOW; wrapper restores 10×10. A client (or interactive resize) with legal but unsatisfiable aspect bypasses ICCCM min. Overflow should mean "product does not fit in `int`", not "aspect cannot be satisfied".

**Fix:** If height adjustment is in-range, use it; else if width adjustment is in-range, use it; else leave the already-clamped size and return `ICCCM_CONSTRAIN_OK`. Reserve OVERFLOW for `!wide_fits_int`. If the wrapper must fail closed, restore the *clamped* size, not the pre-constrain size:

```c
if (!wide_fits_int(new_w)) {
	return ICCCM_CONSTRAIN_OVERFLOW;
}
if (within_max(hints, (int)new_w, hints->max_width)) {
	*width = (int)new_w;
}
/* else keep clamped size */
```

### WR-03: `FindScreenOfXY` can return NULL; several callers still dereference it

**File:** `libs/FScreen.c:1274-1291`
**Issue:** The old point lookup `exit(106)`'d on a total miss. The new wrapper logs and returns `NULL` when there is no primary and no enabled non-global monitor. That is the intended fail-open policy, but callers were not updated:

- `fvwm/placement.c:1997-1999` — `mnew->virtual_scr.CurrentDesk` with no NULL check
- `fvwm/move_resize.c:841-844`, `864-880`, `892-894` — `m->virtual_scr`
- `fvwm/placement.c:1154-1161` — `EWMH_GetStrutIntersection(m, ...)` then `m->Desktops`
- `fvwm/update.c:705` — `mnew->virtual_scr` when `fw->m` and `monitor_by_primary()` are both NULL and `UpdateDesk >= 0`

`FScreenOfPointerXY` already guards NULL. The others crash on empty/disabled topology instead of aborting.

**Fix:** Keep the logged fallback, but never return NULL from `FindScreenOfXY` if `monitor_get_global()` or any enabled monitor exists. At remaining NULL sites, treat as no-op / keep `fw->m`:

```c
fallback = monitor_by_primary();
if (fallback == NULL) {
	fallback = monitor_get_global();
}
return fallback;
```

## Info

### IN-01: Empty debug branch left in `ewmh_ComputeAndSetWorkArea`

**File:** `fvwm/ewmh.c:1055-1057`
**Issue:** `if (Scr.bo.do_debug_randr) { }` is a leftover after the old debug body was removed. Dead control flow.
**Fix:** Delete the empty `if`.

### IN-02: Hysteresis fixtures never use a realistic window height

**File:** `tests/test_monitor_ownership.c:446-472`
**Issue:** `test_incumbent_kept_when_extra_overlap_equals_hysteresis` and the one-pixel-remaining case use `height = 1` so extra *area* equals extra *width*. They cannot detect WR-01. The 200×100 steal case asserts `extra_overlap == 1800`, which encodes the area interpretation.
**Fix:** Add a 200×100 seam fixture at `x=899` (one pixel past equal overlap) that documents the intended keep-or-steal rule.

---

_Reviewed: 2026-08-31T01:48:40Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
