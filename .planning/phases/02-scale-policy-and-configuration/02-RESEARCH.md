# Phase 2: Scale Policy and Configuration - Research

**Researched:** 2026-08-30
**Domain:** Exact rational scale policy, FVWM command configuration, RandR Monitor DPI inputs, and deterministic pixel conversion
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

### Scale range and arithmetic
- **D-01:** The supported v0.1 scale domain is exact quarter steps from 1x
  through 4x inclusive. Reject configured values below 1x, above 4x, or between
  quarter steps. Internally preserve scale as an exact rational value rather
  than binary floating point. — **Reversibility:** costly — Geometry, resource,
  transition, and module consumers in later phases will depend on this shared
  scale domain and representation.
- **D-02:** Round rational pixel conversions to nearest, with exact half ties
  away from zero. This rule applies symmetrically to positive and negative
  values.
- **D-03:** A conversion that exceeds the representable pixel range fails with
  an explicit overflow result. It must never clamp or silently wrap, and the
  caller must not receive a plausible-looking inexact geometry.
- **D-04:** Derive every conversion from exact source values and rational scale;
  do not feed a previously rounded result back into subsequent conversions.
  This is the arithmetic foundation for drift-free later phases.

### Automatic DPI policy
- **D-05:** Use 96 DPI as the 1x physical-density baseline. Compute horizontal
  and vertical DPI from the committed Phase 1 pixel and millimetre facts using
  exact rational arithmetic.
- **D-06:** Automatic DPI requires both physical axes to be present and
  individually plausible. Both axis values must fall within 50 through 500 DPI
  inclusive and agree within 10 percent; otherwise automatic DPI is invalid and
  selection continues down the fallback chain.
- **D-07:** When both axes pass validation, combine them with the exact
  arithmetic mean. Map the result to the nearest quarter-step scale relative to
  96 DPI; an exact bucket midpoint selects the higher scale.
- **D-08:** If valid DPI maps below 1x or above 4x, select the nearest supported
  endpoint and retain an explanation that the automatic result hit the policy
  boundary. This is a valid automatic result, not a fallback-chain failure.
- **D-09:** Never infer automatic DPI from one axis alone. A missing,
  implausible, or materially inconsistent axis invalidates the whole automatic
  DPI candidate.

### Override and fallback configuration
- **D-10:** Add separate user-facing directives with these contracts:
  `MonitorScale <exact-name> <factor>` for per-monitor overrides and
  `MonitorScaleFallback <factor>` for the configured global fallback. —
  **Reversibility:** costly — These directive names and argument meanings become
  a documented configuration contract consumed by user config files.
- **D-11:** `MonitorScale` matches only an exact, case-sensitive RandR Monitor
  name from the committed topology. Numeric monitor indexes and dynamic aliases
  such as current or primary are not valid override identities. An ambiguous
  active match is rejected rather than resolved by enumeration order.
- **D-12:** Configuration factors use canonical decimal quarter-step forms in
  the 1x through 4x domain, such as `1`, `1.25`, `1.5`, `1.75`, and `2`.
  Percentages, arbitrary decimal percentages, and arbitrary fractions remain
  outside v0.1.
- **D-13:** Directives are sequential: the last valid declaration for the same
  exact monitor name wins. A malformed or unsupported later declaration emits
  a diagnostic but does not erase or mutate the last valid value.
- **D-14:** Retain a valid override for a monitor name that is not currently
  active and diagnose that it is presently unmatched. This permits the same
  initial configuration to describe docked or temporarily disconnected
  monitors without inventing a less stable identity.
- **D-15:** Effective-scale precedence is exact-name monitor override,
  validated automatic DPI, valid configured global fallback, then safe 1x. An
  invalid declaration or rejected match does not suppress lower-priority
  sources.

### the agent's Discretion
- Choose the internal file and API boundaries for pure rational scale parsing,
  DPI validation, selection reasons, and conversion helpers, following the
  repository pattern of extracting testable policy from X11-heavy code.
- Choose the exact diagnostic and inspection integration points. Every rejected
  scale value, invalid DPI candidate, ambiguous active match, unmatched retained
  override, endpoint-bound automatic result, and chosen fallback must remain
  visibly explainable as required by SCALE-04; Phase 7 module protocol work and
  Phase 6 runtime-reload observability must not be pulled into this phase.
- Choose diagnostic wording, reason enum names, and internal storage layout.
- Choose focused native test organization and edge-case fixtures while covering
  exact parsing, precedence, invalid physical facts, boundaries, signed
  rounding, and overflow.

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope. Existing roadmap deferrals remain:
arbitrary percentages belong to a future compatibility phase, and runtime scale
reload belongs to Phase 6.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SCALE-01 | User can assign an explicit rational scale to a uniquely named monitor. | Strict canonical parser, exact-name override store, table-driven command integration, and retained unmatched-override diagnostics. [VERIFIED: .planning/REQUIREMENTS.md:19-19] |
| SCALE-02 | User can rely on automatic scale derived from validated pixel and millimetre dimensions when no override exists. | Exact DPI cross-product policy over the committed Phase 1 facts, with two-axis plausibility and agreement checks. [VERIFIED: .planning/REQUIREMENTS.md:20-20] |
| SCALE-03 | User gets deterministic precedence: monitor override, validated automatic DPI, configured global fallback, then safe 1x. | One total resolver returns a result for every active monitor and records both selected source and rejected automatic reason. [VERIFIED: .planning/REQUIREMENTS.md:21-21] |
| SCALE-04 | User receives a visible diagnostic when scale input or monitor matching is invalid instead of silent coercion. | Immediate `fvwm_debug()` rejection plus durable `PrintInfo Scale` inspection; no Status or module-protocol expansion in this phase. [VERIFIED: .planning/REQUIREMENTS.md:22-22] |
| SCALE-05 | User can use 1x, 1.25x, 1.5x, 1.75x, 2x, and higher supported quarter-step factors without cumulative rounding drift. | Fixed-denominator exact factors, checked signed conversion, and source-value-only tests including repeated recomputation. [VERIFIED: .planning/REQUIREMENTS.md:23-23] |
</phase_requirements>

## Summary

Phase 2 should be planned as three layers: a pure, allocation-free scale domain; a core-owned sequential configuration store; and a thin coordinator that resolves and attaches one total `effective scale + explanation` result to each active legacy monitor. The committed Phase 1 topology remains the authoritative immutable input. Its exact fields are quoted as `name_atom`, `name`, root-pixel `width`/`height`, and `mm_width`/`mm_height`; `FScreenGetTopologySnapshot()` already returns the committed snapshot. [VERIFIED: libs/monitor_topology.h:21-42; libs/FScreen.c:631-635]

Do **not** insert scale configuration into `monitor_topology_snapshot` or let scale changes advance its topology epoch. Phase 1 defines topology equality over monitor identity, geometry, millimetres, primary state, and outputs, and publishes a new epoch only for those semantic topology changes. Scale is derived policy over that snapshot, so it belongs alongside the topology in monitor runtime state, not inside topology identity. [VERIFIED: libs/monitor_topology.c:325-399]

The arithmetic should use a fixed quarter-unit factor as its canonical stored rational and checked wide intermediates for DPI comparisons. Floating point and repeated pixel-to-pixel conversion are unnecessary. The strict factor grammar can be recognized directly rather than parsed with `strtod()` or `sscanf()`. SEI CERT warns that `sscanf()` does not report numeric overflow and recommends complete conversion/range validation; a direct allowlist grammar is even narrower for this finite domain. [CITED: https://wiki.sei.cmu.edu/confluence/display/c/ERR34-C.+Detect+errors+when+converting+a+string+to+a+number]

**Primary recommendation:** Add scale policy alongside topology, store only exact quarter units and structured reasons, finalize initial configuration once before event processing, and recompute scale immediately after every changed topology commit before any downstream consumer runs. [VERIFIED: fvwm/fvwm3.c:2306-2378; fvwm/events.c:1693-1712]

## Assumption-Delta Decision

| Question | Decision | Planner consequence |
|----------|----------|---------------------|
| Should effective scale replace physical dimensions as the primary monitor fact? | **ADD ALONGSIDE.** Effective scale becomes the primary result for future geometry/rendering consumers, while raw pixel/mm dimensions remain immutable topology inputs and overrides/fallback remain configuration inputs. [VERIFIED: libs/monitor_topology.h:21-42; .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:9-14] | Add an effective scale/result field to `struct monitor` (or an equivalently direct runtime association), but do not remove or reinterpret the Phase 1 facts. |
| Should scale enter topology equality or topology epoch? | **NO CHANGE.** Current equality compares raw topology fields and the epoch advances only when those fields change. [VERIFIED: libs/monitor_topology.c:325-399] | Do not edit `monitor_topology_snapshot_equal()` to compare scale and do not republish topology for a config-only scale change. |
| Where do override and fallback values live? | **ADD CORE-OWNED POLICY STATE.** They are sequential configuration, not X server facts. [VERIFIED: fvwm/functable.c:130-249; fvwm/fvwm3.c:2306-2378] | Keep an owned override list plus optional fallback in the core scale-config unit; the pure resolver receives them as inputs. |

## Project Constraints (from AGENTS.md)

- Target X11/XRandR 1.5+ only; do not introduce a Wayland path. [VERIFIED: AGENTS.md:18-21]
- Keep authoritative X state single-threaded and preserve event-loop ordering. [VERIFIED: AGENTS.md:20-22]
- Perform no blocking monitor or scale work in the main event loop. [VERIFIED: AGENTS.md:24-25]
- Scale FVWM-owned UI and geometry, not arbitrary client pixels, and do not add compositing. [VERIFIED: AGENTS.md:27-30]
- Prefer the coherent FVWM-X configuration contract over incompatible legacy behavior. [VERIFIED: AGENTS.md:29-30]
- Give pure policy native unit coverage and X-visible behavior nested-X integration coverage on native Linux CI. [VERIFIED: AGENTS.md:32-33]
- Route local macOS verification through Podman x86_64 emulation; leave AddressSanitizer to native Linux CI. [VERIFIED: AGENTS.md:35-36]
- Use C11, repository formatting, lower snake-case APIs, explicit status results, close-to-use declarations in new code, and `fvwm_debug(__func__, ...)` for recoverable diagnostics. [VERIFIED: AGENTS.md:45-45; AGENTS.md:154-181; AGENTS.md:196-210]
- Document both new user-facing directives and the new inspection subject in `doc/fvwm3_manpage_source.adoc`. [VERIFIED: AGENTS.md:214-220]

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Canonical factor parsing and checked conversion | Shared support library (`libs/`) | — | Pure C policy has no X or global-state dependency and will be reused by later geometry/UI consumers. [VERIFIED: libs/meson.build:1-80] |
| DPI validation and automatic bucket selection | Shared support library (`libs/`) | Monitor model | It transforms immutable topology facts into an exact policy result. [VERIFIED: libs/monitor_topology.h:21-42] |
| Override/fallback declaration storage | Core command/configuration tier (`fvwm/`) | Shared parser | FVWM commands are table-dispatched and startup config flows through the command engine. [VERIFIED: fvwm/functable.c:130-249; fvwm/fvwm3.c:2306-2378] |
| Effective scale ownership | Core monitor runtime model (`struct monitor`) | Pure scale policy | Future consumers already receive `struct monitor *`; scale must not become a second X query or a topology-epoch field. [VERIFIED: libs/FScreen.h:95-167; libs/FScreen.c:631-790] |
| Initial finalization | Process bootstrap | Scale coordinator | User/default config finishes before normal event processing; finalization can resolve once from the committed snapshot. [VERIFIED: fvwm/fvwm3.c:2306-2378] |
| Hotplug-time recomputation | Event-loop topology coordinator | Scale coordinator | A changed topology currently runs downstream effects in one ordered block; scale recomputation belongs immediately before those effects. [VERIFIED: fvwm/events.c:1693-1712] |
| Visible explanation | Existing logging/PrintInfo command | Documentation/live-X harness | `PrintInfo Topology` establishes the human inspection pattern without changing module packets. [VERIFIED: fvwm/builtins.c:2157-2273] |

## Standard Stack

### Core

| Library / facility | Version | Purpose | Why standard here |
|--------------------|---------|---------|-------------------|
| C | C11 | Exact integer policy, owned configuration, status enums | The repository fixes the language standard to C11. [VERIFIED: AGENTS.md:43-46] |
| Xlib/libXrandr | XRandR 1.5+ | Supplies already-committed Monitor identity, pixels, and millimetres | Phase 1 already isolates the sole `XRRGetMonitors` query in FScreen; Phase 2 must consume its snapshot rather than query X again. [VERIFIED: libs/FScreen.c:637-790] |
| FVWM command engine and `libs/Parse` | In-repo | Parses and dispatches `MonitorScale` and `MonitorScaleFallback` | Existing commands use `CMD_ENT`, `F_CMD_ARGS`, and token helpers. [VERIFIED: fvwm/functable.c:130-249; fvwm/fvwm.h:61-65; libs/Parse.c:300-521] |
| Meson test runner + plain C assertions | Meson 1.5.1+ project floor | Focused pure-policy and integration targets | Existing native targets compile assertions with `-UNDEBUG`. [VERIFIED: tests/meson.build:1-81; AGENTS.md:75-77] |
| Existing Python 3 live-X harness | Python standard library | Nested Xorg-dummy command/diagnostic verification | It already owns process isolation, Status collection, xrandr mutation, deadlines, and evidence. [VERIFIED: tests/live_x/live_x_harness.py:678-716; tests/meson.build:94-199] |

### Supporting

| Facility | Purpose | When to use |
|----------|---------|-------------|
| cJSON string encoding | Escape monitor names before one-line diagnostics | Reuse/extract the existing monitor-name encoding pattern when a server/config-controlled name is printed. [VERIFIED: fvwm/builtins.c:2142-2155] |
| `fvwm_debug(__func__, ...)` | Immediate rejection and fallback diagnostics | Every malformed value, invalid automatic candidate, ambiguous match, unmatched retained override, endpoint boundary, and selected fallback. [VERIFIED: AGENTS.md:196-210] |
| `PrintInfo` | Durable on-demand explanation | Add a `Scale` subject reporting active result/source/reason and unmatched retained overrides. [VERIFIED: fvwm/builtins.c:2243-2273] |

### Alternatives Considered

| Instead of | Could use | Tradeoff |
|------------|-----------|----------|
| Fixed quarter units | General numerator/denominator pairs | General rationals add normalization, zero-denominator, and overflow surface without adding any v0.1 value; quarter units exactly represent the locked domain. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:21-36] |
| Direct allowlist parser | `strtod()`/floating point | Floating point conflicts with the locked exact representation and accepts spellings outside the canonical grammar. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:21-27; .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:66-69] |
| Core-owned override list | Existing `monitor_resolve_name()` | `monitor_resolve_name()` deliberately accepts numeric indexes and dynamic aliases, which the new directive forbids. Its current accepted forms are quoted below. [VERIFIED: libs/FScreen.c:277-310] |
| `PrintInfo Scale` | Status schema expansion | Status v4 currently has an exact topology schema; extending it would require a deliberate public version/schema transition and is unnecessary for Phase 2 diagnostics. [VERIFIED: fvwm/status_json.c:82-124; doc/fvwm3_manpage_source.adoc:8092-8172] |

**Installation:** None. This phase should add no external package and therefore needs no Package Legitimacy Audit. [VERIFIED: libs/meson.build:1-80; tests/meson.build:1-199]

## Architecture Patterns

### System Architecture Diagram

```text
Startup config lines
    |
    v
FVWM command table --> strict token/scale parser
    |                       |
    | invalid               +--> diagnostic; previous valid state unchanged
    v
owned override list + optional fallback
    |
    +---------------------------+
                                v
RandR events --> Phase 1 committed topology snapshot
                                |
                                v
                     pure total scale resolver
                       /       |       \
              override     valid DPI   fallback / safe 1x
                       \       |       /
                                v
                 effective scale + source + reason
                                |
              +-----------------+------------------+
              v                                    v
       `struct monitor` fields               PrintInfo/logging
              |
              v
   later geometry/rendering consumers
```

The topology path remains single-query and immutable; the resolver consumes `FScreenGetTopologySnapshot()` and never invokes RandR. [VERIFIED: libs/FScreen.c:631-790]

### Recommended Project Structure

```text
libs/
├── monitor_scale.h             # exact factor/result/status public contract [ASSUMED]
└── monitor_scale.c             # parser, DPI policy, checked conversion [ASSUMED]
fvwm/
├── monitor_scale_config.h      # override/fallback ownership + finalization API [ASSUMED]
└── monitor_scale_config.c      # command callbacks, monitor association, diagnostics [ASSUMED]
tests/
├── test_monitor_scale.c        # exhaustive pure arithmetic/policy tests [ASSUMED]
└── test_monitor_scale_config.c # sequential store/matching/retention tests [ASSUMED]
```

The names above are recommendations, not existing paths. They follow the repository's lower-snake-case helper and `test_<unit>.c` conventions. [VERIFIED: AGENTS.md:152-172]

### Pattern 1: Total Result, Not Nullable Scale

**What:** Resolve every active topology fact into a value object containing an exact factor, selected source, automatic-candidate status, and boundary/fallback explanation. The resolver must always return safe 1x when higher-priority inputs fail; only pixel conversion itself returns overflow. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:77-80]

**Why:** A nullable scale pushes fallback policy into every later consumer. A total result makes “one scale before use” an invariant and keeps explanations beside the decision. [ASSUMED]

**Recommended proposed shape:** The exact new identifiers remain planner discretion and are therefore `[ASSUMED]`; the important contract is factor + source + reason + boundary metadata.

```c
/* Proposed API skeleton [ASSUMED], not existing code. */
typedef struct
{
	unsigned int quarters;
} monitor_scale_factor;

typedef struct
{
	monitor_scale_factor factor;
	/* selected source, automatic status, and boundary explanation */
} monitor_scale_result;
```

### Pattern 2: Fixed-Denominator Rational Arithmetic

**What:** Store scale as integer quarter units and interpret it as `quarters / 4`. Parse the finite canonical grammar directly. Use exact integer cross-products for DPI and a checked rational conversion helper for signed pixels. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:21-36]

**DPI facts:** The source-of-truth monitor record exposes the following fields verbatim. [VERIFIED: libs/monitor_topology.h:21-34]

```text
DATA_Q7M4P2LX_START
Atom      name_atom;
char     *name;
int       x;
int       y;
int       width;
int       height;
int       mm_width;
int       mm_height;
Bool      primary;
RROutput *outputs;
int       output_count;
DATA_Q7M4P2LX_END
```

Use the exact identities `dpi_x = width * 127 / (mm_width * 5)` and `dpi_y = height * 127 / (mm_height * 5)`, which are algebraically equal to pixels × 25.4 / millimetres. This is a mathematical derivation from the locked 96-DPI policy and Phase 1 integer facts, not an external library requirement. [VERIFIED: libs/monitor_topology.h:21-34; .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:38-55]

**Critical width issue:** `int64_t` is sufficient for each single `127 * pixels` term, but not necessarily for cross-products between two unreduced pixel/mm fractions when all source fields are `int`. Use private signed `__int128` intermediates for DPI cross-products, keep that extension out of public structs/APIs, and retain explicit destination-range checks. Both local Apple Clang and the established Linux build route accepted `__int128` under `-std=c11 -Werror` in this session; the repository's supported compiler families are GCC and Clang. Do not multiply in a narrow type and cast afterward. [VERIFIED: local and `tests/run_in_linux.py` compile probes, 2026-08-30; AGENTS.md:77-80] Signed overflow is undefined behavior. [CITED: https://wiki.sei.cmu.edu/confluence/display/c/INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow]

### Pattern 3: Transactional Sequential Configuration

**What:** Parse all tokens and allocate/copy a new exact-name entry before mutating the override store. If the name already exists, replace only its factor after the new declaration has fully validated. Invalid later declarations leave the prior entry untouched. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:70-76]

**Token choice:** Use `GetNextToken()` for the monitor name because it returns owned memory; `PeekToken()` uses one static buffer overwritten by the next parser call. These behaviors are quoted verbatim below. [VERIFIED: libs/Parse.c:302-366; libs/Parse.c:452-500]

```text
DATA_H3V9C6RN_START
WARNING: The returned pointer points to a static array that will be
         overwritten in all functions in this file!
DATA_H3V9C6RN_END
```

```text
DATA_F8K2W5TZ_START
The memory in *token is allocated with malloc and the
calling function has to free() it.
DATA_F8K2W5TZ_END
```

Require exactly the prescribed argument count and reject a trailing third token. Missing/extra input is part of strict allowlist validation. [CITED: https://cwe.mitre.org/data/definitions/20.html]

### Pattern 4: Exact Name Matching, Never Legacy Resolution

The existing resolver accepts a numeric monitor number and the dynamic strings quoted below before trying a literal name. That makes it explicitly unsuitable for `MonitorScale`. [VERIFIED: libs/FScreen.c:277-310]

```text
DATA_B6N1R9JD_START
pos = strtonum(scr, 0, INT_MAX, &errstr);
if (errstr == NULL)
        return (monitor_by_number(pos));
...
if (strcmp(scr, "g") == 0) {
...
else if (strcmp(scr, "c") == 0)
...
else if (strcmp(scr, "p") == 0)
DATA_B6N1R9JD_END
```

Match override names with `strcmp()` against the committed snapshot and count matches before selecting. The Phase 1 validator currently rejects duplicate committed names, as quoted below, so ambiguity is a defensive branch today, but D-11 still requires it to be represented and tested rather than silently taking the first match. [VERIFIED: libs/monitor_topology.c:311-319]

```text
DATA_M4S8Y2QA_START
if (compare_monitors(previous, monitor) >= 0
    || previous->name_atom == monitor->name_atom
    || strcmp(previous->name, monitor->name) == 0) {
        return false;
}
DATA_M4S8Y2QA_END
```

### Pattern 5: Initialize Safe, Finalize Once, Recompute Before Downstream

Initialize every newly staged legacy monitor's effective result to safe 1x so no field is ever indeterminate. After startup configuration finishes, run one final resolver pass. The startup config/command-file path completes at the following boundary. [VERIFIED: fvwm/fvwm3.c:2306-2378]

On a changed runtime topology, place scale recomputation between `FScreenRefreshTopology()` and current downstream calls. The exact existing order is quoted below. [VERIFIED: fvwm/events.c:1693-1712]

```c
/* DATA_L5C2T8WP_START */
result = FScreenRefreshTopology(dpy);
fvwm_topology_refresh_complete(&topology_refresh_state);
if (!fvwm_topology_refresh_requests_downstream(result)) {
	return;
}

monitor_update_ewmh();
monitor_emit_broadcast();
initPanFrames(NULL);
status_send();
/* DATA_L5C2T8WP_END */
```

The new recomputation call belongs after the changed-result guard and before `monitor_update_ewmh()`. It must be bounded iteration over in-memory topology/config only—no X query, filesystem access, subprocess, or allocation. [VERIFIED: AGENTS.md:20-25; libs/FScreen.c:631-790]

### Pattern 6: Immediate and Durable Explanation

Emit immediate `fvwm_debug(__func__, ...)` diagnostics for rejected declarations and selection failures, then expose the current decision set through `PrintInfo Scale`. `CMD_PrintInfo()` already dispatches named subjects and reports unknown subjects. [VERIFIED: fvwm/builtins.c:2243-2273]

The inspection line should include encoded monitor name, exact factor, selected source, raw pixel/mm inputs, per-axis validation result, boundary state, and fallback reason. It should also list retained unmatched override names. [ASSUMED]

Do not add scale to module packets in this phase, and do not silently add keys to Status v4. Status currently writes the exact literal version and topology key shown below. [VERIFIED: fvwm/status_json.c:97-106]

```c
/* DATA_R9D3K6HF_START */
cJSON_AddNumberToObject(message, "version", 4)
...
monitor_topology_json_add(message, "topology", topology)
/* DATA_R9D3K6HF_END */
```

### Anti-Patterns to Avoid

- **Mutating topology facts with policy:** breaks Phase 1 epoch semantics and mixes X observations with user decisions. [VERIFIED: libs/monitor_topology.c:325-399]
- **Calling `monitor_resolve_name()`:** admits forbidden numeric/current/primary aliases. [VERIFIED: libs/FScreen.c:277-310]
- **Using `double`, `float`, `strtod()`, or `%f`:** expands accepted syntax and weakens exact tie behavior. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:21-36; .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:66-69]
- **Updating a previous valid declaration before a later one validates:** violates last-valid retention. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:70-72]
- **First-match ambiguity resolution:** enumeration order is not policy. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:62-65]
- **Reusing a rounded output as the next input:** creates cumulative drift. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:34-36]
- **Clamp/wrap on pixel overflow:** creates plausible but false geometry. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:31-33]
- **Logging raw monitor names on one line:** server/config-controlled valid UTF-8 may contain control bytes; reuse the cJSON escaping pattern. [VERIFIED: fvwm/builtins.c:2142-2155; fvwm/builtins.c:2182-2239]
- **Doing runtime reload reconciliation now:** Phase 6 owns config reload and affected-window updates. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:9-14]

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| General decimal arithmetic | A decimal/fraction parser or arbitrary rational package | A finite canonical quarter-step recognizer and integer quarter units | The accepted language and denominator are locked and finite. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:21-27; .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:66-69] |
| Monitor discovery | A second RandR query/cache | `FScreenGetTopologySnapshot()` | Phase 1 already owns query, validation, publication, and last-good retention. [VERIFIED: libs/FScreen.c:631-790] |
| Monitor alias resolution | A wrapper over legacy `monitor_resolve_name()` | Exact `strcmp()` match/count over the committed snapshot | Legacy behavior is broader than the new contract. [VERIFIED: libs/FScreen.c:277-310] |
| Diagnostic string escaping | Ad-hoc replacement of quotes/control bytes | Existing cJSON string encoding pattern | Existing PrintInfo already handles valid UTF-8 names safely. [VERIFIED: fvwm/builtins.c:2142-2155] |
| Test process lifecycle | New shell scripts with sleeps/fixed displays | `tests/live_x/live_x_harness.py` and `tests/run_in_linux.py` | Existing harness supplies display-fd allocation, monotonic deadlines, isolation, cleanup, Status collection, and evidence. [VERIFIED: .planning/phases/01-topology-snapshot-and-live-x-harness/01-06-SUMMARY.md:88-115] |
| New test framework | Third-party C/Python test dependency | Plain C assertions + Meson + Python standard library | This is the established repository pattern and needs no package install. [VERIFIED: tests/meson.build:1-92] |

**Key insight:** The hard part is not representing a quarter-step; it is preserving a single explainable decision boundary across malformed config, extreme integer facts, hotplug ordering, and future consumers. Keep one resolver and one conversion contract. [ASSUMED]

## Numeric Policy Details for Planning

### Canonical factor representation

- Store quarter units in a small integer but expose conversion through a named factor type, not raw arithmetic in callers. The locked domain is quoted in User Constraints as “exact quarter steps from 1x through 4x inclusive.” [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:21-27]
- Parse only canonical spellings. A simple grammar recognizes one-digit whole factors and the allowed `.25`, `.5`, `.75` suffixes, then range-checks the resulting quarter count. Reject signs, leading zeroes, exponent syntax, whitespace within a token, percentages, fractions, and alternate spellings such as a trailing zero. The strict interpretation of “canonical” is a recommendation. [ASSUMED]
- Format from the same factor representation for diagnostics; never preserve user spelling as the semantic value. [ASSUMED]

### DPI validation and bucketing

1. Reject automatic DPI if either millimetre axis is zero. Pixel widths/heights are already positive in a valid committed snapshot. [VERIFIED: libs/monitor_topology.c:75-80; libs/monitor_topology.c:178-205]
2. Build each axis as the exact fraction `127 * pixels / (5 * millimetres)`. [VERIFIED: libs/monitor_topology.h:21-34]
3. Check each axis inclusively against the locked plausible range without integer division truncation. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:42-45]
4. Check axis agreement symmetrically. The phrase “within 10 percent” does not name its denominator; use symmetric percent difference relative to the arithmetic mean unless the user clarifies otherwise: `abs(x-y) / ((x+y)/2) <= 10%`. [ASSUMED]
5. Compare the exact arithmetic mean with quarter-step midpoint thresholds. Midpoint equality selects the upper bucket. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:46-48]
6. If the selected bucket is outside the supported range, return the nearest endpoint with an endpoint-bound explanation; do not continue to fallback. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:49-51]

### Signed rounding and overflow

- Convert from an exact source every time using `source * numerator / denominator`, quotient/remainder inspection, and magnitude-based tie handling. Never call C integer division alone and assume it implements nearest rounding. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:28-36]
- Handle negative minimum values without applying unary minus in their signed type; CERT identifies minimum-value negation and minimum/`-1` division as overflow cases. [CITED: https://wiki.sei.cmu.edu/confluence/display/c/INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow]
- Check the final rounded magnitude against the destination pixel type before assignment and return an explicit overflow status with the output initialized to a safe sentinel. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:31-33; AGENTS.md:196-205]
- Test multiply and divide directions independently, exact halves on both signs, values one unit either side of a tie, zero, destination limits, and overflow just beyond each limit. [ASSUMED]

## Likely Files and Exact Existing Integration Symbols

| File | Existing symbols/pattern | Planned role |
|------|--------------------------|--------------|
| `libs/monitor_topology.h` | `monitor_topology_monitor`, `monitor_topology_snapshot` | Read-only policy inputs; do not add config state. [VERIFIED: libs/monitor_topology.h:21-42] |
| `libs/FScreen.h` | `struct monitor`, `FScreenGetTopologySnapshot()` | Add effective scale association to the runtime monitor record; include the pure scale type. [VERIFIED: libs/FScreen.h:95-167; libs/FScreen.h:218-228] |
| `libs/FScreen.c` | `legacy_monitor_initialize()`, changed publication/reconciliation | Initialize new monitors to safe 1x; preserve existing snapshot ownership. [VERIFIED: libs/FScreen.c:904-986; libs/FScreen.c:1004-1078] |
| `fvwm/functable.c` | sorted `CMD_ENT(...)` command table | Register lowercase `monitorscale` and `monitorscalefallback` entries in alphabetical position. [VERIFIED: fvwm/functable.c:130-249] |
| `fvwm/commands.h` | command ID enum and `CMD_*` declarations | Add command IDs only if semantics require them; `F_NOP` is sufficient for config-only commands that need no unique execution behavior, as existing config commands demonstrate. [VERIFIED: fvwm/commands.h:14-133; fvwm/functable.c:137-138; fvwm/functable.c:232-233] |
| `fvwm/fvwm3.c` | startup config ends before `exc_destroy_context()` | Finalize effective scales after lines 2313-2377 finish loading config and before normal event processing. [VERIFIED: fvwm/fvwm3.c:2306-2378] |
| `fvwm/events.c` | `topology_refresh_if_due()` | Recompute scales after a changed commit and before `monitor_update_ewmh()`. [VERIFIED: fvwm/events.c:1693-1712] |
| `fvwm/builtins.c` | `CMD_PrintInfo()`, `topology_encoded_name()` | Add `Scale` subject and reuse/extract safe name encoding. [VERIFIED: fvwm/builtins.c:2142-2155; fvwm/builtins.c:2243-2273] |
| `fvwm/meson.build` | `fvwm_sources` | Link the core config/coordinator unit exactly once. [VERIFIED: fvwm/meson.build:1-59] |
| `libs/meson.build` | `libfvwm3_sources` | Link pure scale policy exactly once for production/shared tests. [VERIFIED: libs/meson.build:1-80] |
| `tests/meson.build` | focused assertion executables and live-X suite | Register native scale targets and one Xorg-dummy initial-config scenario. [VERIFIED: tests/meson.build:1-81; tests/meson.build:94-199] |
| `tests/live_x/live_x_harness.py` | `fvwm_arguments()`, Xorg-dummy scenario primitives | Allow scenario-owned extra `-c` directives and parse `PrintInfo Scale`/diagnostics. [VERIFIED: tests/live_x/live_x_harness.py:678-716; tests/live_x/live_x_harness.py:1408-1542] |
| `doc/fvwm3_manpage_source.adoc` | command reference, `PrintInfo Topology`, Status v4 docs | Document exact syntax, precedence, DPI rejection, rounding/overflow, and `PrintInfo Scale`. [VERIFIED: doc/fvwm3_manpage_source.adoc:3430-3458; doc/fvwm3_manpage_source.adoc:8092-8172] |

## Common Pitfalls

### Pitfall 1: Treating `int64_t` as automatically sufficient
**What goes wrong:** Cross-multiplying two extreme pixel/mm fractions can overflow even though each numerator and denominator individually fits. [VERIFIED: libs/monitor_topology.h:21-34]
**Why it happens:** The committed fields are full C `int` values and Phase 1 deliberately preserves extreme nonnegative millimetres. [VERIFIED: libs/monitor_topology.c:178-205; tests/test_monitor_topology.c:749-761]
**How to avoid:** Design checked-wide comparison primitives first and test maximum facts; never detect signed overflow after evaluating the overflowing expression. [CITED: https://wiki.sei.cmu.edu/confluence/display/c/INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow]
**Warning signs:** Multiplication of two dimensions/fraction components directly in `int` or `int64_t`; casts applied after multiplication. [ASSUMED]

### Pitfall 2: Reusing legacy monitor resolution
**What goes wrong:** A directive accepts numeric, current, global, or primary aliases contrary to D-11. [VERIFIED: libs/FScreen.c:277-310]
**Why it happens:** `monitor_resolve_name()` is convenient and already used by older per-screen commands. [VERIFIED: fvwm/virtual.c:2042-2095]
**How to avoid:** Match exact name tokens against the committed snapshot with `strcmp()` and count matches. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:62-65]
**Warning signs:** Calls to `monitor_resolve_name()`, `StrEquals()`, or number lookup in new scale config code. [ASSUMED]

### Pitfall 3: Losing last-valid declaration on an invalid replacement
**What goes wrong:** The parser clears or partially edits the existing override before validating a later declaration. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:70-72]
**How to avoid:** Parse/validate/allocate off-state, then commit one replacement. [ASSUMED]
**Warning signs:** Removal by name at callback entry, or in-place string writes before factor validation. [ASSUMED]

### Pitfall 4: Explaining only the selected source
**What goes wrong:** A fallback is shown but the user cannot see why automatic DPI failed, or an endpoint automatic result looks like fallback. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:42-55; .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:86-90]
**How to avoid:** Carry automatic candidate status and boundary state even when the effective source is fallback/safe. [ASSUMED]
**Warning signs:** A result enum with only override/automatic/fallback/safe and no rejected-candidate metadata. [ASSUMED]

### Pitfall 5: Making diagnostics transient only
**What goes wrong:** Startup logs scroll away and the current scale cannot be inspected. [ASSUMED]
**How to avoid:** Pair immediate `fvwm_debug()` with `PrintInfo Scale`. [VERIFIED: fvwm/builtins.c:2243-2273]
**Warning signs:** Tests only grep one startup error and never inspect the final chosen result. [ASSUMED]

### Pitfall 6: Changing topology before scale is ready
**What goes wrong:** Existing downstream calls observe newly reconciled monitors with stale/uninitialized scale. [VERIFIED: fvwm/events.c:1693-1712]
**How to avoid:** Safe-initialize new monitor fields and recompute within the changed topology transaction before downstream calls. [ASSUMED]
**Warning signs:** Scale recomputation after `status_send()` or deferred to another event-loop iteration. [ASSUMED]

### Pitfall 7: Accidentally implementing Phase 6 reload
**What goes wrong:** Runtime directives trigger window/resource reconciliation before those contracts exist. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:9-14]
**How to avoid:** Phase 2 may maintain policy and new-monitor scale, but must not rescale existing windows/resources. Explicitly reject or defer post-startup scale config mutation until Phase 6 rather than partially applying it. [ASSUMED]
**Warning signs:** Calls from scale command callbacks into window iteration, redraw, placement, or resource-cache invalidation. [ASSUMED]

## Code Examples

### Existing command registration pattern

The table uses lowercase command keywords and `CMD_*` callbacks. [VERIFIED: fvwm/functable.c:171-179]

```c
/* DATA_E2P7V4NS_START */
CMD_ENT("desktopconfiguration", CMD_DesktopConfiguration,
	F_DESKTOP_CONFIGURATION, 0, 0),
CMD_ENT("desktopname", CMD_DesktopName, F_DESKTOP_NAME, 0, 0),
CMD_ENT("desktopsize", CMD_DesktopSize, F_SETDESK, 0, 0),
/* DATA_E2P7V4NS_END */
```

### Existing strict diagnostic/leave-state pattern

Older code demonstrates early diagnostic return, although the new scale code must not use its alias resolver. [VERIFIED: fvwm/virtual.c:2048-2078]

```c
/* DATA_C5W9J1RK_START */
if ((m = monitor_resolve_name(option)) == NULL) {
	fvwm_debug(__func__, "Invalid screen: %s", option);
	return;
}
...
if (n != 1)
{
	fvwm_debug(__func__,
		   "EdgeThickness requires 1 numeric argument,"
		   " found %d args",n);
	return;
}
/* DATA_C5W9J1RK_END */
```

### Existing focused native test registration

The exact direct-policy registration pattern is: [VERIFIED: tests/meson.build:63-70]

```meson
# DATA_T6H3M8QZ_START
topology_refresh_policy_test = executable(
    'test-topology-refresh-policy',
    ['test_topology_refresh_policy.c', '../fvwm/topology_refresh_policy.c'],
    include_directories: includedirs,
    c_args: ['-UNDEBUG'],
)

test('topology-refresh-policy', topology_refresh_policy_test)
# DATA_T6H3M8QZ_END
```

### Proposed checked conversion contract

The following is a behavioral skeleton, not a locked identifier set. [ASSUMED]

```c
/* Proposed [ASSUMED]. Output is initialized before any failure path. */
monitor_scale_conversion_result
monitor_scale_apply(int source, monitor_scale_factor factor, int *output);

monitor_scale_conversion_result
monitor_scale_remove(int source, monitor_scale_factor factor, int *output);
```

Each call derives from `source`; callers must retain logical/source geometry separately rather than feeding `output` into the next transition. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:34-36]

## State of the Art

| Old/current repository approach | Phase 2 approach | Impact |
|---------------------------------|------------------|--------|
| Monitor topology exposes only raw X facts. [VERIFIED: libs/monitor_topology.h:21-42] | Add an exact effective policy result alongside each active runtime monitor. [ASSUMED] | Later phases receive one scale without re-deriving DPI. |
| Legacy monitor selection accepts indexes and aliases. [VERIFIED: libs/FScreen.c:277-310] | Scale override identity is exact, case-sensitive committed Monitor name only. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:62-65] | Stable, explainable config identity. |
| Existing geometry code commonly uses direct integer ratios for unrelated pager scaling. [VERIFIED: modules/FvwmPager/x_pager.c:256-334] | New scale conversion centralizes exact nearest/ties-away/overflow semantics. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:28-36] | No duplicated rounding rules or drift. |
| Status v4 reports raw topology. [VERIFIED: fvwm/status_json.c:82-124] | Keep v4 unchanged; add human `PrintInfo Scale` for Phase 2. [ASSUMED] | Avoids premature structured/module protocol changes. |

**Deprecated/outdated for this phase:** `monitor_resolve_name()` semantics, floating-point factors, Xft/global DPI mutation, RandR output transforms as scale, and rounded-result chaining must not define the new policy. The latter three are explicitly outside the project contract or locked decisions. [VERIFIED: .planning/REQUIREMENTS.md:68-79; .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:21-36]

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | “Axes agree within 10 percent” should use symmetric percent difference relative to their arithmetic mean. | Numeric Policy Details | A different denominator changes acceptance near the threshold; lock the formula before tests. |
| A2 | Only canonical shortest decimal spellings are accepted; alternate mathematically equal spellings such as trailing-zero forms are rejected. | Canonical factor representation | Parser compatibility and documentation examples differ if “canonical” was intended more loosely. |
| A3 | Effective result fields should live directly on `struct monitor`, while pure policy types live in `libs/monitor_scale.[ch]`. | Architecture Patterns | A separate scale snapshot could be preferred, but it would require additional ownership/publication coordination. |
| A4 | Phase 2 should use `PrintInfo Scale` and leave Status v4 unchanged. | Explanation / State of the Art | If machine-readable scale inspection is required now, a deliberate Status version/schema decision is needed. |
| A5 | Post-startup scale directives should not partially apply before Phase 6; exact reject/defer behavior remains to be locked. | Pitfall 7 | Accepting them may accidentally implement reload; rejecting them may surprise users invoking `Read` at runtime. |

## Open Questions (RESOLVED)

1. **RESOLVED — What exactly is the denominator for “within 10 percent”?**
   - What we know: both axes must be checked symmetrically and D-07 then uses their arithmetic mean. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:42-48]
   - Resolution basis: D-06 did not distinguish percent difference from ratio-to-larger or relative-to-one-axis definitions, so the plans adopt the symmetric arithmetic-mean formula. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:42-45]
   - Adopted resolution: use symmetric percent difference relative to the arithmetic mean, encoded exactly as `20 * abs(dpi_x - dpi_y) <= dpi_x + dpi_y` over rational values, with exact boundary tests. This is PA-01 in the executable plans. [ASSUMED]

2. **RESOLVED — Does “canonical decimal” reject alternate equivalent spellings?**
   - What we know: the accepted examples use shortest quarter-step forms and arbitrary decimals/fractions are excluded. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:66-69]
   - Resolution basis: trailing-zero and leading-plus spellings were not explicitly named, so the plans adopt one shortest spelling for each supported value. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:66-69]
   - Adopted resolution: accept only shortest canonical quarter-step decimals (`1`, `1.25`, `1.5`, `1.75` … `4`) and reject signs, leading zeroes, trailing zeroes, exponents, percentages, and fractions. This is PA-02 in the executable plans. [ASSUMED]

3. **RESOLVED — What should a scale directive do after startup but before Phase 6 exists?**
   - What we know: initial configuration is in Phase 2 and runtime reload/reconciliation is in Phase 6. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:9-14]
   - Resolution basis: command-table entries remain invokable after startup, so the plans add a finalized-state rejection boundary. [VERIFIED: fvwm/functable.c:130-249]
   - Adopted resolution: reject post-startup mutation with a clear diagnostic and preserve policy/result state until Phase 6 supplies atomic reload; do not mutate hidden pending state. This is PA-03 in the executable plans. [ASSUMED]

4. **RESOLVED — Should structured Status expose effective scale now?**
   - What we know: Status v4 has an exact documented topology schema and PrintInfo is an established human path. [VERIFIED: fvwm/status_json.c:82-124; doc/fvwm3_manpage_source.adoc:8092-8172]
   - Resolution basis: SCALE-04 requires visibility rather than JSON specifically, so the plans use the existing human inspection path. [VERIFIED: .planning/REQUIREMENTS.md:22-22]
   - Adopted resolution: expose scale through `PrintInfo Scale` and immediate diagnostics only; do not expand Status v4 or module packets in Phase 2. Machine-readable observability remains a deliberate versioned Phase 6/7 concern. [ASSUMED]

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|-------------|-----------|---------|----------|
| Python 3 | Linux router and live-X harness | ✓ | 3.14.7 | — [VERIFIED: local command `python3 --version`, 2026-08-30] |
| Podman | macOS native-Linux route | ✓ | 6.1.0; active arm64 VM | Existing wrapper requests Linux/amd64. [VERIFIED: local `podman --version` and `podman info`, 2026-08-30] |
| Meson | Build/test runner | ✗ on host | — | Run inside `tests/run_in_linux.py` Podman route. [VERIFIED: local `command -v meson`, 2026-08-30] |
| Ninja | Meson backend | ✗ on host | — | Run inside the Podman build image. [VERIFIED: local `command -v ninja`, 2026-08-30] |
| C compiler | Local syntax/format support | ✓ | Apple clang 21.0.0 | Production verification remains Linux GCC/Clang through wrapper/CI. [VERIFIED: local `cc --version`, 2026-08-30] |
| clang-format | Repository formatting | ✓ | Apple clang-format 21.0.0 via `xcrun` | Use local formatter because Phase 1 found the Linux image lacks it. [VERIFIED: local `xcrun clang-format --version`; .planning/phases/01-topology-snapshot-and-live-x-harness/01-01-SUMMARY.md:164-167] |
| Existing Linux router | All native tests | ✓ | self-test exit 0 | — [VERIFIED: local `python3 tests/run_in_linux.py --self-test`, 2026-08-30] |

**Missing dependencies with no fallback:** None. [VERIFIED: environment probes above]

**Missing dependencies with fallback:** Host Meson and Ninja are absent; the established Podman Linux route is available. [VERIFIED: environment probes above]

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Meson test runner; plain C assertion executables; Python 3 standard-library live-X harness. [VERIFIED: tests/meson.build:1-199] |
| Config file | `tests/meson.build`; `live-x-tests` remains the opt-in gate. [VERIFIED: tests/meson.build:94-199] |
| Quick run command | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-scale monitor-scale-config --print-errorlogs` [ASSUMED] |
| Full native suite | `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs` [VERIFIED: .planning/phases/01-topology-snapshot-and-live-x-harness/01-VALIDATION.md:23-25] |
| Focused live-X | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir scale-policy-xorg-dummy --print-errorlogs` [ASSUMED] |
| Full live-X suite | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs` [VERIFIED: .planning/phases/01-topology-snapshot-and-live-x-harness/01-VALIDATION.md:23-25] |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SCALE-01 | Canonical named override parses, exact case-sensitive unique match wins, unmatched remains retained, ambiguous match rejects | unit + live-X startup | wrapped `monitor-scale-config`; wrapped `scale-policy-xorg-dummy` | ❌ Wave 0 |
| SCALE-02 | Both exact axis DPIs validate, invalid zero/implausible/inconsistent facts reject, mean buckets with midpoint-up and endpoint explanation | unit + authoritative Xorg-dummy | wrapped `monitor-scale`; wrapped `scale-policy-xorg-dummy` | ❌ Wave 0 |
| SCALE-03 | Override → automatic → configured fallback → safe 1x precedence is total and deterministic | unit + live-X startup matrix | wrapped `monitor-scale`; wrapped `scale-policy-xorg-dummy` | ❌ Wave 0 |
| SCALE-04 | Invalid value/match emits diagnostic and `PrintInfo Scale` shows chosen result/reason/unmatched override | unit callback capture + live-X log assertion | wrapped `monitor-scale-config`; wrapped `scale-policy-xorg-dummy` | ❌ Wave 0 |
| SCALE-05 | Every supported quarter factor, signed nearest/ties-away, overflow, inverse direction, and repeated source-based conversions are exact | exhaustive unit | wrapped `monitor-scale` | ❌ Wave 0 |

### Native Unit Scenario Matrix

- Parser: every supported quarter bucket, shortest canonical strings, bounds, just-outside bounds, between-quarter decimals, percent/fraction/exponent/sign/trailing input, missing/extra args. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:21-27; .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:66-72]
- Override store: first insert, valid replacement, invalid replacement retention, case distinction, allocation failure before commit, disconnected-name retention. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:62-76]
- DPI: both axes missing combinations, inclusive plausible limits, just outside limits, exact agreement threshold and one unit beyond, arithmetic-mean buckets, every midpoint, endpoint mapping, extreme `int` dimensions. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:38-55; libs/monitor_topology.h:21-34]
- Precedence: all combinations of present/absent/invalid override, auto, fallback, and safe result; invalid higher source never suppresses lower. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:77-80]
- Conversion: all factor buckets, positive/negative exact values, half ties, near ties, zero, `INT_MIN`, `INT_MAX`, exact destination limits, overflow, untouched/safe output on failure, and 1,000 repeated source-based round trips. The 1,000-repeat expectation is already a milestone verification requirement. [VERIFIED: .planning/REQUIREMENTS.md:51-55]

### Live-X Scenario

Reuse Xorg-dummy because Phase 1 established its output `WIDTH_MM` and `HEIGHT_MM` facts as the authoritative physical-size path. [VERIFIED: .planning/phases/01-topology-snapshot-and-live-x-harness/01-08-SUMMARY.md:126-131]

Run isolated startup invocations rather than runtime reload: (1) exact named override, (2) valid automatic DPI, (3) invalid dimensions plus configured fallback, (4) invalid dimensions with safe 1x, (5) invalid declaration retaining a prior valid declaration, and (6) retained unmatched override. Each invocation should assert the queried X facts, `PrintInfo Scale` result/source/reason, and expected diagnostic text. [ASSUMED]

Do not use Xvfb as the oracle for automatic physical DPI; Phase 1 found Xvfb Monitor mutation capability conditional and made Xorg-dummy the non-skippable authority. [VERIFIED: .planning/phases/01-topology-snapshot-and-live-x-harness/01-07-SUMMARY.md:114-119; .planning/phases/01-topology-snapshot-and-live-x-harness/01-08-SUMMARY.md:101-105]

### Sampling Rate

- **Per task commit:** run the narrow named C target through `tests/run_in_linux.py`; expected feedback under 10 seconds based on the existing pure-policy contract. [VERIFIED: .planning/phases/01-topology-snapshot-and-live-x-harness/01-VALIDATION.md:32-44]
- **Per config/integration task:** run both focused native targets. [ASSUMED]
- **Per live-X change:** run the named Xorg-dummy scale test, then the complete live-X suite. [VERIFIED: .planning/phases/01-topology-snapshot-and-live-x-harness/01-VALIDATION.md:38-44]
- **Per wave merge:** run the full wrapped native suite. [VERIFIED: .planning/phases/01-topology-snapshot-and-live-x-harness/01-VALIDATION.md:36-42]
- **Phase gate:** full native suite, focused scale Xorg-dummy test, complete live-X suite, documentation build, format check, and native-Linux ASan CI responsibility must be green. [VERIFIED: AGENTS.md:32-36]

### Wave 0 Gaps

- [ ] `tests/test_monitor_scale.c` — pure parser, DPI, precedence, rounding, drift, and overflow coverage. [ASSUMED]
- [ ] `tests/test_monitor_scale_config.c` — sequential store, exact matching, allocation retention, and explanation coverage. [ASSUMED]
- [ ] `monitor-scale` and `monitor-scale-config` registrations in `tests/meson.build`. [ASSUMED]
- [ ] `scale-policy-xorg-dummy` scenario/registration, built by extending the existing harness rather than creating a second harness. [ASSUMED]
- [ ] Scenario-owned extra startup command support in `fvwm_arguments()` with self-tests preserving argv-only execution. [VERIFIED: tests/live_x/live_x_harness.py:678-716]

No new test framework or fixture service is needed. [VERIFIED: tests/meson.build:1-199]

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No authentication surface is introduced by local WM configuration. [VERIFIED: phase scope in .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:9-14] |
| V3 Session Management | no | No application session/token mechanism is introduced. [VERIFIED: phase scope in .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:9-14] |
| V4 Access Control | no | Existing user-local config authority is unchanged; no new privilege boundary is added. [ASSUMED] |
| V5 Input Validation | yes | Strict token count, canonical factor allowlist, exact-name matching, full range/consistency validation, safe output initialization. [CITED: https://cwe.mitre.org/data/definitions/20.html] |
| V6 Cryptography | no | No cryptographic operation or secret is in scope. [VERIFIED: phase scope in .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:9-14] |

### Known Threat Patterns for C/X11 Configuration

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Malformed/extra numeric syntax changes policy | Tampering | Direct allowlist recognizer, exact argument count, and last-valid transactional commit. [CITED: https://cwe.mitre.org/data/definitions/20.html] |
| Integer overflow in DPI/conversion arithmetic | Tampering / Denial of Service | Check before arithmetic, use sufficiently wide exact intermediates, initialize output, return explicit failure. [CITED: https://wiki.sei.cmu.edu/confluence/display/c/INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow] |
| Crafted monitor name forges diagnostic lines | Repudiation | Encode names with the existing cJSON string pattern; do not log raw names in structured one-line explanations. [VERIFIED: fvwm/builtins.c:2142-2155] |
| Many malformed declarations erase usable config | Denial of Service | Invalid declarations do not mutate prior valid entries and lower precedence remains available. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:70-80] |
| Hotplug exposes uninitialized/stale scale | Tampering / Denial of Service | Safe-initialize monitor scale and recompute inside the changed-topology ordering before downstream work. [VERIFIED: fvwm/events.c:1693-1712] |

### Security Verification

- Fuzz/table-drive the finite factor recognizer with empty, long, control-byte, embedded-NUL-at-API, suffix, and extra-token cases; the config-line parser itself supplies NUL-terminated tokens. [ASSUMED]
- Exercise maximum/minimum pixel conversion and extreme positive topology dimensions under UBSan in the normal CI suite; ASan remains native Linux CI. [VERIFIED: AGENTS.md:32-36]
- Verify diagnostics escape monitor names and never use them as a format string. [VERIFIED: fvwm/builtins.c:2142-2155]
- Verify every failure leaves previous config/result and initialized outputs intact. [VERIFIED: AGENTS.md:196-205]

## Sources

### Primary (HIGH confidence)

- `.planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md` — all locked scale/config policy decisions and phase boundaries. [VERIFIED: .planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md:9-94]
- `libs/monitor_topology.[ch]` — exact committed facts, uniqueness, equality, and epoch semantics. [VERIFIED: libs/monitor_topology.h:21-69; libs/monitor_topology.c:178-399]
- `libs/FScreen.[ch]` — committed getter, sole live query, stable legacy monitor model, and forbidden legacy alias behavior. [VERIFIED: libs/FScreen.h:95-167; libs/FScreen.h:218-261; libs/FScreen.c:277-365; libs/FScreen.c:631-790]
- `fvwm/fvwm3.c`, `fvwm/events.c`, `fvwm/functable.c`, `libs/Parse.c`, `fvwm/builtins.c` — startup, changed-topology, command, token, diagnostics, and inspection seams. [VERIFIED: fvwm/fvwm3.c:2306-2378; fvwm/events.c:1693-1712; fvwm/functable.c:130-249; libs/Parse.c:300-521; fvwm/builtins.c:2142-2273]
- `tests/meson.build`, `tests/live_x/live_x_harness.py`, Phase 1 validation/summaries — native and live-X reuse contracts. [VERIFIED: tests/meson.build:1-199; tests/live_x/live_x_harness.py:678-716; .planning/phases/01-topology-snapshot-and-live-x-harness/01-VALIDATION.md:16-44]

### Secondary / Official References (seam classified LOW)

- SEI CERT C INT32-C — signed-overflow and checked arithmetic guidance. [CITED: https://wiki.sei.cmu.edu/confluence/display/c/INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow]
- SEI CERT C ERR34-C — complete string-to-number error detection and `sscanf()` limitations. [CITED: https://wiki.sei.cmu.edu/confluence/display/c/ERR34-C.+Detect+errors+when+converting+a+string+to+a+number]
- MITRE CWE-20 — strict known-good input validation, syntax/range/consistency checks. [CITED: https://cwe.mitre.org/data/definitions/20.html]
- MITRE CWE-190 — integer-overflow consequences and range/width mitigations. [CITED: https://cwe.mitre.org/data/definitions/190.html]

### Tertiary (LOW confidence)

- X.org RandR protocol documentation fetched from the current release describes separate pixel/mm facts and zero physical size for unknown output dimensions, but the fetched document identifies itself as RandR 1.4 rather than the required 1.5 Monitor specification. Phase 2 therefore relies on the opened Phase 1 XRRMonitorInfo adapter as the authoritative implementation contract. [CITED: https://www.x.org/releases/current/doc/randrproto/randrproto.txt]

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependency; all relevant build/parser/X facilities were opened in this session. [VERIFIED: libs/meson.build:1-80; tests/meson.build:1-199]
- Architecture: HIGH — integration order, topology ownership, parser lifetime, and command seams are verified in current source. [VERIFIED: fvwm/events.c:1693-1712; fvwm/fvwm3.c:2306-2378; libs/Parse.c:300-521]
- Numeric policy: MEDIUM — locked outcomes are exact and `__int128` was compile-probed through both available routes, but the 10-percent denominator needs one pre-plan lock. [VERIFIED: local and `tests/run_in_linux.py` compile probes, 2026-08-30]
- Pitfalls: HIGH — most derive directly from current source and locked decisions; compiler-width details are backed by official CERT guidance. [CITED: https://wiki.sei.cmu.edu/confluence/display/c/INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow]
- Validation architecture: HIGH — Phase 1's native router and authoritative Xorg-dummy harness are implemented and previously verified. [VERIFIED: .planning/phases/01-topology-snapshot-and-live-x-harness/01-VERIFICATION.md:73-91]

**Research date:** 2026-08-30
**Valid until:** 2026-09-29 (stable in-repo C/X11 architecture; recheck if Phase 1 topology or Status contracts change)
