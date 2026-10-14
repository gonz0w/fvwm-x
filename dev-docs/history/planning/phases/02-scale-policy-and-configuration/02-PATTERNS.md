# Phase 02: Scale Policy and Configuration - Pattern Map

**Mapped:** 2026-08-30
**Files analyzed:** 20 new/modified files
**Analog families found:** 5

## Scope Extracted

The phase adds a pure exact-scale domain, a core-owned sequential configuration/coordinator, two native unit targets, and one scenario in the existing live-X harness. It modifies monitor runtime state, startup and hotplug ordering, command dispatch, `PrintInfo`, Meson source/test lists, the Linux live-X manifest, and the manual.

`libs/monitor_topology.h` is an input contract only: consume `monitor_topology_monitor` at lines 21-34 and do **not** put scale policy into topology equality or epoch state.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog / In-place Seam | Match Quality |
|---|---|---|---|---|
| `libs/monitor_scale.h` (new) | model | transform | `fvwm/topology_refresh_policy.h` | role-match |
| `libs/monitor_scale.c` (new) | utility | transform | `libs/ImageSafety.c` | role-match |
| `fvwm/monitor_scale_config.h` (new) | service | request-response, event-driven | `libs/FScreen.h` adapter/coordinator APIs | role-match |
| `fvwm/monitor_scale_config.c` (new) | service | request-response, event-driven | `libs/FScreen.c` staged publication | role/data-flow match |
| `libs/FScreen.h` | model | event-driven | existing `struct monitor` / refresh API | exact in-place seam |
| `libs/FScreen.c` | service | event-driven | `legacy_monitor_initialize()` and staged reconciliation | exact in-place seam |
| `fvwm/commands.h` | route | request-response | existing `CMD_*` declarations | exact in-place seam |
| `fvwm/functable.c` | route | request-response | sorted `CMD_ENT` table | exact in-place seam |
| `fvwm/fvwm3.c` | controller | batch | startup config completion boundary | exact in-place seam |
| `fvwm/events.c` | controller | event-driven | `topology_refresh_if_due()` | exact in-place seam |
| `fvwm/builtins.c` | controller | request-response | `print_topology_info()` / `CMD_PrintInfo()` | exact in-place seam |
| `libs/meson.build` | config | batch | `libfvwm3_sources` | exact in-place seam |
| `fvwm/meson.build` | config | batch | `fvwm_sources` | exact in-place seam |
| `tests/test_monitor_scale.c` (new) | test | transform | `tests/test_topology_refresh_policy.c` and `tests/test_image_safety.c` | role/data-flow match |
| `tests/test_monitor_scale_config.c` (new) | test | request-response | `tests/test_fscreen_topology.c` allocator/staging tests | role-match |
| `tests/test_fscreen_topology.c` | test | event-driven | existing failure-retention fixtures | exact in-place seam |
| `tests/meson.build` | config | batch | focused C targets and Xorg-dummy registrations | exact in-place seam |
| `tests/live_x/live_x_harness.py` | test | event-driven, file-I/O | existing Xorg-dummy topology scenario | role/data-flow match |
| `tests/run_in_linux.py` | utility | batch | `LIVE_X_TEST_NAMES` manifest | exact in-place seam |
| `doc/fvwm3_manpage_source.adoc` | config | file-I/O | `PrintInfo Topology` and command-reference entries | exact in-place seam |

## Pattern Assignments

### `libs/monitor_scale.h` (model, transform)

**Analog:** `fvwm/topology_refresh_policy.h:3-32`

Copy the compact public-contract shape: standard-library types only, uppercase include guard, named state/result types, and explicit functions rather than exposing arithmetic to callers.

```c
#ifndef FVWM_TOPOLOGY_REFRESH_POLICY_H
#define FVWM_TOPOLOGY_REFRESH_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	bool     dirty;
	uint64_t first_dirty_ns;
	uint64_t last_dirty_ns;
} fvwm_topology_refresh_state;

bool
fvwm_topology_refresh_is_due(
    const fvwm_topology_refresh_state *state, uint64_t now_ns);
#endif
```

Define named factor, source, automatic-status/boundary, total result, and conversion-status types. Keep quarter units private behind helpers where practical. Conversion APIs must initialize output and return explicit overflow status.

### `libs/monitor_scale.c` (utility, transform)

**Analog:** `libs/ImageSafety.c:19-37,40-83`

Copy defensive outputs and explicit failure, not its saturating distance calculation:

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

Also copy the checked-before-operation discipline from `fvwm/topology_refresh_policy.c:24-37`:

```c
if (duration_ms > UINT64_MAX / NS_PER_MS) {
	return UINT64_MAX;
}
duration_ns = duration_ms * NS_PER_MS;
if (start_ns > UINT64_MAX - duration_ns) {
	return UINT64_MAX;
}
```

For scale code, return an overflow enum rather than saturating. Use private `__int128` for exact DPI cross-products; cast operands before multiplication. Parse only canonical quarter-step tokens and never use `double`, `strtod()`, `%f`, or rounded-output chaining.

### `fvwm/monitor_scale_config.h` (service, request-response/event-driven)

**Analog:** `libs/FScreen.h:230-255`

Copy the explicit context/callback API style used to make stateful X integration testable:

```c
typedef struct
{
	void *context;
	struct monitor *(*allocate_monitor)(void *);
	struct screen_info *(*allocate_screen)(void *);
	char *(*copy_name)(void *, const char *);
	void (*free_monitor)(void *, struct monitor *);
	void (*free_screen)(void *, struct screen_info *);
	void (*free_name)(void *, char *);
} fscreen_legacy_allocator;
```

Expose lifecycle/finalization/recompute/inspection APIs and command callbacks, but keep the owned override-list representation private. Any allocator seam added for unit tests should be narrow and conditional or context-based like the existing adapter pattern.

### `fvwm/monitor_scale_config.c` (service, request-response/event-driven)

**Primary analog:** `libs/FScreen.c:938-986,1004-1054`

Copy the stage-then-commit transaction. Allocate/copy/validate off-state, destroy the stage on any failure, then mutate authoritative state only after all checks pass:

```c
if (monitor == NULL || screen == NULL || name == NULL) {
	allocator->free_name(allocator->context, name);
	allocator->free_screen(allocator->context, screen);
	allocator->free_monitor(allocator->context, monitor);
	legacy_stage_destroy(stage);
	return false;
}
monitor->si       = screen;
monitor->si->name = name;
legacy_monitor_initialize(monitor, fact);
if (RB_INSERT(monitors, &stage->monitors, monitor) != NULL) {
	/* free complete candidate; authoritative state is untouched */
	...
	return false;
}
```

**Token ownership pattern:** `libs/Parse.c:452-499`

```c
/* token is copied into malloced space and must be freed after use. */
char *GetNextToken(char *indata, char **token)
{
	return DoGetNextToken(indata, token, NULL, NULL, NULL);
}
```

Use `GetNextToken()` for the exact monitor name and factor, fetch a third token to reject extras, then free all owned tokens. Do not use `PeekToken()` for two retained values because its static buffer is overwritten (`libs/Parse.c:302-366`).

**Invalid-input diagnostic pattern:** `fvwm/virtual.c:2063-2078`

```c
n = GetIntegerArguments(action, NULL, &val, 1);
if (n != 1)
{
	fvwm_debug(__func__,
		   "EdgeThickness requires 1 numeric argument, found %d args", n);
	return;
}
if (val < 0 || val > 2)
{
	fvwm_debug(__func__, "... found %d", val);
	return;
}
```

For the new callbacks, validate exact token count and factor before lookup/store mutation. A bad later declaration must leave the prior factor unchanged. Match active names with case-sensitive `strcmp()` and count all matches. Explicitly do **not** copy `monitor_resolve_name()` because `libs/FScreen.c:287-308` accepts numeric indexes and `g`/`c`/`p` aliases.

### `libs/FScreen.h` and `libs/FScreen.c` (runtime association)

**In-place seam:** `libs/FScreen.h:95-167`, `libs/FScreen.c:904-913`

Add the pure scale-result type to `struct monitor`, alongside runtime monitor state—not to `monitor_topology_snapshot`. Safe-initialize it in the one initializer used for newly staged monitors:

```c
static void
legacy_monitor_initialize(
    struct monitor *monitor, const monitor_topology_monitor *fact)
{
	monitor->flags        = MONITOR_NEW;
	monitor->si->name_atom = fact->name_atom;
	monitor_set_topology_coords(monitor, fact);
	monitor->virtual_scr.edge_thickness      = 2;
	monitor->virtual_scr.last_edge_thickness = 2;
}
```

Set a total safe-1x result here before publication. Preserve the existing transaction: candidate topology and staged legacy monitors publish/reconcile together (`libs/FScreen.c:730-758`). Scale derivation must consume `FScreenGetTopologySnapshot()` (`libs/FScreen.c:631-635`) and must not query RandR or alter topology epoch/equality.

### `fvwm/commands.h` and `fvwm/functable.c` (route, request-response)

**Analog:** existing declarations at `fvwm/commands.h:191-356` and sorted entries at `fvwm/functable.c:385-419`.

```c
void CMD_PrintInfo(F_CMD_ARGS);

CMD_ENT("printinfo", CMD_PrintInfo, F_PRINTINFO, 0, 0),
```

Add `CMD_MonitorScale` and `CMD_MonitorScaleFallback` declarations. Register lowercase `monitorscale` and `monitorscalefallback` consecutively in alphabetical position. Use `F_NOP` unless a unique command ID is demonstrably required; existing config-only commands do so (`fvwm/functable.c:124-138,232-233`). Callback signatures use `F_CMD_ARGS` from `fvwm/fvwm.h:61-64`.

### `fvwm/fvwm3.c` (startup finalization)

**In-place seam:** `fvwm/fvwm3.c:2310-2378`

```c
exc = exc_create_context(&ecc, ECC_TYPE | ECC_WCONTEXT);
/* command-line -c directives or startup config file execute here */
...
exc_destroy_context(exc);
```

Finalize initial scale policy once after all startup directives have run and before normal event processing. This pass may resolve/associate every active monitor and diagnose retained unmatched overrides. It must not implement Phase 6 runtime window/resource reconciliation.

### `fvwm/events.c` (hotplug ordering)

**In-place seam:** `fvwm/events.c:1693-1712`

```c
result = FScreenRefreshTopology(dpy);
fvwm_topology_refresh_complete(&topology_refresh_state);
if (!fvwm_topology_refresh_requests_downstream(result)) {
	return;
}

monitor_update_ewmh();
monitor_emit_broadcast();
initPanFrames(NULL);
status_send();
```

Insert bounded in-memory scale recomputation after the changed-result guard and before `monitor_update_ewmh()`. No X query, allocation, file I/O, subprocess, or deferred extra event-loop turn belongs in that call.

### `fvwm/builtins.c` (`PrintInfo Scale`)

**Analog:** `fvwm/builtins.c:2142-2155,2157-2240,2243-2273`

Reuse or extract the cJSON string encoder exactly:

```c
static char *
topology_encoded_name(const char *name)
{
	cJSON *string;
	char  *encoded;

	string = cJSON_CreateString(name);
	if (string == NULL) {
		return NULL;
	}
	encoded = cJSON_PrintUnformatted(string);
	cJSON_Delete(string);
	return encoded;
}
```

Dispatch a case-insensitive `Scale` subject beside `Topology`:

```c
} else if (StrEquals(subject, "Topology")) {
	print_topology_info();
} else {
	fvwm_debug(__func__, "Unknown subject '%s'", action);
}
```

Encode every monitor/override name before one-line output. Report exact factor, selected source, raw pixel/mm facts, automatic validation/rejection, endpoint boundary, fallback/safe reason, and retained unmatched overrides. Preserve Status v4 and module packets unchanged.

### `libs/meson.build` and `fvwm/meson.build` (production linkage)

**In-place patterns:** `libs/meson.build:1-61`, `fvwm/meson.build:1-59`.

Add `monitor_scale.c` once to alphabetized `libfvwm3_sources`; add `fvwm/monitor_scale_config.c` once near the other monitor/topology policy units in `fvwm_sources`. Do not compile either production implementation a second time into the main executable.

### `tests/test_monitor_scale.c` (pure policy test)

**Analogs:** `tests/test_topology_refresh_policy.c:26-39,126-137` and `tests/test_image_safety.c:39-63,82-90`.

```c
static void
test_marks_preserve_first_and_update_last(void)
{
	fvwm_topology_refresh_state state = FVWM_TOPOLOGY_REFRESH_STATE_INIT;
	...
	assert(state.first_dirty_ns == MS(100));
}

int
main(void)
{
	test_marks_preserve_first_and_update_last();
	...
	return 0;
}
```

Use focused `static void test_<behavior>(void)` cases called explicitly from `main()`. Table-drive all 13 quarter factors, strict grammar, DPI axis limits/agreement/midpoints/endpoints, precedence combinations, signed half-away ties, `INT_MIN`/`INT_MAX`, safe output on overflow, and repeated recomputation from the original source.

### `tests/test_monitor_scale_config.c` (transactional config test)

**Analog:** `tests/test_fscreen_topology.c:25-91,161-216,481-539`.

Copy controllable allocation failure and invariant snapshots:

```c
fixture->allocation_count++;
if (fixture->allocation_count == fixture->fail_allocation) {
	return NULL;
}
...
before = committed;
before_digest = legacy_digest(&legacy_monitors, &legacy_screens);
...
assert(committed == before && committed->epoch == 1);
assert(legacy_digest(&legacy_monitors, &legacy_screens) == before_digest);
```

Test first insertion, valid replacement, malformed replacement retention, allocation-failure retention, case distinction, exact-match count 0/1/>1, disconnected-name retention, fallback replacement, and explanation status. Capture diagnostics through an injected callback or similarly narrow seam; do not make production state globally writable merely for tests.

### `tests/test_fscreen_topology.c` (runtime safe initialization)

Extend the existing staged-monitor assertions around `tests/test_fscreen_topology.c:504-539`. Assert a newly staged monitor has the total safe-1x result before coordinator recomputation, and that failed topology staging leaves both the monitor digest and scale association unchanged. Reuse the existing allocator fixtures rather than creating an X dependency.

### `tests/meson.build` (native and live-X targets)

**Analog:** `tests/meson.build:63-81`.

```meson
topology_refresh_policy_test = executable(
    'test-topology-refresh-policy',
    ['test_topology_refresh_policy.c', '../fvwm/topology_refresh_policy.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)

test('topology-refresh-policy', topology_refresh_policy_test)
```

Register `monitor-scale` and `monitor-scale-config` with `-UNDEBUG`; link `libfvwm3` only when the config target needs shared parser/log symbols. Add `scale-policy-xorg-dummy` inside the existing Linux/`live-x-tests` guard, with `suite: 'live-x'`, `is_parallel: false`, a bounded timeout, and the existing harness/config/artifact arguments (`tests/meson.build:137-177`).

### `tests/live_x/live_x_harness.py` (authoritative live-X scenario)

**Analogs:** startup argv at lines 678-716, log assertions at 1362-1405, Xorg-dummy scenario at 1445-1503, self-tests at 2795-2918, dispatch at 3047-3072.

Preserve argv-only execution and add scenario-owned startup commands as list elements:

```python
def fvwm_arguments(
    fvwm_path: Path, display: str, fvwm_log: Path
) -> list[str]:
    return [
        str(fvwm_path), "-d", display, "-o", str(fvwm_log), "-v",
        "-c", "Schedule 0 Status On",
        "-c", "PrintInfo Topology",
    ]
```

Prefer an `extra_commands: Sequence[str] = ()` parameter and append each as `['-c', command]`; do not invoke a shell. Add a self-test asserting exact argv order and preservation of commands containing spaces/quotes.

Follow `assert_print_info_topology()` for bounded log polling and compare encoded names with `json.dumps(name)`. Add a `scale-policy` parser choice and dispatch branch, but reuse `start_xorg_dummy`, `run_required_xrandr`, `wait_for_predicate`, `ManagedProcess`, artifacts, monotonic deadlines, and cleanup. Run isolated startup cases for override, automatic, fallback, safe 1x, invalid replacement retention, and unmatched retained override.

### `tests/run_in_linux.py` (live-X manifest)

**In-place seam:** `tests/run_in_linux.py:30-43`.

```python
LIVE_X_TEST_NAMES = (
    "topology-fault-injection-probe",
    "topology-xorg-dummy",
    "topology-xorg-dummy-failed-query",
    "topology-xvfb",
)
```

Add `scale-policy-xorg-dummy` in sorted order so `--check-live-x-tests` continues to verify the complete Meson suite. Do not add packages or a second runner.

### `doc/fvwm3_manpage_source.adoc` (user contract)

**Analogs:** `PrintInfo` subject prose at lines 3415-3458 and command syntax at 7385-7423.

Document `MonitorScale <exact-name> <factor>` and `MonitorScaleFallback <factor>` in the monitor/desktop command section. State exact case-sensitive name identity, canonical 1x..4x quarter forms, last-valid semantics, unmatched retention, precedence, 96-DPI two-axis validation, nearest-quarter midpoint-up, endpoint explanation, signed ties-away conversion, and overflow failure. Add `_Scale_` to `PrintInfo` subjects and show `PrintInfo Scale`. Do not claim Status v4 or module metadata includes scale.

## Shared Patterns

### Explicit Status and Safe Outputs

**Sources:** `libs/ImageSafety.c:27-37`, `libs/FScreen.h:32-37`.

Apply to pure parsing, DPI validation, resolution, conversion, topology refresh, and configuration commits. Use enums for multi-state outcomes; initialize output parameters before any failure; never return a plausible clamped value after overflow.

### Transactional Mutation

**Sources:** `libs/FScreen.c:938-986,730-758`; tests at `tests/test_fscreen_topology.c:481-539`.

Validate and allocate a complete candidate before touching authoritative state. On failure, free only candidate ownership, retain the previous valid declaration/result, and test identity/digest invariants.

### Exact Identity

**Source input:** `libs/monitor_topology.h:21-34`.

Use `strcmp()` against committed monitor `name`, count matches, and accept exactly one active match. Retain zero-match overrides and reject ambiguity. Never call `monitor_resolve_name()` (`libs/FScreen.c:277-310`).

### Diagnostics and Encoding

**Sources:** `fvwm/virtual.c:2063-2078`, `fvwm/builtins.c:2142-2155`.

Use `fvwm_debug(__func__, ...)` for recoverable invalid input and policy explanations. Encode config/X-controlled names with cJSON before one-line logs; never treat a name as a format string.

### Topology Transaction Ordering

**Sources:** `fvwm/events.c:1702-1711`, `libs/FScreen.c:745-758`.

On changed topology: commit/reconcile topology, recompute scale from the committed snapshot, then run EWMH/broadcast/pan-frame/Status downstream work. Scale is derived runtime policy and must not advance topology epoch.

### No Authentication Pattern

This phase adds local user configuration and inspection only. No auth/authorization guard applies.

## No Analog Found

No file is wholly without a useful in-repository analog. The exact rational DPI bucketing and signed ties-away conversion are new algorithms; use the locked formulas in `02-CONTEXT.md`/`02-RESEARCH.md` while copying API/error/test structure from the analogs above.

## Metadata

**Analog search scope:** `libs/`, `fvwm/`, `tests/`, `tests/live_x/`, `doc/`

**Primary analog families (stopped at five):**
1. `libs/ImageSafety.[ch]` — pure checked policy and safe outputs
2. `fvwm/topology_refresh_policy.[ch]` + test — compact C policy/status/test shape
3. `libs/FScreen.c` + `tests/test_fscreen_topology.c` — staged transactional publication and failure retention
4. `fvwm/builtins.c` — encoded `PrintInfo` output and subject dispatch
5. `tests/live_x/live_x_harness.py` — bounded Xorg-dummy scenario, argv, evidence, and self-tests

**Pattern extraction date:** 2026-08-30
