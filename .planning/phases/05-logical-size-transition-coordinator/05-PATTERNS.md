# Phase 5: Logical-Size Transition Coordinator - Pattern Map

**Mapped:** 2026-08-31
**Files analyzed:** 15 new/modified/guardrail files
**Analogs found:** 12 / 12 work files (3 protocol guardrails have in-place analogs)

## Scope Extracted

Phase 5 adds one allocation-free `libs/` transition planner (`geometry_transition_plan`) plus thin single-threaded wiring at `update_fvwm_monitor`, `move_loop`, maximize/unmaximize, and `EWMH_fullscreen`. Canonical logical **client** width/height is stored on `FvwmWindow` as a new `size_rect` (not inside `window_g`). Dest pixels are `monitor_scale_apply(logical, dest_factor)` then `icccm_constrain_size`; never `remove(apply(...))`. During grab, dest owner is a 1×1 pointer rectangle with incumbent hysteresis; the existing `GrabEm` is kept. Maximize/fullscreen recompute from dest work area / dest monitor. Native unit tests only; no live-X matrix, no packet schema change.

Input-only contracts (consume, do not reimplement): `libs/monitor_scale.h`, `libs/icccm_size_hints.h`, `libs/monitor_ownership.h`, `libs/wm_ui_metrics.h`, `libs/gravity.c`, `fvwm/frame.h`, `fvwm/misc.c` (`GrabEm`).

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `libs/geometry_transition.h` (new) | utility | transform | `libs/icccm_size_hints.h` + `libs/monitor_scale.h` | exact |
| `libs/geometry_transition.c` (new) | utility | transform | `libs/icccm_size_hints.c` + `libs/monitor_scale.c` apply/init-0 | exact |
| `tests/test_geometry_transition.c` (new) | test | transform | `tests/test_icccm_size_hints.c` + `tests/test_monitor_scale.c` + `tests/test_monitor_ownership.c` | exact |
| `fvwm/fvwm.h` | model | — | existing `window_g` / `size_rect max_defect` / `fullscreen.g` | exact in-place seam |
| `fvwm/update.c` | service | request-response | existing `update_fvwm_monitor` + `apply_decor_change` | exact in-place seam |
| `fvwm/move_resize.c` | controller | event-driven | existing `move_loop` / `GrabEm` / `maximize_fvwm_window` / `unmaximize_fvwm_window` | exact in-place seam |
| `fvwm/ewmh.c` | service | request-response | existing `EWMH_fullscreen` / `EWMH_SetFrameStrut` | exact in-place seam |
| `fvwm/add_window.c` | controller | request-response | existing first constrain + dest chrome rebind after `update_fvwm_monitor` | exact in-place seam |
| `fvwm/events.c` | controller | event-driven | existing `_handle_cr_on_client` + `SET_FORCE_NEXT_CR` | exact in-place seam |
| `libs/meson.build` | config | batch | existing `libfvwm3_sources` (`icccm_size_hints.c`, `wm_ui_metrics.c`) | exact in-place seam |
| `tests/meson.build` | config | batch | existing `icccm_size_hints_test` / `wm_ui_metrics_test` registration | exact in-place seam |
| `doc/fvwm3_manpage_source.adoc` | config | file-I/O | existing MonitorScale retained-source / pixel-contract paragraph | exact in-place seam |
| `libs/vpacket.h` | model | request-response | `ConfigWinPacket` pixel fields | guardrail |
| `fvwm/module_interface.c` | route | request-response | `CONFIGARGS` pixel fields | guardrail |
| `fvwm/status_json.c` | route | request-response | Status v4 actual-pixel geometry | guardrail |

## Pattern Assignments

### `libs/geometry_transition.h` (utility, transform)

**Analog:** `libs/icccm_size_hints.h:1-36` (status enum + output-parameter API) and `libs/monitor_scale.h:1-10,55-60,94-98` (guard, quarters, conversion contract).

Copy the compact public-contract shape: uppercase include guard from filename, in-repo types only, named status/kind enums, explicit `plan()` function. No X/`FvwmWindow`/`struct monitor *` in this header. Dest chrome is an **input** (`size_borders`), not computed here.

**Guard + includes** (`libs/icccm_size_hints.h:1-6`):
```c
#ifndef FVWM_ICCCM_SIZE_HINTS_H
#define FVWM_ICCCM_SIZE_HINTS_H

#include "libs/fvwmrect.h"

#include <X11/Xutil.h>
```

**Status enum to copy and extend** (`libs/icccm_size_hints.h:8-13`):
```c
typedef enum
{
	ICCCM_CONSTRAIN_OK,
	ICCCM_CONSTRAIN_OVERFLOW,
	ICCCM_CONSTRAIN_INVALID_ARGUMENT
} icccm_constrain_status;
```

Add `GEOMETRY_TRANSITION_INELIGIBLE` and `GEOMETRY_TRANSITION_NOOP` after the three shared names. Kind enum is new (`NORMAL` / `MAXIMIZED` / `FULLSCREEN` / `RESTORE`) — locked by research, not present in analogs.

**Conversion contract comment to preserve** (`libs/monitor_scale.h:94-98`):
```c
/* Always convert from the retained exact source, never a rounded result. */
monitor_scale_conversion_status
monitor_scale_apply(int source, monitor_scale_factor factor, int *converted);
monitor_scale_conversion_status
monitor_scale_remove(int source, monitor_scale_factor factor, int *converted);
```

The planner calls `monitor_scale_apply` only. Do not declare a `remove` path on the transition API.

**Rectangle / size types to reuse, not reinvent** (`libs/fvwmrect.h:16-49`):
```c
typedef struct
{
	int x;
	int y;
	int width;
	int height;
} rectangle;

typedef struct
{
	int width;
	int height;
} size_rect;

typedef struct
{
	size_rect top_left;
	size_rect bottom_right;
	size_rect total_size;
} size_borders;
```

**Safe-init analog if a result struct is needed** (`libs/monitor_ownership.h:33-35`):
```c
#define MONITOR_OWNERSHIP_RESULT_SAFE_INIT                                 \
	{None, NULL, MONITOR_OWNERSHIP_REASON_FALLBACK, 0}
```

Recommended `plan()` signature (research skeleton; follow `icccm_constrain_size` output-parameter style):
```c
geometry_transition_status
geometry_transition_plan(geometry_transition_kind kind,
    size_rect logical_client, monitor_scale_factor dest_factor,
    const size_borders *dest_chrome, const icccm_size_hints *hints,
    rectangle *dest_frame /* in: origin; out: dest frame */);
```

---

### `libs/geometry_transition.c` (utility, transform)

**Primary analog:** `libs/icccm_size_hints.c:1-36,205-276` (GPL banner, `config.h` first, zero-outputs on failure, constrain then gravity).
**Secondary analog:** `libs/monitor_scale.c:1-23,243-253` (init `*converted = 0` before any failure) and `libs/wm_ui_metrics.c:22-31,56-60` (thin wrapper around `monitor_scale_apply`).

**File banner + `config.h` first** (`libs/icccm_size_hints.c:1-23`):
```c
/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * ...
 */

#include "config.h"

#include "libs/icccm_size_hints.h"
#include "libs/gravity.h"

#include <limits.h>
#include <stdbool.h>
#include <X11/Xutil.h>
```

**Zero outputs on invalid/overflow** (`libs/icccm_size_hints.c:27-36,215-218`):
```c
static void
zero_outputs(int *width, int *height)
{
	if (width != NULL) {
		*width = 0;
	}
	if (height != NULL) {
		*height = 0;
	}
}

if (hints == NULL || width == NULL || height == NULL) {
	zero_outputs(width, height);
	return ICCCM_CONSTRAIN_INVALID_ARGUMENT;
}
```

**Init-0 before conversion** (`libs/monitor_scale.c:243-253`):
```c
monitor_scale_conversion_status
monitor_scale_apply(int source, monitor_scale_factor factor, int *converted)
{
	if (converted != NULL) {
		*converted = 0;
	}
	if (!factor_is_valid(factor)) {
		return MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT;
	}

	return convert_ratio(source, factor.quarters, 4, converted);
}
```

**Compose path (NORMAL / RESTORE):** apply logical width and height independently via `monitor_scale_apply`, then `icccm_constrain_size` on dest client pixels, then add dest chrome `total_size` to form dest frame. Do **not** copy `constrain_size` from `fvwm/geometry.c`.

**Gravity when client+chrome size changes** (`libs/gravity.c:111-123`) — use for non-grab origin; skip during grab:
```c
void gravity_resize(int gravity, rectangle *rect, int wdiff, int hdiff)
{
	int xoff;
	int yoff;

	gravity_get_offsets(gravity, &xoff, &yoff);
	rect->x -= (wdiff * (xoff + 1)) / 2;
	rect->width += wdiff;
	rect->y -= (hdiff * (yoff + 1)) / 2;
	rect->height += hdiff;
}
```

`icccm_constrain_size` already calls `gravity_resize` when `frame != NULL` (`libs/icccm_size_hints.c:266-274`). For MAXIMIZED/FULLSCREEN, dest frame is the dest work/monitor rect; constrain client = frame − dest chrome (skip constrain for true fullscreen).

**Work-rect analog for MAXIMIZED kind** (`libs/monitor_ownership.c:263-313`) — zero `out` first, overflow leaves dest unchanged:
```c
static void
zero_work_rect(rectangle *out)
{
	if (out == NULL) {
		return;
	}
	out->x	    = 0;
	out->y	    = 0;
	out->width  = 0;
	out->height = 0;
}

monitor_ownership_status
monitor_ownership_work_rect(const rectangle *monitor_rect, int strut_left,
    int strut_right, int strut_top, int strut_bottom, rectangle *out)
{
	/* ... */
	if (out == NULL) {
		return MONITOR_OWNERSHIP_INVALID_ARGUMENT;
	}
	zero_work_rect(out);
```

Planner policy on `OVERFLOW`/`INVALID_ARGUMENT`: leave dest unchanged (last good) if the caller passed a live frame; tests that pass a dummy may still assert 0-initialized outputs like the scale/constrain units. Do not clamp to `INT_MAX`.

**Eligibility (do not invent flags):** consume `icccm_size_hints` min==max and/or a boolean `eligible` input. Core maps `IS_SIZE_FIXED` / `IS_PSIZE_FIXED` / `HAS_OVERRIDE_SIZE_HINTS` (`fvwm/window_flags.h:236-265`). Ineligible: keep client pixels, still compose dest chrome.

**NOOP:** same `(dest identity, dest quarters, kind)` as last plan — return `GEOMETRY_TRANSITION_NOOP` without writing dest. Identity comparison lives in core (last-transition tuple on `FvwmWindow`); the planner may also no-op when dest client+frame equal the inputs.

Do not allocate. Do not call X. Do not walk monitors.

---

### `tests/test_geometry_transition.c` (test, transform)

**Analogs:** `tests/test_icccm_size_hints.c:1-23,318-387` (scale-then-constrain + 1,000-trip + `main` of `test_*` functions), `tests/test_monitor_scale.c:353-384` (apply-from-original-source), `tests/test_monitor_ownership.c:446-507` (hysteresis freeze).

**Banner, `config.h` first, assert `main`** (`tests/test_icccm_size_hints.c:1-23,372-387`):
```c
/* -*-c-*- */
/* GPL notice ... */

#include "config.h"

#include "libs/icccm_size_hints.h"
#include "libs/monitor_scale.h"

#include <assert.h>
#include <limits.h>

int
main(void)
{
	test_min_max_clamp();
	/* ... explicit calls, no test runner ... */
	return 0;
}
```

**GEO-02: apply from stored source, not previous dest** (`tests/test_monitor_scale.c:353-365` and `tests/test_icccm_size_hints.c:344-369`):
```c
static void
test_repeated_conversions_start_from_the_original_source(void)
{
	monitor_scale_factor factor = {5};
	int		     iteration;

	for (iteration = 0; iteration < 1000; iteration++) {
		int converted = 0;

		assert(monitor_scale_apply(7, factor, &converted)
		       == MONITOR_SCALE_CONVERSION_OK);
		assert(converted == 9);
	}
}

static void
test_repeated_constrain_from_original_source(void)
{
	/* loop applies 7, then constrains; never feeds width back */
	assert(monitor_scale_apply(7, factor, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	width  = converted;
	height = converted;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
}
```

GEO-02 tests must loop `geometry_transition_plan` from a **stored** logical of 7 through quarters 5/6/7/8; dest pixels match one `apply`. Increment snap must not rewrite the logical source. Overflow/invalid dest outputs stay 0.

**GEO-04: pointer 1×1 + hysteresis freeze** (`tests/test_monitor_ownership.c:446-507`). Copy the seam-monitor fixtures and extra-overlap assertions. A 1×1 pointer rect that stays on the incumbent must not change dest even if a larger post-resize frame would flip overlap. Challenger wins only when extra overlap exceeds `MONITOR_OWNERSHIP_HYSTERESIS_PX` (8).

```c
status = monitor_ownership_resolve(&window, monitors, 2, 1,
    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);

assert(status == MONITOR_OWNERSHIP_OK);
assert_owner(
    &result, 1, "HDMI-1", MONITOR_OWNERSHIP_REASON_INCUMBENT);
assert(result.extra_overlap == MONITOR_OWNERSHIP_HYSTERESIS_PX);
```

Grab-offset conversion: store logical offset; `apply` at dest; origin = pointer + dest_offset. Test this as pure integers in the planner; do not open X.

**GEO-06:** maximize dest = dest work rect (not scaled `g.max`); fullscreen dest = dest monitor rect; restore uses stored logical + dest chrome; fixed-size client pixels unchanged while chrome totals follow dest.

Register like `icccm-size-hints` (`tests/meson.build:62-71`):
```meson
icccm_size_hints_test = executable(
    'test-icccm-size-hints',
    'test_icccm_size_hints.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('icccm-size-hints', icccm_size_hints_test)
```

Name the test `geometry-transition` / executable `test-geometry-transition`. Link `libfvwm3` so `monitor_scale_apply` and `icccm_constrain_size` resolve.

---

### `fvwm/fvwm.h` (model)

**Analog:** existing `window_g` at `fvwm/fvwm.h:699-712` and `fullscreen.g` copy at `fvwm/fvwm.h:959-978`.

Do **not** put logical size inside `window_g`. That struct is copied into `fullscreen.g` and published through configure/module paths.

```c
typedef struct window_g
{
	rectangle frame;
	/* absolute geometry when not maximized */
	rectangle normal;
	/* maximized window geometry */
	rectangle max;
	size_rect max_defect;
	position max_offset;
} window_g;
```

Add a **new** `size_rect logical_client` plus `Bool logical_client_valid` on `FvwmWindow` near `m` / `m_prev` (`fvwm/fvwm.h:977-978`), not inside `g` or `fullscreen`:

```c
	struct monitor *m;
	struct monitor *m_prev;
} FvwmWindow;
```

Also store a last-transition tuple `(Atom name_atom, unsigned int quarters, geometry_transition_kind kind)` so a second `update_fvwm_monitor` with the same tuple is `NOOP`. Dest/src quarters come from `fw->m_prev` / `fw->m` at commit time; do not store quarters as the logical source.

`g.frame` / `g.normal` / `g.max` remain actual root pixels.

---

### `fvwm/update.c` (service, request-response)

**Analog:** existing `update_fvwm_monitor` (`fvwm/update.c:666-742`) and dest-chrome rebind leftover in `fvwm/add_window.c:2547-2566`. Secondary: `apply_decor_change` (`fvwm/update.c:651-663`).

This is the **one commit**. Do not add preservation at every `fw->m =` site.

**Current owner commit** (`fvwm/update.c:675-742`):
```c
	if (IS_MAPPED(fw))
	{
		g = fw->g.frame;
	}
	else
	{
		g = IS_MAXIMIZED(fw) ? fw->g.max : fw->g.normal;
	}
	incumbent_atom = (fw->m != NULL && fw->m->si != NULL) ?
		fw->m->si->name_atom : None;
	snapshot = FScreenGetTopologySnapshot();
	status = monitor_ownership_resolve(&g,
	    snapshot != NULL ? snapshot->monitors : NULL,
	    snapshot != NULL ? snapshot->monitor_count : 0, incumbent_atom,
	    MONITOR_OWNERSHIP_HYSTERESIS_PX, &result);
	/* ... */
	if (fw->m != mnew) {
		fw->m_prev = fw->m;
		fw->m = mnew;
	}
	EWMH_SetCurrentDesktop(fw->m);
	desk_add_fw(fw);
	BroadcastConfig(M_CONFIGURE_WINDOW, fw);
```

**Required insert after `fw->m` swap, before BroadcastConfig:**

1. During grab (`IS_WINDOW_BEING_MOVED_OPAQUE`), do **not** use `fw->g.frame` as the ownership rectangle. Caller (`move_loop`) must pass or the function must resolve a 1×1 pointer rect. Freeze dest identity until pointer extra overlap exceeds hysteresis.
2. If dest `effective_scale.factor.quarters` differs from src, or dest monitor identity changed: dest chrome rebind **then** `geometry_transition_plan` **then** `frame_setup_window`.
3. If quarters are equal but monitor identity changed: rebind chrome, skip client resize.
4. Do not re-run `monitor_ownership_resolve` on the post-resize frame in the same commit.
5. Then `EWMH_SetFrameStrut` + existing `BroadcastConfig`.

**Dest chrome analog (invert Phase 4 leftover)** (`fvwm/add_window.c:2547-2566`):
```c
	update_fvwm_monitor(fw);
	SelectDecor(fw, &style, &buttons);
	/* ... */
	setup_window_font(fw, &style, True);
	setup_icon_font(fw, &style, True);
	BroadcastConfig(M_ADD_WINDOW,fw);
```

Live crossings must run SelectDecor + `setup_window_font`/`setup_icon_font` True **before** frame compose. `apply_decor_change` already sets `do_redecorate` + `do_update_window_font_height` (`fvwm/update.c:651-663`) — reuse that rather than inventing a second rebind.

**Scale source** (`libs/FScreen.h:104`):
```c
	monitor_scale_result effective_scale;
```

**Frame commit** (`fvwm/frame.h:65-67`):
```c
void frame_setup_window(
	FvwmWindow *fw, int x, int y, int w, int h,
	Bool do_send_configure_notify);
```

**Chrome totals after dest rebind** (`fvwm/geometry.c:511-536`):
```c
void get_window_borders(
	const FvwmWindow *fw, size_borders *borders)
{
	borders->top_left.width = fw->boundary_width;
	/* ... title_thickness by GET_TITLE_DIR ... */
	borders->total_size.width =
		borders->top_left.width + borders->bottom_right.width;
	borders->total_size.height =
		borders->top_left.height + borders->bottom_right.height;
}
```

Do not `XSync` except the existing `HAS_NEW_WM_NORMAL_HINTS` path inside `constrain_size`. Reuse Phase 4 AtScale caches; do not walk all windows; do not decode SVG on every motion.

---

### `fvwm/move_resize.c` (controller, event-driven)

**Analogs:** `move_loop` grab + mid-loop owner refresh (`fvwm/move_resize.c:1721-1732,2755-2898`), `GrabEm` nested cursor swap (`fvwm/misc.c:136-158`), maximize/unmaximize (`fvwm/move_resize.c:5123-5227,5410-5414`).

**Offset at grab start** (`fvwm/move_resize.c:1721-1732`):
```c
	offset.x = orig_drag.x - drag.x;
	offset.y = orig_drag.y - drag.y;
	{
		size_rect sz = { drag.width, drag.height };

		move_loop(exc, offset, sz, pFinal, do_move_opaque, CRS_MOVE);
	}
```

Store the grab offset's **logical** form at drag start from src scale; `apply` at dest. After dest size changes, `sz` must become dest frame size so rubber-band/opaque move matches. New origin is `pointer + dest_offset`.

**Keep GrabEm; do not Ungrab/re-Grab** (`fvwm/move_resize.c:2799-2802` and `fvwm/misc.c:136-158`):
```c
	if (!GrabEm(cursor, GRAB_NORMAL))
	{
		XBell(dpy, 0);
		return False;
	}
	/* already grabbed, just change the grab cursor */
	if (grab_count[GRAB_ALL] > grab_count[GRAB_PASSIVE])
	{
		/* ... */
		change_grab_cursor(cursor);
		return True;
	}
```

Use `change_grab_cursor` for dest-scale cursors (Phase 4 cache already keys by quarters). `confine_to` stays `None`.

**Replace frame-based mid-loop owner refresh** (`fvwm/move_resize.c:2898`):
```c
			update_fvwm_monitor(fw);
```

During grab, resolve ownership from a 1×1 pointer rectangle with incumbent hysteresis; freeze dest until extra overlap exceeds `MONITOR_OWNERSHIP_HYSTERESIS_PX`. In-drag flag (`fvwm/window_flags.h:555-558`):
```c
#define IS_WINDOW_BEING_MOVED_OPAQUE(fw) \
	((fw)->flags.is_window_being_moved_opaque)
#define SET_WINDOW_BEING_MOVED_OPAQUE(fw,x) \
	(fw)->flags.is_window_being_moved_opaque = !!(x)
```

Do not put `PImageLoad` / `FlocaleLoadFontAtScale` inside the `FCheckMaskEvent` wait loop — only on the committed dest-change branch.

**Maximize: dest work rect, then constrain** (`fvwm/move_resize.c:5202-5227,5410-5414`):
```c
static void maximize_fvwm_window(
	FvwmWindow *fw, rectangle *geometry)
{
	SET_MAXIMIZED(fw, 1);
	fw->g.max_defect.width = 0;
	fw->g.max_defect.height = 0;
	constrain_size(
		fw, NULL, &geometry->width, &geometry->height, 0, 0,
		CS_UPDATE_MAX_DEFECT);
	fw->g.max = *geometry;
	/* ... */
	frame_setup_window(
		fw, geometry->x, geometry->y, geometry->width,
		geometry->height, True);
```

```c
		EWMH_GetWorkAreaIntersection(
			fw->m, &scr.x, &scr.y, &scr.width, &scr.height,
			EWMH_MAXIMIZE_MODE(fw));
```

`CS_UPDATE_MAX_DEFECT` stays `0x02` (`fvwm/geometry.h:8`). Do not overwrite `logical_client` on maximize. Do not scale `g.max` when a maximized window crosses monitors — re-maximize onto dest work area.

**Unmaximize restore** (`fvwm/move_resize.c:5142-5173`):
```c
	get_relative_geometry(
		fw, &new_g, fw->fullscreen.was_maximized ?
		&fw->fullscreen.g.max : &fw->g.normal);
	/* ... */
	frame_setup_window(
		fw, new_g.x, new_g.y, new_g.width, new_g.height, True);
```

Restore through `apply(logical_client, dest_factor)` + dest chrome + gravity, not raw `g.normal` pixels that may be src-scale.

Grow-to (`grow`, `growleft`, percent forms at `fvwm/move_resize.c:5433+`) stays existing pixel closest-window logic unless it already goes through `maximize_fvwm_window`. 100% maximize / EWMH max / fullscreen / restore are in-scope.

Direct `fw->m = possibly_new_monitor` at `fvwm/move_resize.c:5405-5407` is an opportunistic assignment — do not add a second transition authority here; route through `update_fvwm_monitor`.

---

### `fvwm/ewmh.c` (service, request-response)

**Analogs:** `EWMH_fullscreen` (`fvwm/ewmh.c:2024-2083`) and `EWMH_SetFrameStrut` (`fvwm/ewmh.c:1309-1335`).

**Fullscreen dest monitor rect** (`fvwm/ewmh.c:2048-2083`):
```c
	memcpy(&fw->fullscreen.g, &fw->g, sizeof(struct window_g));
	/* ... */
	SET_EWMH_FULLSCREEN(fw,True);
	apply_decor_change(fw);
	fscr.xypos.x = fw->g.frame.x + fw->g.frame.width / 2;
	fscr.xypos.y = fw->g.frame.y + fw->g.frame.height / 2;
	FScreenGetScrRect(
		&fscr, FSCREEN_XYPOS, &scr_g.x, &scr_g.y,
		&scr_g.width, &scr_g.height);
	get_window_borders(fw, &b);
	snprintf(cmd, sizeof(cmd),
		"ResizeMoveMaximize %dp %dp +%dp +%dp ewmhiwa",
		scr_g.width, scr_g.height,
		scr_g.x - b.top_left.width + page_x,
		scr_g.y - b.top_left.height + page_y);
```

Keep the `ResizeMoveMaximize` command seam. Dest monitor is `fw->m` after ownership commit (not a scaled copy of the current fullscreen frame). `apply_decor_change` already runs before size — keep that order. Do not overwrite `logical_client`. Leave-fullscreen restores via stored logical normal (see unmaximize).

When a fullscreen window crosses monitors, re-fullscreen onto dest `si` rectangle.

**Frame extents stay dest chrome pixels** (`fvwm/ewmh.c:1309-1335`):
```c
void EWMH_SetFrameStrut(FvwmWindow *fw)
{
	long val[4];
	size_borders b;

	get_window_borders(fw, &b);
	val[0] = b.top_left.width;
	val[1] = b.bottom_right.width;
	val[2] = b.top_left.height;
	val[3] = b.bottom_right.height;
	ewmh_ChangeProperty(/* _KDE_NET_WM_FRAME_STRUT */);
	ewmh_ChangeProperty(/* _NET_FRAME_EXTENTS */);
}
```

Call after dest chrome rebind. Never publish logical units.

---

### `fvwm/add_window.c` (controller, request-response)

**Analog:** first constrained client size + `SET_FORCE_NEXT_CR` + dest chrome after ownership (`fvwm/add_window.c:2524-2566`).

Initialize `logical_client` from the first constrained **client** width/height (frame minus borders at the owning monitor's scale, which at map is the dest). Set `logical_client_valid`. Do not initialize from `g.frame`.

```c
	if (is_resizing_event_pending(fw) == True)
	{
		SET_FORCE_NEXT_CR(fw, 1);
		SET_FORCE_NEXT_PN(fw, 1);
		mr_args = frame_create_move_resize_args(
			fw, FRAME_MR_FORCE_SETUP_NO_W | FRAME_MR_DONT_DRAW,
			NULL, &fw->g.frame, 0, DIR_NONE);
	}
	frame_move_resize(fw, mr_args);
	/* ... */
	update_fvwm_monitor(fw);
	SelectDecor(fw, &style, &buttons);
	setup_window_font(fw, &style, True);
	setup_icon_font(fw, &style, True);
	BroadcastConfig(M_ADD_WINDOW,fw);
```

Map path leftover (frame then dest chrome) is Phase 4; do not invert the entire AddWindow order in this phase unless a follow-up plan needs dest chrome before the initial `frame_move_resize`. Live crossings (update.c) invert it. Initial logical size is still recorded after first constrain.

---

### `fvwm/events.c` (controller, event-driven)

**Analog:** `_handle_cr_on_client` (`fvwm/events.c:920-1155`) and `SET_FORCE_NEXT_CR` (`fvwm/window_flags.h:597-601`).

Rewrite `logical_client` only on a user/client resize that **actually changes** client size. Ignore ConfigureRequest echoes of a size the WM just sent.

**Existing echo / merge gate** (`fvwm/events.c:943-956,1138-1152`):
```c
	/* Merge all pending ConfigureRequests for the window into a single
	 * event. */
	if (ea && CR_MOTION_METHOD(fw) != CR_MOTION_METHOD_AUTO) {
		count = _merge_cr_moveresize(ea, e, fw, &b);
		if (count > 0) {
			return 0;
		}
	}
	/* ... */
	} else if ((cre->value_mask & CWX) || (cre->value_mask & CWY)
		   || d_g.width || d_g.height) {
		frame_setup_window_app_request(/* ... */);
		update_absolute_geometry(fw);
		maximize_adjust_offset(fw);
	} else if (DO_FORCE_NEXT_CR(fw)) {
		*ret_do_send_event = 1;
	}
	SET_FORCE_NEXT_CR(fw, 0);
```

Research A3: reuse `SET_FORCE_NEXT_CR` / merge-CR as the echo gate. If a CR size matches the last WM-sent client pixels, do not `remove()` dest pixels into a new logical source. If the existing gate is insufficient, compare against last-sent client size explicitly — do not invent a second detector unless that comparison is required.

Do not update logical size on maximize/fullscreen, chrome-only dest rebind, or WM scale transitions.

---

### `libs/meson.build` (config, batch)

**Analog:** `libs/meson.build:26-58` — insert `geometry_transition.c` next to the other Phase 2–4 policy units:

```meson
    'icccm_size_hints.c',
    'monitor_ownership.c',
    'monitor_scale.c',
    'monitor_topology.c',
    'wm_ui_metrics.c',
```

Keep alphabetical-ish grouping with those files. No new dependency.

---

### `tests/meson.build` (config, batch)

**Analog:** `tests/meson.build:31-40` (`wm-ui-metrics`) and `62-71` (`icccm-size-hints`). Copy `link_with: libfvwm3`, `c_args: ['-UNDEBUG']`, `dependencies: all_found_deps`. Place the new block after `icccm_size_hints_test`. Do not add a live-X scenario.

---

### `doc/fvwm3_manpage_source.adoc` (config, file-I/O)

**Analog:** existing MonitorScale retained-source / pixel-contract paragraph (`doc/fvwm3_manpage_source.adoc:7505-7526`).

```asciidoc
Scale-sensitive integer conversion rounds to nearest with exact half ties
away from zero for both signs.  Overflow is an explicit failure and never
wraps or clamps.  Each conversion uses the retained exact source value;
a previously rounded result is never fed back as a new source, preventing
cumulative drift.
+
These scales apply to FVWM-owned UI and later FVWM geometry policy.  X11,
ICCCM, EWMH, and existing module geometry remain actual root pixels, and
FVWM does not scale arbitrary client pixels.
```

Extend that block (do not start a new conflicting contract): eligible windows preserve logical **client** size across monitor scale changes; maximize/fullscreen fill dest work area / dest monitor; `_NET_FRAME_EXTENTS` remains dest chrome pixels; Status v4 and module packets stay actual pixels.

---

## Shared Patterns

### Status enums and init-0 outputs
**Source:** `libs/icccm_size_hints.h:8-13`, `libs/monitor_scale.c:243-253`, `libs/monitor_ownership.c:175-178`
**Apply to:** `libs/geometry_transition.[ch]`, tests
```c
typedef enum
{
	ICCCM_CONSTRAIN_OK,
	ICCCM_CONSTRAIN_OVERFLOW,
	ICCCM_CONSTRAIN_INVALID_ARGUMENT
} icccm_constrain_status;

if (converted != NULL) {
	*converted = 0;
}
*result = (monitor_ownership_result)MONITOR_OWNERSHIP_RESULT_SAFE_INIT;
```
Never clamp overflow to `INT_MAX`. Recoverable diagnostics in core use `fvwm_debug(__func__, ...)`.

### Retained exact source (no `remove(apply(...))`)
**Source:** `libs/monitor_scale.h:94-98`, `tests/test_monitor_scale.c:353-365`
**Apply to:** planner, logical store, grab offset, 1,000-trip tests
Every dest conversion starts from stored logical integers. Update logical only on user/client resize, initial map, not on WM transitions.

### One coordinator, one commit
**Source:** `fvwm/update.c:666-742`
**Apply to:** `update.c`, `move_resize.c` (`fw->m` assignments), `ewmh.c`
Do not scale opportunistically at every `fw->m =`. `update_fvwm_monitor` is the commit. Last-transition tuple makes a second identical call `NOOP`.

### Pointer-frozen drag
**Source:** `libs/monitor_ownership.h:7`, `tests/test_monitor_ownership.c:446-507`, `fvwm/move_resize.c:2799-2898`, `fvwm/misc.c:136-158`
**Apply to:** `move_loop` + planner GEO-04 tests
```c
#define MONITOR_OWNERSHIP_HYSTERESIS_PX 8
```
1×1 pointer rectangle + incumbent hysteresis. Keep `GrabEm`. Convert offset; origin = pointer + dest_offset.

### Dest chrome then frame
**Source:** `fvwm/geometry.c:511-536`, `fvwm/add_window.c:2547-2566`, `fvwm/ewmh.c:1309-1335`
**Apply to:** live crossings in `update_fvwm_monitor`
SelectDecor + font rebind at dest `effective_scale`, then `get_window_borders`, then compose, then `EWMH_SetFrameStrut`. `_NET_FRAME_EXTENTS` equals dest chrome pixels.

### Derived states recompute
**Source:** `fvwm/move_resize.c:5202-5227,5410-5414`, `fvwm/ewmh.c:2024-2083`
**Apply to:** GEO-06 wiring
Maximize = dest work rect via `EWMH_GetWorkAreaIntersection`. Fullscreen = dest monitor via `FScreenGetScrRect`. Restore = stored logical normal + dest chrome. Do not scale `g.max`.

### Eligibility flags
**Source:** `fvwm/window_flags.h:236-265`
**Apply to:** core eligibility mapping into the planner
```c
#define HAS_OVERRIDE_SIZE_HINTS(fw) \
	((fw)->flags.common.s.has_override_size)
#define IS_SIZE_FIXED(fw) \
	((fw)->flags.common.s.is_size_fixed)
#define IS_PSIZE_FIXED(fw) \
	((fw)->flags.common.s.is_psize_fixed)
```
Fixed-size (`min == max`, not override): keep client pixels, still dest chrome. Iconified: skip (icon geometry is separate). Shaded: convert unshaded client size.

### ConfigureRequest echo gate
**Source:** `fvwm/events.c:943-956,1149-1152`, `fvwm/window_flags.h:597-601`
**Apply to:** logical-size writes
```c
#define DO_FORCE_NEXT_CR(fw) \
	((fw)->flags.do_force_next_cr)
#define SET_FORCE_NEXT_CR(fw,x) \
	(fw)->flags.do_force_next_cr = !!(x)
```
Echoes of WM-sent dest size must not rewrite `logical_client`.

### Native test registration
**Source:** `tests/meson.build:62-71`, `tests/test_icccm_size_hints.c:372-387`
**Apply to:** `tests/test_geometry_transition.c` + `tests/meson.build`
GPL banner, `config.h` first, `static void test_<behavior>(void)`, explicit `main()` calls, `-UNDEBUG`, `link_with: libfvwm3`. No live-X in this phase.

### Protocol pixel guardrail
**Source:** `libs/vpacket.h:18-65`, `doc/fvwm3_manpage_source.adoc:7522-7526`
**Apply to:** every plan — do not touch `vpacket.h`, `CONFIGARGS` / `module_interface.c`, or Status v4 schema.
```c
	signed long    frame_x;
	signed long    frame_y;
	unsigned long  frame_width;
	unsigned long  frame_height;
	unsigned short title_height;
	unsigned short border_width;
```
Those remain actual X11 pixels. No logical fields, no scale metadata.

### Don't hand-roll
| Problem | Use instead | Analog |
|---------|-------------|--------|
| Pixel conversion | `monitor_scale_apply` | `libs/monitor_scale.h:94-96` |
| Hint solving | `icccm_constrain_size` | `libs/icccm_size_hints.h:34-36` |
| Owner selection | `monitor_ownership_resolve` | `libs/monitor_ownership.h:36-39` |
| Gravity origin | `gravity_resize` | `libs/gravity.c:111-123` |
| Chrome pixels | dest `get_window_borders` | `fvwm/geometry.c:511-536` |
| Frame/X configure | `frame_setup_window` | `fvwm/frame.h:65-67` |
| Pointer grab | `GrabEm` / `change_grab_cursor` | `fvwm/misc.c:136-158` |
| Work area | `EWMH_GetWorkAreaIntersection` | `fvwm/move_resize.c:5412-5414` |

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| — | — | — | All work files have exact or in-place analogs. Extra enumerators `GEOMETRY_TRANSITION_INELIGIBLE` / `GEOMETRY_TRANSITION_NOOP` and `geometry_transition_kind` are new names (research-locked), not missing analogs. |

## Guardrails (not work items)

| File | Why listed | Planner action |
|------|------------|----------------|
| `libs/vpacket.h` | GEO-07 pixel contract | `git diff` must be empty |
| `fvwm/module_interface.c` | `CONFIGARGS` pixel fields | `git diff` must be empty |
| `fvwm/status_json.c` | Status v4 actual-pixel geometry | `git diff` must be empty |

`fvwm/geometry.c` `constrain_size` is the X-facing wrapper (borders, styles, `XSync`, `CS_*`). Core wiring may keep calling it after dest chrome; planner tests call `icccm_constrain_size` only. Do not duplicate constrain logic in `geometry_transition.c`.

## Metadata

**Analog search scope:** `libs/` (scale, constrain, ownership, metrics, gravity, rect), `fvwm/` (update, move_resize, ewmh, add_window, events, geometry, frame, misc, fvwm.h, window_flags), `tests/` (meson + assertion executables), `doc/fvwm3_manpage_source.adoc`, prior phase PATTERNS (`03`, `04`)
**Files scanned:** 28
**Pattern extraction date:** 2026-08-31
