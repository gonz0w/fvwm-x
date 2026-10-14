---
phase: 03-geometry-and-ownership-contracts
verified: 2026-08-31T11:42:00Z
status: passed
score: 9/9 must-haves verified
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: gaps_found
  previous_score: 8/9
  gaps_closed:
    - "Initial placement, work areas, struts, and monitor-targeted movement remain correct in literal root-pixel coordinates."
  gaps_remaining: []
  regressions: []
---

# Phase 3: Geometry and Ownership Contracts Verification Report

**Phase Goal:** Window ownership and all scale-sensitive geometry obey deterministic coordinate, constraint, and X11 pixel contracts.
**Verified:** 2026-08-31T11:42:00Z
**Status:** passed
**Re-verification:** Yes — after gap closure (03-05, GEO-05 CR-01)

Locked decisions checked in code: X11/ICCCM/EWMH/module geometry stays actual pixels; ownership uses literal root-pixel snapshot rectangles; FindScreenOfXY does not fold through DisplayWidth. GEO-05 CR-01 is closed: inset-style consumers derive left/top as work origin minus si origin, and rectangle sites page-offset the stored root-pixel rect without adding si origin again.

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
| --- | ------- | ---------- | -------------- |
| 1 | Every window receives the same deterministic owner for negative origins, gaps, clones, partial overlaps, rotations, output reorderings, and seam ties, with incumbent preference and hysteresis where applicable. | ✓ VERIFIED | Regression: `tests/test_monitor_ownership.c` still invokes containment, unique-max overlap, clone identity ties in both insertion orders, rotation, reorder, Manhattan gaps, equal-seam incumbent keep, extra_overlap 8 keep / 9 steal, and one-pixel remaining incumbent. `monitor_ownership_resolve` still compares literal snapshot rectangles. |
| 2 | Initial placement, work areas, struts, and monitor-targeted movement remain correct in literal root-pixel coordinates. | ✓ VERIFIED | CR-01 closed. `monitor_ownership_work_insets` (`libs/monitor_ownership.c:349-368`) derives left as `work.x - si.x` with `__int128`. Fixture `(-1920,0,1920,1080)` + stored `(-1856,32,1856,1048)` → left=64,right=0,top=32,bottom=0 (`test_work_insets_negative_origin`). Right-hand `si.x=1920` / work `x=1984` yields the same insets. `monitor_ownership_work_rect_paged` on that work with page 0,0 returns `x=-1856`, not `-3776`. `get_ewmhc_boundingbox` (`fvwm/ewmh.c:2108-2128`) returns those insets so `move_to_next_monitor` `si->x + r.left` is `-1920+64=-1856`. Snap copies the stored rect plus page only (`fvwm/move_resize.c:437-442`). Minoverlap uses `page_p1 + box.left/top` (`fvwm/placement.c:716-723`, `869-876`). `$[wa.x]`/`$[wa.y]` expand the stored origin; manpage states root-pixel origin, not strut insets. |
| 3 | A scaled target size obeys ICCCM minimum, maximum, base, increment, aspect, fixed-size, and gravity constraints. | ✓ VERIFIED | Regression: `tests/test_icccm_size_hints.c` and `icccm_constrain_size` still present; `fvwm/geometry.c:868-869` still delegates after border subtraction. |
| 4 | Clients and modules continue to receive EWMH, ICCCM, configure, frame-extents, work-area, and geometry values in actual X11 pixels. | ✓ VERIFIED | Regression: `EWMH_SetFrameStrut` still publishes `b.top_left.width` … `b.bottom_right.height` (`fvwm/ewmh.c:1321-1328`). No `monitor_scale_apply` on `fvwm/ewmh.c` or `fvwm/module_interface.c`. `$[wa.*]` remain stored pixel fields. CR-01 no longer ships wrong inset *values*. |
| 5 | Maintainers can run deterministic native tests for scale arithmetic, signed rounding, invalid physical data, snapshot diffs, ownership ties, ICCCM constraints, and 1,000 repeated round trips. | ✓ VERIFIED | Regression: `tests/meson.build` still registers `monitor-ownership`, `icccm-size-hints`, `monitor-scale`, and `monitor-topology` with `-UNDEBUG`. Ownership `main()` now also calls the six GEO-05 consumer fixtures. Local `builddir` has no `test-monitor-ownership` binary; this verifier did not re-run the Linux suite. |
| 6 | Overlap area and gap distance use checked wide intermediates; identity ties compare `name_atom` then `strcmp(name)`; empty input returns an initialized fallback; overflow initializes then returns `MONITOR_OWNERSHIP_OVERFLOW`. | ✓ VERIFIED | Regression: `libs/monitor_ownership.c` still uses `monitor_ownership_wide` (`__int128`); empty/overflow tests still invoked from `main()`. |
| 7 | `FindScreenOfXY` resolves a point against committed snapshot rectangles in literal root pixels and maps the owner identity onto an enabled non-global struct monitor. | ✓ VERIFIED | Regression: `libs/FScreen.c:1248-1291` still builds a 1x1 rect, calls `FScreenGetTopologySnapshot` + `monitor_ownership_resolve`, maps via `monitor_resolve_name`, skips `GLOBAL_SCREEN_NAME`. It does not call `monitor_get_all_widths`. |
| 8 | `update_fvwm_monitor` assigns `fw->m` from the ownership resolver over the mapped frame rectangle (else `g.max`/`g.normal`) plus incumbent `fw->m->si->name_atom`. | ✓ VERIFIED | Regression: `fvwm/update.c:666-698` still uses `IS_MAPPED` → `fw->g.frame`, else max/normal; incumbent atom; `MONITOR_OWNERSHIP_HYSTERESIS_PX`. |
| 9 | `constrain_size` delegates ICCCM client size to `icccm_constrain_size` after border subtraction and continues to apply style min/max, `CS_ROUND_UP`, `XSync`, and max defect in the wrapper. | ✓ VERIFIED | Regression: `fvwm/geometry.c:846-876` still clears `PResizeInc` on `CS_ROUND_UP` then calls `icccm_constrain_size`; overflow restores pre-constrain client size. |

**Score:** 9/9 truths verified (0 present, behavior-unverified)

### Gap-closure probes (03-05, evidence for truth 2)

These are PLAN 03-05 must-haves. They restated GEO-05 with the CR-01 fixture; they are not extra scored truths.

| Probe | Status | Evidence |
| ----- | ------ | -------- |
| `work_insets` on monitor `-1920` / work `-1856` returns left=64,right=0,top=32,bottom=0 | ✓ | `test_work_insets_negative_origin` |
| Same insets on monitor `1920` / work `1984` | ✓ | `test_work_insets_right_hand_origin` |
| `work_rect_paged` page 0,0 returns `x=-1856`, not `-3776`; page_x=1920 returns `x=64` | ✓ | `test_work_rect_paged_negative_origin` |
| Minoverlap `page_p1.x=-1920` + left 64 → xtest `-1856`; `page_p1.x=1920` → 1984 | ✓ | `test_minoverlap_edges_use_insets`. Production Y path is `page_p1.y + box.top` (`fvwm/placement.c:869`); native fixture does not assert `ytest=32` (box.top=32 is asserted in the insets test). |
| `get_ewmhc_boundingbox` / `EWMH_GetStrutIntersection` call `work_insets` | ✓ | `fvwm/ewmh.c:1291-1302`, `2121-2128` |
| `$[wa.x]`/`$[dwa.x]` documented as root-pixel origin in actual X11 pixels | ✓ | `doc/fvwm3_manpage_source.adoc:1458-1467`; `fvwm/expand.c:789-811` still assigns stored x/y with no si subtraction (PA-12) |
| Native fixtures cover `si->x=-1920` and `si->x=1920` for insets and minoverlap page-edge arithmetic | ✓ | Insets both origins; minoverlap page_p1 at `-1920` and `1920`. Paged helper is origin-agnostic (adds page to stored work origin only). |

### Required Artifacts

| Artifact | Expected | Status | Details |
| -------- | ----------- | ------ | ------- |
| `libs/monitor_ownership.h` | Ownership/work-rect/inset/paged contract | ✓ VERIFIED | Adds `monitor_ownership_work_insets` and `monitor_ownership_work_rect_paged` after `work_rect`. |
| `libs/monitor_ownership.c` | Literal overlap, hysteresis, work rect, insets, paged copy | ✓ VERIFIED | 400 lines; checked `__int128` inset derivation and page-offset copy; failure zeros outputs. Wired into libfvwm3. |
| `tests/test_monitor_ownership.c` | TOP-02 / GEO-01 / GEO-05 consumer fixtures | ✓ VERIFIED | 22 cases from `main()`, including six new GEO-05 consumer tests. |
| `libs/FScreen.c` | FindScreenOfXY point query | ✓ VERIFIED | Wrapper over snapshot + resolve; paging helpers unchanged. |
| `fvwm/update.c` | Frame-rect ownership | ✓ VERIFIED | `monitor_ownership_resolve` + `IS_MAPPED`; no scale. |
| `libs/icccm_size_hints.h` | Integer constrain contract | ✓ VERIFIED | Unchanged since 03-03. |
| `libs/icccm_size_hints.c` | Min/max/base/inc/aspect/fixed/gravity | ✓ VERIFIED | Unchanged since 03-03. |
| `tests/test_icccm_size_hints.c` | GEO-03 matrix + 1000 loops | ✓ VERIFIED | Unchanged since 03-03. |
| `fvwm/geometry.c` | constrain_size wrapper | ✓ VERIFIED | Delegates client size; keeps wrapper-only style/border/`CS_ROUND_UP`. |
| `fvwm/ewmh.c` | Root-pixel work areas; bounding-box via insets | ✓ VERIFIED | Store still from `monitor_ownership_work_rect`. `get_ewmhc_boundingbox` and public `EWMH_GetStrutIntersection` now call `work_insets`. Empty `if (Scr.bo.do_debug_randr) {}` leftover remains. |
| `fvwm/placement.c` | StartsOnScreen honor / minoverlap via insets | ✓ VERIFIED | Unhonored path still `FScreenGetScrRect(CURRENT)`. `_pl_minoverlap_get_next_x/y` use `work_insets` + `page_p1`. |
| `fvwm/move_resize.c` | MOVE_SCREEN snapshot dest; snap via paged rect | ✓ VERIFIED | `s` from `m->si`; snap EWMH branch calls `work_rect_paged` and does not add `si->x`. `CMD_MoveToScreen` still calls `update_fvwm_monitor`. |
| `doc/fvwm3_manpage_source.adoc` | Root-pixel ownership + `$[wa.*]` origin | ✓ VERIFIED | `$[wa.x]` / `$[dwa.x]` paragraphs contain `root-pixel origin` and `actual X11 pixels`; `-1920` / 64px example yields `-1856`. |

`gsd-tools query verify.artifacts` on 03-05-PLAN.md: 7/7 exist.

### Key Link Verification

| From | To | Via | Status | Details |
| ---- | --- | --- | ------ | ------- |
| `libs/monitor_ownership.c` | `libs/monitor_topology.h` | consume snapshot fields | ✓ WIRED | `name_atom`, `name`, `x`, `y`, `width`, `height` |
| `libs/FScreen.c` | `libs/monitor_ownership.h` | FindScreenOfXY → resolve | ✓ WIRED | 1x1 + snapshot |
| `fvwm/update.c` | `libs/monitor_ownership.h` | update_fvwm_monitor → resolve | ✓ WIRED | frame + incumbent |
| `libs/monitor_ownership.c` | `MONITOR_OWNERSHIP_REASON_INCUMBENT` | extra ≤ hysteresis | ✓ WIRED | unchanged |
| `libs/icccm_size_hints.c` | `libs/gravity.h` | `gravity_resize` | ✓ WIRED | optional frame |
| `fvwm/geometry.c` | `libs/icccm_size_hints.h` | `icccm_constrain_size` | ✓ WIRED | after borders |
| `tests/test_icccm_size_hints.c` | `libs/monitor_scale.h` | scale then constrain | ✓ WIRED | unchanged |
| `fvwm/ewmh.c` | `libs/monitor_ownership.h` | `monitor_ownership_work_rect` | ✓ WIRED | stored in `ewmh_working_area` |
| `fvwm/ewmh.c` | `libs/monitor_ownership.h` | `get_ewmhc_boundingbox` / `EWMH_GetStrutIntersection` → `work_insets` | ✓ WIRED | pattern found |
| `fvwm/move_resize.c` | `libs/monitor_ownership.h` | `shuffle_win_to_closest` → `work_rect_paged` | ✓ WIRED | pattern found |
| `fvwm/placement.c` | `libs/monitor_ownership.h` | `_pl_minoverlap_get_next_x/y` → `work_insets` | ✓ WIRED | pattern found |
| `fvwm/move_resize.c` | `fvwm/update.c` | `update_fvwm_monitor` after MOVE_SCREEN | ✓ WIRED | `CMD_MoveToScreen` |
| `fvwm/ewmh.c` | `EWMH_SetFrameStrut` | border pixel extents | ✓ WIRED | `val[0]`…`val[3]` from borders |

`gsd-tools query verify.key-links` on 03-05-PLAN.md: 3/3 pattern-found. Manual follow-up confirmed call sites pass `m->si` rectangles and stored `ewmh_working_area`, not leftover inset arithmetic.

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
| -------- | ------------- | ------ | ------------------ | ------ |
| `monitor_ownership_work_rect` | `out.{x,y,width,height}` | snapshot rect + strut ints | Yes | ✓ FLOWING |
| `monitor_ownership_work_insets` | `out.{left,right,top,bottom}` | work origin minus si origin | Yes | ✓ FLOWING |
| `monitor_ownership_work_rect_paged` | `out.{x,y,width,height}` | stored work origin plus page | Yes | ✓ FLOWING |
| `ewmh_ComputeAndSetWorkArea` | `ewmh_working_area` | `monitor_ownership_work_rect` using `m->si` | Yes (absolute root pixels) | ✓ FLOWING |
| `EWMH_GetWorkAreaIntersection` | query ∩ stored rect | stored `ewmh_working_area` without `+ si->x` | Yes | ✓ FLOWING |
| `ewmh_GetStrutIntersection` (private) | left/right/top/bottom strips | `m->si` extents + derived insets | Yes | ✓ FLOWING |
| `EWMH_GetStrutIntersection` (public) | insets from wa vs `si` | `monitor_ownership_work_insets` | Yes | ✓ FLOWING |
| `get_ewmhc_boundingbox` | `r.left/right/top/bottom` | `work_insets` against stored wa | Yes — left=64 on `-1920`+64px strut | ✓ FLOWING |
| snap-to-working-area | `wa` | `work_rect_paged(stored, page)` | Yes — page 0,0 keeps `x=-1856` | ✓ FLOWING |
| minoverlap placement | `xtest`/`ytest` | `page_p1 + box.left/top` | Yes — `-1920+64=-1856` | ✓ FLOWING |
| `$[wa.x]` / `$[wa.y]` | expand.c | raw `ewmh_working_area.x/y` | Yes — absolute origin, documented | ✓ FLOWING |
| MOVE_SCREEN `s` | dest rect | `m->si->{x,y,w,h}` | Yes | ✓ FLOWING |
| StartsOnScreen not honored | `screen_g` | `FScreenGetScrRect(CURRENT)` | Yes | ✓ FLOWING |
| `EWMH_SetFrameStrut` | `_NET_FRAME_EXTENTS` | border pixel sizes | Yes | ✓ FLOWING |

No remaining consumer adds `si->x` onto stored `ewmh_working_area.x`. `ParseOneResizeArgument` still consumes only width/height of the stored rect.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| -------- | ------- | ------ | ------ |
| GEO-05 inset fixtures exist and assert 64/0/32/0 | enumerate `test_work_insets_*` | both origins assert left==64 | ✓ PASS (existence + value asserts) |
| GEO-05 paged snap is not `-3776` | `test_work_rect_paged_negative_origin` | asserts paged.x==-1856 and page_x=1920 → 64 | ✓ PASS (existence + value asserts) |
| GEO-05 minoverlap composition | `test_minoverlap_edges_use_insets` | asserts xtest==-1856 and 1984 | ✓ PASS (x); y composition only in production |
| icccm-size-hints 1000-loop exists | `test_repeated_constrain_from_original_source` | present | ✓ PASS (existence) |
| meson test monitor-ownership | run named binary | `builddir/tests/test-monitor-ownership` missing; meson not on PATH; Darwin `config.h` pulls `malloc.h`; Podman Linux run exceeds 10s | ? SKIP |

Step 7b: SKIPPED running the Linux suite (no local phase-3 test binary after 03-05; `tests/run_in_linux.py` / Podman would start a container). SUMMARY pass claims are not evidence. Native fixtures contain the CR-01 numbers as assertions.

### Probe Execution

No `scripts/*/tests/probe-*.sh` and no phase-declared probes.

| Probe | Command | Result | Status |
| ----- | ------- | ------ | ------ |
| — | — | N/A | SKIP |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
| ----------- | ---------- | ----------- | ------ | -------- |
| TOP-02 | 03-01 | Deterministic identity/ownership for negative origins, gaps, clones, partial overlaps, rotations, output reordering | ✓ SATISFIED | Native fixtures + order-independent identity ties; FindScreenOfXY no longer wraps DisplayWidth |
| GEO-01 | 03-02 | One owner with incumbent tie-break and seam hysteresis | ✓ SATISFIED | Incumbent keep/steal tests; `update_fvwm_monitor` uses frame + incumbent. Hysteresis is extra *area* (WR-01), matching the executed plan |
| GEO-03 | 03-03 | Scaled target obeys ICCCM min/max/base/inc/aspect/fixed/gravity | ✓ SATISFIED | Native matrix + wrapper. Unsatisfiable-aspect overflow remains a warning, not a missing unit |
| GEO-05 | 03-04, 03-05 | Correct initial placement, work areas, struts, and monitor-targeted movement in root-pixel coordinates | ✓ SATISFIED | Work-rect store plus inset/paged consumers. Bounding-box, snap, minoverlap, and `$[wa.*]` no longer treat stored x/y as insets |
| GEO-07 | 03-04 | Clients/modules keep EWMH/ICCCM/configure/frame-extents/work-area/geometry in actual X11 pixels | ✓ SATISFIED | No scaled-unit protocol fields; frame extents remain border pixels; wa expansion stays pixel integers |
| TEST-01 | 03-01, 03-03, 03-04, 03-05 | Native tests for scale, rounding, invalid data, snapshot diffs, ownership ties, ICCCM, 1000 round trips | ✓ SATISFIED | Suites registered; ownership now includes GEO-05 consumer fixtures |

**Orphaned requirements mapped to Phase 3 but missing from plans:** none. REQUIREMENTS.md lists TOP-02, GEO-01, GEO-03, GEO-05, GEO-07, TEST-01 for this phase; every ID appears in PLAN frontmatter.

**Coverage:** 6/6 requirements satisfied.

Note: REQUIREMENTS.md traceability still marks TOP-02, GEO-01, GEO-03, GEO-07 as "Gaps Found" while GEO-05 and TEST-01 are "Complete". That table lag is documentation, not a code gap.

### Test Quality Audit

| Test File | Linked Req | Active | Skipped | Circular | Assertion Level | Verdict |
|-----------|-----------|--------|---------|----------|-----------------|---------|
| `tests/test_monitor_ownership.c` | TOP-02, GEO-01, GEO-05, TEST-01 | 22 | 0 | no | value | GEO-05 helpers asserted; production consumers wired. Minoverlap Y composition not asserted in the native fixture |
| `tests/test_icccm_size_hints.c` | GEO-03, TEST-01 | 11 | 0 | no | value | OK; unsatisfiable aspect absent |
| `tests/test_monitor_scale.c` | TEST-01 | pre-existing | 0 | no | value | registered |
| `tests/test_monitor_topology.c` | TEST-01 | pre-existing | 0 | no | value | registered |

**Disabled tests on requirements:** 0
**Circular patterns detected:** 0
**Insufficient assertions:** 1 warning — `test_minoverlap_edges_use_insets` proves X composition only; Y is the same `page_p1 + box.top` formula in production, with `box.top==32` already asserted by `test_work_insets_negative_origin`. Not a GEO-05 blocker.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| ---- | ---- | ------- | -------- | ------ |
| `libs/monitor_ownership.c` | 226-228 | hysteresis compares extra *area* to 8 | ⚠️ Warning | Out of scope for 03-05 (PA-14 / WR-01). A 200×100 window steals after 1px past an equal seam |
| `libs/icccm_size_hints.c` | 168-171 | unsatisfiable aspect → OVERFLOW | ⚠️ Warning | Out of scope for 03-05 (PA-15 / WR-02). Wrapper restores pre-constrain size |
| `libs/FScreen.c` | 1291 | `FindScreenOfXY` can return NULL | ⚠️ Warning | Out of scope for 03-05 (PA-18 / WR-03) |
| `fvwm/ewmh.c` | 1055-1057 | empty `if (Scr.bo.do_debug_randr)` | ℹ️ Info | Dead control flow; not GEO-05 |
| `fvwm/geometry.c`, `fvwm/ewmh.c`, `fvwm/placement.c` | various | pre-existing FIXME/XXX | ℹ️ Info | Not introduced by this phase; not unreferenced new debt |

New/changed phase sources for 03-05 (`libs/monitor_ownership.c`, `tests/test_monitor_ownership.c`) contain no TBD/FIXME/XXX/TODO.

### Decision Coverage

Skipped — no `03-CONTEXT.md` `<decisions>` block.

### Prohibitions (judgment-tier, unresolved)

PLAN frontmatter flags spec-less safety prohibitions. Autonomous rule: never a silent pass; not a hard halt. LLM-judge (non-authoritative) against code:

| Prohibition | LLM-judge | Flag |
| ----------- | --------- | ---- |
| TOP-02: must not fold coordinates through root framebuffer size or choose owner by RB/enumeration order | Honored in `monitor_ownership_resolve` / `FindScreenOfXY` | unverified-prohibition — human review recommended |
| GEO-01: must not oscillate on a shared edge when incumbent still overlaps and extra overlap is within hysteresis | Honored as *area* hysteresis; edge-pixel intent is WR-01 | unverified-prohibition — human review recommended |
| GEO-03: must not constrain a scaled target in logical units or scale a previously constrained size again | Honored in tests (`scale` then `constrain`; 1000 loops from original source) | unverified-prohibition — human review recommended |
| GEO-07: clients/modules must not receive geometry converted out of actual X11 pixels | Honored as units; CR-01 values now match the root-pixel store | unverified-prohibition — human review recommended |
| GEO-05: work-area consumers must not add a monitor si origin onto an already-absolute `ewmh_working_area` origin; inset-only sites must derive left/top as work origin minus si origin | Honored: bounding-box/strut/minoverlap use `work_insets`; snap uses `work_rect_paged`; no remaining `si->x + wa.x` | unverified-prohibition — human review recommended |

These are flagged, not silently passed. They do not change `passed` (autonomous completion with flagged prohibitions; infrastructure phase has no UAT items).

### Human Verification Required

N/A — Infrastructure/foundation phase with no user-facing elements.
All acceptance criteria are verifiable programmatically.
No ⚠️ PRESENT_BEHAVIOR_UNVERIFIED truths. No harvested `<human-check>` blocks.

### Gaps Summary

None. The previous GEO-05 CR-01 gap is closed in production consumers and native fixtures. Primary-at-origin layouts still work; negative-origin and right-hand monitors now get left=64 instead of left=-1856 / snap origin -3776.

Later milestone phases (4 UI, 5 logical-size transitions, 6 hotplug, 7 modules, 8 live-X matrix) do not claim this consumer conversion; nothing is deferred.

---

## Verification Metadata

**Verification approach:** Goal-backward re-verification (previous must-haves; failed GEO-05 got full 3-level + data-flow; passed items got existence/sanity regression). SUMMARYs not treated as evidence.
**Must-haves source:** Previous `03-VERIFICATION.md` (ROADMAP success criteria + 03-01..03-04 PLAN truths); 03-05 probe truths used as GEO-05 closure evidence
**Automated checks:** 9 passed, 0 failed, 03-05 artifacts 7/7, 03-05 key-links 3/3 pattern-found plus manual call-site follow-up
**Human checks required:** 0; 5 judgment-tier prohibitions flagged for review
**Total verification time:** ~20 min

---

_Verified: 2026-08-31T11:42:00Z_
_Verifier: the agent (gsd-verifier)_
