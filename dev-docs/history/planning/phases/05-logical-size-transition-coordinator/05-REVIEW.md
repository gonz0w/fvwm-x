---
phase: 05-logical-size-transition-coordinator
reviewed: 2026-09-01T10:25:22Z
depth: standard
files_reviewed: 4
files_reviewed_list:
  - fvwm/move_resize.c
  - fvwm/events.c
  - fvwm/update.c
  - tests/test_geometry_transition.c
findings:
  critical: 2
  warning: 2
  info: 2
  total: 6
status: issues_found
---

# Phase 05: Code Review Report

**Reviewed:** 2026-09-01T10:25:22Z
**Depth:** standard
**Files Reviewed:** 4
**Status:** issues_found

## Summary

Gap-closure plans 05-07 and 05-08 were reviewed in `unmaximize_fvwm_window`, `_handle_cr_on_client`, `update_fvwm_monitor`, and the two new geometry-transition fixtures.

05-07 is correctly wired: successful `GEOMETRY_TRANSITION_RESTORE` writes dest client pixels into `last_wm_sent_client`, arms `SET_FORCE_NEXT_CR` before `frame_setup_window`, and `_handle_cr_on_client` now skips logical rewrite from last-sent size match plus maximized/fullscreen — not blanket `DO_FORCE_NEXT_CR`. OVERFLOW/INVALID restore still leaves the echo gate unarmed.

05-08 does move `last_transition_*` to after OK/INELIGIBLE/NOOP on the non-shaded path, which closes WR-02 for that branch. The new shaded dest path does **not** close CR-02 for the live shade→dest→unshade sequence: converted `g.normal` is not the unshade rectangle for maximized/fullscreen windows, and even for normal shaded windows `update_absolute_geometry` restores the strip axis from `g.frame`, undoing dest conversion and leaving `last_transition` already committed so the dest will not re-plan.

## Narrative Findings (AI reviewer)

## Critical Issues

### CR-01: Shaded dest conversion of `g.normal` is undone by the shaded geometry invariant

**File:** `fvwm/update.c:915-974`
**Issue:** On dest change while `IS_SHADED` and `logical_client_valid`, the new branch writes dest-sized `fw->g.normal` and records `last_transition_*`, without touching `fw->g.frame` (the shaded strip). That breaks the invariant `update_absolute_geometry` enforces for shaded windows: the unshaded axis of `g.normal`/`g.max` is taken from the strip, and only the collapsed axis is restored from the saved unshaded rectangle.

`update_absolute_geometry` (`fvwm/geometry.c:636-672`) always calls `update_fvwm_monitor` first when shaded, then:

```c
dest_g = (IS_MAXIMIZED(fw)) ? &fw->g.max : &fw->g.normal;
frame_g = *dest_g;                 /* converted dest size */
dest_g->width = fw->g.frame.width; /* still source strip */
dest_g->height = fw->g.frame.height;
/* N/S: restore height from frame_g; width stays strip width */
/* E/W: restore width from frame_g; height stays strip height */
```

For a north/south-shaded window the dest-scaled **width** is replaced with source strip width; for east/west-shaded the dest-scaled **height** is replaced. One axis of GEO-02 conversion is lost.

This is the primary CR-02 user path. `CMD_Move` runs `update_absolute_geometry` then `update_fvwm_monitor`; opaque move calls `update_fvwm_monitor` in the loop and `update_absolute_geometry` at grab end (`fvwm/move_resize.c:2389`, `4955`). After the clobber, `last_transition_name_atom` / `quarters` / `kind` already match dest, so a later `update_fvwm_monitor` will not re-plan. Shade, drag onto another scale, unshade still yields mixed or source-sized unshaded geometry.

RandR-only dest change that never hits `update_absolute_geometry` can look fine; drag/move does not.

**Fix:** Keep the strip’s unshaded axis in sync with converted `g.normal` without expanding the shade axis, **or** convert at unshade time and do not record `last_transition` until a client-size commit actually lands. Minimal dest-time repair that preserves the existing shaded invariant:

```c
				if (plan_status != GEOMETRY_TRANSITION_NOOP)
				{
					fw->g.normal = dest_frame;
					switch (SHADED_DIR(fw))
					{
					case DIR_N:
					case DIR_S:
					case DIR_NW:
					case DIR_NE:
					case DIR_SW:
					case DIR_SE:
						fw->g.frame.width =
							dest_frame.width;
						break;
					default:
						fw->g.frame.height =
							dest_frame.height;
						break;
					}
				}
```

Better: skip `last_transition_*` on the shaded path and run `GEOMETRY_TRANSITION_NORMAL` from `get_unshaded_geometry` / `CMD_WindowShade` when dest atom/quarters differ from the last *successful unshaded* commit. That avoids lying to `update_absolute_geometry` and does not require a `frame_setup_window` of the strip.

### CR-02: Shaded maximized/fullscreen dest records last-transition without converting the unshade rectangle

**File:** `fvwm/update.c:954-971`
**Issue:** The shaded branch always plans `GEOMETRY_TRANSITION_NORMAL` into `fw->g.normal`, then stores the original `kind` (`MAXIMIZED` or `FULLSCREEN`) into `last_transition_kind`. Unshade does not read that `g.normal` when the window is maximized:

```c
/* fvwm/geometry.c:455-463 */
if (IS_SHADED(fw))
{
	if (IS_MAXIMIZED(fw))
		*ret_g = fw->g.max;
	else
		*ret_g = fw->g.normal;
```

`CMD_WindowShade` uses `get_unshaded_geometry` as `end_g`. A maximized shaded window that changes dest therefore:

1. Converts restore-sized `g.normal` (unused on unshade).
2. Leaves `g.max` at source work-area pixels.
3. Marks dest+MAXIMIZED as already transitioned.

Unshade expands to the **source** maximize rectangle on the dest monitor. Because `dest_changed` is already false (`fw->m` is dest) and `last_transition_kind` matches, the non-shaded `GEOMETRY_TRANSITION_MAXIMIZED` path that fills dest work area never runs. Same hole for `IS_EWMH_FULLSCREEN` (`kind` stored as `FULLSCREEN`, plan still `NORMAL` into `g.normal`).

PA-shaded-normal assumed `g.normal` is “the restore/unshaded source.” That is only true for non-maximized windows. Recording success for a plan that did not touch the unshade rectangle reintroduces original CR-02 for maximize/fullscreen.

**Fix:** Do not record `last_transition_*` on the shaded path when `kind` is `MAXIMIZED` or `FULLSCREEN`. Convert `fw->g.max` (and fullscreen dest rect) the same way the non-shaded branch does, without `frame_setup_window` on the strip. Smallest retry-preserving change:

```c
			plan_status = geometry_transition_plan(
				GEOMETRY_TRANSITION_NORMAL,
				fw->logical_client, src_client, eligible,
				fw->m->effective_scale.factor, &dest_chrome,
				&hints, &dest_frame);
			if (plan_status == GEOMETRY_TRANSITION_OVERFLOW ||
			    plan_status ==
			    GEOMETRY_TRANSITION_INVALID_ARGUMENT)
			{
				fvwm_debug(__func__,
					"geometry transition failed (%d)",
					(int)plan_status);
			}
			else
			{
				if (plan_status != GEOMETRY_TRANSITION_NOOP)
				{
					fw->g.normal = dest_frame;
				}
				if (kind == GEOMETRY_TRANSITION_NORMAL)
				{
					fw->last_transition_name_atom =
						dest_atom;
					fw->last_transition_quarters =
						dest_quarters;
					fw->last_transition_kind = (int)kind;
				}
				/* MAXIMIZED/FULLSCREEN: leave tuple unchanged
				 * so unshade can still dest-plan g.max. */
			}
```

For maximized, also plan `GEOMETRY_TRANSITION_MAXIMIZED` into a copy of dest work area and store that in `fw->g.max` (still no strip `frame_setup_window`).

## Warnings

### WR-01: Shaded `src_client` is packed with dest chrome after rebind

**File:** `fvwm/update.c:793-805,917-930`
**Issue:** The non-shaded path captures `src_client` from `fw->g.frame` minus `src_borders` **before** `SelectDecor` / font rebind, then reads dest chrome after. The shaded path overwrites `src_client` from `fw->g.normal` minus **dest** chrome after that rebind.

`src_client` is only consumed on `INELIGIBLE`, where `plan_from_logical` does `compose_dest_frame(src_client, dest_chrome)`. If dest title chrome is thicker than source chrome, `src_client` is understated by the chrome delta and dest `g.normal` stays at source *frame* size instead of source *client* plus dest chrome. If dest chrome is thinner, dest `g.normal` is overstated.

The 05-08 contract required packing from `g.normal` so INELIGIBLE would not rebuild from the shaded strip. It still needs the **source** chrome captured before rebind.

**Fix:** Reuse the pre-rebind `src_borders` already computed at the start of `do_plan`:

```c
			dest_frame = fw->g.normal;
			get_window_borders(fw, &dest_chrome);
			src_client.width = fw->g.normal.width -
				src_borders.total_size.width;
			src_client.height = fw->g.normal.height -
				src_borders.total_size.height;
			if (src_client.width < 0)
			{
				src_client.width = 0;
			}
			if (src_client.height < 0)
			{
				src_client.height = 0;
			}
```

Do not subtract `dest_chrome` here.

### WR-02: Restore-echo fixture never exercises the echo gate

**File:** `tests/test_geometry_transition.c:315-355`
**Issue:** `test_restore_echo_keeps_stored_logical_seven` is the GEO-06 coverage for “dest-sized CR must not rebuild logical 7.” It does not call `_handle_cr_on_client`, does not write `last_wm_sent_client`, and cannot observe stored logical:

- `logical` is passed to `geometry_transition_plan` **by value**, so `assert(logical.width == 7)` cannot fail.
- `last_sent_w = dest_frame.width` then `assert(last_sent_w == dest_frame.width)` is a tautology. Production stores dest *client* pixels (`dest_frame` minus dest chrome); this fixture uses zero chrome so it never documents that subtraction.
- `removed != 7` only shows increment snap moved dest pixels off logical 7. That is already covered by `test_increment_snap_does_not_rewrite_logical`.

A regression that skipped the `last_wm_sent_client` arm or reintroduced blanket `DO_FORCE_NEXT_CR` skip would still pass this test.

**Fix:** Keep the dest-versus-7 inequality if it documents snap, but assert the production echo-gate inputs and do not claim logical was preserved by a by-value copy:

```c
	assert(status == GEOMETRY_TRANSITION_OK);
	assert(dest_frame.width != converted);
	last_sent_w = dest_frame.width - dest_chrome.total_size.width;
	last_sent_h = dest_frame.height - dest_chrome.total_size.height;
	assert(last_sent_w != converted);
	assert(last_sent_w == dest_frame.width); /* chrome is 0 in this fixture */
	/* skip_logical_client when new_client_w == last_sent_w (events.c) */
```

A production-shaped check belongs in a focused unit of the skip predicate (last-sent match sets skip; `DO_FORCE_NEXT_CR` alone does not). Source-contract over `_handle_cr_on_client` is not a substitute for an assertion that can fail.

## Info

### IN-01: Eligibility / ICCCM hint packing is now triplicated

**File:** `fvwm/update.c:816-837`, `fvwm/update.c:931-952`, `fvwm/move_resize.c:5285-5306`
**Issue:** The shaded dest branch copies the same `eligible` predicate and 13-field `icccm_size_hints` fill already present on the non-shaded dest path and in `unmaximize_fvwm_window`. A later hint-field or fixed-size rule change will desync restore vs dest vs shaded dest.
**Fix:** Extract `bool fvwm_window_logical_eligible(const FvwmWindow *fw)` and `void fvwm_window_pack_icccm_hints(const FvwmWindow *fw, icccm_size_hints *hints)`.

### IN-02: `test_unshaded_dest_from_logical_seven_replaces_source_sized_normal` does not cover `update.c`

**File:** `tests/test_geometry_transition.c:358-380`
**Issue:** The fixture asserts planner dest-size replacement (`7→9` at origin `100,200`). It never constructs a shaded `FvwmWindow`, never assigns `fw->g.normal`, and never checks that `last_transition_*` is withheld on OVERFLOW. TDD RED was already green because this is existing planner behavior. CR-01/CR-02 would not fail this test.
**Fix:** Keep it as a planner contract if useful; add a production-shaped assertion that shaded dest writes `g.normal` from `logical_client` and does not store `last_transition_*` on OVERFLOW — or convert at unshade in a testable helper.

---

_Reviewed: 2026-09-01T10:25:22Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
