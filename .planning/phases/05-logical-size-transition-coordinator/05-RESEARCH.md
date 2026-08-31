# Phase 5: Logical-Size Transition Coordinator - Research

**Researched:** 2026-08-31
**Domain:** Drift-free logical client-size preservation, pointer-grab scale crossings, and maximize/fullscreen/restore geometry at destination scale
**Confidence:** HIGH for in-repo seams and conversion/constrain contracts; MEDIUM for mid-drag ownership freeze details that are product policy (no CONTEXT.md)

<user_constraints>
## User Constraints (from CONTEXT.md)

No Phase 5 CONTEXT.md exists (user chose continue-without-context). The following inherited locked decisions constrain this phase. Planner must not reopen them.

### Locked Decisions (inherited)

From PROJECT.md / REQUIREMENTS.md / STATE.md / Phase 1–4:

- v0.1 keeps all X11, ICCCM, EWMH, and module geometry in actual pixels. [VERIFIED: .planning/STATE.md:101]
- Scale FVWM-owned UI and geometry, not arbitrary client pixels. [VERIFIED: .planning/PROJECT.md:93]
- Prefer a coherent FVWM-X contract over legacy config or module compatibility when they conflict. [VERIFIED: .planning/PROJECT.md:95]
- Pure policy requires native unit coverage; X-visible behavior requires nested-X integration on native Linux CI. [VERIFIED: .planning/PROJECT.md:96]
- Authoritative X state stays single-threaded; no blocking monitor or scale work in the main event loop. [VERIFIED: .planning/PROJECT.md:87-89]
- Do not add compositing; integrate with external compositors. [VERIFIED: .planning/PROJECT.md:94]
- Per-monitor mutation of global Xft DPI, XSETTINGS, or toolkit environment is out of scope. [VERIFIED: .planning/REQUIREMENTS.md:76]
- Phase 2 scale domain is exact quarter steps 1x–4x (`MONITOR_SCALE_QUARTERS_MIN` 4 through `MONITOR_SCALE_QUARTERS_MAX` 16); conversions round nearest with half ties away from zero; overflow fails explicitly; never feed a rounded result back into the next conversion. [VERIFIED: libs/monitor_scale.h:9-10; libs/monitor_scale.h:94-98]
- Status v4 and module packets remain actual-pixel interfaces without scale metadata. [VERIFIED: .planning/STATE.md:140]
- Geometry and ownership contracts precede UI resources; X-visible transitions follow destination UI metrics. [VERIFIED: .planning/STATE.md:103]
- Preserve logical window size across scale changes. [VERIFIED: .planning/PROJECT.md:109]
- Pixel-sensitive application opt-out is deferred (COMPAT-01). [VERIFIED: .planning/REQUIREMENTS.md:63]
- Automatic DPI thresholds, ownership hysteresis, and toolkit transition feedback require empirical calibration during their owning phases. [VERIFIED: .planning/STATE.md:210]

### the agent's Discretion

All Phase 5 policy details not locked above, including:

- File and API boundary for the pure transition planner versus core wiring
- Where the canonical logical client size is stored on `FvwmWindow`
- Whether interactive-move ownership uses the pointer (1×1) rather than the post-resize frame
- How grab-offset is converted when chrome thickness changes
- Exact eligible/ineligible window matrix beyond ICCCM fixed-size
- Whether maximize/fullscreen crossing recomputes derived geometry immediately or on ButtonRelease

**Research recommendation for each discretion item is prescriptive below.** Planner should follow those recommendations unless discuss-phase later overrides them.

### Deferred Ideas (OUT OF SCOPE)

- SCALE-06 runtime scale reload and affected-window updates — Phase 6. [VERIFIED: .planning/REQUIREMENTS.md:25]
- TOP-04 / TOP-05 hotplug recovery and inspection/simulation — Phase 6. [VERIFIED: .planning/REQUIREMENTS.md:13-14]
- Module scale metadata and module-owned UI — Phase 7. [VERIFIED: .planning/REQUIREMENTS.md:47-50]
- Nested-X layout matrix, client-toolkit, compositor variants — Phase 8 (TEST-02/03/04). [VERIFIED: .planning/REQUIREMENTS.md:54-56]
- COMPAT-01 pixel-preservation client opt-out. [VERIFIED: .planning/REQUIREMENTS.md:63]
- Arbitrary client-content scaling; RandR output transforms as scale; EWMH/ICCCM/strut/module geometry in logical units. [VERIFIED: .planning/REQUIREMENTS.md:69-77]
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| GEO-02 | Eligible window preserves logical client size across scale transitions and repeated fractional round trips without drift | Store canonical logical **client** width/height as the exact source; `monitor_scale_apply` at dest; never `remove(apply(...))`. 1,000-trip pattern already exists for apply-from-source. [VERIFIED: libs/monitor_scale.h:94-98; tests/test_monitor_scale.c:353-365] |
| GEO-04 | Drag across scales without losing the grabbed pointer anchor or causing ownership oscillation | `move_loop` already `GrabEm`s and calls `update_fvwm_monitor` on the **frame**; a dest-scale resize of that frame can flip overlap. Resolve drag ownership from the pointer, freeze dest, convert grab offset, do not Ungrab/re-Grab. [VERIFIED: fvwm/move_resize.c:2799-2802; fvwm/move_resize.c:2898; fvwm/update.c:675-689] |
| GEO-06 | Correct maximize, fullscreen, unmaximize, restore, and monitor-transition geometry at every supported scale | Maximized/fullscreen are **derived** states: recompute from dest work area / dest monitor pixels; restore through stored logical normal + dest chrome. Do not scale `g.max`. [VERIFIED: fvwm/move_resize.c:5123-5198; fvwm/move_resize.c:5202-5227; fvwm/ewmh.c:2024-2083] |
</phase_requirements>

## Summary

Phase 5 should be planned as one allocation-free `libs/` transition planner plus thin single-threaded wiring at the existing ownership, move, maximize, and fullscreen seams. Phase 2 already supplies exact quarter conversion and forbids feeding a rounded result back as the next source. Phase 3 already supplies rectangle ownership with 8 px hysteresis and integer ICCCM constrain. Phase 4 already scales destination chrome and, on map, rebinds fonts after `update_fvwm_monitor` — but **does not** convert `g.normal` / `g.frame`. That leftover is this phase.

Today a window that crosses monitors keeps its root-pixel client size. `update_fvwm_monitor` assigns `fw->m` and broadcasts; it never resizes. `move_loop` keeps `offset = orig_drag - pointer` and re-resolves ownership from the **frame** while the pointer is grabbed on the root. Maximize fills the current work-area rectangle in pixels; fullscreen issues `ResizeMoveMaximize` of the monitor rectangle; unmaximize restores `g.normal` or `fullscreen.g.max` without dest-scale conversion.

**Primary recommendation:** Add `libs/geometry_transition.[ch]` that maps `(logical client size, src/dest scale, dest chrome, ICCCM hints, kind)` to dest client/frame pixels; store the logical client size on `FvwmWindow`; apply it from `update_fvwm_monitor` after dest chrome rebind; during grab resolve ownership from the pointer and freeze the dest; recompute maximize/fullscreen from dest work area / dest monitor rather than scaling derived rectangles.

## Assumption-Delta Decision

| Question | Decision | Planner consequence |
|----------|----------|---------------------|
| Should dest pixels be `apply(remove(current_px, src), dest)`? | **NO.** That reconstructs from a rounded result and drifts. Convert from the stored logical source every time. [VERIFIED: libs/monitor_scale.h:94-98] | Store logical client size; update it only on user/client resize, not on WM scale transitions. |
| Should this phase change Status/module/EWMH units to logical? | **NO.** GEO-07 remains in force. Dest geometry is still actual X11 pixels. [VERIFIED: .planning/REQUIREMENTS.md:76-77] | After transition, `_NET_FRAME_EXTENTS` equals dest chrome pixels. No packet schema work. |
| Should maximized/fullscreen windows preserve logical size of the derived rectangle? | **NO.** Recompute from dest work area (maximize) or dest monitor (fullscreen). Restore uses stored logical **normal** client size. [CITED: specifications.freedesktop.org/wm-spec/1.5/ar01s05.html] | `geometry_transition_kind` distinguishes NORMAL / MAXIMIZED / FULLSCREEN / RESTORE. |
| Should mid-drag ownership keep using the frame rectangle? | **NO.** Use a 1×1 pointer rectangle with incumbent hysteresis; freeze dest until the pointer's extra overlap exceeds hysteresis again. [VERIFIED: fvwm/move_resize.c:2898; libs/monitor_ownership.h:7] | `move_loop` must not call today's frame-based `update_fvwm_monitor` as the scale authority. |
| Should the coordinator Ungrab/re-Grab the pointer when size changes? | **NO.** Keep the existing `GrabEm` root grab; `change_grab_cursor` already updates cursor on a live grab. [VERIFIED: fvwm/misc.c:136-158] | Convert `offset`; set frame origin so `pointer + offset` stays the grab point. |
| Should TEST-02/03 live-X toolkit matrix land here? | **NO.** Native policy + existing live-X non-regression. Client/toolkit matrix is Phase 8. [VERIFIED: .planning/REQUIREMENTS.md:54-56] | `tests/test_geometry_transition.c` only. |
| Should COMPAT-01 opt-out ship? | **NO.** Deferred. Fixed-size (`min == max`) already skips client resize. [VERIFIED: .planning/REQUIREMENTS.md:63] | Ineligible windows keep client pixels and still get dest chrome. |

## Project Constraints (from AGENTS.md)

- Target X11/XRandR 1.5+ only; do not introduce a Wayland path. [VERIFIED: AGENTS.md:18-21]
- Keep authoritative X state single-threaded and preserve event-loop ordering. [VERIFIED: AGENTS.md:20-22]
- Perform no blocking monitor or scale work in the main event loop. [VERIFIED: AGENTS.md:24-25]
- Scale FVWM-owned UI and geometry, not arbitrary client pixels, and do not add compositing. [VERIFIED: AGENTS.md:27-30]
- Prefer the coherent FVWM-X contract over incompatible legacy behavior. [VERIFIED: AGENTS.md:29-30]
- Give pure policy native unit coverage and X-visible behavior nested-X integration coverage on native Linux CI. [VERIFIED: AGENTS.md:32-33]
- Route local macOS verification through Podman x86_64 emulation; leave AddressSanitizer to native Linux CI. [VERIFIED: AGENTS.md:35-36]
- Use C11, repository formatting, lower snake-case APIs, explicit status results, close-to-use declarations in new code, and `fvwm_debug(__func__, ...)` for recoverable diagnostics. [VERIFIED: AGENTS.md:154-181; AGENTS.md:196-210]
- Document user-visible geometry/transition contract changes in `doc/fvwm3_manpage_source.adoc`. [VERIFIED: AGENTS.md:214-220]
- Include generated `config.h` first in each C implementation. [VERIFIED: AGENTS.md:377-387]

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Canonical logical client size | Shared support library (`libs/geometry_transition`) | Core `FvwmWindow` storage | Pure apply/constrain/compose has no X dependency; core stores the source integers. |
| Scale-then-constrain-then-chrome | Shared support library | `fvwm/geometry.c` / `constrain_size` | `monitor_scale_apply` then `icccm_constrain_size`; dest frame adds dest `size_borders`. |
| Ownership change → one transition | Core `update_fvwm_monitor` | Transition planner | Today ownership mutates `fw->m` only. This is the single commit point. [VERIFIED: fvwm/update.c:666-742] |
| Interactive drag anchor + dest freeze | Core `move_loop` / `GrabEm` | Ownership + transition planner | Pointer grab already lives here; frame-based re-resolve is the oscillation bug. [VERIFIED: fvwm/move_resize.c:2755-2898] |
| Maximize / unmaximize | Core `maximize_fvwm_window` / `unmaximize_fvwm_window` | Work-area helper + planner | Fill dest work rect; restore logical normal. [VERIFIED: fvwm/move_resize.c:5123-5227] |
| Fullscreen / leave fullscreen | Core `EWMH_fullscreen` + unmaximize | Dest monitor rect | EWMH requires restore of original geometry. [CITED: specifications.freedesktop.org/wm-spec/1.5/ar01s05.html] |
| Dest chrome before frame compose | Core AddWindow/ownership path | `wm_ui_metrics` / SelectDecor / fonts | Phase 4 already rebinds after ownership on map; live crossings must do the same **before** frame size. [VERIFIED: fvwm/add_window.c:2548-2565] |
| Protocol emission | Core EWMH / module host | — | Keep pixels. Call `EWMH_SetFrameStrut` after dest chrome. [VERIFIED: fvwm/ewmh.c:1309-1335] |
| Native verification | Meson C assertion target | Existing live-X as non-regression | GEO-02/04/06 policy is native; nested-X matrix is Phase 8. |

## Standard Stack

### Core

| Library / facility | Version | Purpose | Why Standard |
|--------------------|---------|---------|--------------|
| C11 | `c_std=c11` | Exact integer conversion and constrain | Repository language standard. [VERIFIED: AGENTS.md:43-46] |
| `monitor_scale_apply` / `monitor_scale_remove` | in-repo Phase 2 | Exact quarter-unit conversion from retained source | Locked no-drift contract. [VERIFIED: libs/monitor_scale.h:94-98] |
| `monitor->effective_scale.factor` | in-repo Phase 2 | Dest/src scale quarters | Already on every runtime monitor. [VERIFIED: libs/FScreen.h:104] |
| `icccm_constrain_size` | in-repo Phase 3 | Min/max/base/inc/aspect/fixed/gravity on dest client pixels | Scale then constrain. [VERIFIED: libs/icccm_size_hints.h:8-36] |
| `monitor_ownership_resolve` | in-repo Phase 3 | Dest owner with incumbent hysteresis | Do not invent a second owner. [VERIFIED: libs/monitor_ownership.h:7-39] |
| `gravity_resize` / `gravity_get_offsets` | in-repo | Preserve ICCCM reference point when client+chrome size changes | Already used by constrain/placement. [VERIFIED: libs/gravity.c:69-123] |
| `wm_ui_scale_px` + SelectDecor / font AtScale | in-repo Phase 4 | Dest chrome metrics before frame compose | X-visible transitions follow dest UI metrics. [VERIFIED: libs/wm_ui_metrics.h:24-26] |
| `frame_setup_window` | in-repo | Commit dest frame pixels and synthetic configure | Existing X seam. [VERIFIED: fvwm/frame.h:65-67] |
| `GrabEm` / `change_grab_cursor` | in-repo | Keep the move grab; update cursor | Already nested-grab aware. [VERIFIED: fvwm/misc.c:136-158] |
| Meson native tests | Meson 1.5.1+ project floor | Deterministic transition fixtures | Established `-UNDEBUG` assertion pattern. [VERIFIED: tests/meson.build:1-40] |

### Supporting

| Facility | Purpose | When to Use |
|----------|---------|-------------|
| `get_window_borders` | Dest chrome totals in actual pixels | After dest SelectDecor, before frame compose. [VERIFIED: fvwm/geometry.c:511-537] |
| `constrain_size` | X-facing wrapper (borders, styles, `XSync`, `CS_*`) | Core wiring only; planner tests call `icccm_constrain_size`. [VERIFIED: fvwm/geometry.c:724-755] |
| `EWMH_GetWorkAreaIntersection` | Dest maximize rectangle in root pixels | GEO-06 maximize. [VERIFIED: fvwm/move_resize.c:5410-5414] |
| `FScreenGetScrRect` / `FSCREEN_XYPOS` | Dest fullscreen monitor rectangle | GEO-06 fullscreen. [VERIFIED: fvwm/ewmh.c:2072-2076] |
| `fvwm_debug(__func__, ...)` | Overflow / ineligible / noop diagnostics | Recoverable failures. [VERIFIED: AGENTS.md:196-210] |
| Existing live-X harness | Non-regression of topology/scale suites | Do not add a Phase 5 live-X matrix. [VERIFIED: tests/run_in_linux.py:37-43] |

### Alternatives Considered

| Instead of | Could use | Tradeoff |
|------------|-----------|----------|
| Stored logical client size | Reconstruct via `monitor_scale_remove` on each crossing | Reconstructing from rounded pixels drifts; forbidden by Phase 2 D-04. [VERIFIED: libs/monitor_scale.h:94] |
| One coordinator | Scale opportunistically in every `fw->m` assignment | Domain pitfall: several transition authorities oscillate. [CITED: .planning/research/PITFALLS.md:89-117] |
| Frame ownership during drag | Pointer 1×1 ownership during grab | Frame resize after scale can steal ownership back. [VERIFIED: fvwm/update.c:675-689] |
| Re-`XGrabPointer` on resize | Keep `GrabEm` + convert offset | Second grab risks `GrabInvalidTime`/`AlreadyGrabbed` and can warp if `confine_to` is set. [CITED: https://tronche.com/gui/x/xlib/input/XGrabPointer.html] |
| Scale `g.max` on monitor change | Recompute maximize from dest work area | Derived state is not a logical size. [CITED: specifications.freedesktop.org/wm-spec/1.5/ar01s05.html] |
| New geometry library | Gdk/Qt types | Extra dependency; in-repo `rectangle` / `size_rect` already exist. [VERIFIED: libs/fvwmrect.h:16-49] |

**Installation:** None. This phase adds no external package and needs no Package Legitimacy Audit. [VERIFIED: libs/meson.build:1-84]

## Package Legitimacy Audit

Not applicable — no packages are installed.

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## Architecture Patterns

### System Architecture Diagram

```text
User drag / MoveToScreen / maximize / fullscreen / ownership update
        |
        v
Destination owner (pointer 1x1 during grab; else frame + incumbent hysteresis)
        |
        +-- same monitor and same quarters --> NOOP
        |
        v
Dest chrome rebind (SelectDecor + fonts at dest effective_scale)     [Phase 4]
        |
        v
kind?
  NORMAL      --> dest_client = apply(stored_logical, dest_factor)
  MAXIMIZED   --> dest_frame  = dest work rect (root px); client = frame - dest chrome
  FULLSCREEN  --> dest_frame  = dest monitor rect; no decorations
  RESTORE     --> dest_client = apply(stored_logical, dest_factor)
        |
        v
icccm_constrain_size on dest client (skip for true fullscreen)
        |
        v
gravity_resize origin by (dest_size - src_size) using win_gravity
  during grab: origin = pointer + converted_offset (overrides gravity)
        |
        v
frame_setup_window + EWMH_SetFrameStrut + BroadcastConfig
  (all actual X11 pixels)
        |
        v
Do not re-run ownership on the post-resize frame until pointer/topology moves
```

### Recommended Project Structure

```text
libs/
├── geometry_transition.h   # kind, status, plan() contract
├── geometry_transition.c   # apply, constrain, compose, eligibility
├── monitor_scale.h         # input only — do not reimplement arithmetic
├── icccm_size_hints.h      # input only
├── monitor_ownership.h     # input only
└── wm_ui_metrics.h         # dest chrome is an input, not computed here
fvwm/
├── fvwm.h                  # store logical client size + last-transition tuple
├── update.c                # one commit: owner change -> chrome -> plan -> frame
├── move_resize.c           # drag freeze, offset convert, maximize/restore
├── ewmh.c                  # fullscreen dest monitor; restore original geometry
└── add_window.c            # initialize logical size from first constrained client
tests/
└── test_geometry_transition.c
```

Names follow `test_<unit>.c` and `libs/` lower-snake-case. [VERIFIED: AGENTS.md:152-172]

### Pattern 1: Retained Exact Logical Client Source

**What:** Canonical size is client-area width/height at 1x-equivalent source integers. Every dest size is `monitor_scale_apply(logical, dest_factor)` then constrain. Decorations are dest chrome, not part of the canonical size.

Phase 2 conversion contract, quoted verbatim. [VERIFIED: libs/monitor_scale.h:94-98]

```c
/* DATA_K4M8Q2NX_START */
/* Always convert from the retained exact source, never a rounded result. */
monitor_scale_conversion_status
monitor_scale_apply(int source, monitor_scale_factor factor, int *converted);
monitor_scale_conversion_status
monitor_scale_remove(int source, monitor_scale_factor factor, int *converted);
/* DATA_K4M8Q2NX_END */
```

Quarter bounds, quoted verbatim. [VERIFIED: libs/monitor_scale.h:9-10]

```c
/* DATA_R7P1W9CJ_START */
#define MONITOR_SCALE_QUARTERS_MIN 4U
#define MONITOR_SCALE_QUARTERS_MAX 16U
/* DATA_R7P1W9CJ_END */
```

The 1,000-iteration fixture already proves apply-from-original-source, not apply-from-previous-output. [VERIFIED: tests/test_monitor_scale.c:353-365]

```c
/* DATA_H3B6T8VL_START */
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
/* DATA_H3B6T8VL_END */
```

`monitor_scale_apply(7, {5})` is 9 because 7×5/4 = 8.75, half-up away from zero. Feeding 9 back through `remove` then `apply` is a different function and must not be the GEO-02 path.

**When to rewrite the stored logical size:** interactive user resize, a non-echo `ConfigureRequest` that actually changes client size, and initial map after first constrain. **When not to:** WM scale transitions, maximize/fullscreen, chrome-only dest rebind, or client configure echoes of a size the WM just sent.

### Pattern 2: One Coordinator, One Commit

**What:** Do not add preservation at every `fw->m =` site. `update_fvwm_monitor` is the commit. Phase 4 already left this gap:

[VERIFIED: .planning/phases/04-wm-owned-ui-metrics-and-resources/04-12-SUMMARY.md:133]

```text
DATA_Q9F2Y5DM_START
- Frame size is still the pre-ownership estimate (`frame_move_resize` is not re-run); converting `g.normal` / `g.frame` remains Phase 5
DATA_Q9F2Y5DM_END
```

Current owner commit, quoted verbatim. [VERIFIED: fvwm/update.c:666-742]

```c
/* DATA_A1C8N4ZR_START */
Bool update_fvwm_monitor(FvwmWindow *fw)
{
	rectangle g;
	struct monitor *mnew;
	const monitor_topology_snapshot *snapshot;
	monitor_ownership_result result;
	monitor_ownership_status status;
	Atom incumbent_atom;

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
/* DATA_A1C8N4ZR_END */
```

After `fw->m` changes (existing `fw->m_prev` / `fw->m` swap at lines 734-736), if dest `effective_scale.factor.quarters` differs from src, run dest chrome rebind then `geometry_transition_plan` then `frame_setup_window`. If quarters are equal, chrome may still change (different monitor, same scale) — still rebind chrome, skip client resize.

Store a last-transition tuple `(name_atom, quarters, kind)` so a second call with the same tuple is `GEOMETRY_TRANSITION_NOOP`.

### Pattern 3: Pointer-Frozen Drag (GEO-04)

**What:** During `move_loop`, destination owner is the monitor of the **pointer**, not the post-resize frame. Apply at most one scale transition per dest identity until the pointer's extra overlap exceeds `MONITOR_OWNERSHIP_HYSTERESIS_PX` in favor of a challenger.

Hysteresis constant, quoted verbatim. [VERIFIED: libs/monitor_ownership.h:7]

```c
/* DATA_E6S3U7KB_START */
#define MONITOR_OWNERSHIP_HYSTERESIS_PX 8
/* DATA_E6S3U7KB_END */
```

Grab + current mid-loop owner refresh, quoted verbatim. [VERIFIED: fvwm/move_resize.c:2799-2802; fvwm/move_resize.c:2898]

```c
/* DATA_P2L9X5WH_START */
	if (!GrabEm(cursor, GRAB_NORMAL))
	{
		XBell(dpy, 0);
		return False;
	}
/* ... */
			update_fvwm_monitor(fw);
/* DATA_P2L9X5WH_END */
```

Offset is window-origin minus pointer at grab start. [VERIFIED: fvwm/move_resize.c:1721-1732]

```c
/* DATA_M5D1G8TQ_START */
	offset.x = orig_drag.x - drag.x;
	offset.y = orig_drag.y - drag.y;
/* ... */
		move_loop(exc, offset, sz, pFinal, do_move_opaque, CRS_MOVE);
/* DATA_M5D1G8TQ_END */
```

After dest size changes: convert `offset` through the same retained-source rule (store grab offset's logical form at drag start from src scale; `apply` at dest). New origin is `pointer + dest_offset` so the grabbed titlebar point stays under the cursor. Then `sz` in `move_loop` must become dest frame size so rubber-band/opaque move matches.

Existing grab nesting already changes cursor without dropping the grab. [VERIFIED: fvwm/misc.c:136-158]

```c
/* DATA_J7V4B2YF_START */
	if (grab_count[GRAB_ALL] > grab_count[GRAB_PASSIVE])
	{
		/* already grabbed, just change the grab cursor */
		if (grab_context == GRAB_FREEZE_CURSOR)
		{
			if (XGrabPointer(
				    dpy, (PressedW != None) ?
				    PressedW : Scr.NoFocusWin, True,
				    GRAB_EVMASK, GrabModeAsync, GrabModeAsync,
				    None, Scr.FvwmCursors[CRS_DEFAULT],
				    CurrentTime) != GrabSuccess)
			{
				return False;
			}
			return True;
		}
		grab_count[grab_context]++;
		grab_count[GRAB_ALL]++;
		if (grab_context != GRAB_BUSY || grab_count[GRAB_STARTUP] == 0)
		{
			change_grab_cursor(cursor);
		}
		return True;
	}
/* DATA_J7V4B2YF_END */
```

Official `XGrabPointer`: `confine_to None` does not warp; a specified cursor is shown regardless of window; failure codes include `AlreadyGrabbed`, `GrabNotViewable`, `GrabFrozen`, `GrabInvalidTime`. [CITED: https://tronche.com/gui/x/xlib/input/XGrabPointer.html]

Do not Ungrab/re-Grab on scale cross. Use `change_grab_cursor` for dest-scale cursors (Phase 4 cache already keys cursors by quarters).

`IS_WINDOW_BEING_MOVED_OPAQUE` is the in-drag flag. [VERIFIED: fvwm/move_resize.c:2846]

### Pattern 4: Derived States Recompute (GEO-06)

**What:** Maximize and fullscreen are not scaled copies of the current rectangle.

EWMH fullscreen, quoted verbatim from the spec. [CITED: https://specifications.freedesktop.org/wm-spec/1.5/ar01s05.html]

```text
DATA_W8N6C3HR_START
_NET_WM_STATE_FULLSCREEN indicates that the window should fill the entire
screen and have no window decorations. Additionally the Window Manager is
responsible for restoring the original geometry after a switch from
fullscreen back to normal window.
DATA_W8N6C3HR_END
```

EWMH maximize atoms are `_NET_WM_STATE_MAXIMIZED_VERT` and `_NET_WM_STATE_MAXIMIZED_HORZ`. [CITED: https://specifications.freedesktop.org/wm-spec/1.5/ar01s05.html]

Current maximize constrain + store, quoted verbatim. [VERIFIED: fvwm/move_resize.c:5202-5227]

```c
/* DATA_S4K1Z9PE_START */
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
/* DATA_S4K1Z9PE_END */
```

Current unmaximize restore source, quoted verbatim. [VERIFIED: fvwm/move_resize.c:5142-5144]

```c
/* DATA_U2T7F5QA_START */
	get_relative_geometry(
		fw, &new_g, fw->fullscreen.was_maximized ?
		&fw->fullscreen.g.max : &fw->g.normal);
/* DATA_U2T7F5QA_END */
```

Current fullscreen builds a `ResizeMoveMaximize` of dest monitor pixels using **current** borders. [VERIFIED: fvwm/ewmh.c:2072-2083]

```c
/* DATA_C9L3W6XR_START */
	fscr.xypos.x = fw->g.frame.x + fw->g.frame.width / 2;
	fscr.xypos.y = fw->g.frame.y + fw->g.frame.height / 2;
	FScreenGetScrRect(
		&fscr, FSCREEN_XYPOS, &scr_g.x, &scr_g.y,
		&scr_g.width, &scr_g.height);
	get_window_borders(fw, &b);
/* DATA_C9L3W6XR_END */
```

**Required dest-scale behavior:**

| Kind | Dest geometry | Logical store |
|------|---------------|---------------|
| Maximize | Dest monitor work rect (`EWMH_GetWorkAreaIntersection`) in root pixels, then constrain with dest chrome | Do not overwrite logical client; keep `g.normal` as the restore source |
| Fullscreen | Dest monitor `si` rectangle; no decorations (`apply_decor_change` already runs) | Keep `fullscreen.g` / logical normal; WM must restore original geometry |
| Unmaximize / leave fullscreen | `apply(logical_client, dest_factor)` + dest chrome + gravity | Logical unchanged |
| Maximized window crosses monitors | Re-maximize onto dest work area (not scale `g.max`) | Logical unchanged |
| Fullscreen window crosses monitors | Re-fullscreen onto dest monitor | Logical unchanged |

`CS_UPDATE_MAX_DEFECT` stays 0x02. [VERIFIED: fvwm/geometry.h:8]

### Pattern 5: Dest Chrome Then Frame (Phase 4 Handoff)

Map path already rebinds after ownership and **before** `BroadcastConfig`, but after `frame_move_resize`. [VERIFIED: fvwm/add_window.c:2538-2566]

Live crossings must invert that leftover: dest chrome (SelectDecor + `setup_window_font`/`setup_icon_font` True) **then** compose frame from logical client + `get_window_borders`. Border totals, quoted verbatim. [VERIFIED: fvwm/geometry.c:511-536]

```c
/* DATA_B7Y2H4QL_START */
void get_window_borders(
	const FvwmWindow *fw, size_borders *borders)
{
	borders->top_left.width = fw->boundary_width;
	borders->bottom_right.width = fw->boundary_width;
	borders->top_left.height = fw->boundary_width;
	borders->bottom_right.height = fw->boundary_width;
/* DATA_B7Y2H4QL_END */
```

`size_borders` has `top_left`, `bottom_right`, `total_size`. [VERIFIED: libs/fvwmrect.h:44-49]

`window_g` stores frame/normal/max in pixels, not logical. [VERIFIED: fvwm/fvwm.h:699-712]

```c
/* DATA_N1F3S7KA_START */
typedef struct window_g
{
	rectangle frame;
	/* absolute geometry when not maximized */
	rectangle normal;
	/* maximized window geometry */
	rectangle max;
	/* defect between maximized geometry before and after
	 * constraining size. */
	size_rect max_defect;
	/* original delta between normalized and maximized window,
	 * used to keep unmaximized window at same screen position */
	position max_offset;
} window_g;
/* DATA_N1F3S7KA_END */
```

Add a **new** `size_rect` (or two ints) on `FvwmWindow` for logical client size. Do not overload `g.normal` — that is already a root-pixel rectangle and is published through configure/module paths.

After dest frame commit, `EWMH_SetFrameStrut` writes dest chrome pixels. [VERIFIED: fvwm/ewmh.c:1309-1335]

### Pattern 6: Eligibility

ICCCM size specifiers are client pixels excluding borders. [CITED: https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.2.3]

EWMH: non-resizable windows have min == max in `WM_NORMAL_HINTS`. [CITED: https://specifications.freedesktop.org/wm-spec/1.5/ar01s05.html]

In-repo flags: `IS_SIZE_FIXED`, `IS_PSIZE_FIXED`, `HAS_OVERRIDE_SIZE_HINTS`. [VERIFIED: fvwm/window_flags.h:236-265]

| Window | GEO-02 logical resize | Dest chrome | GEO-06 derived recompute |
|--------|----------------------|-------------|--------------------------|
| Normal resizable | Yes | Yes | n/a |
| `IS_SIZE_FIXED` / `IS_PSIZE_FIXED` | No — keep client pixels | Yes | Maximize/fullscreen still fill dest if the command is allowed |
| PMinSize==PMaxSize and not override | No — keep client pixels | Yes | Same |
| `HAS_OVERRIDE_SIZE_HINTS` | Yes (hints ignored) | Yes | Yes |
| Iconified | No (icon geometry is separate) | Icon UI already Phase 4 | n/a |
| Shaded | Yes on unshaded client size | Yes | Restore unshaded logical |
| Maximized / fullscreen | No (derived) | Fullscreen has none | Yes |
| Increment/aspect terminals | Yes, then constrain; logical source unchanged when constrain snaps | Yes | Yes |

Planner must test increment windows: dest pixels may snap off the ideal apply() result; round-trip back to src still uses the **original logical**, so 1,000 trips do not accumulate the snap.

### Anti-Patterns to Avoid

- **Opportunistic `apply` at every `fw->m` write:** several authorities, oscillation. Use the coordinator once.
- **`logical = remove(current_px, src)` on each crossing:** reconstructs from rounded pixels; drifts on 1.25× round trips.
- **Scaling `g.max` / fullscreen frame:** derived states must recompute from dest work area / dest monitor.
- **Composing frame with src chrome then rebinding fonts:** dest title thickness would be wrong; Phase 4 leftover on map.
- **Re-running ownership on the post-resize frame in the same commit:** the new chrome/client size can flip overlap.
- **Updating logical size from ConfigureRequest echoes:** ICCCM clients receive synthetic/real ConfigureNotify and may request the same size back. [CITED: https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.5]
- **Publishing logical units in `_NET_FRAME_EXTENTS`, configure, or module packets.**
- **Mutating `Xft.dpi` so clients "match" dest scale.**

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Pixel conversion | Custom float/DPI multiply | `monitor_scale_apply` / `remove` | Locked quarter arithmetic, overflow, half-away-from-zero. |
| Hint solving | Copy of `constrain_size` | `icccm_constrain_size` then existing wrapper | Aspect, inc, overflow already tested. |
| Owner selection | Second FindScreen | `monitor_ownership_resolve` | Incumbent + 8 px hysteresis + identity ties. |
| Gravity origin | Ad-hoc corner math | `gravity_resize` / `gravity_get_offsets` | All 11 gravities already mapped. |
| Chrome pixels | Multiply BorderWidth in the coordinator | Dest `get_window_borders` after Phase 4 rebind | Chrome is dest UI metrics, not logical size. |
| Frame/X configure | Direct `XConfigureWindow` | `frame_setup_window` | Synthetic ConfigureNotify + parent/client layout. |
| Pointer grab | New grab helper | `GrabEm` / `change_grab_cursor` | Nested grab counts, cursor swap, `confine_to None`. |
| Work area | DisplayWidth wrap | `EWMH_GetWorkAreaIntersection` / `monitor_ownership_work_rect` | Phase 3 root-pixel contract. |

**Key insight:** The hard part is **when** to convert and **what** is the source, not the arithmetic. The arithmetic already exists. A second conversion implementation will drift relative to chrome and tests.

## Common Pitfalls

### Pitfall 1: Reconstructing Logical Size from Rounded Pixels

**What goes wrong:** 800 @ 1x → 1000 @ 1.25x → 801 @ 1x after enough trips.
**Why it happens:** `remove(apply(source))` is not identity for quarter scales (7→9 at 5/4).
**How to avoid:** Store logical client size; apply from that source every time; 1,000-trip test asserts the **source** and dest pixels, not a reconstructed source.
**Warning signs:** Test that does `src = remove(dest)` inside the loop.

### Pitfall 2: Frame-Based Ownership After Dest Resize (GEO-04 oscillation)

**What goes wrong:** Crossing the seam resizes the frame; overlap flips back; inverse resize repeats while the pointer is stationary.
**Why it happens:** `update_fvwm_monitor` uses `fw->g.frame` for mapped windows; dest chrome+client change the rectangle.
**How to avoid:** During grab, resolve from pointer 1×1; freeze dest identity; do not re-feed post-resize frame into selection until the pointer moves enough to beat hysteresis.
**Warning signs:** Alternating `fw->m` with a stationary `MotionNotify` position.

### Pitfall 3: Losing the Grab Anchor

**What goes wrong:** Window grows around NorthWest; titlebar slides out from under the cursor; user feels the grab "broke".
**Why it happens:** `offset` is computed once at src size; dest frame origin stays `pointer+old_offset` while width/height change.
**How to avoid:** Convert offset with the same retained-source rule; origin = `pointer + dest_offset`. Do not Ungrab.
**Warning signs:** `pFinal` origin jumps when quarters change; `XGrabPointer` return not `GrabSuccess` mid-move.

### Pitfall 4: Scaling Derived Maximize/Fullscreen Geometry

**What goes wrong:** A maximized window moved to a smaller 2x monitor overflows the work area, or unmaximize restores already-scaled pixels.
**Why it happens:** `g.max` / fullscreen frame are dest-derived, not logical.
**How to avoid:** Recompute maximize from dest work rect; fullscreen from dest monitor; restore via stored logical normal + dest chrome.
**Warning signs:** `g.normal.width` changes on maximize; leave-fullscreen size depends on dest scale of the fullscreen rectangle.

### Pitfall 5: Chrome/Client Order Inversion

**What goes wrong:** Titlebar gaps, client covering dest title, or `_NET_FRAME_EXTENTS` still 1x after a 2x crossing.
**Why it happens:** Phase 4 map path resizes the frame **before** dest font/SelectDecor. Live crossings that call `frame_setup_window` with src borders repeat that.
**How to avoid:** Dest SelectDecor + font rebind, then `get_window_borders`, then compose, then `EWMH_SetFrameStrut`.
**Warning signs:** `title_thickness` still 1x in the same function as dest client apply.

### Pitfall 6: ConfigureRequest Echo Overwrites Logical Size

**What goes wrong:** WM sends dest size; toolkit requests it back; coordinator treats that as user resize and `remove()`s dest pixels into a new logical source; next crossing drifts.
**Why it happens:** ICCCM clients must accept allocated size and may ConfigureWindow the same geometry. [CITED: https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.5]
**How to avoid:** Ignore configure size that matches the last WM-sent client pixels; reuse existing `SET_FORCE_NEXT_CR` / merge-CR machinery as the echo gate. Do not invent a second echo detector if the existing one covers it — verify in planning against `fvwm/events.c` `_handle_cr_on_client`.
**Warning signs:** Logical width changes on a crossing with no user resize.

### Pitfall 7: Overflow Produces a Plausible Dest Size

**What goes wrong:** Hostile or huge logical source × 4x overflows; caller uses uninitialized dest and maps a huge window.
**Why it happens:** Conversion and constrain already init outputs to 0 on failure; a new planner that skips those helpers will not.
**How to avoid:** Status enum like the other `libs/` units; on `OVERFLOW`/`INVALID_ARGUMENT` leave dest unchanged (last good) and `fvwm_debug`. Same policy as Phase 3 constrain-size restore. [VERIFIED: .planning/STATE.md:154]
**Warning signs:** Planner that clamps to `INT_MAX` or `SHRT_MAX`.

### Pitfall 8: Blocking Work in the Move Loop

**What goes wrong:** Font/image dest rebind inside `move_loop` stalls the grab.
**Why it happens:** Phase 4 AtScale may open Xft/Xcursor. That is X-round-trip work on the one thread.
**How to avoid:** Reuse cached dest-scale fonts/pictures (generation still 1). Do not walk all windows. Do not `XSync` except the existing `HAS_NEW_WM_NORMAL_HINTS` path. If dest cache misses, one open is acceptable; do not decode SVG from disk on every motion event — only on the committed dest change.
**Warning signs:** `PImageLoad` / `FlocaleLoadFontAtScale` inside the `FCheckMaskEvent` wait loop rather than the dest-commit branch.

## Code Examples

### Transition planner shape (follow `icccm_size_hints.h`)

Status/API analog, quoted verbatim. [VERIFIED: libs/icccm_size_hints.h:8-36]

```c
/* DATA_T3P8M1KD_START */
typedef enum
{
	ICCCM_CONSTRAIN_OK,
	ICCCM_CONSTRAIN_OVERFLOW,
	ICCCM_CONSTRAIN_INVALID_ARGUMENT
} icccm_constrain_status;
/* ... */
icccm_constrain_status
icccm_constrain_size(const icccm_size_hints *hints, int *width, int *height,
    rectangle *frame);
/* DATA_T3P8M1KD_END */
```

Recommended planner (values in this skeleton also appear in the quotes above: `OK`/`OVERFLOW`/`INVALID_ARGUMENT`, `monitor_scale_apply`, `icccm_constrain_size`):

```c
typedef enum
{
	GEOMETRY_TRANSITION_OK,
	GEOMETRY_TRANSITION_OVERFLOW,
	GEOMETRY_TRANSITION_INVALID_ARGUMENT,
	GEOMETRY_TRANSITION_INELIGIBLE,
	GEOMETRY_TRANSITION_NOOP
} geometry_transition_status;

typedef enum
{
	GEOMETRY_TRANSITION_NORMAL,
	GEOMETRY_TRANSITION_MAXIMIZED,
	GEOMETRY_TRANSITION_FULLSCREEN,
	GEOMETRY_TRANSITION_RESTORE
} geometry_transition_kind;
```

`INELIGIBLE` and `NOOP` are new status names for this unit (not present in `icccm_constrain_status`). They are the recommended API; treat the exact extra enumerators as planner-locked by this research rather than as pre-existing code. [ASSUMED: enumerator spellings]

### Scale then constrain (already the Phase 3 test)

[VERIFIED: tests/test_icccm_size_hints.c:318-341]

```c
/* DATA_V6R2H9WL_START */
static void
test_scale_then_constrain(void)
{
	monitor_scale_factor factor    = {5};
	icccm_size_hints     hints     = {
		.flags	    = PMinSize,
		.min_width  = 8,
		.min_height = 8,
	};
	int		     converted = 0;
	int		     width;
	int		     height;
	icccm_constrain_status status;

	assert(monitor_scale_apply(7, factor, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 9);

	width  = converted;
	height = converted;
	status = icccm_constrain_size(&hints, &width, &height, NULL);
	assert(status == ICCCM_CONSTRAIN_OK);
	assert(width == 9);
	assert(height == 9);
}
/* DATA_V6R2H9WL_END */
```

GEO-02 tests must loop this from a **stored** source of 7, not from `width` after constrain.

### Gravity when client+chrome size changes

[VERIFIED: libs/gravity.c:111-123]

```c
/* DATA_G4C8Y1NF_START */
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
/* DATA_G4C8Y1NF_END */
```

During grab, skip gravity and use pointer+offset instead.

### Frame commit seam

[VERIFIED: fvwm/frame.h:65-67]

```c
/* DATA_L8Q5D2JM_START */
void frame_setup_window(
	FvwmWindow *fw, int x, int y, int w, int h,
	Bool do_send_configure_notify);
/* DATA_L8Q5D2JM_END */
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Keep client pixels when `fw->m` changes | Convert from stored logical client via dest quarters | This phase | GEO-02 |
| `update_fvwm_monitor` on frame during drag | Pointer 1×1 + dest freeze during grab | This phase | GEO-04 |
| Maximize/fullscreen use current pixel rect | Recompute from dest work area / dest monitor | This phase | GEO-06 |
| Phase 4 map: frame then dest chrome | Live crossing: dest chrome then frame | Phase 4 leftover | UI-01 + GEO-02 together |
| Float aspect in `constrain_size` | Integer `icccm_constrain_size` | Phase 3 | Input to this planner |
| Reconstruct logical via `remove` | Forbidden; retained source | Phase 2 D-04 | Drift-free trips |

**Deprecated/outdated:**

- Scaling `g.max` as if it were a user size: replaced by dest work-area recompute.
- Treating RandR `--scale` / CRTC transform as logical UI scale: out of scope. [VERIFIED: .planning/REQUIREMENTS.md:75]
- Per-monitor `Xft.dpi` so toolkits self-resize: out of scope. [VERIFIED: .planning/REQUIREMENTS.md:76]

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Extra status enumerators `GEOMETRY_TRANSITION_INELIGIBLE` and `GEOMETRY_TRANSITION_NOOP` are the API to add | Code Examples | Planner bikeshed; behavior is what matters |
| A2 | During grab, pointer 1×1 ownership is better than a larger drag-only hysteresis | Pattern 3 | If discuss-phase prefers frame ownership, oscillation tests must still pass |
| A3 | ConfigureRequest echo can be gated with existing `SET_FORCE_NEXT_CR` rather than a new matcher | Pitfall 6 | If existing gate is insufficient, planner needs an explicit last-sent client size compare |
| A4 | Dest font/cursor cache hits make mid-drag rebind cheap enough for the event loop | Pitfall 8 | If misses are common, planner should defer chrome rebind to ButtonRelease (still convert size immediately) |
| A5 | Storing logical client as `size_rect` on `FvwmWindow` (not inside `window_g`) | Pattern 5 | `window_g` is copied into `fullscreen.g`; mixing logical there would restore wrong units |

**If this table is empty:** All claims in this research were verified or cited — no user confirmation needed.

A1–A5 need planner lock, not user product confirmation. No CONTEXT.md means these are research recommendations, not user decisions.

## Open Questions

1. **Should dest chrome rebind run on every motion dest-cross or only ButtonRelease?**
   - What we know: Phase 4 caches by quarters; X-visible transitions should follow dest UI metrics. [VERIFIED: .planning/STATE.md:103]
   - What's unclear: AtScale miss cost under a live grab on slow X.
   - Recommendation: Commit chrome+geometry on dest identity change (not every motion). If profiling later shows stalls, defer chrome to release but still convert client size immediately so the grab anchor stays correct.

2. **Exact storage field names on `FvwmWindow`.**
   - What we know: `g` is pixel `window_g`; `fullscreen.g` copies it. [VERIFIED: fvwm/fvwm.h:699-712; fvwm/fvwm.h:962-968]
   - What's unclear: Whether `logical_client` plus `logical_client_valid` is enough versus also storing src quarters.
   - Recommendation: store `size_rect logical_client` and `Bool logical_client_valid`. Dest/src quarters come from `fw->m_prev` / `fw->m` at commit time. Do not store quarters on the window unless a crossing can happen with `m_prev == NULL`.

3. **Grow-to / partial maximize (`growup`, horizontal-only).**
   - What we know: `CMD_Maximize` has grow and percent forms. [VERIFIED: fvwm/move_resize.c:5433-5497]
   - What's unclear: Whether GEO-06 includes grow-to as "maximize".
   - Recommendation: Treat 100% maximize / EWMH max / fullscreen / restore as in-scope. Leave grow-to using existing pixel closest-window logic unless it already goes through `maximize_fvwm_window`.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| C11 compiler (Clang) | Native policy + tests | ✓ | Apple clang 21.0.0 | — |
| Python 3 | `tests/run_in_linux.py` | ✓ | 3.14.7 | — |
| Podman | macOS → Linux Meson tests | ✓ | 6.1.0 | Native Linux CI |
| pkg-config | Meson deps inside container | ✓ | 3.0.6 | Container image |
| Meson | `meson test` | ✗ on host | — | `python3 tests/run_in_linux.py -- meson test -C builddir ...` |
| Ninja | compile | ✗ on host | — | Same Podman wrapper |
| Xvfb / Xorg-dummy | live-X | ✗ on host | — | Not required for Phase 5 native policy; existing live-X suite remains non-regression |

**Missing dependencies with no fallback:**
- none for this phase's native policy work (Podman wraps Meson/Ninja)

**Missing dependencies with fallback:**
- Host Meson/Ninja → `python3 tests/run_in_linux.py -- meson test -C builddir …`

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Meson test runner; plain C assertion executables (`-UNDEBUG`) |
| Config file | `tests/meson.build` |
| Quick run command | `python3 tests/run_in_linux.py -- meson test -C builddir geometry-transition icccm-size-hints monitor-scale monitor-ownership --print-errorlogs` |
| Full suite command | `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs` |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| GEO-02 | Stored logical 7 stays 7 after 1,000 trips through 5/6/7/8 quarters; dest pixels match one `apply` | unit | `python3 tests/run_in_linux.py -- meson test -C builddir geometry-transition --print-errorlogs` | ❌ Wave 0 |
| GEO-02 | Increment snap does not rewrite logical source | unit | same | ❌ Wave 0 |
| GEO-02 | Overflow/invalid dest outputs stay 0; last-good policy documented | unit | same | ❌ Wave 0 |
| GEO-04 | Pointer 1×1 dest freeze: frame resize that would flip overlap does not change dest | unit | same | ❌ Wave 0 |
| GEO-04 | Grab offset converts with apply-from-source; origin = pointer + dest_offset | unit | same | ❌ Wave 0 |
| GEO-06 | Maximize dest = dest work rect, not scaled `g.max` | unit | same | ❌ Wave 0 |
| GEO-06 | Fullscreen dest = dest monitor rect; restore uses stored logical | unit | same | ❌ Wave 0 |
| GEO-06 | Fixed-size client pixels unchanged; chrome totals follow dest | unit | same | ❌ Wave 0 |
| GEO-07 guard | No logical fields in `vpacket.h` / Status | source audit | `git diff` of `libs/vpacket.h`, `fvwm/module_interface.c`, `fvwm/status_json.c` | ✅ files exist |

### Sampling Rate

- **Per task commit:** `python3 tests/run_in_linux.py -- meson test -C builddir geometry-transition --print-errorlogs`
- **Per wave merge:** `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs`
- **Phase gate:** Full native suite green before `/gsd-verify-work`. Existing live-X suite is non-regression only.

### Wave 0 Gaps

- [ ] `tests/test_geometry_transition.c` — covers GEO-02, GEO-04, GEO-06
- [ ] `geometry-transition` registration in `tests/meson.build`
- [ ] Link `libs/geometry_transition.c` via `libs/meson.build` `libfvwm3_sources`

None of the existing tests cover logical-size crossings. Keep `monitor-scale`, `icccm-size-hints`, `monitor-ownership`, `wm-ui-metrics` green.

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | no | X11 authorization is outside this phase |
| V5 Input Validation | yes | `icccm_constrain_size` + `monitor_scale_apply` overflow; untrusted `WM_NORMAL_HINTS` and ConfigureRequest |
| V6 Cryptography | no | — |

### Known Threat Patterns for C11 X11 WM geometry

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Hostile min/max/inc causing overflow | Tampering / DoS | Existing `ICCCM_CONSTRAIN_OVERFLOW` zeros outputs; planner must not invent a dest size [VERIFIED: libs/icccm_size_hints.h:8-13] |
| Scale apply overflow (`INT_MAX` × 5/4) | DoS | `MONITOR_SCALE_CONVERSION_OVERFLOW` inits `*converted = 0` [VERIFIED: libs/monitor_scale.c:243-253] |
| ConfigureRequest loop fighting dest size | Tampering / DoS | One WM transition per dest tuple; echoes do not rewrite logical; existing CR merge in `events.c` |
| Grab starvation / event-loop stall | DoS | No blocking work; dest rebind only on committed dest change; keep `GrabEm` budgets |
| Logical units leaking into EWMH/module packets | Tampering | GEO-07: pixels only; `EWMH_SetFrameStrut` uses dest `size_borders` |
| Use-after-free of dest font during grab | Tampering | Reuse Phase 4 AtScale cache; do not unload `Scr.DefaultFont` from a window slot |

## Sources

### Primary (HIGH confidence)

- `libs/monitor_scale.h:9-10,94-98` and `libs/monitor_scale.c:243-267` — conversion contract
- `libs/icccm_size_hints.h:8-36` and `tests/test_icccm_size_hints.c:318-370` — scale then constrain
- `libs/monitor_ownership.h:7-39` and `fvwm/update.c:666-742` — owner commit
- `fvwm/move_resize.c:1721-1732,2755-2898,5123-5227,5410-5414` — grab, maximize
- `fvwm/ewmh.c:1309-1335,2024-2083` — frame extents, fullscreen
- `fvwm/add_window.c:2548-2565` — Phase 4 dest chrome rebind leftover
- `fvwm/misc.c:136-158` — nested GrabEm
- `fvwm/fvwm.h:699-712,962-968` — pixel `window_g` / fullscreen copy
- `tests/test_monitor_scale.c:353-365` — 1,000 apply-from-source trips
- `.planning/phases/04-wm-owned-ui-metrics-and-resources/04-12-SUMMARY.md:133` — Phase 5 handoff
- `.planning/research/PITFALLS.md:89-117,215-245` — oscillation and derived-state pitfalls (in-repo domain research)

### Secondary (MEDIUM confidence)

- [EWMH 1.5 Application Window Properties](https://specifications.freedesktop.org/wm-spec/1.5/ar01s05.html) — fullscreen restore, maximize atoms, fixed-size min==max, frame extents
- [ICCCM §4.1.2.3 and §4.1.5](https://tronche.com/gui/x/icccm/sec-4.html) — client-pixel hints, configure coordinate spaces
- [XGrabPointer](https://tronche.com/gui/x/xlib/input/XGrabPointer.html) — grab failure codes, `confine_to`, cursor

### Tertiary (LOW confidence)

- classify-confidence seam rated `webfetch` LOW even with `--verified`; official pages are still cited, not treated as in-repo fact
- Graph store last_build 2026-08-30T17:35Z (~28h old); graphify disabled; capability queries returned no nodes — treated as unused

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all facilities are in-repo and were Read this session
- Architecture: HIGH for retained-source + one coordinator; MEDIUM for pointer-freeze vs release-time chrome
- Pitfalls: HIGH — matching in-repo behavior plus domain PITFALLS.md written for this phase

**Research date:** 2026-08-31
**Valid until:** 30 days (stable C11/X11 contracts)

**Graph note:** `.gsd-graph` exists (`stale: false`, `age_hours` ~28). `gsd-tools graphify` is disabled. Queries abstained (`seeds_disconnected`). No graph edges were used as source of truth.

**Runtime State Inventory:** omitted — this is not a rename/refactor/migration phase.
