# Phase 3: Geometry and Ownership Contracts - Research

**Researched:** 2026-08-30
**Domain:** Deterministic monitor ownership, ICCCM size constraints, root-pixel placement/work-area/struts, and actual-pixel X11 protocol boundaries
**Confidence:** HIGH

<user_constraints>
## User Constraints

No Phase 3 CONTEXT.md exists (discuss-phase has not run). The following inherited locked decisions constrain this phase. Planner must not reopen them.

### Locked Decisions (inherited)

From PROJECT.md / REQUIREMENTS.md / STATE.md / Phase 1–2:

- v0.1 keeps all X11, ICCCM, EWMH, and module geometry in actual pixels. [VERIFIED: .planning/STATE.md:82-82]
- Scale FVWM-owned UI and geometry, not arbitrary client pixels. [VERIFIED: .planning/PROJECT.md:87-87]
- Prefer a coherent FVWM-X contract over legacy config or module compatibility when they conflict. [VERIFIED: .planning/PROJECT.md:89-90]
- Pure policy requires native unit coverage; X-visible behavior requires nested-X integration on native Linux CI. [VERIFIED: .planning/PROJECT.md:91-92]
- Authoritative X state stays single-threaded; no blocking monitor or scale work in the main event loop. [VERIFIED: .planning/PROJECT.md:83-86]
- Phase 1 identity is RandR Monitor name/atom; output XIDs are membership facts, not identity; monitors compare by stable identity so enumeration/output-order noise is not a topology change. [VERIFIED: .planning/phases/01-topology-snapshot-and-live-x-harness/01-CONTEXT.md:19-32]
- Phase 2 scale domain is exact quarter steps 1x–4x; conversions round nearest with half ties away from zero; overflow fails explicitly; never feed a rounded result back into the next conversion. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:21-36]
- Status v4 and module packets remain actual-pixel interfaces without scale metadata. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-04-SUMMARY.md:38-38]
- Geometry and ownership contracts precede UI resources; X-visible transitions follow destination UI metrics. [VERIFIED: .planning/STATE.md:84-84]
- Automatic DPI thresholds, ownership hysteresis, and toolkit transition feedback require empirical calibration during their owning phases. [VERIFIED: .planning/STATE.md:132-132]

### the agent's Discretion

All Phase 3 policy details not locked above, including:

- Ownership reference geometry (frame vs client vs top-left)
- Overlap metric, incumbent rule, and hysteresis constant
- Tie-break identity order among equal overlaps
- Gap (zero-overlap) destination rule
- Whether `constrain_size` becomes a thin wrapper over a new pure ICCCM unit
- How far to rewrite live placement/work-area/strut/MoveToScreen call sites versus extracting policy first
- Diagnostic/inspection surface for ownership (if any) without expanding Status or module packets

### Deferred Ideas (OUT OF SCOPE)

- GEO-02 logical-size preservation across scale transitions — Phase 5. [VERIFIED: .planning/REQUIREMENTS.md:109-109]
- GEO-04 drag across scales / pointer-anchor / ownership oscillation during grab — Phase 5. [VERIFIED: .planning/REQUIREMENTS.md:111-111]
- GEO-06 maximize, fullscreen, unmaximize, restore, monitor-transition geometry — Phase 5. [VERIFIED: .planning/REQUIREMENTS.md:112-112]
- WM-owned UI metrics, fonts, cursors, caches — Phase 4. [VERIFIED: .planning/ROADMAP.md:117-128]
- Runtime scale reload and hotplug window recovery — Phase 6. [VERIFIED: .planning/ROADMAP.md:146-156]
- Module scale metadata — Phase 7. [VERIFIED: .planning/ROADMAP.md:159-169]
- Nested-X layout matrix (negative/overlapping/cloned live-X) — Phase 8 TEST-02. [VERIFIED: .planning/REQUIREMENTS.md:117-117]
- `_NET_WM_STRUT_PARTIAL` (currently unimplemented) — pre-existing gap, not a Phase 3 requirement. [VERIFIED: fvwm/ewmh.c:35-35]
- Arbitrary client-content scaling; RandR output transforms as scale; EWMH/ICCCM/strut/module geometry in logical units. [VERIFIED: .planning/REQUIREMENTS.md:69-77]
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| TOP-02 | Deterministic monitor identity and ownership for negative origins, gaps, exact clones, partial overlaps, rotations, and output reordering | Replace `FindScreenOfXY` DisplayWidth wrap with literal root-pixel ownership over committed topology rectangles; identity ties use name/atom, not RB/enumeration order. [VERIFIED: libs/FScreen.c:1247-1307; .planning/phases/01-topology-snapshot-and-live-x-harness/01-CONTEXT.md:19-32] |
| GEO-01 | One deterministic owning monitor with incumbent tie-breaking and seam hysteresis | New pure ownership resolver over window rectangle + optional incumbent; `update_fvwm_monitor` currently uses top-left only and has no hysteresis. [VERIFIED: fvwm/update.c:665-715] |
| GEO-03 | Scaled target size obeys ICCCM min, max, base, increment, aspect, fixed-size, and gravity | Extract integer ICCCM constrain from `constrain_size`/`GetWindowSizeHints`; apply after `monitor_scale_apply`, never the reverse. [VERIFIED: fvwm/geometry.c:826-1028; libs/monitor_scale.h:94-98] |
| GEO-05 | Correct initial placement, work areas, struts, and monitor-targeted movement in root-pixel coordinates | Use monitor `si->{x,y,w,h}` from the committed snapshot; stop treating `DisplayWidth`/`DisplayHeight` as the monitor coordinate space. [VERIFIED: libs/FScreen.c:482-491; fvwm/ewmh.c:1008-1159] |
| GEO-07 | Clients and modules continue receiving EWMH, ICCCM, configure, frame-extents, work-area, and geometry values in actual X11 pixels | Guardrail: do not convert `g.frame`, hints, `_NET_FRAME_EXTENTS`, `_NET_WORKAREA`, or `ConfigWinPacket` fields to logical units. [VERIFIED: fvwm/module_interface.c:308-322; libs/vpacket.h:27-30; fvwm/ewmh.c:1269-1295] |
| TEST-01 | Native tests for scale arithmetic, signed rounding, invalid physical data, snapshot diffs, ownership ties, ICCCM constraints, and 1,000 repeated round trips | Reuse existing scale and topology tests; add ownership-tie and ICCCM-constraint native targets. [VERIFIED: tests/test_monitor_scale.c:269-366; tests/test_monitor_topology.c:800-842] |
</phase_requirements>

## Summary

Phase 3 should be planned as three extracted, allocation-free policy units plus thin single-threaded wiring: (1) monitor ownership over literal root-pixel rectangles, (2) ICCCM size-constraint application in pixel space, and (3) root-pixel work-area/strut/placement helpers that never wrap through `DisplayWidth`. The committed Phase 1 snapshot remains the only monitor-geometry input. Effective scale from Phase 2 is an input to constraint application, not a reason to change any X11 protocol unit.

The current owner path is not a contract. `FindScreenOfXY` folds coordinates through `monitor_get_all_widths()`/`monitor_get_all_heights()` — which are `DisplayWidth`/`DisplayHeight` — before a first-match containment scan. That maps a point on a negative-origin monitor onto the primary framebuffer and makes clones, overlaps, and output reorderings depend on red-black enumeration order. `update_fvwm_monitor` then assigns ownership from the top-left of `g.normal` or `g.max` with no incumbent or hysteresis.

ICCCM constraint logic already exists inside `constrain_size` and `GetWindowSizeHints`, but it is coupled to `FvwmWindow`, styles, borders, `XSync`, and `double` aspect arithmetic. GEO-03 needs a pure integer function that can constrain a scaled target without touching X. GEO-07 is a non-regression: keep publishing actual pixels.

**Primary recommendation:** Extract `monitor_ownership` and `icccm_size_hints` as `libs/` policy with native tests, then replace `FindScreenOfXY`/`update_fvwm_monitor` and wrap `constrain_size` without changing Status v4, module packets, or any protocol unit to logical coordinates.

## Assumption-Delta Decision

| Question | Decision | Planner consequence |
|----------|----------|---------------------|
| Should ownership wrap coordinates into the root framebuffer? | **NO CHANGE TO DISPLAY SIZE; STOP WRAPPING OWNERSHIP.** `monitor_get_all_widths()` remains `DisplayWidth` for virtual-desktop paging. Ownership, placement, work areas, and struts use literal snapshot rectangles. [VERIFIED: libs/FScreen.c:482-491; libs/FScreen.c:1247-1260] | Do not change paging math. Do change `FindScreenOfXY` and work-area intersection to root-pixel monitor rects. |
| Should scale enter module packets or Status? | **NO CHANGE.** Phase 2 already locked actual-pixel packets. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-04-SUMMARY.md:38-38] | GEO-07 is a diff guard, not a schema task. |
| Should this phase preserve logical client size across monitors? | **NO.** That is GEO-02 / Phase 5. Phase 3 only supplies the constrain-after-scale contract. [VERIFIED: .planning/REQUIREMENTS.md:109-109] | Provide `icccm` constrain of a pixel target produced by `monitor_scale_apply`. Do not iterate windows to rescale them. |
| Should `_NET_WORKAREA` become per-monitor? | **NO CHANGE TO THE PUBLISHED ROOT PROPERTY SHAPE.** EWMH defines one geometry per desktop relative to the viewport. Internal intersection must use monitor root pixels. [CITED: https://specifications.freedesktop.org/wm-spec/1.5/ar01s03.html] | Fix `EWMH_GetWorkAreaIntersection` / strut intersection; do not invent a new EWMH property. |

## Project Constraints (from AGENTS.md)

- Target X11/XRandR 1.5+ only; do not introduce a Wayland path. [VERIFIED: AGENTS.md:18-21]
- Keep authoritative X state single-threaded and preserve event-loop ordering. [VERIFIED: AGENTS.md:20-22]
- Perform no blocking monitor or scale work in the main event loop. [VERIFIED: AGENTS.md:24-25]
- Scale FVWM-owned UI and geometry, not arbitrary client pixels, and do not add compositing. [VERIFIED: AGENTS.md:27-30]
- Prefer the coherent FVWM-X contract over incompatible legacy behavior. [VERIFIED: AGENTS.md:29-30]
- Give pure policy native unit coverage and X-visible behavior nested-X integration coverage on native Linux CI. [VERIFIED: AGENTS.md:32-33]
- Route local macOS verification through Podman x86_64 emulation; leave AddressSanitizer to native Linux CI. [VERIFIED: AGENTS.md:35-36]
- Use C11, repository formatting, lower snake-case APIs, explicit status results, close-to-use declarations in new code, and `fvwm_debug(__func__, ...)` for recoverable diagnostics. [VERIFIED: AGENTS.md:154-181; AGENTS.md:196-210]
- Document user-visible geometry/ownership contract changes in `doc/fvwm3_manpage_source.adoc`. [VERIFIED: AGENTS.md:214-220]

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Monitor ownership of a window rectangle | Shared support library (`libs/`) | Core window model (`FvwmWindow.m`) | Pure rectangle/identity policy has no X dependency; core stores the chosen `struct monitor *`. [VERIFIED: fvwm/fvwm.h:977-978; libs/FScreen.c:1247-1307] |
| Point-to-monitor lookup (`FindScreenOfXY`) | Shared support library | Event loop / pointer queries | Same ownership function with a degenerate 1×1 or 0-area point and no incumbent. [VERIFIED: libs/FScreen.c:254-268] |
| ICCCM min/max/base/inc/aspect/gravity constrain | Shared support library (`libs/`) | `fvwm/geometry.c` wrapper | Official size hints are integer fields; current aspect path uses `double` inside a `FvwmWindow`-coupled function. [VERIFIED: fvwm/geometry.c:693-816; fvwm/geometry.c:826-1028] |
| Scale-then-constrain | Shared support library | Core resize/placement | `monitor_scale_apply` already exists; constrain must consume its pixel output, never invert the order. [VERIFIED: libs/monitor_scale.h:94-98] |
| Work area / strut intersection | Core EWMH (`fvwm/ewmh.c`) | Pure rectangle helpers | EWMH properties stay on the X root/client; intersection math should use monitor root-pixel rects. [VERIFIED: fvwm/ewmh.c:983-1159] |
| Initial placement and `MoveToScreen` | Core (`fvwm/placement.c`, `fvwm/move_resize.c`) | Ownership + work-area helpers | Already call `FScreenGetScrRect` / `FindScreenOfXY`; they must consume the new owner contract. [VERIFIED: fvwm/placement.c:1767-1824; fvwm/move_resize.c:2335-2338] |
| Protocol emission (configure, frame extents, work area, module packets) | Core EWMH / module host | — | GEO-07 forbids unit conversion at these seams. [VERIFIED: fvwm/module_interface.c:308-322; fvwm/ewmh.c:1269-1295] |
| Native verification | Meson C assertion targets | Existing live-X suite as non-regression | TEST-01 is native; nested-X layout matrix is Phase 8. [VERIFIED: .planning/REQUIREMENTS.md:53-55; .planning/REQUIREMENTS.md:117-117] |

## Standard Stack

### Core

| Library / facility | Version | Purpose | Why Standard |
|--------------------|---------|---------|--------------|
| C11 | `c_std=c11` | Exact integer ownership, overlap, and ICCCM arithmetic | Repository language standard. [VERIFIED: AGENTS.md:43-46] |
| Committed topology snapshot | in-repo `monitor_topology_monitor` | Root-pixel `x,y,width,height` plus `name_atom`/`name` identity | Phase 1 facts are the only legal ownership input. [VERIFIED: libs/monitor_topology.h:21-34] |
| `monitor_scale_apply` / `monitor_scale_remove` | in-repo Phase 2 | Exact quarter-unit pixel conversion before constrain | Locked conversion contract. [VERIFIED: libs/monitor_scale.h:9-14; libs/monitor_scale.h:94-98] |
| `libs/gravity.c` | in-repo | `gravity_get_offsets` / `gravity_resize` for PWinGravity | Already maps all core gravities. [VERIFIED: libs/gravity.c:69-93] |
| `libs/Rectangles.c` / `libs/fvwmrect.c` | in-repo | Intersection tests for overlap area | Existing helpers; do not invent a third rectangle type. [VERIFIED: libs/Rectangles.c:40-56; libs/fvwmrect.h:16-22] |
| Meson native tests | Meson 1.5.1+ project floor | Deterministic ownership and ICCCM fixtures | Established `-UNDEBUG` assertion pattern. [VERIFIED: tests/meson.build:1-40] |

### Supporting

| Facility | Purpose | When to Use |
|----------|---------|-------------|
| `FScreenGetTopologySnapshot()` | Authoritative monitor rects/identity | Ownership inputs; never a second RandR query. [VERIFIED: libs/FScreen.h:218-228] |
| `FScreenGetScrRect()` | Monitor root-pixel rectangle for placement/MoveToScreen | After ownership is resolved. [VERIFIED: libs/FScreen.c:1395-1414] |
| `fvwm_debug(__func__, ...)` | Recoverable ownership/constraint diagnostics | Invalid hint sets, empty topology, overflow. [VERIFIED: AGENTS.md:196-210] |
| Existing live-X harness | Non-regression of topology/scale suites | Do not add a Phase 3 live-X matrix; TEST-02 is Phase 8. [VERIFIED: tests/meson.build:94-199] |

### Alternatives Considered

| Instead of | Could use | Tradeoff |
|------------|-----------|----------|
| Overlap-area ownership | Keep top-left `FindScreenOfXY` | Top-left fails at seams, clones, and negative origins; GEO-01 requires incumbent + hysteresis. [VERIFIED: fvwm/update.c:670-671] |
| `double` aspect in `constrain_size` | Integer cross-products | Floating aspect conflicts with Phase 2 exact arithmetic and overflows less predictably. [VERIFIED: fvwm/geometry.c:721-727] |
| Rewriting `constrain_size` in place | New pure unit + wrapper | In-place rewrite cannot be tested without `FvwmWindow`/X. [VERIFIED: fvwm/geometry.c:826-850] |
| Expanding Status/module geometry to logical units | Keep pixels | Forbidden by GEO-07 and Phase 2. [VERIFIED: .planning/REQUIREMENTS.md:76-77] |

**Installation:** None. This phase adds no external package and needs no Package Legitimacy Audit. [VERIFIED: libs/meson.build:1-80]

## Package Legitimacy Audit

Not applicable — no packages are installed.

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## Architecture Patterns

### System Architecture Diagram

```text
Committed topology snapshot (root-pixel rects + name/atom)
        |
        v
Window frame rectangle (root pixels) + optional incumbent identity
        |
        v
pure ownership resolver
   /          |           \
containment  overlap     gap (Manhattan)
   |          |           |
incumbent+hysteresis   identity tie-break
   \          |           /
        v
struct monitor * owner  -----> FvwmWindow.m / m_prev
        |
        +--> placement / MoveToScreen / work-area intersection
        |
Scaled target (monitor_scale_apply from exact source)
        |
        v
pure ICCCM constrain (min/max/base/inc/aspect/fixed/gravity)
        |
        v
pixel width/height + gravity-adjusted origin
        |
        +--> constrain_size wrapper (borders/styles/X hints only)
        |
        v
X11 seams stay pixels: ConfigureNotify, _NET_FRAME_EXTENTS,
_NET_WORKAREA, WM_NORMAL_HINTS application, ConfigWinPacket
```

### Recommended Project Structure

```text
libs/
├── monitor_ownership.h     # rectangle owner + reason [ASSUMED]
├── monitor_ownership.c     # overlap, incumbent, hysteresis, ties, gaps [ASSUMED]
├── icccm_size_hints.h      # hint struct + constrain status [ASSUMED]
└── icccm_size_hints.c      # min/max/base/inc/aspect/fixed/gravity [ASSUMED]
fvwm/
├── update.c                # update_fvwm_monitor calls ownership resolver
├── geometry.c              # constrain_size wraps icccm + borders/styles
├── ewmh.c                  # work-area/strut intersection in monitor root pixels
├── placement.c             # consume owner + monitor rect, no DisplayWidth wrap
└── move_resize.c           # MoveToScreen uses monitor root-pixel rect
tests/
├── test_monitor_ownership.c
└── test_icccm_size_hints.c
```

Names are recommendations following `test_<unit>.c` and `libs/` lower-snake-case. [VERIFIED: AGENTS.md:152-172]

### Pattern 1: Literal Root-Pixel Ownership

**What:** Resolve owner from committed monitor rectangles in root coordinates. Do not modulo by `DisplayWidth`/`DisplayHeight`.

Current wrap, quoted verbatim. [VERIFIED: libs/FScreen.c:1247-1260]

```c
/* DATA_K8R2N4WP_START */
struct monitor *
FindScreenOfXY(int x, int y)
{
	struct monitor *m, *m_min = NULL;
	int64_t		dmin	    = INT64_MAX;
	int		all_widths  = monitor_get_all_widths();
	int		all_heights = monitor_get_all_heights();

	x %= all_widths;
	y %= all_heights;
	if (x < 0)
		x += all_widths;
	if (y < 0)
		y += all_heights;
/* DATA_K8R2N4WP_END */
```

Those extents are the X root framebuffer, not the bounding box of RandR monitors. [VERIFIED: libs/FScreen.c:482-491]

```c
/* DATA_Z1D6P9VW_START */
int
monitor_get_all_widths(void)
{
	return (DisplayWidth(disp, DefaultScreen(disp)));
}
/* DATA_Z1D6P9VW_END */
```

A window at `(-100, 100)` on a monitor whose snapshot `x` is negative currently wraps onto the primary framebuffer. Rotations are already baked into snapshot `width`/`height`; output reorderings must not change owner when identity and geometry are unchanged. [VERIFIED: libs/monitor_topology.h:21-34]

### Pattern 2: Overlap, Incumbent, Hysteresis, Identity Ties

**What:** Score each enabled non-global monitor by integer overlap area with the window's frame rectangle. Keep the incumbent when it still overlaps and the challenger's extra overlap is at most `MONITOR_OWNERSHIP_HYSTERESIS_PX` along the shared edge. Otherwise pick unique maximum overlap. Remaining ties use Phase 1 identity: `name_atom` then `strcmp(name)`. Gaps (all overlaps 0) use existing Manhattan `fscreen_monitor_distance_to_point`, with the same identity tie-break. Clones (identical rects) never use RB insertion order.

Current assignment has no incumbent policy. [VERIFIED: fvwm/update.c:665-715]

```c
/* DATA_Q3M7T1ZX_START */
Bool update_fvwm_monitor(FvwmWindow *fw)
{
	rectangle g;
	struct monitor *mnew;

	g = IS_MAXIMIZED(fw) ? fw->g.max : fw->g.normal;
	mnew = FindScreenOfXY(g.x, g.y);
	/* Avoid unnecessary updates. */
	if (mnew == fw->m && fw->UpdateDesk < 0)
		return False;
/* DATA_Q3M7T1ZX_END */
```

Recommended hysteresis constant: **8 root pixels** of extra overlap required to steal ownership. This is empirical and must be confirmed in discuss-phase. [ASSUMED]

Recommended reference rectangle: **frame geometry** `fw->g.frame` when mapped, else `g.normal`/`g.max`. Top-left-only is insufficient for GEO-01 seams. [ASSUMED]

Containment currently returns the first RB match. RB order is disabled-bit, then `y,x`, then `w,h`, then name — not Phase 1 identity. [VERIFIED: libs/FScreen.c:838-868; libs/FScreen.c:1274-1277]

Do not `exit(106)` from pure policy when no monitor matches; return primary, else the identity-least enabled monitor, and let the caller log. [VERIFIED: libs/FScreen.c:1302-1306]

### Pattern 3: Scale Then Constrain, Integer ICCCM Only

**What:** `target_px = monitor_scale_apply(source, factor)`; then `icccm_constrain(hints, target_px, gravity)`. Never constrain in logical units and never scale a previously constrained pixel size.

Phase 2 conversion contract. [VERIFIED: libs/monitor_scale.h:94-98]

```c
/* DATA_C9L3W6XR_START */
/* Always convert from the retained exact source, never a rounded result. */
monitor_scale_conversion_status
monitor_scale_apply(int source, monitor_scale_factor factor, int *converted);
monitor_scale_conversion_status
monitor_scale_remove(int source, monitor_scale_factor factor, int *converted);
/* DATA_C9L3W6XR_END */
```

ICCCM 2.0 WM_NORMAL_HINTS rules the pure function must implement. [CITED: https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.2.3]

```text
DATA_R4D8M2QC_START
The size specifiers refer to the width and height of the client's window excluding borders.
...
width = base_width + (i × width_inc)
height = base_height + (j × height_inc)
If a base size is not provided, the minimum size is to be used in its place and vice versa.
...
If a base size is provided along with the aspect ratio fields, the base size should be subtracted from the window size prior to checking that the aspect ratio falls in range. If a base size is not provided, nothing should be subtracted from the window size. (The minimum size is not to be used in place of the base size for this purpose.)
DATA_R4D8M2QC_END
```

Existing GetWindowSizeHints already documents the aspect/base interaction and fills missing min/base. [VERIFIED: fvwm/add_window.c:2875-2877; fvwm/geometry.c:707-713]

```text
DATA_B7Y2H4QL_START
 * ICCCM says that PMinSize is the default if no PBaseSize is given,
 * and vice-versa.
DATA_B7Y2H4QL_END
```

Fixed-size: `PMinSize` and `PMaxSize` present and `min_width == max_width` and `min_height == max_height`. EWMH uses that test for non-resizable windows. [CITED: https://specifications.freedesktop.org/wm-spec/1.5/ar01s05.html]

Gravity: reuse `gravity_resize`; default missing `PWinGravity` to `NorthWestGravity` as current code does. [VERIFIED: fvwm/add_window.c:2930-2933; libs/gravity.c:69-93]

```c
/* DATA_U5H1B7MF_START */
	static struct _gravity_offset gravity_offsets[11] = {
		{  0,  0 },     /* ForgetGravity */
		{ -1, -1 },     /* NorthWestGravity */
		{  0, -1 },     /* NorthGravity */
		{  1, -1 },     /* NorthEastGravity */
		{ -1,  0 },     /* WestGravity */
		{  0,  0 },     /* CenterGravity */
		{  1,  0 },     /* EastGravity */
		{ -1,  1 },     /* SouthWestGravity */
		{  0,  1 },     /* SouthGravity */
		{  1,  1 },     /* SouthEastGravity */
		{  0,  0 },     /* StaticGravity */
	};
/* DATA_U5H1B7MF_END */
```

Aspect must use checked wide integer cross-products, not:

```c
/* DATA_H5B9C2LV_START */
	rmin = (double)fw->hints.min_aspect.x / (double)fw->hints.min_aspect.y;
	rmax = (double)fw->hints.max_aspect.x / (double)fw->hints.max_aspect.y;
/* DATA_H5B9C2LV_END */
```

[VERIFIED: fvwm/geometry.c:721-722]

`constrain_size` remains the X-facing wrapper: borders, style Min/Max WindowSize, `HAS_NEW_WM_NORMAL_HINTS` / `XSync`, `CS_ROUND_UP` interactive edges. The extracted function operates on client size excluding borders. [VERIFIED: fvwm/geometry.c:841-866]

### Pattern 4: Work Areas and Struts in Monitor Root Pixels

EWMH `_NET_WORKAREA` is one `x,y,width,height` per desktop relative to the viewport, in actual pixels. [CITED: https://specifications.freedesktop.org/wm-spec/1.5/ar01s03.html]

```text
DATA_E2V6Y9HB_START
_NET_WORKAREA, x, y, width, height CARDINAL[][4]/32
These geometries are specified relative to the viewport on each desktop
and specify an area that is completely contained within the viewport.
DATA_E2V6Y9HB_END
```

`_NET_WM_STRUT` is `left, right, top, bottom` CARDINAL[4]; `_NET_WM_STRUT_PARTIAL` coordinates are root-window coordinates, not Xinerama-relative. [CITED: https://specifications.freedesktop.org/wm-spec/1.5/ar01s05.html]

```text
DATA_T7K5A1NU_START
_NET_WM_STRUT, left, right, top, bottom, CARDINAL[4]/32
...
Struts MUST be specified in root window coordinates, that is, they are
not relative to the edges of any view port or Xinerama monitor.
DATA_T7K5A1NU_END
```

Current internal work area stores insets relative to the monitor origin, then re-adds `m->si->x` at intersection time, while strut intersection still uses `0..DisplayWidth`. [VERIFIED: fvwm/ewmh.c:1008-1159; fvwm/ewmh.c:1198-1236]

```c
/* DATA_P6W4J8YD_START */
	x = left;
	y = top;

	width =  m->si->w - (left + right);
	height = m->si->h - (top + bottom);
/* ... */
	nx = max(*x, area_x + m->si->x);
	ny = max(*y, area_y + m->si->y);
/* DATA_P6W4J8YD_END */
```

Plan: compute each monitor's work rectangle as `intersect(monitor_rect, root_minus_struts)` in root pixels. Keep publishing `_NET_WORKAREA` as actual pixels (do not convert to logical). Do not implement `_NET_WM_STRUT_PARTIAL` in this phase. [VERIFIED: fvwm/ewmh.c:35-35]

Placement already fetches `screen_g` via `FScreenGetScrRect` and work-area intersection; after the helper is root-pixel-correct, those call sites follow. [VERIFIED: fvwm/placement.c:1767-1824]

`CMD_MoveToScreen` moves then `update_fvwm_monitor`. [VERIFIED: fvwm/move_resize.c:2335-2338]

### Pattern 5: Protocol Boundary Stay Pixels

Module configure packets already send signed frame pixels and raw hint integers. [VERIFIED: libs/vpacket.h:27-46; fvwm/module_interface.c:308-322]

```c
/* DATA_N1F3S7KA_START */
		(unsigned long)(0),			\
		&(*(_fw))->g.frame.x,			\
		(unsigned long)(0),			\
		&(*(_fw))->g.frame.y,			\
		(unsigned long)(0),			\
		&(*(_fw))->g.frame.width,		\
		(unsigned long)(0),			\
		&(*(_fw))->g.frame.height,		\
/* DATA_N1F3S7KA_END */
```

```c
/* DATA_J2P8Q4SD_START */
	signed long    frame_x;
	signed long    frame_y;
	unsigned long  frame_width;
	unsigned long  frame_height;
/* DATA_J2P8Q4SD_END */
```

`_NET_FRAME_EXTENTS` is border widths in pixels. [VERIFIED: fvwm/ewmh.c:1269-1295]

Planner verification: `git diff` of `fvwm/status_json.c`, `libs/Module.h`, `libs/vpacket.h`, and `CONFIGARGS` must show no new logical-unit fields.

### Anti-Patterns to Avoid

- **Modulo ownership through `DisplayWidth`:** breaks negative origins and clones. [VERIFIED: libs/FScreen.c:1255-1260]
- **First RB match on overlaps:** output reorder / coordinate sort becomes policy. [VERIFIED: libs/FScreen.c:838-868]
- **Scaling protocol outputs:** violates GEO-07. [VERIFIED: .planning/REQUIREMENTS.md:76-77]
- **Using min size as aspect base:** ICCCM forbids it. [CITED: https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.2.3]
- **`double` aspect / `i * width` in `int`:** overflow and drift. [VERIFIED: fvwm/geometry.c:721-727]
- **Rescaling mapped windows in this phase:** Phase 5. [VERIFIED: .planning/REQUIREMENTS.md:109-109]
- **Changing virtual-desktop page math** that legitimately uses `monitor_get_all_widths()` as the root page size. [VERIFIED: libs/FScreen.c:482-486]
- **`exit(106)` from tests or policy:** ownership must be total. [VERIFIED: libs/FScreen.c:1302-1306]

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Gravity offsets | A second gravity table | `gravity_get_offsets` / `gravity_resize` | Already complete for Forget..Static. [VERIFIED: libs/gravity.c:69-93] |
| Rectangle intersection | Ad-hoc min/max | `frect_get_intersection` or `fvwmrect_do_rectangles_intersect` | Existing helpers; watch signed overflow on area. [VERIFIED: libs/Rectangles.c:40-56] |
| Scale conversion | Local `* scale / 4` | `monitor_scale_apply` | Locked rounding/overflow. [VERIFIED: libs/monitor_scale.h:94-98] |
| Monitor discovery | `XRRGetMonitors` | `FScreenGetTopologySnapshot()` | Phase 1 owns query/publication. [VERIFIED: libs/FScreen.h:218-228] |
| Test harness | pytest / new nested-X runner | Meson C assertions; existing live-X only as non-regression | TEST-01 is native. [VERIFIED: tests/meson.build:1-40] |
| Module geometry protocol | New packet fields | Existing `ConfigWinPacket` pixel fields | GEO-07 / Phase 7 boundary. [VERIFIED: libs/vpacket.h:18-65] |

**Key insight:** The expensive bugs are coordinate-space confusion (`DisplayWidth` vs snapshot rects vs logical scale) and non-deterministic first-match ownership. One owner function and one constrain function, both total and allocation-free, prevent those from leaking into placement, EWMH, and modules.

## Common Pitfalls

### Pitfall 1: Negative origin wrap
**What goes wrong:** Windows on a left-of-primary monitor are owned by the primary.
**Why:** `x %= DisplayWidth` then `if (x < 0) x += DisplayWidth`. [VERIFIED: libs/FScreen.c:1255-1260]
**How to avoid:** Compare the literal point/rect to snapshot `x,y,width,height`.
**Warning signs:** `%` or `monitor_get_all_widths()` inside ownership.

### Pitfall 2: Seam flicker without hysteresis
**What goes wrong:** A window sitting on a shared edge flips owner every ConfigureNotify.
**Why:** Top-left sampling plus first-match containment. [VERIFIED: fvwm/update.c:670-671]
**How to avoid:** Overlap + incumbent + extra-overlap threshold.
**Warning signs:** Tests that only probe the exact shared pixel.

### Pitfall 3: Clone/overlap order dependence
**What goes wrong:** Reordering outputs or equal-geometry monitors changes owner.
**Why:** RB_FOREACH first hit. [VERIFIED: libs/FScreen.c:1274-1277]
**How to avoid:** Identity tie-break even when areas are equal.
**Warning signs:** Tests that insert monitors in only one order.

### Pitfall 4: Aspect uses min as base
**What goes wrong:** Terminals with `base < min` or missing `PBaseSize` get illegal sizes.
**Why:** ICCCM subtracts base only when `PBaseSize` is present; min is not a substitute for aspect. [CITED: https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.2.3]
**How to avoid:** Mirror current GetWindowSizeHints flag clearing when `base > min`, and follow ICCCM for aspect subtraction. [VERIFIED: fvwm/add_window.c:2898-2908]
**Warning signs:** Aspect tests that always pass min as base.

### Pitfall 5: Overflow in overlap area or aspect products
**What goes wrong:** UB or wrapped areas pick the wrong owner / size.
**Why:** `width * height` of two `int` rects can exceed `int64_t` in pathological topology (Phase 1 preserves extreme ints).
**How to avoid:** `__int128` intermediates as in Phase 2 DPI; initialize outputs; return overflow status.
**Warning signs:** `int area = w * h`.

### Pitfall 6: Constrain then scale
**What goes wrong:** Increment/aspect applied in logical space, then rounded, producing illegal pixel sizes and drift.
**Why:** GEO-03 is on the scaled target. [VERIFIED: .planning/REQUIREMENTS.md:39-39]
**How to avoid:** `monitor_scale_apply` first, ICCCM second, from the exact source each time.
**Warning signs:** `monitor_scale_apply` of `hints.min_width`.

### Pitfall 7: Logical units at X seams
**What goes wrong:** Clients/modules interpret frame extents or configure events as scaled CSS-like units.
**Why:** X11 protocol is pixels. [VERIFIED: .planning/REQUIREMENTS.md:76-77]
**How to avoid:** Diff-guard listed protocol files; tests assert packet fields equal `g.frame` pixels.
**Warning signs:** `effective_scale` in `ewmh_ChangeProperty` or `CONFIGARGS`.

### Pitfall 8: Rewriting DisplayWidth paging
**What goes wrong:** Virtual desktop page size changes, breaking MoveToPage and EWMH desktop geometry.
**Why:** `monitor_get_all_widths()` is the root framebuffer size used as the page. [VERIFIED: libs/FScreen.c:482-486]
**How to avoid:** Touch only ownership, work-area intersection, and monitor-targeted geometry.
**Warning signs:** Edits to `virtual.c` page arithmetic.

### Pitfall 9: Implementing Phase 5
**What goes wrong:** Windows are resized on monitor cross before UI metrics and transition coordinator exist.
**How to avoid:** No window iteration that applies scale to `g.normal` in this phase.
**Warning signs:** `monitor_scale_apply` inside `update_fvwm_monitor`.

## Code Examples

### Proposed ownership contract

Behavioral skeleton only; identifiers are planner discretion. [ASSUMED]

```c
typedef struct {
	int x;
	int y;
	int width;
	int height;
	Atom name_atom;
	const char *name;
} monitor_ownership_monitor;

typedef struct {
	Atom name_atom; /* 0 if none */
	int extra_overlap; /* challenger - incumbent, 0 if kept */
} monitor_ownership_result;

monitor_ownership_result
monitor_ownership_resolve(
    const rectangle *window,
    const monitor_ownership_monitor *monitors,
    int monitor_count,
    Atom incumbent_atom,
    int hysteresis_px);
```

Inputs are committed snapshot fields quoted here. [VERIFIED: libs/monitor_topology.h:21-34]

```text
DATA_A4S9K2GD_START
	Atom	  name_atom;
	char	 *name;
	int	  x;
	int	  y;
	int	  width;
	int	  height;
DATA_A4S9K2GD_END
```

### Proposed ICCCM constrain contract

[ASSUMED]

```c
typedef struct {
	unsigned long flags; /* PMinSize | PMaxSize | PResizeInc | PAspect | PBaseSize | PWinGravity */
	int min_width, min_height;
	int max_width, max_height;
	int base_width, base_height;
	int width_inc, height_inc;
	int min_aspect_x, min_aspect_y;
	int max_aspect_x, max_aspect_y;
	int win_gravity;
} icccm_size_hints;

typedef enum {
	ICCCM_CONSTRAIN_OK,
	ICCCM_CONSTRAIN_OVERFLOW,
	ICCCM_CONSTRAIN_INVALID_ARGUMENT
} icccm_constrain_status;

icccm_constrain_status
icccm_constrain_size(const icccm_size_hints *hints,
    int *width, int *height, rectangle *frame /* optional gravity adjust */);
```

Strut record already in core. [VERIFIED: fvwm/fvwm.h:180-186]

```c
/* DATA_W8N6E3TY_START */
typedef struct
{
	int left;
	int right;
	int top;
	int bottom;
} ewmh_strut;
/* DATA_W8N6E3TY_END */
```

### Existing 1,000-iteration scale test to keep

[VERIFIED: tests/test_monitor_scale.c:353-366]

```c
/* DATA_M3C7F5RX_START */
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
/* DATA_M3C7F5RX_END */
```

## Likely Files and Exact Existing Integration Symbols

| File | Existing symbols | Planned role |
|------|------------------|--------------|
| `libs/monitor_topology.h` | `monitor_topology_monitor` `x,y,width,height,name_atom,name` | Read-only ownership inputs. [VERIFIED: libs/monitor_topology.h:21-34] |
| `libs/FScreen.c` | `FindScreenOfXY`, `fscreen_monitor_distance_to_point`, `monitor_get_all_widths` | Replace wrap/first-match; keep paging helpers unchanged. [VERIFIED: libs/FScreen.c:871-890; libs/FScreen.c:1247-1307] |
| `fvwm/update.c` | `update_fvwm_monitor` | Pass frame/normal rect + `fw->m` incumbent into resolver. [VERIFIED: fvwm/update.c:665-715] |
| `fvwm/fvwm.h` | `Fw->m`, `fw->m_prev`, `fw->hints`, `fw->g` | Store owner; do not add logical geometry fields. [VERIFIED: fvwm/fvwm.h:833-833; fvwm/fvwm.h:858-858; fvwm/fvwm.h:977-978] |
| `fvwm/geometry.c` | `constrain_size`, `gravity_constrain_size` | Wrap pure ICCCM; keep border/style/XSync behavior. [VERIFIED: fvwm/geometry.c:826-1060] |
| `fvwm/add_window.c` | `GetWindowSizeHintsWithCheck` | Keep sanitizing untrusted client hints; feed sanitized integers to pure constrain. [VERIFIED: fvwm/add_window.c:2742-2933] |
| `fvwm/ewmh.c` | `ewmh_ComputeAndSetWorkArea`, `EWMH_GetWorkAreaIntersection`, `ewmh_GetStrutIntersection`, `EWMH_SetFrameStrut` | Root-pixel intersection; pixel frame extents. [VERIFIED: fvwm/ewmh.c:1008-1295] |
| `fvwm/placement.c` | `FScreenGetScrRect`, `EWMH_GetWorkAreaIntersection` | Consume corrected monitor rects. [VERIFIED: fvwm/placement.c:1767-1824] |
| `fvwm/move_resize.c` | `CMD_MoveToScreen` | Ownership after move. [VERIFIED: fvwm/move_resize.c:2335-2338] |
| `fvwm/module_interface.c` | `CONFIGARGS` | Unchanged pixel fields. [VERIFIED: fvwm/module_interface.c:308-322] |
| `libs/meson.build` | `libfvwm3_sources` | Add ownership + icccm units once. [VERIFIED: libs/meson.build:1-62] |
| `tests/meson.build` | existing assertion targets | Register `monitor-ownership` and `icccm-size-hints`. [VERIFIED: tests/meson.build:1-40] |
| `doc/fvwm3_manpage_source.adoc` | geometry / StartsOnScreen / MoveToScreen | Document root-pixel ownership and protocol units. |

## State of the Art

| Old/current approach | Phase 3 approach | Impact |
|----------------------|------------------|--------|
| Point-in-rect after DisplayWidth wrap, first RB hit | Literal overlap + incumbent + identity | Deterministic TOP-02/GEO-01 |
| Top-left of normal/max | Frame overlap with hysteresis | Seams stop oscillating |
| `constrain_size` + `double` aspect inside `FvwmWindow` | Pure integer ICCCM then wrapper | Testable GEO-03 |
| Work-area insets vs DisplayWidth struts | Monitor root-pixel intersection | GEO-05 |
| Pixel module/EWMH fields | Unchanged pixels | GEO-07 |

**Deprecated/outdated for this phase:** `FindScreenOfXY` wrap, first-match overlap, floating aspect as the source of truth, logical protocol units.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Ownership uses frame rectangle, not client or top-left. | Pattern 2 | Seam tests and `update_fvwm_monitor` wiring differ. |
| A2 | Hysteresis is 8 extra root pixels of overlap to steal. | Pattern 2 | Flicker vs sticky-wrong-monitor; STATE.md requires calibration. |
| A3 | Gap destinations use Manhattan distance then identity, not primary-always. | Pattern 2 | Windows in dead zones jump to primary instead of nearest. |
| A4 | File names `monitor_ownership` / `icccm_size_hints`. | Structure | Planner may choose different identifiers. |
| A5 | `_NET_WM_STRUT_PARTIAL` stays unimplemented. | Pattern 4 | Panels that only set PARTIAL remain invisible to work area. |
| A6 | No new PrintInfo subject for ownership in this phase. | Discretion | TOP-05 observability is Phase 6. |

## Open Questions

1. **What hysteresis constant and reference rectangle should discuss-phase lock?**
   - What we know: GEO-01 requires incumbent + seam hysteresis; current code has neither. [VERIFIED: fvwm/update.c:665-671]
   - What's unclear: 8px vs percent-of-window; frame vs client vs pointer.
   - Recommendation: lock frame rectangle + 8px extra overlap before planning.

2. **Should `FindScreenOfXY` remain a public point API wrapping ownership, or die?**
   - What we know: pointer, placement, and modules call it. [VERIFIED: libs/FScreen.c:254-268]
   - Recommendation: keep the symbol as a point query with no incumbent and no wrap.

3. **How much live `constrain_size` behavior must the pure function reproduce?**
   - What we know: wrapper also applies style min/max, borders, CS_ROUND_UP, max defect. [VERIFIED: fvwm/geometry.c:841-1024]
   - Recommendation: pure function = ICCCM client size only; wrapper keeps the rest.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|-------------|-----------|---------|----------|
| Python 3 | Linux router | ✓ | 3.14.7 | — [VERIFIED: local `python3 --version`, 2026-08-30] |
| Podman | macOS native-Linux route | ✓ | 6.1.0 | Existing wrapper. [VERIFIED: local `podman --version`, 2026-08-30] |
| `tests/run_in_linux.py --self-test` | Native tests | ✓ | exit 0 | — [VERIFIED: local run, 2026-08-30] |
| Meson | Build/test | ✗ on host | — | Podman Linux route. [VERIFIED: local `command -v meson`, 2026-08-30] |
| Ninja | Meson backend | ✗ on host | — | Podman Linux route. [VERIFIED: local `command -v ninja`, 2026-08-30] |
| Apple clang | Local syntax | ✓ | 21.0.0 | Production remains Linux GCC/Clang. [VERIFIED: local `cc --version`, 2026-08-30] |
| clang-format | Formatting | ✓ | 21.0.0 via xcrun | Linux image lacks it. [VERIFIED: local `xcrun clang-format --version`, 2026-08-30] |

**Missing dependencies with no fallback:** none.

**Missing dependencies with fallback:** host Meson and Ninja — use `python3 tests/run_in_linux.py`.

Step 2.6 otherwise skipped for new native libraries: none required.

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Meson test runner; plain C assertion executables. [VERIFIED: tests/meson.build:1-40] |
| Config file | `tests/meson.build` |
| Quick run command | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-ownership icccm-size-hints monitor-scale monitor-topology --print-errorlogs` [ASSUMED] |
| Full native suite | `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs` |
| Live-X (non-regression only) | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs` |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| TOP-02 | Negative origin, gap, clone, partial overlap, rotation (swapped w/h), output reorder all yield the same owner | unit | `meson test -C builddir monitor-ownership` | ❌ Wave 0 |
| GEO-01 | Incumbent kept on seam; challenger wins only beyond hysteresis; identity ties | unit | `meson test -C builddir monitor-ownership` | ❌ Wave 0 |
| GEO-03 | min/max/base/inc/aspect/fixed/gravity on a scaled target; overflow initialized | unit | `meson test -C builddir icccm-size-hints` | ❌ Wave 0 |
| GEO-05 | Work-area/strut intersection and monitor-rect placement math in root pixels including negative origins | unit | `meson test -C builddir monitor-ownership` (helpers) or colocated work-area tests | ❌ Wave 0 |
| GEO-07 | No logical conversion in packet/EWMH sources | other | `git diff` of `status_json.c`, `Module.h`, `vpacket.h`, `CONFIGARGS`, `EWMH_SetFrameStrut` | ✅ files exist |
| TEST-01 scale/rounding/invalid DPI | Existing Phase 2 tests | unit | `meson test -C builddir monitor-scale` | ✅ `tests/test_monitor_scale.c` |
| TEST-01 snapshot diffs | Existing Phase 1 tests | unit | `meson test -C builddir monitor-topology` | ✅ `tests/test_monitor_topology.c` |
| TEST-01 1,000 round trips | Existing loop from original source | unit | `meson test -C builddir monitor-scale` | ✅ [VERIFIED: tests/test_monitor_scale.c:353-366] |
| TEST-01 ownership ties + ICCCM | New | unit | see TOP-02/GEO-03 | ❌ Wave 0 |

### Sampling Rate

- **Per task commit:** focused named C target via `tests/run_in_linux.py`
- **Per wave merge:** full wrapped native suite
- **Phase gate:** full native suite green; existing live-X suite as non-regression; no new live-X cases required for TEST-01

### Wave 0 Gaps

- [ ] `tests/test_monitor_ownership.c` — TOP-02 / GEO-01 / GEO-05 rectangle fixtures
- [ ] `tests/test_icccm_size_hints.c` — GEO-03 matrix including scaled targets
- [ ] `monitor-ownership` and `icccm-size-hints` registrations in `tests/meson.build`
- [ ] Optional: extract work-area intersection tests if not folded into ownership helpers

Existing coverage that must stay green: `monitor-scale`, `monitor-scale-config`, `monitor-topology`, `fscreen-topology-adapter`.

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No new auth surface. |
| V3 Session Management | no | No session tokens. |
| V4 Access Control | no | Existing local WM authority unchanged. |
| V5 Input Validation | yes | Sanitize untrusted `XSizeHints` and `_NET_WM_STRUT` CARD32s; range-check before arithmetic. [VERIFIED: fvwm/add_window.c:2742-2808] |
| V6 Cryptography | no | No crypto. |

### Known Threat Patterns for C/X11 Geometry

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Malicious WM_NORMAL_HINTS (0 inc, negative min, inverted aspect) | Tampering / DoS | Keep GetWindowSizeHints sanitizer; pure constrain rejects invalid and initializes outputs. [VERIFIED: fvwm/add_window.c:2777-2808] |
| Huge strut/overlap values overflow area | DoS | Checked wide intermediates; never `int * int` for area. |
| Crafted monitor name in diagnostics | Repudiation | Encode names if any new log line is added (cJSON pattern). |
| Logical-unit confusion as protocol injection | Tampering | GEO-07: protocol fields remain pixels. |

Client properties are untrusted X input. Do not treat `hints.width_inc == 0` as a divisor; current code forces inc to 1. [VERIFIED: fvwm/add_window.c:2777-2814]

## Sources

### Primary (HIGH confidence)

- `libs/FScreen.c` `FindScreenOfXY`, `monitor_get_all_widths`, `fscreen_monitor_distance_to_point` — current ownership. [VERIFIED: libs/FScreen.c:482-491; libs/FScreen.c:871-890; libs/FScreen.c:1247-1307]
- `fvwm/update.c` `update_fvwm_monitor` — incumbent absence. [VERIFIED: fvwm/update.c:665-715]
- `fvwm/geometry.c` `constrain_size` / `_cs_handle_aspect_ratio`. [VERIFIED: fvwm/geometry.c:693-1028]
- `fvwm/add_window.c` `GetWindowSizeHintsWithCheck`. [VERIFIED: fvwm/add_window.c:2742-2933]
- `fvwm/ewmh.c` work area, struts, frame extents. [VERIFIED: fvwm/ewmh.c:983-1295]
- `libs/monitor_scale.h`, `tests/test_monitor_scale.c`, `tests/test_monitor_topology.c`. [VERIFIED: libs/monitor_scale.h:94-98; tests/test_monitor_scale.c:353-366; tests/test_monitor_topology.c:800-842]
- `libs/vpacket.h`, `fvwm/module_interface.c` `CONFIGARGS`. [VERIFIED: libs/vpacket.h:18-46; fvwm/module_interface.c:308-322]
- Phase 1–2 CONTEXT/SUMMARY and REQUIREMENTS/ROADMAP/STATE.

### Secondary (MEDIUM — official specs via WebFetch; seam classifies webfetch LOW even with --verified)

Treat as [CITED], not [VERIFIED]:

- ICCCM 2.0 §4.1.2.3 WM_NORMAL_HINTS. [CITED: https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.2.3]
- EWMH 1.5 `_NET_WORKAREA`, `_NET_WM_STRUT`, `_NET_WM_STRUT_PARTIAL`, `_NET_FRAME_EXTENTS`. [CITED: https://specifications.freedesktop.org/wm-spec/1.5/ar01s03.html; https://specifications.freedesktop.org/wm-spec/1.5/ar01s05.html]

### Tertiary (LOW)

- Graph store exists (`stale: false`, ~6h old) but ownership/constrain queries returned no actionable cited paths. Continue from opened source.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependencies; files opened this session.
- Architecture: HIGH — current owner, constrain, work-area, and packet seams were read with line ranges.
- Ownership/hysteresis constants: MEDIUM — algorithm shape is forced by GEO-01; 8px/frame rect need discuss-phase lock.
- ICCCM numeric rules: MEDIUM — official spec cited; existing sanitizer verified in-tree.
- Pitfalls: HIGH — wrap bug and top-left assignment are quoted from current source.

**Research date:** 2026-08-30
**Valid until:** 2026-09-29 (stable in-repo C/X11 geometry; recheck if Phase 2 scale APIs or Status/module packets change)

**Graph note:** `.gsd-graph` last_build 2026-08-30T17:35:25Z, `stale: false`, age ~6h — semantic relationships treated as approximate only; all discrete claims above come from opened source files.
