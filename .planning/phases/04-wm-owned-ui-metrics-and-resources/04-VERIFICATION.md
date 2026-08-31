---
phase: 04-wm-owned-ui-metrics-and-resources
verified: 2026-08-31T20:04:41Z
status: human_needed
score: 17/19 must-haves verified
behavior_unverified: 0
overrides_applied: 0
decision_coverage:
  skipped: true
  reason: "No 04-CONTEXT.md (discuss-phase did not run)"
re_verification:
  previous_status: gaps_found
  previous_score: 14/19
  gaps_closed:
    - "After update_fvwm_monitor(fw) in AddWindow, SelectDecor then setup_window_font/setup_icon_font(..., True) rebind title/icon faces and border/handle/title thickness at the owning-monitor factor."
    - "bind_default_font_at_scale drops the extra FlocaleFont count when AtScale returns the fallback pointer; destroy_*_font still skip Scr.DefaultFont."
    - "Default-config map path no longer keeps the 1x DefaultFont face as the title/icon font once fw->m is known."
  gaps_remaining: []
  regressions: []
behavior_unverified_items: []
human_verification:
  - test: "Write or run a held-out Picture cache lookup that actually calls PCacheFvwmPictureAtScale (or the production walk) at two scale_quarters / target / generation combinations."
    expected: "Same name bytes miss across quarters or generation; matching quarters, target_width, target_height, generation, name, stamp, and fpa_mask hit."
    why_human: "Truth tagged verification: backstop. Native test_picture_lookup_requires_scale_and_generation compares a stack FvwmPicture, not a live cache lookup. Presence of the production compare is not explicit evidence."
  - test: "Write or run a held-out Flocale cache lookup that actually calls FlocaleLoadFontAtScale at two scale_quarters / generation values with identical name bytes."
    expected: "Cache hit requires matching name bytes, scale_quarters, and generation; a quarters or generation mismatch misses."
    why_human: "Truth tagged verification: backstop. Native test_flocale_lookup_requires_scale_and_generation compares a stack FlocaleFont, not a live cache lookup. FlocaleLoadFontAtScale needs a Display, so this Darwin tree cannot observe the behavior."
---

# Phase 4: WM-Owned UI Metrics and Resources Verification Report

**Phase Goal:** All FVWM-owned visuals and interaction regions remain physically usable and internally consistent at every supported monitor scale.
**Verified:** 2026-08-31T20:04:41Z
**Status:** human_needed
**Re-verification:** Yes — after gap-closure plans 04-10, 04-11, and 04-12

Do not trust SUMMARY.md. 04-12 is present in `fvwm/add_window.c`: after `update_fvwm_monitor(fw)` and before `BroadcastConfig(M_ADD_WINDOW, fw)`, AddWindow re-runs `SelectDecor` then `setup_window_font` / `setup_icon_font(..., True)`. `bind_default_font_at_scale` unloads the extra Flocale count when AtScale returns the fallback pointer. 04-11 pan-frame holes remain closed. Two PLAN-tagged `verification: backstop` cache-lookup truths still have no held-out test.

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
| --- | ------- | ---------- | -------------- |
| 1 | Titlebars, borders, title fonts, and window buttons render at the owning monitor's scale. | ✓ VERIFIED | AddWindow binds fonts at 2320-2321 with `False` (placement estimate), then at 2548-2565 `update_fvwm_monitor(fw)` → `SelectDecor(fw, &style, &buttons)` → transient/shaped follow-ups → `setup_window_font(fw, &style, True)` / `setup_icon_font(fw, &style, True)`. `setup_window_font` with `True` calls `destroy_window_font` then `FlocaleLoadFontAtScale` or `bind_default_font_at_scale(Scr.DefaultFont, fw->m)` and `setup_title_geometry`. `SelectDecor` uses `t->m->effective_scale.factor` when `t->m != NULL` (`decorations.c:338-345`). default-config `DefaultFont` at `default-config/config:219` with no WindowFont therefore opens `Scr.DefaultFont->name` at the owning factor before compose. |
| 2 | Menus, icons, iconified-window UI, and move/resize feedback render at the active monitor's scale. | ✓ VERIFIED | Regression: `apply_popup_menu_metrics` in `fvwm/menus.c:2059` still called from popup; icon `PCacheFvwmPictureAtScale` in `fvwm/icons.c:2166`. |
| 3 | Cursors, resize handles, title-button targets, invisible interaction regions, pan zones, edge actions, snapping distances, and drag thresholds are appropriately scaled. | ✓ VERIFIED | **Handles/title-buttons:** post-ownership `SelectDecor` plus `setup_title_geometry` inside the True font setup. **Pan:** `HandlePaging` initializes `edge_thickness` from `mapped_edge_thickness(m, m->virtual_scr.edge_thickness)` (`virtual.c:692-693`) and does not write `virtual_scr.edge_thickness`; `checkPanFrames` writes `last_edge_thickness = source_thickness` (`virtual.c:1185`). `CMD_EdgeThickness` still rejects values outside 0-2. Snap/`MoveThreshold` conversion sites unchanged. Cursors still load at startup from the pointer factor (residual warning, not this gap). |
| 4 | Fonts and source images are scaled exactly once with target-aware caches, and fractional chrome has no clipped text, gaps, stale resources, or displaced cursor hot spots. | ✓ VERIFIED | Picture/Flocale caches still compare scale extras. After ownership, default title/icon bind is AtScale at `fw->m` factor, so compose (`geometry.c:318-321`) sees a scaled `title_font->height` plus `wm_ui_scale_px` extra. 1x AtScale cache-hit of `Scr.DefaultFont` now `FlocaleUnloadFont`s the extra count and still returns fallback (`add_window.c:1760-1763`); destroy/CMD still skip that pointer. |
| 5 | Maintainers can build the supported profile with Xcursor, XRender, PNG, and SVG acceptance paths explicitly enabled. | ✓ VERIFIED | `tests/test_supported_features.c` asserts `HAVE_XCURSOR/HAVE_XRENDER/HAVE_PNG/HAVE_RSVG == 1`. CI meson setup still passes `-Dxcursor=enabled -Dxrender=enabled -Dpng=enabled -Dsvg=enabled`. |
| 6 | Source chrome integers 1, 7, and 13 convert at 1x, 1.25x, 1.5x, 2x, and 4x to the same pixel results as monitor_scale_apply. | ✓ VERIFIED | `test_chrome_sources_match_apply_at_supported_factors` still invoked from `main()`. `wm_ui_scale_px` wraps `monitor_scale_apply` (`libs/wm_ui_metrics.c:57`). |
| 7 | Same cache-key name bytes at two scale_quarters values are unequal; matching quarters, target dimensions, and generation hit; a generation mismatch misses. | ✓ VERIFIED | `test_cache_key_misses_across_quarters_and_generation` still invoked from `main()`. |
| 8 | A NULL converted pointer returns MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT; source 0 stays 0; overflow initializes the output integer to 0. | ✓ VERIFIED | `test_overflow_initializes_output_zero` / `test_edge_zero_stays_zero` still in `main()`. |
| 9 | Cache-key equality is unsigned integer field equality plus the existing exact C-string name walk; it does not use Unicode code points or grapheme clusters. | ✓ VERIFIED | Production walks remain byte `*c1 != *c2` (`Flocale.c:1566-1572`, `Picture.c:123-129`). No grapheme/code-point API added. |
| 10 | A source hot-spot scaled at 2x is monitor_scale_apply then clamped into [0, width-1]. | ✓ VERIFIED | `test_hotspot_scaled_then_clamped` still in `main()`. |
| 11 | Edge source 0 stays 0 at every factor; edge 1 at 2x is 2; MoveThreshold 3 at 1.5x is 5; snap 8 at 2x is 16. | ✓ VERIFIED | Existing fixtures plus `test_paging_edge_source_two_at_2x_is_four` (source 2 at quarters 8 → 4, at quarters 4 → 2). |
| 12 | Explicit title height below the scaled font raises the titlebar to the font; the font is never clipped. | ✓ VERIFIED | `test_explicit_title_below_font_is_raised` and compose helper unchanged. |
| 13 | Picture cache agreement requires matching scale_quarters, target_width, target_height, generation, name, stamp, and fpa_mask. | ⚠️ insufficient_spec | Tagged `verification: backstop` in 04-03-PLAN. Production lookup compares those fields (`Picture.c:132-137`). Native test still compares a stack `FvwmPicture`, not `PCacheFvwmPictureAtScale`. |
| 14 | PCacheFvwmPicture without scale extras still hits generation 1 and MONITOR_SCALE_QUARTERS_MIN so unscaled module callers keep working. | ✓ VERIFIED | Wrapper at `Picture.c:167-175` forwards 1x gen 1, targets 0,0. |
| 15 | Flocale font cache hits require matching name bytes, scale_quarters, and generation. | ⚠️ insufficient_spec | Tagged `verification: backstop` in 04-04-PLAN. Production compare is present (`Flocale.c:1562-1578`). No held-out cache-hit test. |
| 16 | Xft fonts open at an integer pixelsize produced by wm_ui_scale_px from the source font pixel size; display-wide fontconfig DPI is not written. | ✓ VERIFIED | `FftGetFontAtScale` still rewrites `FC_PIXEL_SIZE` from `wm_ui_scale_px` (`Fft.c:182-207`). No `XcursorSetDefaultSize` / display-wide DPI mutation. Residual: default-config `size=8` names still go through this path. |
| 17 | title_thickness and boundary_width remain short actual X11 pixels; g.normal and g.frame client size are not converted. | ✓ VERIFIED | 04-12 does not convert `g.normal` / `g.frame`. No logical-unit fields on `libs/vpacket.h`. |
| 18 | Snap proximity and grid convert from style source integers with the owning-monitor factor at use; MoveThreshold converts from Scr source with the pointer-monitor factor at drag start. | ✓ VERIFIED | `move_resize.c` snap `wm_ui_scale_px`; `events.c:1227` / `functions.c:753` `scaled_move_threshold`. |
| 19 | Manpage documents that listed chrome pixel metrics follow owning or active monitor scale, remain actual X11 pixels, and that display-wide font DPI and cursor default size are not mutated. | ✓ VERIFIED | `doc/fvwm3_manpage_source.adoc:7511-7520`. |

**Score:** 17/19 truths verified (0 present, behavior-unverified; 2 backstop abstentions)

### 04-10 / 04-11 / 04-12 plan must_haves

| Plan | Must-have | Status | Evidence |
| ---- | --------- | ------ | -------- |
| 04-10 | Default title fonts load `Scr.DefaultFont->name` through `FlocaleLoadFontAtScale` at `fw->m->effective_scale.factor` (MIN when `fw->m` is NULL) before `setup_title_geometry`. | ✓ VERIFIED | Helper plus post-ownership True rebind; compose runs inside `setup_window_font` after bind. |
| 04-10 | Default icon fonts use the same AtScale bind; AtScale failure keeps `Scr.DefaultFont`; that pointer is never passed to `FlocaleUnloadFont` from a window or icon slot. | ✓ VERIFIED | `setup_icon_font` else-branch calls the helper. Destroy unloads only when pointer `!= Scr.DefaultFont`. Cache-hit UnloadFont is inside the helper and still returns fallback. |
| 04-10 | `CMD_DefaultFont` still loads the process-global face with `FlocaleLoadFont` and rebinds `USING_DEFAULT_*` windows through `bind_default_font_at_scale` at each owning factor. | ✓ VERIFIED | `fvwm/builtins.c:2449-2477`. |
| 04-11 | `HandlePaging` converts once with `mapped_edge_thickness` and uses that local for every proximity and warp test; `virtual_scr.edge_thickness` stays the 0-2 command source. | ✓ VERIFIED | `virtual.c:692-693`; no `virtual_scr.edge_thickness =` in `HandlePaging`. `monitor_get_all_widths` remains. |
| 04-11 | After mapping, `checkPanFrames` writes `m->virtual_scr.last_edge_thickness = source_thickness`, never mapped pixels. | ✓ VERIFIED | `virtual.c:1185`. |
| 04-11 | Native wm-ui-metrics asserts EdgeThickness source 2 at 2x is 4 and source 2 at 1x is 2. | ✓ VERIFIED | `tests/test_wm_ui_metrics.c:211-220` and `main()` at 428. Binary not present in this Darwin tree. |
| 04-12 | After `update_fvwm_monitor(fw)` in AddWindow, `setup_window_font` / `setup_icon_font(..., True)` destroy and rebind USING_DEFAULT_* and explicit WindowFont/IconFont slots at owning factor. | ✓ VERIFIED | Span 2548-2565; `do_destroy` True always, not gated on `USING_DEFAULT_*`. |
| 04-12 | After that ownership assignment, AddWindow re-runs `SelectDecor` then the True font setup so border, handle, and title thickness match the owning factor. | ✓ VERIFIED | `SelectDecor` before `setup_window_font` in the same span; transient and `wShaped` follow-ups re-applied. |
| 04-12 | When `bind_default_font_at_scale` cache-hits the fallback pointer, it `FlocaleUnloadFont`s that pointer and still returns fallback; destroy/CMD still skip `Scr.DefaultFont`. | ✓ VERIFIED | `add_window.c:1760-1763`; destroy 520-523 and 537-540. |

### Required Artifacts

| Artifact | Expected | Status | Details |
| -------- | ----------- | ------ | ------- |
| `fvwm/add_window.c` | Post-ownership SelectDecor plus True font rebind; DefaultFont cache-hit count drop | ✓ VERIFIED | gsd `verify.artifacts` 04-12-PLAN: exists, no stub issues. Substantive bind/rebind, not a placeholder. Wired from AddWindow. |
| `fvwm/add_window.h` | `bind_default_font_at_scale` | ✓ VERIFIED | Declared at lines 39-40. |
| `fvwm/builtins.c` | `CMD_DefaultFont` rebinds at owning scale | ✓ VERIFIED | Uses `t->m` after ownership exists. |
| `fvwm/virtual.c` | HandlePaging mapped thickness; last_edge_thickness source write-back | ✓ VERIFIED | 04-11 holes remain closed. |
| `tests/test_wm_ui_metrics.c` | `test_paging_edge_source_two_at_2x_is_four` | ✓ VERIFIED | Defined and called from `main()`. |
| Prior 04-01..04-09 artifacts | Converter, caches, CI, menus, icons, cursors, snap, manpage | ✓ VERIFIED | Existence regression; no deletions. |

### Key Link Verification

| From | To | Via | Status | Details |
| ---- | --- | --- | ------ | ------- |
| `fvwm/add_window.c` | `fvwm/update.c` | AddWindow calls `update_fvwm_monitor` then rebinds chrome | ✓ WIRED | gsd `verify.key-links` + source span 2548-2565. |
| `fvwm/add_window.c` | `fvwm/decorations.c` | AddWindow re-calls `SelectDecor` after `fw->m` is set | ✓ WIRED | `SelectDecor(fw, &style, &buttons)` immediately after `update_fvwm_monitor`. |
| `fvwm/add_window.c` | `libs/Flocale.c` | `bind_default_font_at_scale` drops extra count on fallback cache-hit | ✓ WIRED | `FlocaleLoadFontAtScale` then `FlocaleUnloadFont` when `loaded == fallback`. |
| `fvwm/add_window.c` | `fvwm/geometry.c` | `setup_window_font` → `setup_title_geometry` after bind | ✓ WIRED | Order inside `setup_window_font` unchanged; True path now has `fw->m`. |
| `fvwm/builtins.c` | `fvwm/add_window.h` | `CMD_DefaultFont` → `bind_default_font_at_scale` | ✓ WIRED | |
| `fvwm/virtual.c` | `mapped_edge_thickness` | HandlePaging local | ✓ WIRED | |
| `fvwm/virtual.c` | `libs/FScreen.h` | `last_edge_thickness = source_thickness` | ✓ WIRED | |
| Prior 04-01..04-09 links | — | — | ✓ WIRED | Regression: converter, Picture, Flocale, menus, snap still connected. |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
| -------- | ------------- | ------ | ------------------ | ------ |
| `fvwm/add_window.c` | `fw->title_font` / `fw->icon_font` | `FlocaleLoadFontAtScale` / `bind_default_font_at_scale` after `update_fvwm_monitor` | Owning-monitor factor on the map path | ✓ FLOWING |
| `fvwm/geometry.c` | `title_thickness` | compose(`title_font->height`, extra, explicit, `fw->m` factor) | True font setup runs with `fw->m` set | ✓ FLOWING |
| `fvwm/decorations.c` | `border_width` / `handle_width` | Style source → clamp → `wm_ui_scale_px(t->m)` | Second `SelectDecor` sees non-NULL `t->m` | ✓ FLOWING |
| `fvwm/add_window.c` | `fw->mini_icon` | AtScale after placement | Placement assigns `fw->m` (`placement.c:1842-1843` / `2027-2028`) before `setup_mini_icon` at 2519 | ✓ FLOWING |
| `fvwm/virtual.c` | HandlePaging `edge_thickness` | `mapped_edge_thickness(m, source)` | Mapped pixels | ✓ FLOWING |
| `fvwm/virtual.c` | `last_edge_thickness` | `source_thickness` (0-2) | Source integer write-back | ✓ FLOWING |
| `fvwm/builtins.c` | `USING_DEFAULT_*` fonts | `bind_default_font_at_scale(Scr.DefaultFont, t->m)` | Owning factor | ✓ FLOWING |

Frame X windows are still first sized by `frame_move_resize` at 2538 (before the 04-12 rebind). 04-12 explicitly did not re-run that call (Phase 5 `g.normal` / `g.frame`). Title windows are created 1x1 (`setup_title_window`) and get size from `frame_move_resize`. This is an explicit Phase 5 handoff, not a remaining Phase 4 must-have failure: fonts and chrome *fields* used for later draw/compose are owning-scale.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| -------- | ------- | ------ | ------ |
| 04-12 AddWindow span source-contract | python3 path check on AddWindow after `update_fvwm_monitor` before `BroadcastConfig(M_ADD_WINDOW` | SelectDecor, True font/icon setup, SelectDecor before font, transient/shaped follow-ups | ✓ PASS |
| 04-12 cache-hit UnloadFont source-contract | python3 on `bind_default_font_at_scale` / destroy_*_font | LoadAtScale, UnloadFont on `loaded == fallback`, destroy skips `Scr.DefaultFont` | ✓ PASS |
| 04-11 HandlePaging / last_edge_thickness | python3 on `virtual.c` | mapped_edge_thickness, no assign into `virtual_scr.edge_thickness`, `last_edge_thickness = source_thickness`, EdgeThickness 0-2 message | ✓ PASS |
| wm-ui-metrics binary | `builddir/tests/test-wm-ui-metrics` | Not present in this Darwin tree | ? SKIP |
| Linux `meson test wm-ui-metrics` | — | Not run (Darwin; do not claim CI) | ? SKIP |

Did not compile or run the Meson suite.

### Probe Execution

No `scripts/*/tests/probe-*.sh` and no PLAN-declared probe scripts. Step 7c skipped.

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
| ----------- | ---------- | ----------- | ------ | -------- |
| UI-01 | 04-01, 04-05, 04-09, 04-10, 04-12 | Titlebars, borders, title fonts, window buttons at owning scale | ✓ SATISFIED | Post-ownership SelectDecor + True font rebind. |
| UI-02 | 04-01, 04-03, 04-04, 04-09 | Scale-once fonts/images; caches by scale/size/generation | ? NEEDS HUMAN | Caches present and wired. Flocale/Picture lookup truths abstained (backstop). |
| UI-03 | 04-01, 04-03, 04-04, 04-09 | Menus, icons, iconified UI, move/resize feedback at active scale | ✓ SATISFIED | Popup/icon/outline wiring unchanged. Visual mixed-DPI is Phase 8 (TEST-02). REQUIREMENTS.md traceability still says Gaps Found — stale vs code. |
| UI-04 | 04-01, 04-07, 04-12 | Cursors, handles, title-button targets, invisible regions | ✓ SATISFIED | Handles/title-buttons follow post-ownership SelectDecor. Cursors still startup-only (warning). |
| UI-05 | 04-01, 04-08, 04-11 | Pan zones, edge actions, snap, drag thresholds | ✓ SATISFIED | 04-11 remains closed. Snap/MoveThreshold wired. REQUIREMENTS.md traceability still says Gaps Found — stale vs code. |
| UI-06 | 04-01, 04-05, 04-07, 04-10, 04-12 | No clip/gaps/stale resources/hot-spot displacement | ✓ SATISFIED | Default font at owning scale before compose; cache-hit count drop. |
| TEST-05 | 04-02 | Supported profile enables Xcursor, XRender, PNG, SVG | ✓ SATISFIED | Test + CI flags. REQUIREMENTS.md traceability still says Gaps Found — stale vs code. |

No orphaned Phase 4 IDs in REQUIREMENTS.md. GEO-07 remains out of this phase.

### Decision Coverage

Skipped — no `04-CONTEXT.md`. Inherited locks (actual X11 pixels, no display-wide DPI, no module scale keys) remain honored: no `XcursorSetDefaultSize`, no `vpacket.h` scale fields, 04-12 does not convert `g.normal` / `g.frame`.

### Test Quality Audit

| Test File | Linked Req | Active | Skipped | Circular | Assertion Level | Verdict |
|-----------|-----------|--------|---------|----------|-----------------|---------|
| `tests/test_wm_ui_metrics.c` | UI-01..UI-06, UI-05 paging | 19 | 0 | no | Value for converters including source-2-at-2x; existence-only for Picture/Flocale/Fft/PImage | WARNING — cache lookup not exercised (feeds backstop abstention) |
| `tests/test_supported_features.c` | TEST-05 | 1 | 0 | no | Value (`HAVE_* == 1`) | OK |

**Disabled tests on requirements:** 0
**Circular patterns detected:** 0
**Insufficient assertions:** 2 (picture/flocale “lookup” tests still compare struct fields / function pointers)

`test_paging_edge_source_two_at_2x_is_four` proves the converter, not `HandlePaging` itself. HandlePaging is verified from the production source walk. 04-12 forbade new test symbols (`FlocaleLoadFontAtScale` needs a Display).

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| ---- | ---- | ------- | -------- | ------ |
| `fvwm/add_window.c` | 2538 vs 2548-2565 | `frame_move_resize` before chrome rebind | ℹ️ Info | Explicit 04-12 / Phase 5 handoff; chrome *fields* rebound; frame X geometry stays the pre-ownership estimate |
| `fvwm/menus.c` | 2000-2026 | Popup AtScale rebind increments cache count every show | ⚠️ Warning | 04-REVIEW WR-01 (menus); out of 04-12 scope; menus still scale |
| `fvwm/menus.c` | DestroyMenu | Scaled popup fonts not unloaded | ⚠️ Warning | 04-REVIEW WR-02; leak until process exit |
| `fvwm/icons.c` | 2166-2187 | AtScale picture IDs copied, handle discarded | ⚠️ Warning | Prior WR-05 / 04-REVIEW WR-04; out of 04-12 scope |
| `fvwm/cursor.c` | — | `cursor_table` quarters never read after startup | ⚠️ Warning | Prior WR-04 / 04-REVIEW WR-05; startup size only |
| `libs/Fft.c` | 182-208 | Scale `FC_PIXEL_SIZE` after name parse | ⚠️ Warning | default-config `size=8` still goes through AtScale |
| Dated `FIXME` in unmodified-this-pass files (`virtual.c`, `builtins.c`, …) | — | Historical | ℹ️ Info | Not new 04-12 debt |

No unreferenced `TBD`/`FIXME`/`XXX` in `fvwm/add_window.c`. `XcursorSetDefaultSize` remains absent (prohibition held). Prior AddWindow font-before-ownership and DefaultFont count-leak blockers are closed.

### Backstop Abstention

Truths 13 and 15 remain `verification: backstop` without a passing lookup test. Interactive disposition: unverified — held-out test recommended (`reason: insufficient_spec`). Not scored as FAILED and not silently passed. These two items are the reason overall status is `human_needed` rather than `passed`.

### Unverified prohibitions

PLAN frontmatter lists judgment-tier prohibitions with `status: unresolved` / `verification: null`. Not a silent pass. LLM-judge (non-authoritative): converters still take retained source integers; EdgeThickness command still stores 0-2; `last_edge_thickness` stores source; cache-hit UnloadFont still returns fallback and destroy still skips `Scr.DefaultFont`; no display-wide cursor default size; 04-12 did not edit `virtual.c` or convert `g.normal` / `g.frame`. Human review recommended for the remaining judgment-tier items.

### Human Verification Required

### 1. Picture cache held-out lookup

**Test:** Write or run a held-out Picture cache lookup that actually calls `PCacheFvwmPictureAtScale` (or the production walk) at two `scale_quarters` / target / generation combinations.
**Expected:** Same name bytes miss across quarters or generation; matching quarters, `target_width`, `target_height`, generation, name, stamp, and `fpa_mask` hit.
**Why human:** Truth tagged `verification: backstop`. Native test compares a stack `FvwmPicture`, not a live cache lookup.

### 2. Flocale cache held-out lookup

**Test:** Write or run a held-out Flocale cache lookup that actually calls `FlocaleLoadFontAtScale` at two `scale_quarters` / generation values with identical name bytes.
**Expected:** Cache hit requires matching name bytes, `scale_quarters`, and generation; a quarters or generation mismatch misses.
**Why human:** Truth tagged `verification: backstop`. Native test compares a stack `FlocaleFont`. `FlocaleLoadFontAtScale` needs a Display.

Visual mixed-DPI title/menu/cursor rendering remains nested-X work (TEST-02, Phase 8), not this phase's UAT.

### Gaps Summary

No remaining Phase 4 must-have failures. 04-10 shipped the default-font helper; 04-11 closed pan-frame proximity and `last_edge_thickness` source write-back; 04-12 closed AddWindow ownership order (CR-01) and the DefaultFont cache-hit count leak.

Two non-inferable cache-lookup truths stay abstained until a held-out test exists. That is an evidence gap, not a missing implementation.

---

_Verified: 2026-08-31T20:04:41Z_
_Verifier: the agent (gsd-verifier)_
