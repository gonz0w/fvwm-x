# Phase 3: Geometry and Ownership Contracts - Pattern Map

**Mapped:** 2026-08-30
**Files analyzed:** 21 new/modified/guardrail files
**Analog families found:** 5

## Scope Extracted

Phase 3 extracts two allocation-free `libs/` policy units (monitor ownership over literal root-pixel rectangles; integer ICCCM size constraints), then wires them through `FindScreenOfXY`, `update_fvwm_monitor`, `constrain_size`, work-area/strut intersection, placement, and `MoveToScreen`. Protocol seams stay actual pixels. Native tests cover ownership ties and ICCCM; existing scale/topology tests remain green. No live-X matrix in this phase.

`libs/monitor_topology.h` is an input contract only: consume `monitor_topology_monitor` `name_atom`/`name`/`x`/`y`/`width`/`height` at lines 21-34. Do **not** put ownership or scale into topology equality or epoch state.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog / In-place Seam | Match Quality |
|---|---|---|---|---|
| `libs/monitor_ownership.h` (new) | model | transform | `libs/monitor_scale.h` | exact |
| `libs/monitor_ownership.c` (new) | utility | transform | `libs/monitor_scale.c` + `libs/FScreen.c` distance | role-match |
| `libs/icccm_size_hints.h` (new) | model | transform | `libs/monitor_scale.h` | exact |
| `libs/icccm_size_hints.c` (new) | utility | transform | `libs/monitor_scale.c` + `fvwm/geometry.c` constrain | role-match |
| `libs/FScreen.c` | service | request-response | existing `FindScreenOfXY` / `FScreenGetScrRect` | exact in-place seam |
| `libs/FScreen.h` | model | request-response | existing `FindScreenOfXY` declaration | exact in-place seam |
| `fvwm/update.c` | service | request-response | existing `update_fvwm_monitor` | exact in-place seam |
| `fvwm/geometry.c` | service | transform | existing `constrain_size` / `gravity_constrain_size` | exact in-place seam |
| `fvwm/add_window.c` | service | transform | existing `GetWindowSizeHintsWithCheck` | exact in-place seam |
| `fvwm/ewmh.c` | service | transform | existing work-area / strut / frame-extents | exact in-place seam |
| `fvwm/placement.c` | controller | request-response | existing `FScreenGetScrRect` + work-area | exact in-place seam |
| `fvwm/move_resize.c` | controller | request-response | existing `CMD_MoveToScreen` / `MOVE_SCREEN` | exact in-place seam |
| `fvwm/fvwm.h` | model | — | `fw->m` / `g.frame` storage; no logical fields | guardrail |
| `fvwm/module_interface.c` | route | request-response | `CONFIGARGS` pixel fields | guardrail |
| `libs/vpacket.h` | model | request-response | `ConfigWinPacket` pixel fields | guardrail |
| `libs/meson.build` | config | batch | `libfvwm3_sources` | exact in-place seam |
| `tests/test_monitor_ownership.c` (new) | test | transform | `tests/test_monitor_scale.c` + topology identity | role/data-flow match |
| `tests/test_icccm_size_hints.c` (new) | test | transform | `tests/test_monitor_scale.c` | exact |
| `tests/meson.build` | config | batch | `monitor-scale` executable registration | exact in-place seam |
| `doc/fvwm3_manpage_source.adoc` | config | file-I/O | `MoveToScreen` / `StartsOnScreen` | exact in-place seam |
| `libs/monitor_topology.h` | model | — | committed snapshot fields | input-only |

## Pattern Assignments

### `libs/monitor_ownership.h` (model, transform)

**Analog:** `libs/monitor_scale.h:1-100`

Copy the compact public-contract shape: uppercase include guard derived from filename, standard-library plus existing domain types, named result/status types, explicit functions, and a safe-init macro. Do not expose arithmetic or RB trees.

```c
#ifndef FVWM_MONITOR_SCALE_H
#define FVWM_MONITOR_SCALE_H

#include "libs/monitor_topology.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum
{
	MONITOR_SCALE_CONVERSION_OK,
	MONITOR_SCALE_CONVERSION_OVERFLOW,
	MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT
} monitor_scale_conversion_status;

typedef struct
{
	monitor_scale_factor	   factor;
	monitor_scale_source	   source;
	monitor_scale_match_status match_status;
	monitor_scale_auto_status  auto_status;
	monitor_scale_boundary	   boundary;
	monitor_scale_reason	   reason;
} monitor_scale_result;

#define MONITOR_SCALE_RESULT_SAFE_INIT                                 \
	{{MONITOR_SCALE_QUARTERS_MIN}, MONITOR_SCALE_SOURCE_SAFE,      \
	    MONITOR_SCALE_MATCH_NONE, MONITOR_SCALE_AUTO_MISSING_AXIS, \
	    MONITOR_SCALE_BOUNDARY_NONE, MONITOR_SCALE_REASON_SAFE_DEFAULT}
```

**Input fields to consume** (`libs/monitor_topology.h:21-34`):

```c
typedef struct
{
	Atom	  name_atom;
	char	 *name;
	int	  x;
	int	  y;
	int	  width;
	int	  height;
	/* mm_*, primary, outputs are not ownership inputs */
} monitor_topology_monitor;
```

**Rectangle type to reuse, not reinvent** (`libs/fvwmrect.h:16-22`):

```c
typedef struct
{
	int x;
	int y;
	int width;
	int height;
} rectangle;
```

Define: a monitor descriptor (or reuse snapshot fields), a result with owner identity + reason (containment / overlap / incumbent-kept / gap / fallback), hysteresis constant `MONITOR_OWNERSHIP_HYSTERESIS_PX` (research default 8), and `monitor_ownership_resolve(window_rect, monitors, count, incumbent_atom, hysteresis_px)`. Keep X/`struct monitor *` out of the pure header; callers map `name_atom` back to `struct monitor *`.

Also copy the compact-header style of `fvwm/event_loop_policy.h:3-35` if the result type stays small: named struct, enum, function prototypes, no implementation.

---

### `libs/monitor_ownership.c` (utility, transform)

**Primary analog:** `libs/monitor_scale.c:1-24,201-241` for GPL banner, `config.h` first, defensive outputs, `__int128` checked arithmetic.

```c
/* -*-c-*- */
/* This program is free software; ... GPL notice ... */

#include "config.h"

#include "libs/monitor_scale.h"

#include <limits.h>
#include <string.h>

typedef __int128 monitor_scale_wide;

static monitor_scale_conversion_status
convert_ratio(int source, unsigned int numerator, unsigned int denominator,
    int *converted)
{
	if (converted == NULL) {
		return MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT;
	}
	*converted = 0;
	if (denominator == 0) {
		return MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT;
	}
	/* wide intermediates; initialize output before any failure */
```

**Secondary analog — gap distance** (`libs/FScreen.c:871-890`). Copy the Manhattan (taxi-cab) metric, but operate on snapshot rects, not `struct monitor *`, and do not wrap through `DisplayWidth`:

```c
int64_t
fscreen_monitor_distance_to_point(const struct monitor *monitor, int x, int y)
{
	int64_t left	 = monitor->si->x;
	int64_t top	 = monitor->si->y;
	int64_t right	 = left + monitor->si->w;
	int64_t bottom	 = top + monitor->si->h;
	int64_t point_x	 = x;
	int64_t point_y	 = y;
	int64_t distance = 0;

	if (point_x < left)
		distance += left - point_x;
	else if (point_x > right)
		distance += point_x - right;
	if (point_y < top)
		distance += top - point_y;
	else if (point_y > bottom)
		distance += point_y - bottom;
	return distance;
}
```

**Do not copy** the wrap/first-match/`exit(106)` path in `FindScreenOfXY` (`libs/FScreen.c:1247-1307`):

```c
	x %= all_widths;
	y %= all_heights;
	if (x < 0)
		x += all_widths;
	if (y < 0)
		y += all_heights;
	/* ... RB_FOREACH first containment hit ... */
	if (m_min == NULL) {
		fvwm_debug(__func__, "Couldn't find any monitor");
		exit(106);
	}
```

**Do not copy** RB compare order as ownership policy (`libs/FScreen.c:838-868`): disabled-bit then `y,x` then `w,h` then name. Ties use Phase 1 identity: `name_atom` then `strcmp(name)`.

**Rectangle intersection analog** (`libs/Rectangles.c:40-56`). Use for overlap tests; compute area with `__int128`, never `int area = w * h`:

```c
Bool frect_get_intersection(
	int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2,
	XRectangle *r)
{
	if (x1 + w1 > x2 && x1 < x2 + w2 &&
	    y1 + h1 > y2 && y1 < y2 + h2)
	{
		if (r)
		{
			r->x = max(x1,x2);
			r->y = max(y1,y2);
			r->width = min(x1+w1,x2+w2) - max(x1,x2);
			r->height =min(y1+h1,y2+h2) - max(y1,y2);
		}
		return True;
	}
	return False;
}
```

Also available: `fvwmrect_do_rectangles_intersect` (`libs/fvwmrect.c:53-73`). Prefer existing helpers; do not invent a third rectangle type.

**Core algorithm to implement (not present in-tree):**

1. Skip disabled / global-screen entries if the caller still passes them; the pure function should only see enabled non-global snapshot monitors.
2. Score integer overlap area of the window rectangle against each monitor rect.
3. Keep incumbent when it still overlaps and challenger extra overlap `<= MONITOR_OWNERSHIP_HYSTERESIS_PX`.
4. Otherwise unique maximum overlap; remaining ties by `name_atom` then `strcmp(name)`.
5. All overlaps 0 (gap): Manhattan distance then the same identity tie-break.
6. Empty input: return a total fallback (primary flag if present, else identity-least) with an explicit reason; never `exit()`.
7. Allocation-free; initialize the result before returning.

---

### `libs/icccm_size_hints.h` (model, transform)

**Analog:** `libs/monitor_scale.h:55-98` status enum + conversion functions that initialize outputs.

```c
typedef enum
{
	MONITOR_SCALE_CONVERSION_OK,
	MONITOR_SCALE_CONVERSION_OVERFLOW,
	MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT
} monitor_scale_conversion_status;

monitor_scale_conversion_status
monitor_scale_apply(int source, monitor_scale_factor factor, int *converted);
```

Mirror that as `icccm_constrain_status` (`OK` / `OVERFLOW` / `INVALID_ARGUMENT`) plus an `icccm_size_hints` struct of ICCCM integer fields (`PMinSize | PMaxSize | PResizeInc | PAspect | PBaseSize | PWinGravity`). Include `libs/fvwmrect.h` for optional gravity-adjusted `rectangle *`. Do not include `fvwm/fvwm.h` or `XSizeHints` as the public contract — copy integer fields in.

---

### `libs/icccm_size_hints.c` (utility, transform)

**Primary analog — checked integer math / safe outputs:** `libs/monitor_scale.c:201-241` and `libs/ImageSafety.c:27-37`.

```c
if (pixel_start == NULL || best_width == NULL || best_height == NULL)
{
	return false;
}
*pixel_start = 0;
*best_width = 0;
*best_height = 0;

if (data == NULL)
{
	return false;
}
```

**Gravity analog — reuse, do not duplicate** (`libs/gravity.c:69-93,111-119`):

```c
void gravity_get_offsets(int grav, int *xp,int *yp)
{
	static struct _gravity_offset gravity_offsets[11] = {
		{  0,  0 },     /* ForgetGravity */
		{ -1, -1 },     /* NorthWestGravity */
		/* ... StaticGravity ... */
	};
	if (grav < ForgetGravity || grav > StaticGravity)
	{
		*xp = *yp = 0;
	}
	else
	{
		*xp = (int)gravity_offsets[grav].x;
		*yp = (int)gravity_offsets[grav].y;
	}
}

void gravity_resize(int gravity, rectangle *rect, int wdiff, int hdiff)
{
	int xoff;
	int yoff;

	gravity_get_offsets(gravity, &xoff, &yoff);
	rect->x -= (wdiff * (xoff + 1)) / 2;
	rect->width += wdiff;
	rect->y -= (hdiff * (yoff + 1)) / 2;
```

Default missing `PWinGravity` to `NorthWestGravity` as `GetWindowSizeHintsWithCheck` does (`fvwm/add_window.c:2930-2933`).

**ICCCM rules to extract from `constrain_size`, replacing `double` aspect:**

From `fvwm/geometry.c:707-722` keep the PBaseSize-for-aspect rule, drop the `double` ratios:

```c
	if (fw->hints.flags & PBaseSize)
	{
		/*
		 * ICCCM 2 demands that aspect ratio should apply to width -
		 * base_width. To prevent funny results, we reset PBaseSize in
		 * GetWindowSizeHints, if base is not smaller than min.
		 */
		s.width -= base.width;
		/* ... */
	}
	rmin = (double)fw->hints.min_aspect.x / (double)fw->hints.min_aspect.y;
	rmax = (double)fw->hints.max_aspect.x / (double)fw->hints.max_aspect.y;
```

Replace `rmin`/`rmax` with checked wide cross-products (`min_aspect.x * height` vs `min_aspect.y * width`), using `__int128` as in `monitor_scale.c:23,136-141`. Never use min size as the aspect base (ICCCM: subtract base only when `PBaseSize` is present).

From `fvwm/geometry.c:914-1013` copy the **order** of ICCCM steps, not the `FvwmWindow` coupling:

1. Clamp to min/max
2. Round to `base + N * inc` (pure function: nearest/down, not `CS_ROUND_UP` interactive)
3. Re-check min/max after increment snap
4. Aspect
5. Return client size excluding borders

Do **not** copy style Min/Max WindowSize, border subtraction, `XSync`, `HAS_NEW_WM_NORMAL_HINTS`, `CS_ROUND_UP`, or max-defect — those stay in the `constrain_size` wrapper.

Treat `width_inc`/`height_inc` of 0 as invalid (sanitizer already forces 1; pure function still rejects 0 as divisor). Fixed-size: `PMinSize` and `PMaxSize` set and `min == max` on both axes.

Callers that have a scaled target must run `monitor_scale_apply` **first** (`libs/monitor_scale.h:94-98`), then `icccm_constrain_size` on that pixel result. Never constrain then scale.

---

### `libs/FScreen.c` (service, request-response)

**In-place seam:** `FindScreenOfXY` at `libs/FScreen.c:1247-1307`.

Keep the public symbol. Replace wrap + first RB hit with a 1×1 (or 0-area point) call into `monitor_ownership_resolve` using committed snapshot rects, **no incumbent**, **no** `x %= monitor_get_all_widths()`.

**Keep unchanged** paging helpers (`libs/FScreen.c:482-491`):

```c
int
monitor_get_all_widths(void)
{
	return (DisplayWidth(disp, DefaultScreen(disp)));
}
```

Do not edit virtual-desktop page arithmetic that legitimately uses those helpers (`libs/FScreen.c:1217-1219`, `1610`, `1639`).

**Keep** `FScreenGetScrRect` returning `m->si->{x,y,w,h}` (`libs/FScreen.c:1395-1414`) — already root-pixel monitor geometry. After `FindScreenOfXY` stops wrapping, `FSCREEN_XYPOS` / pointer queries follow.

**Total fallback instead of `exit(106)`** (`libs/FScreen.c:1302-1306`): if ownership returns no match, use primary else identity-least enabled monitor and `fvwm_debug(__func__, ...)`. Policy itself must not `exit()`.

**Snapshot input:** `FScreenGetTopologySnapshot()` (`libs/FScreen.h:227-228`). Never issue a second RandR query from ownership.

Point-query callers already go through `FindScreenOfXY` (`libs/FScreen.c:268`, `1343`, `1369`). Keep that wrapper; do not scatter ownership math into pointer/placement call sites.

---

### `libs/FScreen.h` (model, request-response)

**In-place seam:** `FindScreenOfXY` at line 283. Keep the signature. Do not add scale or logical-unit fields to `struct monitor`. If tests need a pure distance helper, follow the existing `#ifdef FSCREEN_TESTING` pattern at lines 253-263 rather than making production globals writable.

---

### `fvwm/update.c` (service, request-response)

**In-place seam:** `update_fvwm_monitor` at `fvwm/update.c:665-715`.

Current assignment (do not keep top-left-only):

```c
Bool update_fvwm_monitor(FvwmWindow *fw)
{
	rectangle g;
	struct monitor *mnew;

	g = IS_MAXIMIZED(fw) ? fw->g.max : fw->g.normal;
	mnew = FindScreenOfXY(g.x, g.y);
	/* Avoid unnecessary updates. */
	if (mnew == fw->m && fw->UpdateDesk < 0)
		return False;
```

Replace the `FindScreenOfXY(g.x, g.y)` lookup with the ownership resolver over the **frame** rectangle (`fw->g.frame` when mapped, else `g.normal`/`g.max` per research A1) plus incumbent `fw->m` identity (`name_atom`). Keep the rest of the function: desk map/unmap, `fw->m_prev` / `fw->m` swap, `EWMH_SetCurrentDesktop`, `desk_add_fw`, `BroadcastConfig`.

**Do not** call `monitor_scale_apply` here (GEO-02 / Phase 5). Do not add logical geometry fields to `FvwmWindow`.

Storage already exists (`fvwm/fvwm.h:699-712`, `977-978`):

```c
typedef struct window_g
{
	rectangle frame;
	rectangle normal;
	rectangle max;
	size_rect max_defect;
	position max_offset;
} window_g;
	struct monitor *m;
	struct monitor *m_prev;
```

---

### `fvwm/geometry.c` (service, transform)

**In-place seam:** `constrain_size` at `fvwm/geometry.c:826-1028` and `gravity_constrain_size` at `1035-1060`.

Keep as the X-facing wrapper:

- `DO_DISABLE_CONSTRAIN_SIZE_FULLSCREEN` early return
- `HAS_NEW_WM_NORMAL_HINTS` → `XSync` + `GetWindowSizeHints`
- `CS_UPDATE_MAX_DEFECT` / maximized defect
- `get_window_borders` subtract/add
- style Min/Max WindowSize (`lookup_style`, `SGET_MIN_WINDOW_*`)
- `CS_ROUND_UP` interactive edge against `MotionNotify` (this path still uses `monitor_get_all_widths()` as **root framebuffer** for pointer-off-screen, which is paging-space, not ownership — do not rewrite it into snapshot rects unless a GEO-05 bug is demonstrated)

Delegate the ICCCM client-size core (min/max/base/inc/aspect/fixed) to `icccm_constrain_size`. On overflow/invalid, initialize outputs and log with `fvwm_debug(__func__, ...)`; do not apply a plausible clamped size after overflow.

Keep `gravity_constrain_size` calling `constrain_size` then `gravity_resize` — or have the pure function optionally adjust an origin rectangle via `gravity_resize`. Do not duplicate gravity tables.

---

### `fvwm/add_window.c` (service, transform)

**In-place seam:** `GetWindowSizeHintsWithCheck` at `fvwm/add_window.c:2742-2933`.

Keep the untrusted-client sanitizer. Copy these invariants into the pure ICCCM unit's tests, but do **not** move X property reads into `libs/`:

- `width_inc`/`height_inc` <= 0 → force 1 (`2777-2814`)
- missing `PMinSize` copies `PBaseSize` or 1 (`2817-2848`)
- ICCCM min/base fill (`2875-2928`)
- `base > min` clears `PBaseSize` so aspect does not subtract an illegal base (`2898-2908`)
- missing `PWinGravity` → `NorthWestGravity` (`2930-2933`)

If the wrapper needs a packed `icccm_size_hints`, fill it from the already-sanitized `fw->hints` integers. Do not bypass this function.

---

### `fvwm/ewmh.c` (service, transform)

**In-place seams:**

1. `ewmh_ComputeAndSetWorkArea` / `ewmh_HandleDynamicWorkArea` (`1008-1111`) currently store insets relative to monitor origin (`x = left; width = m->si->w - (left+right)`).
2. `EWMH_GetWorkAreaIntersection` (`1119-1160`) re-adds `m->si->x` at use time.
3. `ewmh_GetStrutIntersection` (`1198-1236`) still uses `0..DisplayWidth` / `0..DisplayHeight`.

Plan: compute each monitor's work rectangle as `intersect(monitor_rect, root_minus_struts)` in **root pixels**. Intersection of a query rect with that work rect must be correct for negative `m->si->x`.

**Do not** change `_NET_WORKAREA` published shape (`ewmh_SetWorkArea` at `986-1006`): one `x,y,width,height` per desktop, actual pixels, relative to the viewport. Do not invent a per-monitor EWMH property. Do not implement `_NET_WM_STRUT_PARTIAL` (`fvwm/ewmh.c:35`).

**GEO-07 guard — keep pixel frame extents** (`1269-1295`):

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
	ewmh_ChangeProperty(..., "_NET_FRAME_EXTENTS", ..., val, 4);
}
```

Do not multiply extents by effective scale. Strut record type already exists (`fvwm/fvwm.h:180-186`).

If work-area math is extracted for TEST-01, put a pure rectangle helper next to ownership (or a small function in `ewmh.c` that tests can reach). Prefer `frect_get_intersection` over the `float get_intersection` at `1172-1196`.

---

### `fvwm/placement.c` (controller, request-response)

**In-place seam:** `fvwm/placement.c:1766-1826`.

Already fetches `screen_g` via `FScreenGetScrRect` and then `EWMH_GetWorkAreaIntersection`. After ownership and work-area helpers are root-pixel-correct, these call sites follow. Do **not** restore the global-screen fallback that uses `monitor_get_all_widths()` as a monitor rectangle (`1791-1794`) for RandR layouts — that is framebuffer paging space, not a monitor. `StartsOnScreen` name resolution (`monitor_resolve_name`) stays as-is.

Do not iterate mapped windows to apply scale.

---

### `fvwm/move_resize.c` (controller, request-response)

**In-place seam:** `CMD_MoveToScreen` at `2335-2338` and `MOVE_SCREEN` at `2211-2244`.

```c
void CMD_MoveToScreen(F_CMD_ARGS)
{
	_move_window(F_PASS_ARGS, False, MOVE_SCREEN);
	update_fvwm_monitor(exc->w.fw);
}

	s.x = m->si->x;
	s.y = m->si->y;
	s.width = m->si->w;
	s.height = m->si->h;
	/* ... */
	t.width = monitor_get_all_widths();
	t.height = monitor_get_all_heights();
	fvwmrect_move_into_rectangle(&r, &t);
	/* then move to screen */
```

Keep `m->si->{x,y,w,h}` as the destination monitor rectangle. Keep `monitor_get_all_widths()` only for the **page** rectangle `t` (virtual desktop). After move, `update_fvwm_monitor` uses the new ownership contract. Do not change `MOVE_PAGE` paging math.

---

### `fvwm/fvwm.h` / `fvwm/module_interface.c` / `libs/vpacket.h` (guardrails)

**GEO-07:** `git diff` of these plus `fvwm/status_json.c` and `libs/Module.h` must show **no** new logical-unit fields.

`CONFIGARGS` already publishes signed frame pixels (`fvwm/module_interface.c:308-322`):

```c
#define CONFIGARGS(_fw) 34,				\
		(unsigned long)(-sizeof(Window)),	\
		&FW_W(*(_fw)),				\
		/* ... */				\
		(unsigned long)(0),			\
		&(*(_fw))->g.frame.x,			\
		(unsigned long)(0),			\
		&(*(_fw))->g.frame.y,			\
		(unsigned long)(0),			\
		&(*(_fw))->g.frame.width,		\
		(unsigned long)(0),			\
		&(*(_fw))->g.frame.height,		\
```

`ConfigWinPacket` (`libs/vpacket.h:27-46`) already has `signed long frame_x/y` and unsigned frame/hint sizes. Do not convert them. Do not add `effective_scale` to packets.

---

### `libs/meson.build` (config, batch)

**In-place seam:** `libs/meson.build:1-62`.

```meson
libfvwm3_sources = files(
    # ...
    'monitor_scale.c',
    'monitor_topology.c',
    'safemalloc.c',
```

Add `monitor_ownership.c` and `icccm_size_hints.c` once, alphabetically with the other `monitor_*` / `i*` sources. Headers are not listed. No new dependencies.

---

### `tests/test_monitor_ownership.c` (test, transform)

**Primary analog:** `tests/test_monitor_scale.c:1-56,368-385` — GPL banner, `config.h` first, quoted includes, `static void test_<behavior>(void)`, explicit `main()`.

```c
#include "config.h"

#include "libs/monitor_scale.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

static void
test_valid_committed_dpi_resolves_automatic_factor(void)
{
	monitor_topology_monitor monitor = {
	    .width     = 1920,
	    .height    = 1080,
	    /* ... */
	};
	monitor_scale_result result;

	result = monitor_scale_resolve(&monitor, NULL, NULL);
	assert(result.factor.quarters == 8);
}

int
main(void)
{
	test_valid_committed_dpi_resolves_automatic_factor();
	/* ... every case named and called ... */
	return 0;
}
```

**Identity / reorder analog:** `tests/test_monitor_topology.c:197-202,807-842` — fixtures with `name_atom` + `name` + signed `x,y`; assert equality is independent of insertion order. Ownership tests **must** insert the same clone/overlap pair in both orders and assert the same owner.

Required cases (TEST-01 / TOP-02 / GEO-01 / GEO-05):

- Negative-origin monitor owns a window whose frame sits in that rect (no `DisplayWidth` wrap)
- Partial overlap: unique max area wins
- Exact clones (identical rects): identity tie (`name_atom` then `strcmp`), independent of array order
- Output reorder with unchanged identity/geometry: same owner
- Rotation already baked into snapshot `width`/`height`: swapped w/h still contains correctly
- Incumbent kept on seam when extra overlap <= hysteresis
- Challenger wins only beyond hysteresis
- Gap (zero overlap): Manhattan nearest, then identity
- Empty monitor list: total fallback, no abort; outputs initialized
- Overlap-area overflow: wide intermediates; status/initialized outputs
- Work-area/strut intersection against a negative-origin monitor rect if that helper lives here

Do not `exit(106)` from tests. Do not depend on `struct monitor` RB trees or X.

---

### `tests/test_icccm_size_hints.c` (test, transform)

**Analog:** `tests/test_monitor_scale.c:269-366` — matrix of rounding, overflow, invalid, initialized outputs, and 1,000 iterations from the original source.

```c
static void
test_conversion_limits_overflow_and_invalid_outputs(void)
{
	int converted = 99;

	assert(
	    monitor_scale_apply(INT_MAX, (monitor_scale_factor){5}, &converted)
	    == MONITOR_SCALE_CONVERSION_OVERFLOW);
	assert(converted == 0);
}

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
```

Required GEO-03 cases:

- min / max clamp
- base + N * inc (missing base uses min for grid, not for aspect)
- aspect with `PBaseSize` present vs absent (min is **not** aspect base)
- `base > min` matches sanitizer: aspect ignores base
- fixed-size (`min == max`)
- gravity origin adjust via `gravity_resize`
- 0 increment → `INVALID_ARGUMENT`, outputs initialized
- overflow of increment/aspect products → `OVERFLOW`, outputs initialized
- **scale then constrain:** `monitor_scale_apply` on an exact source, then constrain; never constrain then scale
- 1,000 repeated constrain-from-same-source (no drift); keep existing `monitor-scale` 1,000-loop green as TEST-01

---

### `tests/meson.build` (config, batch)

**Analog:** `tests/meson.build:20-29` (link `libfvwm3` because ownership/icccm live in the static lib, same as scale):

```meson
monitor_scale_test = executable(
    'test-monitor-scale',
    'test_monitor_scale.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('monitor-scale', monitor_scale_test)
```

Register `monitor-ownership` and `icccm-size-hints` the same way. Do **not** add live-X cases (TEST-02 is Phase 8). Existing `monitor-scale`, `monitor-scale-config`, `monitor-topology`, `fscreen-topology-adapter` must stay registered.

---

### `doc/fvwm3_manpage_source.adoc` (config, file-I/O)

**Analogs:** `MoveToScreen` at lines 4034-4036 and `StartsOnScreen` at 6331-6336.

```asciidoc
*MoveToScreen* [_screen_]::
	Moves the selected window to another screen. The _screen_ argument
	must be a valid RandR name.

_StartsOnScreen_ takes one argument. It must be a valid RandR name. A
new window is placed on the specified screen. The default is to place
windows on the screen that contains the mouse pointer at the time the
window is created.
```

Document the user-visible contract: owning monitor is the unique root-pixel overlap result with incumbent hysteresis; coordinates, work areas, struts, configure events, `_NET_FRAME_EXTENTS`, `_NET_WORKAREA`, and module geometry remain actual X11 pixels. Do not document logical units, `_NET_WM_STRUT_PARTIAL`, or Status/module scale metadata.

---

## Shared Patterns

### Pure Policy Units in `libs/`

**Sources:** `libs/monitor_scale.h`, `libs/monitor_scale.c`, `libs/ImageSafety.c`, `fvwm/event_loop_policy.h`.

Apply to `monitor_ownership` and `icccm_size_hints`. `config.h` first; GPL on `.c` not headers; lower-snake APIs; close-to-use C11 declarations; allocation-free; no `FvwmWindow` / `Display *` in the pure unit.

### Explicit Status and Safe Outputs

**Sources:** `libs/monitor_scale.c:211-215`, `libs/ImageSafety.c:27-37`.

Apply to ownership resolve, overlap area, ICCCM constrain, and work-area intersection. Enums for multi-state outcomes; initialize outputs before any failure; never return a plausible clamped value after overflow; never `exit()` from policy.

### Checked Wide Arithmetic

**Source:** `libs/monitor_scale.c:23,119-141,220-230`.

Use `typedef __int128 ..._wide` for overlap area and aspect cross-products. Cast operands before multiply. Return overflow status. No `double`, no `int w * h`.

### Phase 1 Identity, Not Enumeration Order

**Sources:** `libs/monitor_topology.h:21-34`; anti-pattern `libs/FScreen.c:838-868`, `1274-1277`.

Ties and clones compare `name_atom` then `strcmp(name)`. Tests insert both orders. Output XIDs are membership facts, not identity.

### Literal Root Pixels vs DisplayWidth Paging

**Sources:** `libs/FScreen.c:482-491` vs `1247-1260`; `fvwm/ewmh.c:1198-1236`.

Ownership, placement monitor rects, work areas, and struts use snapshot `x,y,width,height`. `monitor_get_all_widths()` remains `DisplayWidth` for virtual-desktop page size. Do not mix the two.

### Scale Then Constrain

**Sources:** `libs/monitor_scale.h:94-98`; `fvwm/geometry.c:826-1028`.

`monitor_scale_apply` from the retained exact source, then ICCCM constrain in pixels. Never constrain in logical units; never scale a previously constrained size; never `monitor_scale_apply` of `hints.min_width` as a substitute for constraining the scaled target.

### Gravity and Rectangles — Reuse

**Sources:** `libs/gravity.c:69-119`, `libs/Rectangles.c:40-56`, `libs/fvwmrect.h:16-22`.

Do not build a second gravity table or a third rectangle type.

### Untrusted Client Hints

**Source:** `fvwm/add_window.c:2742-2933`.

Keep sanitizing `WM_NORMAL_HINTS` in core. Pure constrain rejects 0 increment, inverted aspect, and overflow. Client properties are untrusted X input.

### Diagnostics

**Source:** `libs/FScreen.c:1304`, `fvwm/ewmh.c:1056-1059`.

`fvwm_debug(__func__, ...)` for empty topology, invalid hints, overflow. Encode monitor names if interpolated. No unconditional debug prints.

### GEO-07 Protocol Boundary

**Sources:** `fvwm/module_interface.c:308-322`, `libs/vpacket.h:27-46`, `fvwm/ewmh.c:1269-1295`.

Configure, frame extents, work area, ICCCM hint application, and `ConfigWinPacket` stay actual pixels. Planner verification is `git diff` of `status_json.c`, `Module.h`, `vpacket.h`, `CONFIGARGS`, `EWMH_SetFrameStrut`.

### Native Tests Only

**Sources:** `tests/meson.build:20-29`, `tests/test_monitor_scale.c:368-385`.

Meson C assertions, `-UNDEBUG`, `link_with: libfvwm3`. No new live-X cases. Keep `monitor-scale` 1,000-iteration loop as TEST-01 coverage.

### No Authentication Pattern

This phase is local geometry policy. No auth/authorization guard applies.

## No Analog Found

| File / algorithm | Role | Data Flow | Reason |
|---|---|---|---|
| Incumbent + hysteresis extra-overlap | utility | transform | No in-tree owner keeps an incumbent; copy API/error shape from `monitor_scale_resolve`, implement GEO-01 rules from RESEARCH |
| Integer aspect cross-products | utility | transform | Current `_cs_handle_aspect_ratio` uses `double`; copy step order from `geometry.c:914-1013`, arithmetic from `monitor_scale.c` |
| Root-pixel work-area intersection | service | transform | Current code mixes origin-relative insets with `DisplayWidth` struts; reuse `frect_get_intersection` |

No file is wholly without a useful in-repository analog for structure, includes, tests, or Meson wiring.

## Metadata

**Analog search scope:** `libs/`, `fvwm/`, `tests/`, `doc/`

**Primary analog families (stopped at five):**
1. `libs/monitor_scale.[ch]` + `tests/test_monitor_scale.c` — pure policy, status enums, safe outputs, Meson native tests
2. `libs/FScreen.c` `FindScreenOfXY` / `fscreen_monitor_distance_to_point` / `FScreenGetScrRect` — ownership seam and gap metric
3. `fvwm/geometry.c` `constrain_size` + `libs/gravity.c` + `fvwm/add_window.c` sanitizer — ICCCM wrapper vs pure core
4. `fvwm/ewmh.c` work-area/strut/frame-extents + `fvwm/placement.c` / `move_resize.c` — GEO-05/GEO-07 seams
5. `libs/Rectangles.c` / `libs/fvwmrect.c` + `tests/test_monitor_topology.c` identity fixtures — overlap and reorder-stable identity

**Files scanned:** ~25 (headers, implementations, tests, Meson, manpage, Phase 2 PATTERNS)
**Pattern extraction date:** 2026-08-30
