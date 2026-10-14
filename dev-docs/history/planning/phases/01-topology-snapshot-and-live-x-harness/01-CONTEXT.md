# Phase 1: Topology Snapshot and Live-X Harness - Context

**Gathered:** 2026-08-29
**Status:** Ready for planning

<domain>
## Phase Boundary

Establish one complete, atomic RandR 1.5 monitor snapshot and a reproducible
live-X harness for monitor creation, mutation, and removal. This phase records
topology facts and commit behavior only; scale policy, monitor ownership,
window recovery, and module scale metadata remain in later phases.

</domain>

<decisions>
## Implementation Decisions

### Snapshot identity and inspection
- **D-01:** Use the RandR Monitor name/atom as the stable identity within an X
  server session. A name change is a remove/add transition; output XIDs are
  membership facts, not identity. — **Reversibility:** costly — Later scale,
  ownership, hotplug, and module contracts will key their state to this
  identity.
- **D-02:** Each snapshot monitor contains root-pixel geometry, raw physical
  dimensions, primary state, and the complete output membership. Preserve zero
  or implausible physical dimensions as observable input for later scale
  fallback policy rather than coercing them in this phase.
- **D-03:** Compare output membership as a set and serialize it in sorted order.
  Order monitors deterministically by stable identity so RandR enumeration or
  output ordering noise is not a topology change.
- **D-04:** Define a versioned JSON representation as the canonical inspection
  format and derive concise human-readable text from the same snapshot data.
  Expose it through an existing command/status path; do not introduce a new
  service or prematurely change the module protocol. — **Reversibility:**
  costly — Once scripts consume the JSON fields, changing their meaning or
  shape requires a versioned contract transition.

### Atomic commit and epochs
- **D-05:** Query and fully copy topology into a detached candidate, validate
  its structure, and publish it with one swap. Consumers must never observe a
  partially updated monitor collection.
- **D-06:** Structural validation covers successful RandR queries, resolvable
  monitor names, safe counts and dimensions, complete output copies, and
  allocation success. Physical DPI plausibility is not a Phase 1 validity
  condition.
- **D-07:** Failure to obtain a valid initial snapshot is a clear startup
  failure. A runtime query, allocation, or structural-validation failure keeps
  the last good snapshot and its epoch active and emits a useful diagnostic.
- **D-08:** The first committed snapshot has epoch 1. Advance the epoch exactly
  once for a semantic topology change; duplicate events, enumeration-order
  changes, and failed refreshes do not advance it. — **Reversibility:** costly
  — Downstream transition and module consumers will use the epoch to identify
  committed topology generations.

### Burst settling behavior
- **D-09:** RandR notifications mark topology dirty and arm at most one deferred
  refresh. Commit after the notification queue settles rather than querying
  and mutating state for every event.
- **D-10:** Use monotonic time and a hard maximum-latency cap so a continuous
  event storm cannot defer topology commits indefinitely. The mechanism must
  use the existing single-threaded event loop and must not sleep or block.
- **D-11:** One settled burst yields at most one semantic commit and one epoch
  advance. If the candidate equals the committed snapshot, clear the dirty
  state without downstream update work.

### Live-X harness contract
- **D-12:** Use dependency-free Python 3 orchestration for process lifecycle,
  readiness polling, hard timeouts, isolated display allocation, and guaranteed
  cleanup. Do not add pytest or another harness dependency.
- **D-13:** Run fast Xvfb RandR Monitor object smoke scenarios once per pull
  request. Run authoritative Xorg-dummy output and mode scenarios in one
  dedicated native-Linux CI lane rather than duplicating live-X work across the
  compiler matrix.
- **D-14:** Both backends cover monitor creation, mutation, and removal. The
  authoritative lane must validate committed snapshots and epochs against the
  server's queried state, including burst coalescing and failed-query recovery.
- **D-15:** Avoid fixed sleeps. Poll explicit readiness and state predicates
  until bounded deadlines. On failure retain server logs, FVWM logs, `xrandr`
  state, and the last committed snapshot.
- **D-16:** Treat Xvfb monitor mutation as capability-gated because the verified
  backend accepts `--setmonitor` without changing queried Monitor state. The
  Xvfb scenario must attempt creation, mutation, and removal, assert the state
  transitions when the capability probe succeeds, and otherwise record only
  the mutation subscenario as unsupported while retaining mandatory startup,
  query, inspection, epoch, and cleanup checks. Xorg-dummy remains the
  non-skippable authority for all topology mutation behavior.

### The Agent's Discretion
- Choose the existing command/status integration point and exact command name
  for snapshot inspection without adding a new service or Phase 7 module
  protocol work.
- Choose JSON field names and formatting within a versioned schema while
  preserving all facts and semantics locked above.
- Choose the quiet-period and maximum-latency constants for burst settling;
  justify them with live-X tests and keep them easy to tune.
- Choose the internal fixture and scenario organization for the Python harness,
  provided scenarios stay dependency-free, deterministic, and backend-aware.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Scope and requirements
- `.planning/ROADMAP.md` Phase 1 — Defines the phase goal, fixed boundary, and
  success criteria.
- `.planning/REQUIREMENTS.md` `TOP-01`, `TOP-03`, and `TOP-06` — Defines the
  required snapshot facts, atomic burst behavior, failure retention, and live-X
  coverage.
- `.planning/PROJECT.md` Constraints — Defines the single-threaded,
  nonblocking, X11/XRandR-only architecture and native-Linux verification rule.

### Existing architecture and verification
- `.planning/codebase/ARCHITECTURE.md` — Maps the monitor model, RandR event
  path, shared state, and bounded event-loop constraints.
- `.planning/codebase/TESTING.md` — Defines current Meson test patterns and the
  absence of existing X-server E2E coverage.
- `.planning/codebase/STACK.md` — Defines the XRandR 1.5+, Python 3, Meson, and
  CI environment available to this phase.

No external specifications or ADRs were referenced during discussion.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `libs/FScreen.c` / `libs/FScreen.h`: Existing RandR 1.5 query, monitor lookup,
  monitor ordering, primary tracking, and debug-dump behavior to replace or
  adapt behind the committed snapshot contract.
- `fvwm/event_loop_policy.c`: Existing bounded event-loop policy pattern for
  keeping topology work fair and nonblocking.
- `tests/meson.build`: Existing registration point for assertion-based native
  tests and new live-X test entry points.
- Python 3 configure-time dependency: Available for a standard-library-only
  live-X process harness without adding a package dependency.

### Established Patterns
- Authoritative X and window state remains in the single-threaded `fvwm3`
  process.
- Recoverable failures return explicit status and log through `fvwm_debug()`;
  startup invariants may fail the process clearly.
- Pure policy is extracted into narrow C units and tested with direct assertions;
  real OS/X primitives are preferred over mocks at integration boundaries.
- CI currently runs the same Meson unit suite across GCC/Clang and Linux
  distributions, so expensive live-X scenarios should not be multiplied across
  every matrix leg.

### Integration Points
- `libs/FScreen.c:scan_screens()` currently mutates global monitor records while
  reading `XRRGetMonitors()` and stores only the first output; candidate creation
  and atomic publication replace this direct-mutation path.
- `fvwm/events.c:dispatch_event()` currently refreshes, updates EWMH, broadcasts,
  and recreates pan frames for every `RRScreenChangeNotify` or `RRNotify`; this
  path becomes dirty marking plus deferred commit handling.
- `libs/FScreen.c:monitor_dump_state()` is debug-only text and lacks physical
  dimensions, complete membership, and epoch; inspection should be derived from
  the committed snapshot instead.
- `.github/workflows/ccpp.yml` is the current build/sanitizer matrix; Phase 1
  needs a non-duplicated live-X smoke path and a dedicated native Xorg-dummy
  lane.

</code_context>

<specifics>
## Specific Ideas

- Prefer the most modern contract first, then runtime performance, then minimum
  complexity when those priorities conflict.
- Keep inspection useful to both humans and automation without adding a daemon
  or committing early to the later module scale protocol.
- Make failures diagnosable from retained CI artifacts rather than relying on
  reruns or timing-sensitive local reproduction.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 01-topology-snapshot-and-live-x-harness*
*Context gathered: 2026-08-29*
