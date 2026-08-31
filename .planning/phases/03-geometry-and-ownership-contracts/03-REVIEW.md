---
phase: 03-geometry-and-ownership-contracts
reviewed: 2026-08-31T11:34:13Z
depth: standard
files_reviewed: 6
files_reviewed_list:
  - libs/monitor_ownership.h
  - libs/monitor_ownership.c
  - tests/test_monitor_ownership.c
  - fvwm/ewmh.c
  - fvwm/move_resize.c
  - fvwm/placement.c
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
status: clean
---

# Phase 03: Code Review Report

**Reviewed:** 2026-08-31T11:34:13Z
**Depth:** standard
**Files Reviewed:** 6
**Status:** clean

## Narrative Findings (AI reviewer)

## Summary

Standard review of the 03-05 GEO-05 gap-closure sources. Checked inset derivation, paged-rect copy, and the three production consumers against double-counted `si` origins, negative-origin arithmetic, and integer overflow.

`monitor_ownership_work_insets` derives left/top as work origin minus `si` origin and right/bottom as monitor far edge minus work far edge, all on `__int128` with `INT_MIN`/`INT_MAX` rejection and zeroed outputs on failure. Negative derived insets are allowed as specified. `monitor_ownership_work_rect_paged` adds only the page offset to the stored root-pixel origin (page `0,0` keeps `x=-1856`; `page_x=1920` yields `x=64`) and does not add `si->x` again.

`get_ewmhc_boundingbox` and `EWMH_GetStrutIntersection` consume those insets, so `move_to_next_monitor`'s `si->x + r.left` lands on the work-area origin. `shuffle_win_to_closest` replaces `wa` with the paged stored rect only on `MONITOR_OWNERSHIP_OK`, leaving the monitor+page rectangle on failure. Minoverlap working-area edges are `page_p1` plus derived insets (and work width/height), including the Y far edge which no longer uses `screen_g.y + ewmh_working_area.y`. Native fixtures cover `si->x=-1920` and `si->x=1920`, null-arg zeroing, and paged overflow.

All reviewed files meet quality standards. No issues found.

---

_Reviewed: 2026-08-31T11:34:13Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
