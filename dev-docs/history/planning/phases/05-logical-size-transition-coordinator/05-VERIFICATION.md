---
phase: 05-logical-size-transition-coordinator
verified: 2026-09-01T10:31:28Z
status: gaps_found
score: 13/16 must-haves verified
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: gaps_found
  previous_score: 12/16
  gaps_closed:
    - "A ConfigureRequest echo of a size the WM just sent must not become a new logical source."
  gaps_remaining:
    - "An eligible window preserves its logical client size across scale changes and repeated fractional round trips without drift."
    - "When IS_SHADED, dest chrome rebinds and client resize is skipped, but unshaded logical size is still converted when the window is later unshaded."
  regressions: []
gaps:
  - truth: "An eligible window preserves its logical client size across scale changes and repeated fractional round trips without drift."
    status: failed
    reason: "Restore echo (previous CR-01) is now armed. Shade→dest→unshade still loses dest conversion: update_absolute_geometry clobbers converted g.normal, and maximized/fullscreen unshade reads unconverted g.max. Same root as the shaded truth below."
    artifacts:
      - path: "fvwm/update.c"
        issue: "Shaded dest writes g.normal and last_transition_*, then records dest as done; later unshade/move cannot re-plan"
      - path: "fvwm/geometry.c"
        issue: "update_absolute_geometry overwrites dest-converted g.normal/g.max from the source-sized shaded strip"
    missing:
      - "Keep the strip's unshaded axis in sync with converted geometry, or convert at unshade and do not record last_transition until an unshaded client-size commit lands"
      - "Plan MAXIMIZED/FULLSCREEN into g.max / dest monitor rect on the shaded path, or leave last_transition unset so unshade can dest-plan"
  - truth: "When IS_SHADED, dest chrome rebinds and client resize is skipped, but unshaded logical size is still converted when the window is later unshaded."
    status: failed
    reason: "05-08 assigns dest-sized g.normal and records last_transition after OK/INELIGIBLE/NOOP, without frame_setup_window. That does not survive the shaded geometry invariant, and it never converts the rectangle CMD_WindowShade actually expands."
    artifacts:
      - path: "fvwm/update.c"
        issue: "915-976: shaded branch always plans GEOMETRY_TRANSITION_NORMAL into g.normal, then stores original kind (including MAXIMIZED/FULLSCREEN) into last_transition_kind"
      - path: "fvwm/geometry.c"
        issue: "636-672: after calling update_fvwm_monitor, copies g.frame strip size onto dest_g then restores only the collapsed axis from the saved rectangle — N/S loses dest width, E/W loses dest height"
      - path: "fvwm/geometry.c"
        issue: "455-463: get_unshaded_geometry returns g.max when IS_MAXIMIZED, so converted g.normal is unused on unshade"
      - path: "fvwm/windowshade.c"
        issue: "193: unshade end_g comes from get_unshaded_geometry; still no geometry_transition_plan"
    missing:
      - "Sync fw->g.frame unshaded-axis size with dest_frame when writing g.normal, without expanding the shade axis"
      - "Do not record last_transition_* on shaded MAXIMIZED/FULLSCREEN; convert g.max (and fullscreen dest rect) the same way the non-shaded branch does"
      - "Pack shaded src_client from pre-rebind src_borders, not dest chrome after SelectDecor"
---

# Phase 5: Logical-Size Transition Coordinator Verification Report

**Phase Goal:** Windows cross monitor scale boundaries through one coherent transition that preserves perceived size, interaction anchors, and window state.
**Verified:** 2026-09-01T10:31:28Z
**Status:** gaps_found
**Re-verification:** Yes — after gap-closure plans 05-07 and 05-08

05-07 SUMMARY and 05-08 SUMMARY claim CR-01 and CR-02 closed. Code review (`05-REVIEW.md`, 2026-09-01T10:25:22Z) reported two remaining criticals in 05-08. Those criticals were re-checked in source, not accepted from the review file. REQUIREMENTS.md `GEO-02 Complete` is still not evidence.

Darwin host did not run `meson test`. Native fixtures are present and wired; absence of a local binary run is not a gap.

## Goal Achievement

### Observable Truths

| #   | Truth   | Status     | Evidence       |
| --- | ------- | ---------- | -------------- |
| 1   | An eligible window preserves its logical client size across scale changes and repeated fractional round trips without drift. | ✗ FAILED | Planner 1000-trip fixture still apply-from-source. Restore echo no longer rebuilds logical_client (truth 14 closed). Shade→dest→unshade still does not preserve dest-converted unshaded size (truth 15). |
| 2   | User can drag a window across scales without losing the grabbed pointer anchor or triggering ownership oscillation. | ✓ VERIFIED | Regression: `SET_WINDOW_BEING_MOVED_OPAQUE` at `move_resize.c:2898`; 1x1 pointer rect `update.c:693-699`; `GrabEm` at loop start `move_resize.c:2851`; `change_grab_cursor` on dest quarters `move_resize.c:2985`; no `XGrabPointer` in `move_resize.c`. |
| 3   | Maximize, fullscreen, unmaximize, restore, and monitor-transition behavior produces correct destination geometry at every supported scale. | ✓ VERIFIED | Regression of unshaded dest kinds: maximize copies dest work rect (`update.c:839-844`, `move_resize.c:5580`); fullscreen uses `fw->m->si`; restore plans `GEOMETRY_TRANSITION_RESTORE`. Shaded maximized/fullscreen dest is truth 15, not a regression of this dest-kind wiring. |
| 4   | `geometry_transition_plan` NORMAL of logical 7 at dest_factor.quarters 5 writes dest 9x9; quarters 4 and 16 are valid; 3 and 17 invalid-and-zero. | ✓ VERIFIED | Regression: fixtures still invoked from `main`. |
| 5   | 1000 iterations through quarters 5, 6, 7, 8 pass the same stored logical 7; dest client equals one `monitor_scale_apply`; increment snap does not rewrite that 7 in the planner. | ✓ VERIFIED | Regression: `test_repeated_plan_from_stored_logical_seven` still in `main`. |
| 6   | NULL dest_frame, NULL dest_chrome on a non-fullscreen kind, or dest_factor outside 4..16 returns `GEOMETRY_TRANSITION_INVALID_ARGUMENT` after zeroing dest size when the pointer is non-NULL. | ✓ VERIFIED | Regression: `fail_invalid` / `zero_dest_frame_size` unchanged. |
| 7   | Maximize dest_frame equals the supplied dest work rect; a 1x1 pointer rectangle with extra_overlap 8 keeps the incumbent name_atom while extra_overlap 9 steals. | ✓ VERIFIED | Regression: maximize and freeze fixtures still in `main`. |
| 8   | Fullscreen kind precedes maximized; dest chrome then `geometry_transition_plan` then `frame_setup_window`. | ✓ VERIFIED | Regression: `update.c:772-783` then SelectDecor/fonts then plan then `frame_setup_window`. |
| 9   | When kind, dest_factor, and dest_frame size already match a NORMAL result, a second `geometry_transition_plan` returns `GEOMETRY_TRANSITION_NOOP` without shrinking dest_frame. | ⚠️ insufficient_spec | Still `verification: backstop`. Equality branch exists. No held-out test asserts NOOP without shrinking. Maximize fixture still allows OK *or* NOOP. |
| 10  | Grab-offset conversion applies stored logical 8 at dest_factor.quarters 5 to 10 so origin equals pointer plus dest offset. | ✓ VERIFIED | Regression: `test_grab_offset_apply_from_source`. |
| 11  | `FvwmWindow` stores `logical_client` / `logical_client_valid` outside `window_g`, next to `m` / `m_prev`; AddWindow records the first constrained client as 1x-equivalent source. | ✓ VERIFIED | Regression: `fvwm.h` placement; `setup_logical_client` still called from `add_window.c`. |
| 12  | After `fw->m` changes, `update_fvwm_monitor` rebinds dest chrome then calls `geometry_transition_plan` then `frame_setup_window` before `BroadcastConfig`. | ✓ VERIFIED | Non-shaded path: last_transition now assigned *after* plan (`update.c:859-875`), then `frame_setup_window` (`882-885`). Shaded path still skips `frame_setup_window` (intended). |
| 13  | During `move_loop`, dest ownership stays on the pointer 1x1 path; `GrabEm` is kept; dest offset comes from the grab-start logical source. | ✓ VERIFIED | Regression: opaque flag, 1x1 resolve, `change_grab_cursor`. |
| 14  | A ConfigureRequest whose client size equals `last_wm_sent_client` does not rewrite `logical_client` — including after unmaximize / leave-fullscreen. | ✓ VERIFIED | **Gap closed (05-07).** Successful RESTORE writes dest client (`dest_frame` minus `dest_chrome.total_size`, floor 0) into `last_wm_sent_client` and `SET_FORCE_NEXT_CR(fw, 1)` at `move_resize.c:5321-5336` *before* `frame_setup_window` at 5340. No `logical_client` assignment on this path. `_handle_cr_on_client` inits `skip_logical_client` from `IS_MAXIMIZED` / `IS_EWMH_FULLSCREEN` only (`events.c:967-970`); last-sent match still sets skip (`1163-1165`); `SET_FORCE_NEXT_CR(fw, 0)` remains at 1233. Blanket `DO_FORCE_NEXT_CR` skip is gone. OVERFLOW/INVALID restore does not arm. |
| 15  | When IS_SHADED, dest chrome rebinds and client resize is skipped, but unshaded logical size is still converted when the window is later unshaded. | ✗ FAILED | **05-08 did not close this.** Shaded dest *does* plan `GEOMETRY_TRANSITION_NORMAL` into `fw->g.normal` (`update.c:954-974`) and records `last_transition_*` after OK/INELIGIBLE/NOOP (`969-971`), with no `frame_setup_window`. Two live paths still miss dest conversion: (1) `update_absolute_geometry` (`geometry.c:636-672`) calls `update_fvwm_monitor` then copies the source strip onto `g.normal`/`g.max` and restores only the collapsed axis — N/S dest width / E/W dest height is lost, and last_transition is already dest so retry is a NOOP. CMD_Move (`move_resize.c:2389` then `2406`) and opaque-move end (`4955`) hit this. (2) Maximized/fullscreen: shaded branch never plans `MAXIMIZED`/`FULLSCREEN`; `get_unshaded_geometry` (`geometry.c:457-459`) returns `g.max` when `IS_MAXIMIZED`; `CMD_WindowShade` (`windowshade.c:193`) expands that source maximize rectangle. |
| 16  | MonitorScale manpage states eligible windows preserve logical client size, dest work-area maximize, dest-monitor fullscreen, restore from stored logical plus dest chrome, and that `_NET_FRAME_EXTENTS` / Status v4 / module packets remain actual X11 pixels. | ✓ VERIFIED | Regression: manpage continuation unchanged. |

**Score:** 13/16 truths verified (0 present, behavior-unverified; 1 abstained backstop)

### Gap-closure plan truths (05-07 / 05-08)

These are extra PLAN must_haves, not extra score rows. They explain why truth 14 closed and truth 15 did not.

| Plan | Truth | Status | Evidence |
| ---- | ----- | ------ | -------- |
| 05-07 | After successful RESTORE, `last_wm_sent_client` is dest client pixels and `SET_FORCE_NEXT_CR` runs before `frame_setup_window` | ✓ VERIFIED | `move_resize.c:5321-5340` |
| 05-07 | `_handle_cr_on_client` inits skip from maximized/fullscreen only; last-sent match still skips | ✓ VERIFIED | `events.c:967-970, 1163-1165` |
| 05-08 | `last_transition_*` assigned only after OK/INELIGIBLE/NOOP | ✓ VERIFIED | Non-shaded `update.c:863-875`; shaded `959-971`; no write between font rebind and plan |
| 05-08 | Shaded success stores dest_atom/quarters/kind the same as non-shaded | ✓ implemented, harmful for MAX/FS | `update.c:969-971` stores original `kind`, including `MAXIMIZED`/`FULLSCREEN`, after a NORMAL plan into `g.normal` |
| 05-08 | On shaded dest change, `g.normal` becomes apply(logical)+dest chrome without `frame_setup_window` | ⚠️ HOLLOW | Assignment at `update.c:974` is real. `update_absolute_geometry` undoes the unshaded axis. `g.max` is never written. |
| 05-08 | Opaque-move dest still 1x1 pointer; no second `XGrabPointer` | ✓ VERIFIED | `update.c:693-699`; `XGrabPointer` stays in `misc.c` `GrabEm` |

### Required Artifacts

| Artifact | Expected    | Status | Details |
| -------- | ----------- | ------ | ------- |
| `libs/geometry_transition.h` | status/kind enums and `geometry_transition_plan` contract | ✓ VERIFIED | Regression. |
| `libs/geometry_transition.c` | Apply-from-logical, constrain, dest-chrome compose | ✓ VERIFIED | Regression. |
| `tests/test_geometry_transition.c` | GEO-02/04/06 fixtures plus 05-07/05-08 tests | ✓ EXISTS; ⚠️ weak on live gaps | 13 tests in `main`. `test_restore_echo_keeps_stored_logical_seven` is tautological (by-value logical; `last_sent_w == dest_frame.width`). `test_unshaded_dest_from_logical_seven_replaces_source_sized_normal` only hits the planner (7→9 at origin 100,200); it never constructs a shaded `FvwmWindow` and would stay green if `update.c` were reverted. |
| `fvwm/fvwm.h` | `logical_client`, last-transition tuple, `last_wm_sent_client` | ✓ VERIFIED | Regression. |
| `fvwm/add_window.c` | Initial logical_client | ✓ VERIFIED | Regression. |
| `fvwm/update.c` | Single ownership-change commit; shaded dest converts unshaded size | ⚠️ HOLLOW on shaded live path | last_transition after success (WR-02 closed). Shaded `g.normal` write is undone by `update_absolute_geometry`. Maximized/fullscreen unshade rectangle not converted. |
| `fvwm/move_resize.c` | Pointer-frozen drag; dest work-area maximize; restore echo arm | ✓ VERIFIED | Restore echo arm present (`5321-5336` before `5340`). Drag/maximize regression holds. |
| `fvwm/ewmh.c` | Fullscreen dest monitor; frame extents dest chrome | ✓ VERIFIED | Regression. |
| `fvwm/events.c` | ConfigureRequest echo gate | ✓ VERIFIED | Skip init no longer uses `DO_FORCE_NEXT_CR`. Last-sent compare kept. |
| `fvwm/geometry.c` | Shaded unshade rectangle must keep dest conversion | ✗ NOT_WIRED to 05-08 intent | `update_absolute_geometry` and `get_unshaded_geometry` were not part of 05-08's files_modified and they break the 05-08 contract. |
| `doc/fvwm3_manpage_source.adoc` | User-visible logical-size transition contract | ✓ VERIFIED | Regression. |

**Artifacts:** 8/10 original fully verified; `update.c` still hollow on shaded dest; `move_resize.c` restore echo now verified.

### Key Link Verification

| From | To  | Via | Status | Details |
| ---- | --- | --- | ------ | ------- |
| `libs/geometry_transition.c` | `libs/monitor_scale.h` | `monitor_scale_apply` on stored logical | ✓ WIRED | Regression. |
| `libs/geometry_transition.c` | `libs/icccm_size_hints.h` | `icccm_constrain_size` except true fullscreen | ✓ WIRED | Regression. |
| `tests/test_geometry_transition.c` | `libs/monitor_ownership.h` | pointer 1x1 + hysteresis 8 | ✓ WIRED | Regression. |
| `fvwm/update.c` | `libs/geometry_transition.h` | `geometry_transition_plan` after dest chrome | ✓ WIRED | Non-shaded and shaded branches. |
| `fvwm/update.c` | `fvwm/frame.h` | `frame_setup_window` | ✓ WIRED | Non-shaded OK/INELIGIBLE; shaded dest still does not call it. |
| `fvwm/add_window.c` | `fvwm/fvwm.h` | `logical_client_valid` after first constrain | ✓ WIRED | Regression. |
| `fvwm/move_resize.c` | `fvwm/update.c` | `update_fvwm_monitor` while opaque-moved | ✓ WIRED | `move_resize.c:2950`. |
| `fvwm/move_resize.c` | `fvwm/misc.c` | `GrabEm` / `change_grab_cursor` | ✓ WIRED | Regression. |
| `fvwm/move_resize.c` | `libs/geometry_transition.h` | `GEOMETRY_TRANSITION_RESTORE` | ✓ WIRED | Plus echo arm matching `update.c` non-fullscreen last-sent arithmetic. |
| `fvwm/events.c` | `fvwm/fvwm.h` | `last_wm_sent_client` before logical write | ✓ WIRED | Producers now include restore. |
| `fvwm/update.c` | `fvwm/geometry.c` | converted `g.normal` is unshade size | ✗ NOT_WIRED | `get_unshaded_geometry` uses `g.max` when maximized. `update_absolute_geometry` clobbers converted size from the strip. |
| `doc/fvwm3_manpage_source.adoc` | `libs/monitor_scale.h` | “retained exact source” extended | ✓ WIRED | Regression. |
| `doc/fvwm3_manpage_source.adoc` | `fvwm/ewmh.c` | `_NET_FRAME_EXTENTS` dest chrome pixels | ✓ WIRED | Regression. |

**Wiring:** restore→echo-gate now WIRED. shaded dest→unshade still NOT_WIRED for the live geometry invariant.

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
| -------- | ------------- | ------ | ------------------ | ------ |
| `geometry_transition_plan` NORMAL/RESTORE | dest client pixels | `monitor_scale_apply(logical_client, dest_factor)` | Yes | ✓ FLOWING |
| `geometry_transition_plan` MAXIMIZED | dest_frame | dest work rect minus chrome, then constrain | Yes | ✓ FLOWING |
| `geometry_transition_plan` FULLSCREEN | dest_frame | dest monitor rect, unmodified | Yes | ✓ FLOWING |
| `setup_logical_client` | `fw->logical_client` | first constrained client | Yes | ✓ FLOWING |
| `update_fvwm_monitor` non-shaded | `fw->g.frame` | plan dest_frame → `frame_setup_window` | Yes | ✓ FLOWING |
| `unmaximize_fvwm_window` | `last_wm_sent_client` | dest_frame minus dest chrome, then `SET_FORCE_NEXT_CR` | Yes | ✓ FLOWING |
| `_handle_cr_on_client` | `logical_client` | skipped when size equals `last_wm_sent_client` | Yes on real resize; restore echo gated | ✓ FLOWING |
| `update_fvwm_monitor` shaded | `fw->g.normal` | NORMAL plan from `logical_client` | Assigned, then clobbered by `update_absolute_geometry` | ⚠️ STATIC after move/drag |
| `update_fvwm_monitor` shaded max/fs | `fw->g.max` / unshade rect | never planned | Unshade reads source `g.max` | ✗ DISCONNECTED |
| `EWMH_SetFrameStrut` | `_NET_FRAME_EXTENTS` | `get_window_borders` dest pixels | Yes | ✓ FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| -------- | ------- | ------ | ------ |
| Planner fixtures exist and are invoked | source enumeration of `tests/test_geometry_transition.c` `main` | 13 tests including restore-echo and unshaded-dest | ✓ PASS (existence) |
| Restore echo arm before `frame_setup_window` | read `unmaximize_fvwm_window` | `last_wm_sent_client` + `SET_FORCE_NEXT_CR(fw, 1)` at 5321-5336, `frame_setup_window` at 5340, no `logical_client` write | ✓ PASS |
| Skip init is not blanket `DO_FORCE_NEXT_CR` | read `_handle_cr_on_client` | init from maximized/fullscreen only; last-sent match still skips | ✓ PASS |
| last_transition not written before plan | read `update_fvwm_monitor` | writes only in post-plan else branches `873-875` and `969-971` | ✓ PASS |
| Shaded dest conversion survives `update_absolute_geometry` | read `geometry.c:636-672` vs `update.c:969-974` | strip overwrite of dest_g; last_transition already dest | ✗ FAIL |
| Maximized shaded unshade uses dest work area | read `get_unshaded_geometry` + shaded plan kind | unshade reads `g.max`; shaded plan is NORMAL into `g.normal` | ✗ FAIL |
| `meson test -C builddir geometry-transition` | native binary | Not executed (Darwin host) | ? SKIP |

Step 7b: native binaries not run on this host. Source contracts are the live-path evidence. Live-X matrix is Phase 8 and is not used to paper over remaining CR-01/CR-02.

### Probe Execution

No `scripts/*/tests/probe-*.sh` and no phase-declared probes. Step 7c skipped.

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
| ----------- | ---------- | ----------- | ------ | -------- |
| GEO-02 | 05-01, 05-02, 05-05, 05-06, 05-07, 05-08 | Eligible window preserves logical client size across scale transitions and repeated round trips without drift | ✗ BLOCKED | Restore echo no longer rebuilds logical from dest pixels. Shade→dest→unshade still loses dest-converted unshaded size (clobber + maximized `g.max`). REQUIREMENTS.md “Complete” remains incorrect. |
| GEO-04 | 05-01, 05-03, 05-06, 05-08 | Drag across scales without losing grab or ownership oscillation | ✓ SATISFIED | 1x1 pointer freeze, kept `GrabEm`, logical grab offset. Shaded drag dest conversion is GEO-02, not a lost grab. |
| GEO-06 | 05-01, 05-03, 05-04, 05-06, 05-07 | Correct maximize, fullscreen, unmaximize, restore, and monitor-transition dest geometry at every supported scale | ✓ SATISFIED | Unshaded dest work rect / dest monitor / RESTORE from stored logical. Restore echo arm is in place. Shaded maximized dest is counted under GEO-02 / truth 15. |

**Coverage:** 2/3 requirements satisfied. No orphaned Phase 5 IDs: GEO-02, GEO-04, GEO-06 are the only REQUIREMENTS.md rows mapped to this phase.

### Prohibitions

PLAN frontmatter prohibitions remain `verification: null` / `flagged-unverified`. Restored echo no longer violates the rebuild prohibition; shaded dest does not write `logical_client`.

| Prohibition | Status | Evidence |
| ----------- | ------ | -------- |
| A scale transition must not rebuild the stored logical client from dest pixels. | held | Restore path arms last-sent; `_handle_cr_on_client` skips on match. Shaded dest does not assign `logical_client`. Remaining gap is failed dest conversion, not a rebuild. |
| last_transition_* must not be recorded on OVERFLOW or INVALID_ARGUMENT. | held | Both branches only write in the else of the OVERFLOW/INVALID check. |
| Maximize/fullscreen dest must not be produced by scaling derived `g.max` / fullscreen frame. | held | Unshaded maximize uses dest work rect; fullscreen uses `fw->m->si`. |
| Planner must not introduce logical units into EWMH, configure, Status, or module packets. | held | Regression. |
| `logical_client` must not live inside `window_g`. | held | Regression. |
| Same `update_fvwm_monitor` commit must not re-run `monitor_ownership_resolve` on the post-resize frame. | held | One call at `update.c:712`. |
| Dest-scale resize during drag must not drop `GrabEm` / start a second `XGrabPointer`. | held | `change_grab_cursor` only. |
| Maximize/unmaximize must not overwrite `logical_client`. | held | Neither assign it. |
| `_NET_FRAME_EXTENTS` must remain dest chrome pixels. | held | Regression. |
| Documentation must not describe EWMH/Status/module geometry as logical units. | held | Regression. |

### Decision Coverage

Skipped — no `05-CONTEXT.md` and no `<decisions>` block.

### Test Quality Audit

| Test File | Linked Req | Active | Skipped | Circular | Assertion Level | Verdict |
|-----------|-----------|--------|---------|----------|-----------------|---------|
| `tests/test_geometry_transition.c` | GEO-02, GEO-04, GEO-06 | 13 | 0 | 0 | Value (planner 9x9, 1000-trip, work/monitor rects) | Planner OK |
| `test_restore_echo_keeps_stored_logical_seven` | GEO-02 echo | 1 | 0 | 0 | Tautology (`logical` by value; `last_sent_w == dest_frame.width`) | INSUFFICIENT — would pass if echo arm were removed |
| `test_unshaded_dest_from_logical_seven_replaces_source_sized_normal` | GEO-02 shaded | 1 | 0 | 0 | Planner dest-size replacement only | INSUFFICIENT — does not cover `update.c` / `geometry.c` |

**Disabled tests on requirements:** 0
**Circular patterns detected:** 0
**Insufficient assertions:** 2 (restore-echo fixture, shaded-dest fixture) → WARNING. Blockers are production wiring vs `update_absolute_geometry` / `g.max`, not skipped tests.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| ---- | ---- | ------- | -------- | ------ |
| `fvwm/geometry.c` | 636-672 | Shaded dest conversion overwritten from strip | 🛑 Blocker | Latest CR-01 — N/S dest width / E/W dest height lost; last_transition already dest |
| `fvwm/update.c` | 954-971 | Shaded MAX/FS records last-transition after NORMAL into `g.normal` | 🛑 Blocker | Latest CR-02 — unshade reads `g.max`; dest MAX/FS path never runs |
| `fvwm/update.c` | 918-921 | `src_client` packed with dest chrome after rebind | ⚠️ Warning | Latest WR-01 — INELIGIBLE compose uses wrong chrome |
| `tests/test_geometry_transition.c` | 315-355 | Restore-echo fixture never exercises echo gate | ⚠️ Warning | Latest WR-02 — tautology |
| `fvwm/add_window.c` | 2236-2251 | Per-axis `monitor_scale_remove`; `logical_client_valid` on partial | ⚠️ Warning | Previous WR-03 — mixed units |
| `fvwm/events.c` | 1210-1226 | Same per-axis split | ⚠️ Warning | Previous WR-03 |
| `fvwm/move_resize.c` | 2960-2976 | Same per-axis split on grab offset | ⚠️ Warning | Previous WR-03 |
| `fvwm/move_resize.c` | 2832-2845 | `m` treated nullable then `m->virtual_scr` | ⚠️ Warning | Previous WR-04 |
| `fvwm/move_resize.c` | 5575 | `update_fvwm_monitor(fw)` instead of assigning `FindScreenOfXY` dest | ⚠️ Warning | Previous WR-05 |
| `fvwm/update.c` / `move_resize.c` | eligibility copies | Now triplicated (shaded branch added) | ℹ️ Info | Latest IN-01 |
| `tests/test_geometry_transition.c` | 358-380 | Planner fixture claimed as shaded-dest coverage | ℹ️ Info | Latest IN-02 |

No `TBD`/`FIXME`/`XXX` in `fvwm/update.c`, `tests/test_geometry_transition.c`, or the 05-07 restore/echo slices. Pre-existing FIXMEs in untouched regions of `events.c` / `geometry.c` are not phase blockers.

Previous blockers (restore without echo arm; last_transition before shaded skip) are gone.

### Human Verification Required

Automated gaps still block UAT of GEO-02. The backstop NOOP item remains outstanding after gap closure:

### 1. NOOP on already-matching NORMAL dest

**Test:** Plan NORMAL to dest size S. Call `geometry_transition_plan` again with the same kind, factor, and dest_frame already equal to S.
**Expected:** `GEOMETRY_TRANSITION_NOOP`; dest_frame width/height unchanged (not zeroed).
**Why human:** Tagged `verification: backstop`. No wired held-out test.

Live-X drag / maximize / mixed-DPI crossings remain Phase 8 (`TEST-02`, `TEST-03`) and are not deferred substitutes for the remaining shaded dest bugs.

## Gaps Summary

05-07 closed the restore echo hole. `unmaximize_fvwm_window` now arms `last_wm_sent_client` and `SET_FORCE_NEXT_CR` before ConfigureNotify, and `_handle_cr_on_client` no longer uses blanket `DO_FORCE_NEXT_CR` to skip logical writes. That previous CR-01 is not present in the tree.

05-08 closed WR-02 (last_transition after success, never on OVERFLOW/INVALID) and *assigned* dest-sized `g.normal` on shaded dest change. That is not the same as converting unshaded size when the window is later unshaded:

1. **Shaded dest conversion undone by `update_absolute_geometry` (latest CR-01).** For a shaded window the function calls `update_fvwm_monitor` (which converts `g.normal` and records dest last_transition), then copies `g.frame` strip size onto `g.normal`/`g.max` and restores only the collapsed axis. Shade, drag/move onto another scale, unshade yields mixed or source-sized unshaded geometry, and the dest tuple will not re-plan.

2. **Shaded maximized/fullscreen records success without converting the unshade rectangle (latest CR-02).** The shaded branch always plans `NORMAL` into `g.normal` and stores `MAXIMIZED`/`FULLSCREEN` as `last_transition_kind`. Unshade expands `g.max` (source work-area pixels). Because `fw->m` is already dest and the tuple matches, the non-shaded MAXIMIZED/FULLSCREEN path that fills dest work area / dest monitor never runs.

Neither remaining gap is scheduled in Phase 6 (hotplug/reload) or Phase 8 (nested-X matrix). They stay actionable.

WR-01 (shaded `src_client` uses dest chrome) and the tautological 05-07/05-08 fixtures are warnings. They do not need to close before the two blockers, but a fixture that cannot fail should not be treated as GEO-02 coverage.

### Deferred Items

None. Later milestone phases do not claim these wiring holes.

## Recommended Fix Plans

### 05-09-PLAN.md: Keep shaded dest conversion through unshade

**Objective:** Shade → dest scale change → unshade must expand to dest-converted unshaded geometry, including maximized/fullscreen.

**Tasks:**
1. Either sync the shaded strip's unshaded axis with converted `g.normal`/`g.max` (so `update_absolute_geometry` cannot restore source strip size on that axis) **or** skip `last_transition_*` on the shaded path and run `geometry_transition_plan` from `CMD_WindowShade` / `get_unshaded_geometry` when dest atom/quarters differ from the last successful *unshaded* commit.
2. Do not record `last_transition_*` for shaded `MAXIMIZED`/`FULLSCREEN` after a NORMAL plan into `g.normal`. Convert `fw->g.max` (and fullscreen dest rect) the same way the non-shaded branch does, still without `frame_setup_window` on the strip.
3. Pack shaded `src_client` from pre-rebind `src_borders`. Verify with a source-contract that `update_absolute_geometry` after shaded dest change leaves dest-converted unshaded-axis size, and that maximized unshade dest equals dest work area.

**Estimated scope:** Small

Do not treat planner-only 7→9 or by-value logical asserts as closing this plan.

---

## Verification Metadata

**Verification approach:** Re-verification of previous 16 truths; failed items full 3-level; passed items existence/sanity; 05-07/05-08 must_haves checked as evidence.
**Must-haves source:** Previous `05-VERIFICATION.md` (ROADMAP success criteria + 05-01..05-06) plus 05-07/05-08 PLAN frontmatter
**Code review accounted:** 05-07 restore echo independently confirmed closed; 05-08 latest CR-01/CR-02 independently re-confirmed in `update.c` / `geometry.c` / `windowshade.c`
**Automated checks:** restore echo passed; shaded dest live path failed
**Human checks required:** 1 (backstop NOOP) — blocked until gaps close
**Local meson test run:** skipped (Darwin)

---
_Verified: 2026-09-01T10:31:28Z_
_Verifier: the agent (gsd-verifier)_
