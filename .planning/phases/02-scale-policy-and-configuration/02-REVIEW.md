---
phase: 02-scale-policy-and-configuration
reviewed: 2026-08-30T23:05:08Z
depth: standard
files_reviewed: 23
files_reviewed_list:
  - doc/fvwm3_manpage_source.adoc
  - fvwm/builtins.c
  - fvwm/commands.h
  - fvwm/events.c
  - fvwm/functable.c
  - fvwm/fvwm3.c
  - fvwm/meson.build
  - fvwm/monitor_scale_config.c
  - fvwm/monitor_scale_config.h
  - fvwm/topology_event_adapter.c
  - fvwm/topology_event_adapter.h
  - libs/FScreen.c
  - libs/FScreen.h
  - libs/meson.build
  - libs/monitor_scale.c
  - libs/monitor_scale.h
  - tests/live_x/live_x_harness.py
  - tests/meson.build
  - tests/run_in_linux.py
  - tests/test_fscreen_topology.c
  - tests/test_monitor_scale.c
  - tests/test_monitor_scale_config.c
  - tests/test_topology_event_adapter.c
findings:
  critical: 0
  warning: 3
  info: 1
  total: 4
status: issues_found
---

# Phase 02: Code Review Report

**Reviewed:** 2026-08-30T23:05:08Z
**Depth:** standard
**Files Reviewed:** 23
**Status:** issues_found

## Narrative Findings (AI reviewer)

## Summary

Phase 02 adds quarter-step scale policy (`libs/monitor_scale.c`), a transactional initial-only config store (`fvwm/monitor_scale_config.c`), scale-first topology downstream dispatch, `PrintInfo Scale`, and live-X coverage. Parser/resolver/conversion math and command arity checks are tight and well tested. The remaining defects are error-handling holes on the production association path and a C11 portability break in the wide-integer DPI path.

## Warnings

### WR-01: Topology scale recompute failure is discarded and partially committed

**File:** `fvwm/events.c:1694-1698`
**Issue:** `topology_recompute_scale()` casts away `monitor_scale_config_recompute()`. On failure, `fvwm_topology_dispatch_refresh_result()` still runs EWMH, broadcast, pan frames, and Status. `recompute_with()` is also not transactional: it associates monitors in snapshot order and returns `false` as soon as `unique_runtime_monitor()` misses, leaving earlier monitors updated and later ones stale, with no diagnostic on the failed identity match (`fvwm/monitor_scale_config.c:670-677`). After a topology CHANGED refresh, effective scale can disagree with the committed snapshot while downstream consumers are told the layout changed.
**Fix:** Fail closed on the downstream path, and either associate into a scratch result then publish or roll back. Log the unmatched identity:

```c
static void
topology_recompute_scale(void *context)
{
	(void)context;
	if (!monitor_scale_config_recompute()) {
		fvwm_debug(__func__,
		    "scale recompute failed after topology change");
	}
}
```

and in `fvwm_topology_dispatch_refresh_result()`, skip later callbacks when recompute fails (or make recompute itself all-or-nothing before mutating `effective_scale`).

### WR-02: Initial finalize failure is ignored, so the store never seals

**File:** `fvwm/fvwm3.c:2382`
**Issue:** `(void)monitor_scale_config_finalize_initial()` drops the only production call that both associates startup scales and sets `config->finalized`. On failure, overrides remain in the store, runtime monitors stay at `SAFE_INIT` 1x, and later `MonitorScale` / `MonitorScaleFallback` commands are still accepted — contradicting the manpage contract that configuration is finalized after startup and late commands are rejected (`doc/fvwm3_manpage_source.adoc:7458-7461`). `finalize_initial_with()` already emits "initial scale association is incomplete"; main still continues as if scale policy is live.
**Fix:** Honor the result. At minimum seal the store after the startup attempt so late mutation cannot succeed, and surface the failure outside debug-only logging:

```c
if (!monitor_scale_config_finalize_initial()) {
	fvwm_debug(__func__,
	    "MonitorScale: startup association failed; using safe 1x");
}
```

If association failed, still set `finalized` after the attempt (or retry once from the committed snapshot) so the initial-only contract holds.

### WR-03: GNU `__int128` is not C11 and will not build on 32-bit

**File:** `libs/monitor_scale.c:23`
**Issue:** `typedef __int128 monitor_scale_wide;` is a GCC/Clang extension. The project standard is `c_std=c11` (`meson.build`). `__int128` is unavailable on 32-bit GCC/Clang, so `automatic_factor()` and `convert_ratio()` will not compile there. `convert_ratio()` does not need 128-bit intermediates (`|INT_MIN| * MONITOR_SCALE_QUARTERS_MAX` fits in 64-bit); only the DPI cross-product in `automatic_factor()` does.
**Fix:** Gate the extension or replace it with portable checked arithmetic. For conversion, `int64_t` is enough. For DPI, either require 64-bit explicitly:

```c
#if defined(__SIZEOF_INT128__)
typedef __int128 monitor_scale_wide;
#else
#error "monitor_scale requires a compiler with __int128"
#endif
```

or compute `x_cross` / `y_cross` with a 64x64→128 portable multiply (or overflow-checked comparison against the 50–500 DPI bounds before forming the mean).

## Info

### IN-01: `MONITOR_SCALE_DPI_BASE` is unused; the formula hardcodes 48 and 127/5

**File:** `libs/monitor_scale.h:11`
**Issue:** `MONITOR_SCALE_DPI_BASE` (96) is declared and documented but never referenced. `automatic_factor()` inlines `127`/`5` (25.4) and `48` (`2 * 96 / 4`). Changing the named baseline would not change behavior.
**Fix:** Derive the scale denominator from the named constants, e.g. `2 * MONITOR_SCALE_DPI_BASE / 4` and `254/10` reduced in one place, so the header is the single source of truth.

---

_Reviewed: 2026-08-30T23:05:08Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
