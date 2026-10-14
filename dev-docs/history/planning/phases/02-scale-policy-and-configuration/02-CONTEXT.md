# Phase 2: Scale Policy and Configuration - Context

**Gathered:** 2026-08-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Resolve one exact, explainable effective scale for every active monitor from an
explicit monitor override, validated automatic DPI, a configured global
fallback, or safe 1x. This phase defines initial configuration, validation,
selection, and pure scale arithmetic. Runtime reload belongs to Phase 6;
monitor ownership and geometry, WM-owned resources, cross-monitor transitions,
and module metadata remain in Phases 3 through 7.

</domain>

<decisions>
## Implementation Decisions

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

### The Agent's Discretion
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

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Scope and requirements
- `.planning/ROADMAP.md` Phase 2 — Defines the fixed phase goal, dependency,
  success criteria, and boundary from later scale consumers.
- `.planning/REQUIREMENTS.md` `SCALE-01` through `SCALE-05` — Defines explicit
  rational overrides, automatic DPI, precedence, visible diagnostics, and
  drift-free supported factors.
- `.planning/PROJECT.md` Constraints and Key Decisions — Defines X11-only
  pixel protocol boundaries, single-threaded nonblocking policy, explicit
  overrides with automatic fallback, and the preference for a coherent new
  contract over legacy compatibility.
- `.planning/STATE.md` Accumulated Context — Carries forward the milestone-wide
  precedence chain and actual-pixel geometry decision.

### Prior-phase contracts
- `.planning/phases/01-topology-snapshot-and-live-x-harness/01-CONTEXT.md` —
  Locks exact RandR Monitor name/atom identity, raw physical-dimension facts,
  atomic snapshots, and topology epoch semantics consumed by Phase 2.

### Existing architecture and conventions
- `.planning/codebase/ARCHITECTURE.md` — Maps the monitor model, shared state,
  command engine, and pure-policy integration constraints.
- `.planning/codebase/STACK.md` — Defines the C11, XRandR 1.5+, Meson, X11, and
  native-test environment available to the phase.
- `.planning/codebase/CONVENTIONS.md` — Defines testable C helper, status-return,
  diagnostics, formatting, and user-visible documentation patterns.

No external specifications or ADRs were referenced during discussion.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `libs/monitor_topology.h`: `monitor_topology_monitor` already provides the
  exact monitor name/atom, pixel width/height, millimetre width/height, and
  primary/output facts required as automatic-policy inputs.
- `libs/FScreen.c` / `libs/FScreen.h`: `FScreenGetTopologySnapshot()` exposes
  the committed Phase 1 snapshot, while existing monitor lookup code shows the
  legacy numeric and dynamic aliases that scale overrides must deliberately not
  accept.
- `fvwm/functable.c` and the command parser helpers used throughout `fvwm/`:
  Existing table-driven command dispatch and token parsing are the integration
  pattern for the two configuration directives.
- `tests/test_monitor_topology.c` and `tests/meson.build`: Existing assertion
  fixtures preserve zero, extreme, and changed physical dimensions and provide
  the native-test registration pattern for pure scale policy.

### Established Patterns
- Authoritative monitor and X state stays in the single-threaded `fvwm3`
  process; scale selection must not add blocking work or a background owner.
- Pure arithmetic and policy belong in narrow C units with explicit status
  results and direct native tests before X11 side effects are wired.
- Recoverable invalid external input logs through `fvwm_debug(__func__, ...)`
  and leaves prior valid state intact rather than silently coercing values.
- User-visible commands are registered in `fvwm/functable.c` and documented in
  `doc/fvwm3_manpage_source.adoc`.

### Integration Points
- `FScreenGetTopologySnapshot()` is the authoritative source for active monitor
  identity and DPI facts; scale policy must not query RandR independently.
- Effective scale and its source/reason need an authoritative association with
  each committed monitor before Phase 3 or Phase 4 consumers can use it.
- Initial config flows through the existing command engine. Runtime reload and
  affected-window reconciliation are explicitly deferred to Phase 6.
- Existing Status/PrintInfo topology output and `fvwm_debug()` provide candidate
  explanation paths, but exact observability wiring remains planner discretion
  within SCALE-04 and the phase boundary.

</code_context>

<specifics>
## Specific Ideas

- Prefer strict, explainable rejection over silent coercion or saturation.
- Keep the user-facing scale syntax simple and exact: decimal quarter steps,
  literal monitor names, and a separate global fallback.
- The user accepted the recommended contract for all remaining choices after
  the initial scale and DPI questions; downstream agents should treat the full
  decision set above as locked rather than reopening individual defaults.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope. Existing roadmap deferrals remain:
arbitrary percentages belong to a future compatibility phase, and runtime scale
reload belongs to Phase 6.

</deferred>

---

*Phase: 02-scale-policy-and-configuration*
*Context gathered: 2026-08-30*
