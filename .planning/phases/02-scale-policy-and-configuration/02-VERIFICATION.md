---
phase: 02-scale-policy-and-configuration
verified: 2026-08-30T23:17:21Z
status: passed
score: 21/21 must-haves verified
behavior_unverified: 0
overrides_applied: 0
decision_coverage:
  honored: 15
  total: 15
  not_honored: []
---

# Phase 2: Scale Policy and Configuration Verification Report

**Phase Goal:** Every active monitor has one explainable, safe, and exact effective scale before any geometry or rendering consumer uses it.
**Verified:** 2026-08-30T23:17:21Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

The production path attaches a total `monitor_scale_result` to every staged `struct monitor` before publication (`SAFE_INIT` 1x), then replaces it with override / automatic / fallback / safe 1x at initial finalization and on changed topology — before EWMH, broadcast, pan-frame, or Status consumers run. `PrintInfo Scale` reads that stored result. Native tests and the Xorg-dummy live-X matrix exercise the contract.

### Observable Truths

| # | Truth | Status | Evidence |
| --- | ------- | ---------- | -------------- |
| 1 | User can assign a rational scale to a uniquely named monitor and see it take precedence over automatic policy. | ✓ VERIFIED | `CMD_MonitorScale` + `monitor_scale_config_associate_monitor()` pass a unique override into `monitor_scale_resolve()`, which returns `MONITOR_SCALE_SOURCE_OVERRIDE` before automatic DPI. Native: `test_association_resolves_every_source_and_retains_rejection_metadata`, `test_resolver_precedence_and_rejected_auto_metadata`. Live-X: `exact-override` expects factor `1.25` / source `override` on DUMMY0 whose automatic candidate is 2x. |
| 2 | Without an override, valid pixel and millimetre dimensions produce an automatic scale; invalid data follows the configured global fallback and then safe 1x chain. | ✓ VERIFIED | `monitor_scale_resolve()` order: valid auto, else fallback, else `MONITOR_SCALE_RESULT_SAFE_INIT`. Tests cover 1920x1080/254x143 → 2x automatic, invalid axes + fallback, invalid axes + safe 1x. Live-X cases `valid-automatic`, `invalid-dpi-fallback`, `invalid-dpi-safe`. |
| 3 | Invalid scale input or ambiguous monitor matching produces a visible diagnostic with the chosen fallback rather than silent coercion. | ✓ VERIFIED | Invalid factor/arity diagnose and leave the store unchanged (`test_invalid_and_allocation_failed_replacements_retain_store`, command-callback tests). Ambiguous match sets `MONITOR_SCALE_MATCH_AMBIGUOUS` and does not apply the override (`test_association_rejects_ambiguous_and_retains_unmatched_overrides`). Live-X `invalid-replacement-retained` asserts diagnostic text and retained `2.5`. |
| 4 | Supported quarter-step factors from 1x through 2x and higher convert deterministically without cumulative rounding drift. | ✓ VERIFIED | 13 canonical factors parse/format in `test_all_canonical_factors_parse_and_format`. `monitor_scale_apply`/`remove` use `__int128` ratio conversion, half-away-from-zero, explicit overflow with zeroed output. `test_repeated_conversions_start_from_the_original_source` applies source `7` 1000 times to the same `9`. |
| 5 | [Probe 2/6 — SCALE-02] Automatic DPI requires both axes in 50..500 inclusive, ≤10% symmetric agreement vs mean, nearest quarter, midpoint ties upward. | ✓ VERIFIED | `automatic_factor()` in `libs/monitor_scale.c`: inclusive 50/500, `20 * \|x_cross-y_cross\| > x_cross+y_cross` rejects, `2*remainder >= denom` increments. Tests: `test_dpi_axis_boundaries_are_exact_and_inclusive`, `test_dpi_agreement_boundary_is_symmetric_and_exact` (95/105 accept, 94/105 reject), `test_every_bucket_midpoint_selects_the_higher_quarter`. |
| 6 | [Probe 5/6 — SCALE-05 boundary] Exact domain is every quarter step from 1x through 4x inclusive; adjacent invalid values rejected. | ✓ VERIFIED | Parser allowlist `"1"`…`"4"`; rejects `"0.75"`, `"4.25"`, `"1.0"`, `"1.50"`, signs, percents, fractions (`test_factor_parser_rejects_noncanonical_and_initializes_output`). 50/500 DPI accepted as bound automatic; 49/501 implausible. |
| 7 | [Probe 6/6 — SCALE-05 precision] Scale stored as quarter units; DPI/bucket/conversion use checked integer arithmetic; signed halves away from zero; overflow explicit; conversions start from original source. | ✓ VERIFIED | `monitor_scale_factor.quarters`; no `float`/`double`/`strtod` in `libs/monitor_scale.c`. `test_apply_and_remove_round_half_away_from_zero`, `test_conversion_limits_overflow_and_invalid_outputs` (INT_MAX*5/4 overflow → 0). |
| 8 | Every staged active runtime monitor has a total initialized effective-scale result before publication. | ✓ VERIFIED | `legacy_monitor_initialize()` assigns `MONITOR_SCALE_RESULT_SAFE_INIT` in `libs/FScreen.c`. `tests/test_fscreen_topology.c` `assert_safe_effective_scale` on staging success and failure retention. Topology equality/epoch unchanged (no scale in `monitor_topology_snapshot_equal`). |
| 9 | [Probe 1/6 — SCALE-01] `MonitorScale <exact-name> <factor>` stores a canonical exact rational for a case-sensitive RandR name; last valid wins; zero matches retained; more than one active match rejected. | ✓ VERIFIED | `monitor_scale_config_declare_override()` + `strcmp` match counting. Tests: sequential replace `DP-1` 1.25→1.75, `dp-1` distinct, 0/1/>1 outcomes (`test_valid_sequential_declarations_and_exact_matching`, `test_zero_unique_and_ambiguous_active_match_outcomes`). |
| 10 | After malformed, unsupported, missing, extra-argument, or allocation-failed declaration, store lookup returns the same last-valid override or fallback bytes. | ✓ VERIFIED | Digest-before/after assertions in `test_invalid_and_allocation_failed_replacements_retain_store` and command-callback arity tests. Injected allocator can fail every candidate allocation. |
| 11 | `MonitorScaleFallback <factor>` stores one valid global factor independently from named overrides, same D-01/D-12 grammar. | ✓ VERIFIED | `monitor_scale_config_declare_fallback()`; `test_fallback_replacement_is_independent_and_transactional`; invalid `"2.50"` retains prior fallback. |
| 12 | [Probe 3/6 — SCALE-03] Every active monitor resolves exactly once per initialization/recompute pass in precedence: unique override, valid automatic, valid fallback, then safe 1x; invalid/ambiguous higher inputs do not suppress lower sources. | ✓ VERIFIED | `associate_monitor` only passes `MONITOR_SCALE_MATCH_UNIQUE` as override; otherwise `monitor_scale_resolve` continues. Matrix in `test_association_resolves_every_source_and_retains_rejection_metadata` and ambiguous/unmatched tests. |
| 13 | Initial configuration finalizes all active results before normal event processing; every changed topology commit recomputes after FScreen publication but before EWMH, broadcast, pan-frame, or Status. | ✓ VERIFIED | `fvwm3.c`: `initialize()` immediately after `InitVariables()`, `finalize_initial()` immediately after startup `exc_destroy_context(exc)`, before `HandleEvents`. `events.c` `topology_refresh_if_due` calls `FScreenRefreshTopology` then `fvwm_topology_dispatch_refresh_result`. Native `test_changed_orders_each_downstream_helper_once` asserts trace `rebps` and zero callbacks for unchanged/failed. `test_finalize_owns_one_store_and_rejects_late_mutation`. |
| 14 | A recompute completes as one bounded pass over the committed in-memory snapshot, configuration, and runtime monitor association. | ✓ VERIFIED | `recompute_with()` iterates `snapshot->monitor_count` and `unique_runtime_monitor()`; no X query, no allocation in the recompute loop. `test_recompute_is_allocation_free_total_and_idempotent`. |
| 15 | Retained unmatched overrides are reconsidered when the exact monitor name appears in a later changed topology; topology equality and epoch remain Phase 1 facts. | ✓ VERIFIED | Overrides stay in the linked store; recompute rematches by exact name. `test_association_rejects_ambiguous_and_retains_unmatched_overrides` keeps `docked`. PrintInfo unmatched lines. No scale field on `monitor_topology_snapshot`. |
| 16 | [Probe 4/6 — SCALE-04] `PrintInfo Scale` and immediate diagnostics report encoded name, exact factor, selected source, raw pixel/mm, automatic validation/rejection, endpoint-bound state, fallback reason, ambiguity, and retained unmatched overrides. | ✓ VERIFIED | `print_monitor_scale()` / `print_unmatched_overrides()`; `CMD_PrintInfo` `StrEquals(subject, "Scale")`. `test_print_info_emits_authoritative_reason_complete_escaped_output` asserts 10 exact lines including crafted `"quote\"line\n…"` with no extra physical newlines. |
| 17 | Endpoint-bound automatic results are described as valid automatic selections; fallback and safe 1x accurately state why automatic policy failed. | ✓ VERIFIED | Bound results keep `source=automatic` with `automatic-bound-low/high`. Fallback/safe lines keep `auto_status=missing-axis` (or implausible) plus selected source. Native print-info expected strings; live-X `automatic-low-bound` / `automatic-high-bound` / `invalid-dpi-*`. |
| 18 | The manual documents exact directive grammar, precedence, two-axis DPI policy, boundaries, rounding, overflow, initial-only lifecycle, inspection output, and Status/module observability boundary. | ✓ VERIFIED | `doc/fvwm3_manpage_source.adoc` `MonitorScale` / `MonitorScaleFallback` / `PrintInfo Scale` (approx. lines 7439–7500, 8253). States 1..4 canonical spellings, `20 * abs(dpi_x - dpi_y) <= dpi_x + dpi_y`, midpoint-up, initial-only, no Status v4 / module scale keys. |
| 19 | An authoritative Xorg-dummy startup matrix proves override, valid automatic, invalid DPI + fallback, invalid DPI + safe 1x, invalid replacement retention, endpoint-bound automatic, and retained unmatched override through `PrintInfo Scale` plus diagnostics. | ✓ VERIFIED | `scale_policy_cases()` in `tests/live_x/live_x_harness.py` enumerates those cases; `run_xorg_dummy_scale_policy()` queries WIDTH_MM/HEIGHT_MM then waits for scheduled `PrintInfo Scale`. Meson target `scale-policy-xorg-dummy` (nonparallel, 90s, `suite: live-x`). Orchestrator regression 2026-08-30: 17/17 meson tests including this target. Harness `--self-test` exit 0 on this host. |
| 20 | Scenario-owned startup directives execute as separate argv `-c` pairs, including names with spaces and quotes; `add_post_finalize_scale_inspection()` appends `-c`, `Schedule 0 PrintInfo Scale`. | ✓ VERIFIED | `fvwm_arguments(..., extra_commands=())` extends `("-c", command)`. `add_post_finalize_scale_inspection` appends exactly those two elements. `test_fvwm_arguments_extra_commands` / `test_post_finalize_scale_inspection_argv` run in harness self-test. |
| 21 | The complete native and live-X suites validate the new scale inspection path against the established Status v4 topology and module-packet baselines. | ✓ VERIFIED | `fvwm/status_json.c` still emits `"version", 4` with no scale keys. No `effective_scale` / `MonitorScale` in `libs/Module.h`. Live-X scale scenario still waits for Status monitors + `assert_print_info_topology` before scale assertions. `LIVE_X_TEST_NAMES` includes `scale-policy-xorg-dummy`. |

**Score:** 21/21 truths verified (0 present, behavior-unverified)

### Required Artifacts

| Artifact | Expected | Status | Details |
| -------- | ----------- | ------ | ------- |
| `libs/monitor_scale.h` | Exact factor/source/result/parser/resolver/conversion contracts | ✓ VERIFIED | 100 lines; quarters 4..16; `MONITOR_SCALE_RESULT_SAFE_INIT`; `monitor_scale_resolve` over `monitor_topology_monitor` |
| `libs/monitor_scale.c` | Allocation-free exact scale arithmetic and automatic DPI | ✓ VERIFIED | 267 lines; finite grammar; `__int128` DPI and conversion; no X/alloc/float |
| `libs/FScreen.h` | Direct `effective_scale` on `struct monitor` | ✓ VERIFIED | `monitor_scale_result effective_scale` at line 104 |
| `libs/FScreen.c` | SAFE_INIT before staged publication | ✓ VERIFIED | `legacy_monitor_initialize` assigns `MONITOR_SCALE_RESULT_SAFE_INIT` |
| `tests/test_monitor_scale.c` | Exhaustive native policy boundary and precision coverage | ✓ VERIFIED | 12 named tests invoked from `main()`; meson `monitor-scale` with `-UNDEBUG` |
| `fvwm/monitor_scale_config.h` | Core-owned config lifecycle and command contracts | ✓ VERIFIED | Declare/match/associate/recompute/finalize/print_info |
| `fvwm/monitor_scale_config.c` | Transactional store, coordinator, PrintInfo, commands | ✓ VERIFIED | 846 lines; file-private `monitor_scale_config_state`; `CMD_MonitorScale*` |
| `tests/test_monitor_scale_config.c` | Native sequential/retention/match/print/lifecycle coverage | ✓ VERIFIED | 12 named tests; meson `monitor-scale-config` |
| `fvwm/fvwm3.c` | Initialize / finalize / shutdown at locked boundaries | ✓ VERIFIED | After `InitVariables`, after startup `exc_destroy_context`, in `Done` before `EWMH_ExitStuff` |
| `fvwm/events.c` | Changed-topology scale recompute before downstream | ✓ VERIFIED | `topology_recompute_scale` → dispatcher |
| `fvwm/topology_event_adapter.h` / `.c` | Injected changed-refresh coordinator | ✓ VERIFIED | `fvwm_topology_downstream_callbacks` + `fvwm_topology_dispatch_refresh_result` |
| `tests/test_topology_event_adapter.c` | Exactly-once scale-before-EWMH/broadcast/pan/status | ✓ VERIFIED | Trace `rebps`; unchanged/failed → 0 callbacks |
| `fvwm/builtins.c` | Case-insensitive `PrintInfo Scale` | ✓ VERIFIED | `StrEquals(subject, "Scale")` → `monitor_scale_config_print_info()` |
| `doc/fvwm3_manpage_source.adoc` | User-visible MonitorScale / fallback / PrintInfo Scale | ✓ VERIFIED | Grammar, precedence, DPI, lifecycle, Status v4 boundary |
| `tests/live_x/live_x_harness.py` | Extra startup commands and Xorg-dummy scale-policy scenario | ✓ VERIFIED | `fvwm_arguments`, `add_post_finalize_scale_inspection`, `run_xorg_dummy_scale_policy`; `--self-test` exit 0 |
| `tests/meson.build` | `monitor-scale`, `monitor-scale-config`, `scale-policy-xorg-dummy` | ✓ VERIFIED | Live-X target inside `live-x-tests` Linux guard, `is_parallel: false` |
| `tests/run_in_linux.py` | Live-X manifest includes scale-policy-xorg-dummy | ✓ VERIFIED | First name in `LIVE_X_TEST_NAMES` |
| `fvwm/commands.h` / `fvwm/functable.c` / `fvwm/meson.build` | Command table + singular production link | ✓ VERIFIED | `monitorscale` / `monitorscalefallback` alphabetically between `moduletimeout` and `mouse`; `fvwm/monitor_scale_config.c` once in `fvwm_sources` |

**Artifacts:** 18/18 verified (union of plan must_haves plus production linkage files)

### Key Link Verification

| From | To | Via | Status | Details |
| ---- | --- | --- | ------ | ------- |
| `libs/monitor_scale.c` | `libs/monitor_topology.h` | `monitor_scale_resolve` consumes `monitor_topology_monitor` | ✓ WIRED | Pattern found by gsd-tools |
| `libs/FScreen.c` | `libs/monitor_scale.h` | `legacy_monitor_initialize` assigns `MONITOR_SCALE_RESULT_SAFE_INIT` | ✓ WIRED | Split across two lines; gsd-tools single-line pattern missed; assignment at `FScreen.c:910-911` |
| `fvwm/functable.c` | `fvwm/monitor_scale_config.c` | lowercase `CMD_ENT` rows | ✓ WIRED | `monitorscale` / `monitorscalefallback` |
| `fvwm/monitor_scale_config.c` | `libs/monitor_scale.h` | `monitor_scale_parse_factor` in both command helpers | ✓ WIRED | |
| `fvwm/monitor_scale_config.c` | `libs/FScreen.h` | snapshot facts → `struct monitor.effective_scale` via `monitor_scale_resolve` | ✓ WIRED | Split assignment `effective_scale = monitor_scale_resolve(...)` at `monitor_scale_config.c:360-361` |
| `fvwm/events.c` | `fvwm/topology_event_adapter.c` | `fvwm_topology_dispatch_refresh_result` | ✓ WIRED | `events.c:1747` |
| `fvwm/fvwm3.c` | `fvwm/monitor_scale_config.c` | initialize / finalize_initial / shutdown | ✓ WIRED | `fvwm3.c:2280`, `2382`, `539` |
| `fvwm/builtins.c` | `fvwm/monitor_scale_config.h` | `CMD_PrintInfo` Scale → `monitor_scale_config_print_info` | ✓ WIRED | Split `StrEquals` then call at `builtins.c:2266-2267` |
| `fvwm/monitor_scale_config.c` | `libs/cJSON.h` | `cJSON_CreateString` name encoding | ✓ WIRED | `monitor_scale_encode_name` |
| `tests/live_x/live_x_harness.py` | `fvwm3 -c` | `extra_commands` as argv pairs | ✓ WIRED | |
| `tests/meson.build` | `tests/live_x/live_x_harness.py` | `scale-policy-xorg-dummy` / `--scenario=scale-policy` | ✓ WIRED | |

**Wiring:** 11/11 connections verified (3 required manual confirmation after gsd-tools split-line false negatives)

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
| -------- | ------------- | ------ | ------------------ | ------ |
| `libs/monitor_scale.c` | `monitor_scale_result` | Committed `monitor_topology_monitor` width/height/mm + optional override/fallback | Yes — exact integer DPI and precedence | ✓ FLOWING |
| `struct monitor.effective_scale` | total result | `legacy_monitor_initialize` SAFE_INIT, then `associate_monitor` → `monitor_scale_resolve` | Yes — written on every staged monitor and every recompute | ✓ FLOWING |
| `PrintInfo Scale` lines | factor/source/reason/raw facts | Reads `monitor->effective_scale` and snapshot facts; does not re-resolve | Yes — print-info test plants stored results and asserts identical output | ✓ FLOWING |
| Live-X expected factor | PrintInfo Scale record | Queried Xorg-dummy Monitor pixels/mm then FVWM inspection after `Schedule 0 PrintInfo Scale` | Yes — `wait_for_authoritative_state` before FVWM, `wait_for_print_info_scale` after | ✓ FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| -------- | ------- | ------ | ------ |
| Harness self-test (argv pairs, scale parser, post-finalize inspection) | `python3 tests/live_x/live_x_harness.py --self-test` | exit 0 | ✓ PASS |
| Native `monitor-scale` / `monitor-scale-config` / `topology-event-adapter` exist as Meson tests | `tests/meson.build` registers executables with `-UNDEBUG` | files + test() entries present | ✓ PASS (existence) |
| Focused named tests exist for precedence, rounding, PrintInfo, ordering | enumeration of `test_*` in the three C tests + adapter | 12 + 12 + ordering tests present | ✓ PASS (existence) |
| Full meson suite including `scale-policy-xorg-dummy` | not re-run here (nested X / Podman; servers forbidden in this pass) | Orchestrator regression 2026-08-30: 17/17 including live-X scale-policy | ? SKIP (external nested-X; prior full run cited) |

### Probe Execution

No `scripts/*/tests/probe-*.sh` and no phase-declared probe scripts. Step 7c skipped.

| Probe | Command | Result | Status |
| ----- | ------- | ------ | ------ |
| — | — | No conventional or phase-declared probes | N/A |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
| ----------- | ---------- | ----------- | ------ | -------- |
| SCALE-01 | 02-02, 02-03, 02-04, 02-05 | User can assign an explicit rational scale to a uniquely named monitor | ✓ SATISFIED | `MonitorScale`, exact-name store, live-X override case, manpage |
| SCALE-02 | 02-01, 02-03, 02-05 | Automatic scale from validated pixel and millimetre dimensions | ✓ SATISFIED | `automatic_factor()`, native DPI tests, live-X `valid-automatic` |
| SCALE-03 | 02-01, 02-03, 02-05 | Precedence: override, validated automatic DPI, configured fallback, safe 1x | ✓ SATISFIED | `monitor_scale_resolve` + associate/recompute matrix + live-X fallback/safe |
| SCALE-04 | 02-02, 02-04, 02-05 | Visible diagnostic on invalid input or matching failure | ✓ SATISFIED | Command diagnostics, PrintInfo Scale, live-X invalid-replacement and unmatched |
| SCALE-05 | 02-01, 02-04, 02-05 | 1x, 1.25x, 1.5x, 1.75x, 2x and higher quarter-steps without rounding drift | ✓ SATISFIED | 13-factor parser, conversion tests, 1000 source-based reps, docs |

REQUIREMENTS.md maps SCALE-01..SCALE-05 to Phase 2 and SCALE-06 to Phase 6. No Phase 2 requirement is missing from plans. No orphaned Phase 2 IDs.

**Coverage:** 5/5 phase requirements satisfied

### Decision Coverage

All trackable CONTEXT.md decisions are honored by shipped artifacts. **15/15 honored**, 0 not honored. Non-blocking gate.

D-01..D-15 appear in `libs/monitor_scale.*`, `fvwm/monitor_scale_config.*`, tests, manpage, and live-X cases (exact quarter domain, signed rounding, overflow, source-based conversion, 96-DPI two-axis policy, inclusive 50..500, 10% agreement, midpoint-up, endpoint-bound automatic, exact-name directives, last-valid retention, unmatched retention, total precedence).

### Test Quality Audit

| Test File | Linked Req | Active | Skipped | Circular | Assertion Level | Verdict |
|-----------|-----------|--------|---------|----------|-----------------|---------|
| `tests/test_monitor_scale.c` | SCALE-02, SCALE-03, SCALE-05 | 12 | 0 | No | Value / behavioral | PASS |
| `tests/test_monitor_scale_config.c` | SCALE-01..SCALE-04 | 12 | 0 | No | Value / behavioral | PASS |
| `tests/test_fscreen_topology.c` | SCALE-03 (SAFE_INIT) | existing + scale asserts | 0 | No | Value | PASS |
| `tests/test_topology_event_adapter.c` | SCALE-03 ordering | `rebps` ordering | 0 | No | Behavioral | PASS |
| `tests/live_x/live_x_harness.py` scale-policy | SCALE-01..05 | 10 isolated cases + self-tests | 0 | No | Value vs queried X facts | PASS |

**Disabled tests on requirements:** 0
**Circular patterns detected:** 0 — expected factors are canonical allowlist strings and independently computed DPI policy, not captured from the SUT
**Insufficient assertions:** 0 — value-level factor/source/reason/match/auto/boundary compares, not mere existence

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| ---- | ---- | ------- | -------- | ------ |
| `fvwm/events.c` | 1697 | `(void)monitor_scale_config_recompute()` | ⚠️ Warning | If `unique_runtime_monitor()` misses, recompute can partially update then still run EWMH/broadcast/pan/status. Success-path FScreen publication pairs facts 1:1, covered by tests. Not a goal miss. |
| `fvwm/fvwm3.c` | 2382 | `(void)monitor_scale_config_finalize_initial()` | ⚠️ Warning | If association is incomplete, `.finalized` stays false so late `MonitorScale` remains accepted. Monitors still have SAFE_INIT. Success path (live-X) finalizes. |
| `libs/monitor_scale.c` | 23 | GNU `__int128` | ℹ️ Info | Exact wide arithmetic; 32-bit/C11-strict ports may need a follow-up. Linux x86_64 CI/live-X path is the v0.1 verification target. |

No `TBD`/`FIXME`/`XXX` in phase-created scale files. Pre-existing `FIXME`/`XXX` in `FScreen.c` / `events.c` / `builtins.c` are outside the scale edits (Chesterton: keep).

**Anti-patterns:** 3 found (0 blockers, 2 warnings, 1 info)

### Prohibitions (judgment-tier, verified by inspection)

Planner left these `verification: null`. They are **not** silent: each must-NOT was checked against the codebase.

| Statement | Verdict | Evidence |
| --------- | ------- | -------- |
| Effective-scale policy must not mutate arbitrary client pixels, global toolkit DPI, RandR output transforms, or topology identity | Holds | No `XRRGetMonitors` / Xft / `XRRSetCrtcTransform` / XSETTINGS in `monitor_scale.c` or `monitor_scale_config.c`. Topology snapshot type unchanged. |
| Scale recomputation must not query X, allocate, touch the filesystem, start a subprocess, sleep, or publish topology | Holds | `recompute_with()` is a snapshot loop + `associate_monitor` + `monitor_scale_resolve`. `malloc` exists only in config allocate and PrintInfo formatting, not recompute. |
| Scale explanations must not conceal a rejected higher-priority source, misrepresent endpoint-bound automatic as fallback, or print a factor/source pair different from the runtime result | Holds | PrintInfo reads `monitor->effective_scale`; tests assert bound-automatic source and preserved `auto_status` on fallback/safe. |
| Live-X scale scenario must not invoke a shell, use a fixed display number, use fixed sleeps, or create a second harness | Holds | Argv lists only; `start_xorg_dummy` + existing `ManagedProcess`; monotonic deadlines; same `live_x_harness.py`. No `shell=True` / `time.sleep` / `DISPLAY=:` in the harness. |
| Phase 2 must not expand or alter Status v4 fields or module packets | Holds | `status_json.c` version 4, no scale keys; no scale symbols in `libs/Module.h`. |

### Human Verification Required

N/A — Foundation/policy phase. User-visible `MonitorScale` / `PrintInfo Scale` output is asserted by native exact-string tests and the Xorg-dummy matrix. No untested user-facing UX remains. No ⚠️ PRESENT_BEHAVIOR_UNVERIFIED truths.

### Confirmation-bias notes (non-blocking)

1. **Partial error path:** discarded `recompute`/`finalize_initial` booleans (warnings above). Happy path and live-X still prove the goal.
2. **Coordinator vs production callback:** ordering is proven on `fvwm_topology_dispatch_refresh_result` with recorders; production wires `monitor_scale_config_recompute` as `recompute_scale`. That is the intended seam, not a hollow test.
3. **1000-rep test** reapplies the same source rather than chaining rounded outputs — which is exactly D-04 (never feed a rounded result forward).

### Gaps Summary

**No gaps found.** Phase goal achieved. Every active monitor has a total, explainable, exact effective scale before geometry/rendering consumers (none of which consume it yet; they are later phases). Ready to proceed.

SCALE-06 live reload remains Phase 6 by roadmap; not a Phase 2 gap.

---

## Verification Metadata

**Verification approach:** Goal-backward from ROADMAP success criteria + all PLAN `must_haves.truths`
**Must-haves source:** ROADMAP.md Phase 2 success criteria (4) merged with PLAN 02-01..02-05 truths (plan may add, not subtract)
**Automated checks:** gsd-tools artifacts 18/18; key-links 8/11 tool-verified + 3/11 manual split-line; harness `--self-test` pass; decision coverage 15/15
**Human checks required:** 0
**Orphaned requirements:** none

---
_Verified: 2026-08-30T23:17:21Z_
_Verifier: the agent (gsd-verifier)_
