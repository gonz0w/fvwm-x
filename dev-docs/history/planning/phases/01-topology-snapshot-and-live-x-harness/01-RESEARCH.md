# Phase 1: Topology Snapshot and Live-X Harness - Research

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

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
- **D-16:** Treat Xvfb Monitor mutation as observed-state capability-gated:
  attempt create/mutate/remove, require transitions when the probe succeeds,
  otherwise record only that mutation subscenario unsupported while startup,
  query, inspection, epoch, and cleanup remain mandatory. Xorg-dummy is the
  non-skippable mutation authority.

### the agent's Discretion
- Choose the existing command/status integration point and exact command name
  for snapshot inspection without adding a new service or Phase 7 module
  protocol work.
- Choose JSON field names and formatting within a versioned schema while
  preserving all facts and semantics locked above.
- Choose the quiet-period and maximum-latency constants for burst settling;
  justify them with live-X tests and keep them easy to tune.
- Choose the internal fixture and scenario organization for the Python harness,
  provided scenarios stay dependency-free, deterministic, and backend-aware.

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| TOP-01 | User can inspect one committed RandR 1.5 Monitor snapshot containing stable identity, root-pixel geometry, physical dimensions, primary state, output membership, and topology epoch. | The snapshot model, one-swap publication, Status JSON v4 schema, and `PrintInfo Topology` mapping below define the implementation contract. [VERIFIED: `.planning/REQUIREMENTS.md:8-15`] |
| TOP-03 | User sees topology event bursts coalesced into one validated atomic commit, with the last good topology retained after query failure. | The monotonic two-deadline state machine, query-result distinction, immutable candidate, and failure-injection test seam below directly cover this behavior. [VERIFIED: `.planning/REQUIREMENTS.md:10-15`] |
| TOP-06 | Maintainers can reproduce monitor creation, changes, and removal through Xvfb smoke tests and authoritative Xorg-dummy scenarios. | The capability-aware Xvfb smoke path and mandatory Xorg-dummy lane below are based on live probes of both Debian backends. [VERIFIED: `.planning/REQUIREMENTS.md:10-15`; empirical Debian X server probes, 2026-08-29] |
</phase_requirements>

**Researched:** 2026-08-29
**Domain:** XRandR 1.5 Monitor snapshots, single-threaded event-loop settling, and nested-X integration testing
**Confidence:** HIGH for repository/XRandR architecture; MEDIUM overall because Xvfb Monitor mutation is backend-dependent

## Summary

The phase should replace `scan_screens()` as the source of truth with an
immutable, detached `monitor_topology_snapshot`. The current representation is
not complete: `struct screen_info` is exactly `name`, `x, y, w, h`, and one
`rr_output`, and `monitor_set_coords()` selects only the first output with
`m->si->rr_output = rrm.noutput > 0 ? *rrm.outputs : None;`. [VERIFIED:
`libs/FScreen.h:59-65`; `libs/FScreen.c:423-443`] The current scan also clears
and mutates live monitor flags before the query has been fully validated, and
the event dispatcher immediately performs scan, EWMH, broadcast, and pan-frame
work for every RandR event. [VERIFIED: `libs/FScreen.c:576-695`;
`fvwm/events.c:4116-4137`] Build the candidate completely, validate and sort it,
compare it semantically, assign its epoch, then publish with one pointer swap.
Only a changed publication should reconcile the legacy `monitor_q` view and run
downstream work.

Event coverage must include core root `ConfigureNotify`, not only RandR masks.
The RandR 1.5 protocol states that both `RRSetMonitor` and `RRDeleteMonitor`
generate `ConfigureNotify` on the root. [VERIFIED: X.Org xorgproto 2024.1
`randrproto.txt:1674-1728`] FVWM currently selects the quoted root mask
`LeaveWindowMask| EnterWindowMask | PropertyChangeMask |
SubstructureRedirectMask | KeyPressMask | KeyReleaseMask |
SubstructureNotifyMask | ColormapChangeMask | ButtonPressMask |
ButtonReleaseMask`, which omits `StructureNotifyMask`, while `FScreenSelect()`
selects only `RRScreenChangeNotifyMask | RROutputChangeNotifyMask`. [VERIFIED:
`fvwm/eventmask.h:55-60`; `libs/FScreen.c:410-415`] Add root
`StructureNotifyMask`, subscribe to the topology-relevant RandR screen/CRTC/
output/output-property/resource masks, and make all of those entry points only
mark one topology refresh state dirty.

For tests, use standard-library Python and one dedicated native-Linux CI job.
Debian Xorg 21.1.16 with dummy 0.4.0 successfully exposed multiple `DUMMY*`
outputs, writable millimetre properties, and Monitor create/delete operations in
this research session. [VERIFIED: empirical Podman x86_64 probe against Debian
trixie packages, 2026-08-29] Debian Xvfb 21.1.16 advertised RandR 1.6 but silently
kept its default `screen` monitor after `--setmonitor`, so Xvfb cannot be the
authority for Monitor mutation on this image. [VERIFIED: empirical Podman
x86_64 probe, 2026-08-29] Keep the Xvfb lane as a startup/query/inspection smoke
with a capability-gated mutation subscenario; require all create/mutate/remove,
epoch, coalescing, and failure-recovery assertions to pass under Xorg-dummy.

**Primary recommendation:** Introduce a pure snapshot model plus pure refresh
deadline policy, adapt `FScreen` into the sole XRandR query/publish boundary,
drive it from `events.c`, expose it as Status JSON v4 and `PrintInfo Topology`,
and prove it in one opt-in `live-x` Meson/CI suite.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| RandR Monitor query and raw fact copy | API / Backend (`fvwm3` core via `FScreen`) | External X server | Only the core owns authoritative X state, and `FScreen` is already the XRandR adapter. [VERIFIED: `AGENTS.md:20-25`; `libs/FScreen.c:697-758`] |
| Snapshot validation, ordering, equality, and epoch | API / Backend (pure C domain model) | Database / Storage (immutable in-process snapshot) | Policy must be unit-testable without X and publication is in-memory, not persistent. [VERIFIED: `AGENTS.md:32-36`; `tests/meson.build:1-29`] |
| Burst settling and maximum latency | API / Backend event loop | — | `HandleEvents()` and `My_XNextEvent()` own work ordering and `select()` timeout calculation. [VERIFIED: `fvwm/events.c:4208-4455`] |
| JSON and text inspection | API / Backend command/status path | Browser / Client equivalent (external FIFO readers) | Existing `Status` emits JSON through a FIFO and `PrintInfo` writes human-readable diagnostics. [VERIFIED: `fvwm/builtins.c:131-215,217-257,2204-2221,2632-2675`] |
| Live topology manipulation | Test / CI harness | External Xvfb or Xorg-dummy server | The harness owns processes and assertions; the nested server remains the authoritative X dependency. [CITED: https://www.x.org/releases/current/doc/man/man1/Xvfb.1.xhtml] |

## Project Constraints (from AGENTS.md)

- Target X11/XRandR 1.5+ only; do not add a native Wayland path. [VERIFIED: `AGENTS.md:18-25`]
- Keep authoritative X state single-threaded and preserve event-loop ordering. [VERIFIED: `AGENTS.md:20-25`]
- Add no sleeps, subprocess work, filesystem work, or other deliberate blocking to the main event loop; the refresh performs one coalesced X server query only. [VERIFIED: `AGENTS.md:20-25`]
- Keep rendering, compositing, scale policy, ownership, recovery, and module-protocol work outside this phase. [VERIFIED: `AGENTS.md:27-33`; `.planning/phases/01-topology-snapshot-and-live-x-harness/01-CONTEXT.md:6-12`]
- Give pure policy native C unit coverage and X-visible behavior nested-X coverage in native Linux CI; use Podman x86_64 emulation locally on macOS and leave ASan to native Linux CI. [VERIFIED: `AGENTS.md:32-36`]
- Build project-owned C as C11; put ordinary helpers in lowercase snake-case files/APIs, reserve `F` filenames for third-party wrappers, mark private helpers `static`, avoid new unprefixed globals, and preserve established public spellings. [VERIFIED: `AGENTS.md:43-55,152-172`]
- Format C with root `.clang-format`: tabs, Mozilla braces, right-aligned pointers, sorted includes, 80 columns; use four spaces in Meson. [VERIFIED: `AGENTS.md:174-188`]
- Include generated `config.h` first in each C implementation and do not introduce cross-layer `.c` inclusion. [VERIFIED: `AGENTS.md:377-387`]
- Validate sizes before arithmetic/allocation, initialize outputs safely, return explicit statuses for recoverable failures, use one cleanup path where useful, and reserve `fxmalloc`-family calls for unrecoverable allocations. [VERIFIED: `AGENTS.md:196-205`]
- Log recoverable runtime errors with `fvwm_debug(__func__, ...)`; do not add unconditional debug printing. [VERIFIED: `AGENTS.md:207-212`]
- Put a GPL notice on new C sources/scripts, not headers; document the user-visible Status schema and `PrintInfo` subject in AsciiDoc. [VERIFIED: `AGENTS.md:214-220`]
- Name standalone tests `test_<unit>.c`, name cases `static void test_<behavior>(void)`, and invoke cases explicitly from `main()`. [VERIFIED: `AGENTS.md:154-162`]

## Standard Stack

### Core

| Library / facility | Version | Purpose | Why Standard |
|--------------------|---------|---------|--------------|
| C / project core | C11 / FVWM-X 1.1.6 | Snapshot model, checked ownership, event-loop integration | This is the repository's fixed implementation language and version. [VERIFIED: `AGENTS.md:43-47`] |
| libXrandr | Required API >=1.5; build image 1.5.4; upstream 1.5.5 published 2026-01-26 | `XRRGetMonitors`, Monitor fields, event selection | Already required by the project; 1.5 is the first Monitor-object API. [VERIFIED: `AGENTS.md:86-90`; X.Org release index and libXrandr 1.5.5 source] |
| POSIX `clock_gettime(CLOCK_MONOTONIC)` | POSIX.1-2024 Issue 8 | Quiet and maximum-latency deadlines | Monotonic intervals are unaffected by real-time clock changes. [CITED: https://pubs.opengroup.org/onlinepubs/9799919799/functions/clock_getres.html] |
| cJSON | Vendored 1.7.17 | Status topology serialization | It is already linked into `libfvwm3` and used by `status_send()`; no new JSON dependency is needed. [VERIFIED: `AGENTS.md:86-98`; `libs/meson.build:1-60`; `fvwm/builtins.c:131-215`] |

### Supporting

| Tool | Verified Version | Purpose | When to Use |
|------|------------------|---------|-------------|
| Python | 3.13.3 in build image | Standard-library process harness, polling, JSON parsing, cleanup | Live-X tests only; add no PyPI dependencies. [VERIFIED: Podman image probe, 2026-08-29; `meson.build:245-250`] |
| Meson / Ninja | 1.7.0 / 1.12.1 in build image; Meson floor 1.5.1 | Build and test registration | Existing unit suite and opt-in `live-x` suite. [VERIFIED: Podman image probe, 2026-08-29; `AGENTS.md:75-80`] |
| xrandr CLI | 1.5.3, published 2024-11-07 | Manipulate/query Monitor objects and outputs in scenarios | Both nested-X backends. [VERIFIED: X.Org xrandr 1.5.3 release and image probe] |
| Xvfb | Xorg 21.1.16 Debian package | Fast headless baseline/startup smoke | Capability-gated smoke, never the authoritative mutation backend. [VERIFIED: Debian trixie apt/image probe, 2026-08-29; CITED: https://www.x.org/releases/current/doc/man/man1/Xvfb.1.xhtml] |
| Xorg + dummy DDX | Xorg 21.1.16 + Debian dummy 0.4.0; upstream dummy 0.4.1 published 2023-05-07 | Deterministic outputs, modes, mm dimensions, Monitor mutations | Mandatory authoritative native-Linux lane. [VERIFIED: Debian trixie apt/image probe and X.Org dummy release source, 2026-08-29] |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Python standard library | pytest | Rejected: D-12 explicitly forbids another harness dependency. |
| Xorg-dummy authoritative lane | Xvfb alone | Rejected: current Xvfb accepted commands but did not mutate Monitor objects in the live probe. [VERIFIED: empirical Debian Xvfb 21.1.16 probe, 2026-08-29] |
| Existing cJSON | New JSON library or handwritten escaping | Rejected: adds dependency/supply-chain surface or repeats escaping/allocation edge cases. [VERIFIED: `libs/meson.build:40`; `fvwm/builtins.c:148-215`] |
| Immutable snapshot | Mutate `monitor_q` while enumerating | Rejected: it cannot meet D-05/D-07 atomicity and failure-retention requirements. [VERIFIED: current direct mutation in `libs/FScreen.c:576-695`] |

**CI installation:**

```bash
apt-get update
apt-get install -y xvfb x11-xserver-utils xserver-xorg-core \
  xserver-xorg-video-dummy
```

The Debian trixie candidates verified in the project image were quoted as
`xvfb=2:21.1.16-1.3+deb13u3`, `x11-xserver-utils=7.7+11`,
`xserver-xorg-core=2:21.1.16-1.3+deb13u3`, and
`xserver-xorg-video-dummy=1:0.4.0-1`. [VERIFIED: Debian apt metadata/image
probe, 2026-08-29]

## Package Legitimacy Audit

No npm, PyPI, or crates packages are introduced, so the package-legitimacy
seam does not apply. The phase uses existing/vendored code and Debian operating
system packages only. [VERIFIED: Standard Stack above]

| Package | Registry | Version | Source | Verdict | Disposition |
|---------|----------|---------|--------|---------|-------------|
| `xvfb` | Debian trixie apt | `2:21.1.16-1.3+deb13u3` | X.Org xserver | N/A — OS package | Approved for CI |
| `x11-xserver-utils` | Debian trixie apt | `7.7+11` | X.Org apps | N/A — OS package | Approved for CI |
| `xserver-xorg-core` | Debian trixie apt | `2:21.1.16-1.3+deb13u3` | X.Org xserver | N/A — OS package | Approved for CI |
| `xserver-xorg-video-dummy` | Debian trixie apt | `1:0.4.0-1` | X.Org dummy DDX | N/A — OS package | Approved for CI |

**Packages removed due to SLOP verdict:** none
**Packages flagged as suspicious:** none

## Architecture Patterns

### System Architecture Diagram

```text
Entry points
  RandR Screen/CRTC/Output/Property/Resource events
  root ConfigureNotify (RRSetMonitor/RRDeleteMonitor)
                         |
                         v
              mark topology refresh dirty
                         |
                         v
        [quiet deadline OR maximum deadline reached?]
                 | no                 | yes
                 |                    v
                 |          one XRRGetMonitors(True)
                 |                    |
                 |                    v
                 |       detached deep-copy candidate
                 |                    |
                 |                    v
                 |          validate + canonical sort
                 |                    |
                 |          +---------+----------+
                 |          | invalid/query fail |
                 |          v                    v
                 |   log + retain old      semantic compare
                 |   snapshot/epoch          |          |
                 |                         equal      changed
                 |                           |          |
                 +<----------------clear dirty         v
                                            epoch + one pointer swap
                                                       |
                                                       v
                                    reconcile legacy monitor_q view
                                                       |
                                                       v
                                EWMH + broadcast + pan frames + Status once
                                                       |
                                      +----------------+----------------+
                                      v                                 v
                             Status JSON v4                    PrintInfo Topology

Test boundary
  Python stdlib harness -> Xvfb smoke / Xorg-dummy authority -> X server
          |                    |                     |
          +-> xrandr mutations +-> fvwm3 + Status FIFO +-> state assertions/logs
```

### Component Responsibilities

| Component | Recommended Responsibility | Implementation Mapping |
|-----------|----------------------------|------------------------|
| Pure topology model | Own candidate allocation, deep copy, validation, canonical sorting, semantic equality, epoch assignment, and destruction; contain no X calls or global `Scr` access. | New `libs/monitor_topology.c` and `libs/monitor_topology.h` |
| XRandR adapter / publisher | Perform one `XRRGetMonitors(..., True, ...)`, resolve names, transfer facts into the candidate, publish one pointer, stage/reconcile legacy monitor nodes, and expose the committed snapshot getter. | Adapt `libs/FScreen.c` and `libs/FScreen.h` |
| Refresh policy | Track dirty state, first/last event monotonic timestamps, quiet/max deadlines, and next timeout using injected time values. | New `fvwm/topology_refresh_policy.c` and `.h` |
| Event-loop coordinator | Recognize topology events, mark dirty, merge the refresh timeout into `select()`, run due refreshes before delivering more work, and invoke downstream updates once on change. | Adapt `fvwm/events.c` and `fvwm/eventmask.h` |
| Inspection | Add snapshot JSON from the committed object to Status and concise lines to a new `PrintInfo Topology` subject. | Adapt `fvwm/builtins.c`, `doc/fvwm3_manpage_source.adoc` |
| Native tests | Exercise the model and refresh state machine with plain assertions and allocator/time fakes. | New `tests/test_monitor_topology.c`, `tests/test_topology_refresh_policy.c` |
| Live-X harness | Own nested server/FVWM lifecycle, mutations, polling, fault injection, cleanup, and artifact retention. | New `tests/live_x/` files plus `tests/meson.build`, `meson.options`, and `.github/workflows/ccpp.yml` |

### Recommended Project Structure

```text
libs/
├── FScreen.c                         # XRandR adapter and committed publication
├── FScreen.h                         # committed snapshot access for core consumers
├── monitor_topology.c                # pure snapshot model
└── monitor_topology.h
fvwm/
├── events.c                          # dirty marking, timeout merge, due refresh
├── eventmask.h                       # root StructureNotifyMask
├── topology_refresh_policy.c         # pure two-deadline state machine
└── topology_refresh_policy.h
tests/
├── test_monitor_topology.c
├── test_topology_refresh_policy.c
├── meson.build
└── live_x/
    ├── live_x_harness.py             # stdlib-only orchestration and scenarios
    ├── xorg-dummy.conf               # deterministic virtual screen/modes
    └── xrr_query_fault.c             # Linux LD_PRELOAD query-failure shim
doc/
└── fvwm3_manpage_source.adoc          # Status v4 and PrintInfo Topology
```

### Pattern 1: Detached Candidate, One-Swap Publication

**What:** Treat the committed topology as an immutable owned object. A refresh
allocates and deep-copies every monitor name and every output list into a
detached candidate, validates it, sorts monitors by identity and outputs by XID,
then compares all semantic facts. Publication sets epoch `1` for the first
snapshot or old epoch plus one for a change and replaces the committed pointer
exactly once. This is the prescribed interpretation of D-05 through D-08.

**Validation rules:**

1. Initialize the count to `-1`; `XRRGetMonitors()` also sets it to `-1` before
   requesting a reply. A `NULL` result with `-1` is failure, while a successful
   zero-monitor reply finishes with count `0`. [VERIFIED: X.Org libXrandr
   1.5.5 `src/XrrMonitor.c:36-75,146-150`]
2. Reject a runtime candidate with no active monitors, but do not destroy the
   committed snapshot; an initial empty candidate is a clear startup failure.
   This preserves current toolkit use of `get_active=True`, for which the
   protocol returns only non-0x0 monitors. [VERIFIED: X.Org xorgproto 2024.1
   `randrproto.txt:1654-1672`]
3. Require a non-`None` Monitor Atom, successful `XGetAtomName`, valid UTF-8 for
   JSON representability, unique identity atoms, positive pixel width/height,
   nonnegative output count, checked allocation arithmetic, and a complete
   copied output array. Preserve zero and implausible millimetre values.
4. Allow the same output to appear in more than one Monitor; RandR explicitly
   permits multiple user-defined monitors on one output. [VERIFIED: X.Org
   xorgproto 2024.1 `randrproto.txt:1693-1704`]
5. Compare identity Atom/name, x/y/width/height, raw mm width/height, primary,
   and the sorted output set. Exclude enumeration order and legacy synthetic
   primary fallback from equality.
6. Use checked `malloc`/`calloc`-style recoverable allocation in the candidate
   path, not `fxcalloc()`, because `fxcalloc()` is reserved for unrecoverable
   allocation and terminates on failure. [VERIFIED: `AGENTS.md:196-203`]

**Legacy view:** Keep existing `struct monitor` nodes stable for current window
and desktop pointers, but make them a derived compatibility view. Stage all new
nodes/strings before publication; after the snapshot swap, update existing
nodes by Monitor Atom, enable/add/disable nodes deterministically, then perform
the existing EWMH/broadcast/pan-frame work. No callback or event dispatch may
run inside reconciliation. Update `monitor_by_output()` to consult every output
in the committed snapshot rather than the quoted single-field comparison
`m->si->rr_output == output`. [VERIFIED: `libs/FScreen.c:316-335`]

### Pattern 2: Two-Deadline Burst Settling

**What:** Store `dirty`, `first_dirty`, and `last_dirty` in one refresh-policy
object. On each relevant event, set `first_dirty` only on the clean-to-dirty
transition and always replace `last_dirty`. The due time is:

```text
min(last_dirty + quiet_period, first_dirty + maximum_latency)
```

Use `CLOCK_MONOTONIC`, return the remaining timeout rounded up to milliseconds,
and merge it with `squeue_get_next_ms()` before the existing `select()` call.
Run a due refresh at the top of every `HandleEvents()` iteration so queued X
events cannot postpone the maximum deadline. The current loop checks deferred
signals/updates before `My_XNextEvent()`, and `My_XNextEvent()` computes the
scheduled-command timeout immediately before `select()`. [VERIFIED:
`fvwm/events.c:4208-4243,4252-4351`]

**Recommended constants:** start with a **50 ms quiet period** and **250 ms
maximum latency**, as named constants in the policy header. These values are a
recommended tuning baseline, not an XRandR standard; unit tests must inject time
and live tests must show one commit for a rapid burst and a commit before the cap
during sustained notifications. [ASSUMED]

**Important:** `XRRGetMonitors()` waits for an X reply through `_XReply`, so it
is a synchronous server round trip. [VERIFIED: X.Org libXrandr 1.5.5
`src/XrrMonitor.c:53-65`] Coalescing must reduce this to one required query per
settled burst; do not add per-output query round trips, sleeps, shell commands,
or filesystem I/O to the core refresh path.

### Pattern 3: Complete Event Coverage

Use these event sources as dirty signals:

- Root `ConfigureNotify` where `event.xconfigure.window == Scr.Root`, enabled by
  adding `StructureNotifyMask` to `XEVMASK_ROOTW`; this is the specified Monitor
  create/delete signal. [VERIFIED: X.Org xorgproto 2024.1
  `randrproto.txt:1702-1728`; `fvwm/fvwm3.c:2231-2242`]
- `RRScreenChangeNotify`; call `XRRUpdateConfiguration(e)` immediately to keep
  Xlib's screen dimensions current, then mark dirty rather than query. [VERIFIED:
  current call in `fvwm/events.c:4123-4130`]
- `RRNotify` subtypes selected by `RRCrtcChangeNotifyMask`,
  `RROutputChangeNotifyMask`, `RROutputPropertyNotifyMask`, and
  `RRResourceChangeNotifyMask`. These exact masks and subtype constants are
  defined by RandR as quoted in `randr.h`. [VERIFIED: X.Org xorgproto 2024.1
  `include/X11/extensions/randr.h:116-141`]

Do not refresh on every unrelated root `PropertyNotify`, provider-property
event, or lease event. The candidate equality check remains the final filter for
duplicate/reordered notifications.

### Pattern 4: Existing Inspection Paths, One Data Source

Use Status as canonical machine output and `PrintInfo` as the human view:

1. Bump the existing Status top-level quoted value from
   `cJSON_AddNumberToObject(msg, "version", 3);` to version `4` and add one
   `topology` object generated from the committed snapshot. [VERIFIED:
   `fvwm/builtins.c:131-215`]
2. Add `PrintInfo Topology` and render it from the same committed snapshot, not
   from `monitor_q`. Existing `PrintInfo` writes subjects to the debug log.
   [VERIFIED: `fvwm/builtins.c:2632-2675`;
   `doc/fvwm3_manpage_source.adoc:3415-3451`]
3. On each semantic topology commit, call `status_send()` once after legacy
   reconciliation. Do not emit for equal candidates or failures.
4. Document the version transition and field semantics. The existing Status
   transport uses the quoted constants `PIPENAME "fvwm3.pipe"` and
   `STATUSENV "FVWM_STATUS_PIPE"`; this phase does not add another socket or
   service. [VERIFIED: `fvwm/builtins.c:217-257`]

**Recommended Status v4 topology shape:**

```json
{
  "version": 4,
  "topology": {
    "epoch": 7,
    "monitors": [
      {
        "name": "DUMMY0",
        "name_atom": 225,
        "primary": true,
        "geometry": {"x": 0, "y": 0, "width": 1920, "height": 1080},
        "physical_mm": {"width": 508, "height": 286},
        "outputs": [63]
      }
    ]
  }
}
```

The exact numbers above are illustrative fixture values. The contract should
use JSON integer numbers for protocol-sized XIDs/dimensions and epoch, a monitor
array sorted by identity, and output arrays sorted numerically. The explicit
`name_atom` preserves the session identity even though `name` is the readable
form.

### Pattern 5: Capability-Aware Smoke, Authoritative Dummy

The Xvfb manual promises a headless dumb framebuffer and configurable X screens,
not physical RandR output/Monitor mutation behavior. [CITED:
https://www.x.org/releases/current/doc/man/man1/Xvfb.1.xhtml] Therefore:

- Always run the Xvfb test and require server startup, RandR >=1.5, FVWM startup,
  initial epoch-1 Status JSON, and clean teardown.
- Probe mutation capability by creating a uniquely named Monitor and polling
  `xrandr --listmonitors`. If it appears, run create/mutate/remove assertions. If
  it does not, record a mutation-subscenario skip while keeping the baseline
  smoke test passing; also assert the ignored command did not advance FVWM's
  epoch.
- Never allow the authoritative Xorg-dummy test to skip after its dependency
  probe succeeds. It must configure `DUMMY0` and `DUMMY1`, set modes/positions/mm
  properties, create/delete user Monitors, change primary state, reverse output
  enumeration, and compare FVWM Status JSON to `xrandr --listmonitors`.
- Use the server's verified `-displayfd fd` option to obtain an unused display
  only after the server is ready, instead of scanning fixed display numbers.
  [VERIFIED: Debian Xvfb/Xorg 21.1.16 `-help` probe, 2026-08-29]
- Launch subprocesses with argv lists and `shell=False`, set `TMPDIR` to a
  private per-test directory, disable TCP with `-nolisten tcp`, use monotonic
  predicate deadlines, and terminate/kill every child in `finally` blocks.

### Anti-Patterns to Avoid

- **Query on every RandR event:** causes repeated synchronous round trips and
  repeated downstream work; mark dirty and settle once instead.
- **Treat every `NULL` from `XRRGetMonitors()` as empty topology:** libXrandr's
  `nmonitors == -1` is the failure discriminator; preserve the last good state.
- **Publish, then finish copying outputs:** violates atomicity and creates
  dangling/partial observations; all names/output arrays must be candidate-owned
  before swap.
- **Use RandR enumeration order:** it produces false epochs; sort monitors by
  identity and outputs as sets.
- **Coerce mm data or primary:** the existing `monitor_check_primary()` adds
  `MONITOR_PRIMARY` to the first monitor if no server primary exists; do not copy
  that synthesized policy into the raw snapshot. [VERIFIED:
  `libs/FScreen.c:368-377,687-694`]
- **Assume `RRNotify` catches Monitor create/delete:** the protocol specifies
  root `ConfigureNotify`; without `StructureNotifyMask`, these changes are
  invisible to the current path.
- **Use the command scheduler's event timestamp for topology deadlines:** use
  monotonic process time and merge one deadline into `select()` instead.
- **Use Xvfb results as output authority:** its Monitor mutation support is not
  guaranteed and was absent in the current Debian probe.
- **Fixed sleeps and display numbers:** these create flaky readiness races and
  parallel-job collisions; use `-displayfd` and state predicates.
- **Production fault-injection command/env hook:** use a Linux test-only
  `LD_PRELOAD` interposer around `XRRGetMonitors` so production behavior has no
  externally triggerable failure switch.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Monitor topology reconstruction | A CRTC/output-to-monitor heuristic | `XRRGetMonitors(..., True, ...)` | RandR 1.5 Monitor objects explicitly separate logical Monitors from hardware outputs. [VERIFIED: X.Org xorgproto 2024.1 `randrproto.txt:150-180`] |
| JSON parser/escaping | `sprintf`-assembled JSON | Existing vendored cJSON | Existing code already owns allocation/escaping/cleanup patterns. [VERIFIED: `fvwm/builtins.c:148-215`] |
| Timer thread or sleep loop | Background worker, signal timer, or `usleep()` | Monotonic deadline state merged into existing `select()` | Preserves single-threaded X ownership and event ordering. [VERIFIED: `AGENTS.md:20-25`; `fvwm/events.c:4252-4378`] |
| Separate inspection daemon/socket | New service or module packet | Status FIFO v4 plus `PrintInfo Topology` | Locked D-04 excludes a new service/protocol, and both paths already exist. |
| Python test framework | pytest fixtures/plugins | `subprocess`, `selectors`, `tempfile`, `json`, `time`, `pathlib`, and `unittest`-style helper functions from stdlib | D-12 forbids a dependency and the scenario needs process orchestration, not a framework. |
| X server emulator | Fake RandR server in Python | Xvfb and Xorg-dummy | Real protocol/event behavior is the subject of TOP-06. |
| Display-number scanning | Search `/tmp/.X11-unix` and guess | X server `-displayfd` | The server performs collision-safe allocation and reports readiness. [VERIFIED: Debian Xserver 21.1.16 `-help` probe, 2026-08-29] |
| Production query-failure switch | User command or permanent env feature | Test-only preload interposer | Prevents a test seam from becoming a runtime denial-of-service control. |

**Key insight:** the deceptively hard parts are not the field copies; they are
failure ownership, event completeness, order-independent equality, deadline
fairness, and real X-server behavior. Reuse XRandR/cJSON/Meson/Python stdlib and
hand-write only the project-specific snapshot and settling policy.

## Runtime State Inventory

This is an in-place monitor-model refactor, so runtime state was checked even
though it is not a rename or persistent-data migration.

| Category | Items Found | Action Required |
|----------|-------------|-----------------|
| Stored data | None — the authoritative topology and legacy monitor collections are process memory rebuilt from XRandR at startup; this project has no application database. [VERIFIED: `AGENTS.md:57-67,134-144`; `libs/FScreen.c:65-73,697-758`] | No data migration. Initialize the first snapshot before normal startup continues. |
| Live service config | The connected X server owns live RandR Monitor objects, outputs, modes, and properties outside git. [VERIFIED: X.Org xorgproto 2024.1 `randrproto.txt:150-180,1654-1728`] | Read only in production. The test harness must create and clean its own nested-server state. |
| OS-registered state | None — this phase does not rename executables, services, launch agents, systemd units, or module registrations. The runtime remains one native X11 process plus child modules. [VERIFIED: `AGENTS.md:57-67`] | None. |
| Secrets/env vars | No secrets. Existing Status derives and exports the quoted `FVWM_STATUS_PIPE`; tests additionally scope `DISPLAY`, `TMPDIR`, and test-only `LD_PRELOAD` to child processes. [VERIFIED: `fvwm/builtins.c:217-257`] | Use a private test temp directory and never export the preload shim outside the spawned FVWM process. |
| Build artifacts | Existing Meson build directories do not know new source files/options until reconfigured. [VERIFIED: `meson.build:495-533`; `tests/meson.build:1-29`] | Re-run `meson setup --reconfigure builddir`; no installed-package migration. |

## Common Pitfalls

### Pitfall 1: Missing Monitor-Object Events
**What goes wrong:** `xrandr --setmonitor` or `--delmonitor` changes the server,
but FVWM never refreshes.
**Why it happens:** RandR 1.5 specifies core root `ConfigureNotify`, while the
current root mask omits `StructureNotifyMask`. [VERIFIED: X.Org xorgproto
2024.1 `randrproto.txt:1702-1728`; `fvwm/eventmask.h:55-60`]
**How to avoid:** add the core mask and root-event check in addition to complete
RandR mask selection.
**Warning signs:** live `xrandr --listmonitors` changes but Status epoch does not.

### Pitfall 2: Conflating Failure with a Valid Empty Reply
**What goes wrong:** a transient reply/allocation failure disables every
monitor or advances the epoch.
**Why it happens:** both cases can return `NULL`; only the initialized count
distinguishes failure (`-1`) from successful zero count (`0`). [VERIFIED: X.Org
libXrandr 1.5.5 `src/XrrMonitor.c:36-75,146-150`]
**How to avoid:** model query status explicitly and leave committed pointer,
epoch, and legacy view untouched on failure.
**Warning signs:** runtime logs show disabled monitors after injected query
failure, or epoch changes without a valid candidate.

### Pitfall 3: Candidate/Legacy Split-Brain
**What goes wrong:** Status shows the new snapshot while placement, EWMH, or
window pointers still use partially updated legacy monitors.
**Why it happens:** publication and legacy reconciliation can allocate/fail in
the wrong order.
**How to avoid:** preflight all allocations, perform one snapshot swap, then
apply a no-fail reconciliation synchronously before any downstream callback.
**Warning signs:** new monitor names appear in JSON before `monitor_by_name()` or
`monitor_by_output()` can resolve them.

### Pitfall 4: False Epochs from Ordering and Synthetic Policy
**What goes wrong:** duplicate notifications, reversed output lists, or the
legacy fallback primary advance the epoch.
**Why it happens:** raw array/tree order or `MONITOR_PRIMARY` flags are compared
instead of canonical server facts.
**How to avoid:** sort/deduplicate each output set, sort by identity, compare raw
primary, and run legacy primary fallback only after publication.
**Warning signs:** reversing `DUMMY0,DUMMY1` to `DUMMY1,DUMMY0` changes epoch.

### Pitfall 5: Debounce Starvation or Busy Loop
**What goes wrong:** continuous events postpone refresh forever, or millisecond
rounding makes `select()` spin at timeout zero.
**Why it happens:** only a trailing-edge quiet deadline is used, or remaining
nanoseconds are truncated rather than rounded up.
**How to avoid:** use `min(last+quiet, first+max)`, round positive remaining time
up to one millisecond, and test exact boundaries with injected timestamps.
**Warning signs:** no epoch during a 500 ms storm, high CPU, or repeated zero-ms
polls.

### Pitfall 6: Treating Xvfb as Authoritative
**What goes wrong:** a test reports success because `xrandr --setmonitor`
returned zero even though the server topology did not change.
**Why it happens:** command exit status is used instead of polling the queried
Monitor list. Debian Xvfb 21.1.16 demonstrated this exact no-op. [VERIFIED:
empirical Podman probe, 2026-08-29]
**How to avoid:** capability-probe by state, mark only the mutation subscenario
unsupported, and require Xorg-dummy assertions.
**Warning signs:** `--listmonitors` always reports only `screen`.

### Pitfall 7: Brittle Xorg-dummy Mutation
**What goes wrong:** replacing an existing user Monitor with another
`--setmonitor` of the same name returns `BadValue` on Debian Xorg 21.1.16.
**Why it happens:** current server behavior does not match the protocol's
delete-and-recreate description in this tested case. [VERIFIED: empirical Xorg
21.1.16/dummy 0.4.0 probe, 2026-08-29; protocol comparison at X.Org xorgproto
2024.1 `randrproto.txt:1681-1707`]
**How to avoid:** test same-identity mutation through automatic Monitor changes
to output mode/position/mm/primary; test user Monitor creation/removal
separately, or send explicit delete/create as one burst.
**Warning signs:** RandR minor opcode 43 fails before FVWM receives a usable
final topology.

### Pitfall 8: Invalid JSON from External Names
**What goes wrong:** an Atom name containing invalid UTF-8 produces a Status
line strict JSON parsers reject.
**Why it happens:** cJSON escapes controls/quotes but copies bytes >=32 without
validating UTF-8. [VERIFIED: `libs/cJSON.c:906-974`]
**How to avoid:** make valid UTF-8 a structural name requirement with a useful
diagnostic, while preserving the numeric Atom as exact identity.
**Warning signs:** Python `json.loads()` fails on a Status line that cJSON
printed.

### Pitfall 9: Lost Failure Evidence
**What goes wrong:** a timeout kills children and deletes the only logs/state
needed to reproduce the failure.
**Why it happens:** `TemporaryDirectory` cleans unconditionally or teardown
runs before final state capture.
**How to avoid:** snapshot `xrandr`/Status/logs first, retain a per-backend
artifact directory on failure, then terminate children in reverse order.
**Warning signs:** CI only reports a Python timeout exception.

## Code Examples

### Correct XRRGetMonitors Failure Distinction

```c
/* Source: X.Org libXrandr 1.5.5 src/XrrMonitor.c:36-75,146-150 */
int nmonitors = -1;
XRRMonitorInfo *monitors;

monitors = XRRGetMonitors(dpy, root, True, &nmonitors);
if (monitors == NULL && nmonitors == -1)
{
	/* Runtime: log and retain the committed snapshot. */
	return false;
}
/* A successful nmonitors == 0 result is distinct and validated by policy. */
```

### RandR 1.5 Monitor Scenario Syntax

```bash
# Source: X.Org xrandr 1.5.3 man page, RandR 1.5 options
xrandr --setmonitor monitor-a 800/200x600/150+0+0 DUMMY0
xrandr --listmonitors
xrandr --delmonitor monitor-a
```

The official grammar is quoted as
`--setmonitor name {auto|w/mmwxh/mmh+x+y} {none|output,output,...}` and
`--delmonitor name`. [VERIFIED: X.Org xrandr 1.5.3
`man/xrandr.man:125-142`]

### Meson Registration Pattern

```meson
# Source: https://mesonbuild.com/Unit-tests.html
test(
    'topology-xorg-dummy',
    py,
    args: [files('live_x/live_x_harness.py'), '--backend=xorg-dummy',
           '--fvwm', fvwm3],
    suite: 'live-x',
    timeout: 45,
    is_parallel: false,
)
```

Meson documents `args`, `env`, suites, timeout seconds, nonparallel tests, and
exit status `77` for runtime skips. [CITED: https://mesonbuild.com/Unit-tests.html]

### Standard-Library Lifecycle Skeleton

```python
# Recommendation based on the verified Xserver -displayfd contract.
read_fd, write_fd = os.pipe()
try:
    server = subprocess.Popen(
        [server_path, "-displayfd", str(write_fd), "-nolisten", "tcp"],
        pass_fds=(write_fd,),
        stdout=server_log,
        stderr=subprocess.STDOUT,
        start_new_session=True,
    )
finally:
    os.close(write_fd)

# Poll the display-fd/readiness predicate against a monotonic hard deadline;
# always terminate, wait, then kill on bounded teardown if necessary.
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Infer logical screens from outputs/CRTCs | Query RandR Monitor objects | RandR 1.5 | Monitor identity, logical geometry, physical mm, primary, and output sets come from one protocol object. [VERIFIED: X.Org xorgproto 2024.1 `randrproto.txt:150-180,434-445`] |
| Immediate mutation for every event | Dirty mark + quiet/max deadline + immutable candidate | Phase 1 project change | Bounds query/downstream work and gives failure retention. |
| One output per FVWM screen record | Complete sorted output set in canonical snapshot | Phase 1 project change | Clone/multi-output Monitor facts become inspectable and order-independent. |
| Debug-only monitor dump | Status JSON v4 plus `PrintInfo Topology` from one snapshot | Phase 1 project change | Automation and humans inspect the same committed epoch. |
| Xvfb assumed sufficient | Xvfb capability-aware smoke + Xorg-dummy authority | Phase 1 project change | CI tests real output/mode/mm behavior and does not accept Xvfb no-ops as evidence. |
| Fixed/guessed display number | Xserver `-displayfd` | Supported by tested Xorg 21.1.16 | Removes socket-number races in parallel CI. [VERIFIED: Debian Xserver 21.1.16 `-help` probe, 2026-08-29] |

**Deprecated/outdated for this phase:**

- `scan_screens()` as a direct live-state mutator: replace its internals with
  query/candidate/publish; retain narrow compatibility entry points only while
  callers migrate. [VERIFIED: current function `libs/FScreen.c:576-695`]
- `screen_info.rr_output` as authoritative membership: retain only as a legacy
  derived first-output field if required; all new consumers use the snapshot
  output set. [VERIFIED: current field `libs/FScreen.h:59-65`]
- Per-event downstream updates in `dispatch_event()`: replace with dirty marking
  and one post-commit sequence. [VERIFIED: `fvwm/events.c:4123-4137`]

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | A 50 ms quiet period and 250 ms maximum latency are suitable initial UX/performance values on supported real hardware. [ASSUMED] | Architecture Pattern 2 | Bursts may split into extra queries or visible topology may lag; constants are named, unit-tested, and tuned from live evidence before lock-in. |
| A2 | `LD_PRELOAD` interposition is not assumed as a prerequisite. [RESOLVED] Every fault-bearing test invocation probes it against actual FVWM and automatically proves/uses a test-build-only link wrapper when interposition cannot affect the call. | Validation Architecture | Neither path adds a production fault hook, and no Meson test consumes another test's selection side effect. |
| A3 | Xvfb Monitor mutation capability may differ across distributions even though Debian 21.1.16 ignores it. [RESOLVED by D-16] | Pattern 5 | Harness capability-probes queried state; only the mutation subscenario may be unsupported, while Xorg-dummy remains mandatory. |

## Open Questions (RESOLVED)

1. **RESOLVED by D-16 — Does D-14 require successful Monitor mutation on Xvfb, or scenario coverage with a capability-gated subscenario?**
   - What we know: Debian Xvfb 21.1.16 reports RandR 1.6, returns success from
     `xrandr --setmonitor`, but `--listmonitors` remains unchanged and delete
     reports no such Monitor. [VERIFIED: empirical Podman probe, 2026-08-29]
   - Resolution: D-16 binds the interpretation. Execute all operations in the
     Xvfb scenario, state-gate assertions from queried state, and record only
     that mutation subscenario unsupported when the state does not change.
     Startup, query, inspection, epoch, and cleanup remain mandatory, and
     Xorg-dummy is the non-skippable mutation authority.

2. **RESOLVED by mandatory executable probe/fallback — Will the Linux CI environment allow the preload query-failure shim?**
   - What we know: the repository's Linux jobs run inside the Debian build image,
     and the proposed shim affects only the spawned FVWM child. [VERIFIED:
     `.github/workflows/ccpp.yml:13-20,45-66`]
   - Resolution: every fault-bearing invocation first probes the actual
     production FVWM call under scoped preload. If the call is unaffected, that
     same invocation must prove and use the non-installed test-build-only
     `--wrap=XRRGetMonitors` executable. It fails if neither mechanism works,
     never reads another Meson test's state, and exposes no production
     command/environment failure switch.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|-------------|-----------|---------|----------|
| Podman | Local macOS live-X verification | ✓ local | 6.1.0 | Native Linux CI |
| Python 3 | Harness | ✓ local / image | 3.14.7 local; 3.13.3 image | None needed |
| Meson | Build/tests | ✗ local; ✓ image | 1.7.0 image | Run in Podman |
| Ninja | Build | ✗ local; ✓ image | 1.12.1 image | Run in Podman |
| libXrandr | Core build | ✓ image | 1.5.4 | Existing project dependency |
| Xvfb | Smoke backend | ✗ local; installable in image | Xorg 21.1.16 package | Capability-gated mutation; Xorg-dummy authority |
| xrandr CLI | Live mutations/query | ✗ local; installable in image | 1.5.3 | None in live lane |
| Xorg core | Authoritative backend | ✗ local; installable in image | 21.1.16 | Native Linux CI |
| dummy DDX | Authoritative virtual outputs | ✗ local; installable in image | Debian 0.4.0 | Native Linux CI package install |

[VERIFIED: local command probes and Podman x86_64 Debian image probes,
2026-08-29]

**Missing dependencies with no fallback:** none; the dedicated CI/Podman lane
must install the four Debian packages before enabling `live-x` tests.

**Missing dependencies with fallback:** local Meson/Ninja/X tools are replaced
by the project build image under Podman, as required by AGENTS.md.

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Meson 1.7.0 runner; plain C `<assert.h>` unit executables; Python 3 stdlib live harness [VERIFIED: `tests/meson.build:1-29`; Podman image probe] |
| Config file | `tests/meson.build`; add opt-in `live-x-tests` boolean to `meson.options` so default matrix/sanitizer runs do not duplicate nested-X work |
| Quick run command | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-topology fscreen-topology-adapter topology-refresh-policy topology-event-adapter --print-errorlogs` |
| Full suite command | `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs` |
| Live suite command | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs`; CI uses the direct Meson equivalent after package installation |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| TOP-01 | Deep-copy all facts; preserve zero/implausible mm; complete/sorted output set; deterministic monitor order; epoch 1; exact Status v4 JSON | Unit + Xorg-dummy integration | wrapped `monitor-topology` and `topology-xorg-dummy` | Planned TDD in Plans 01, 05, 08 |
| TOP-03 | Equal/reordered candidates do not advance; query/staging failure retains snapshot+legacy view; quiet/max deadlines and event-result dispatch coalesce fairly | Focused unit + Xorg-dummy integration | wrapped `fscreen-topology-adapter`, `topology-refresh-policy`, `topology-event-adapter`, and Xorg scenarios | Planned TDD in Plans 02-04, 08 |
| TOP-06 | Start/ready/cleanup Xvfb and Xorg-dummy; create/mutate/remove; compare server and FVWM state; retain diagnostics | Integration | wrapped `live-x` suite | Planned TDD in Plans 06-08 |

### Native Unit Scenario Matrix

| Area | Required Cases |
|------|----------------|
| Candidate ownership | Deep copies survive source destruction; every allocation-failure index returns failure and leaks nothing; cleanup is idempotent. |
| Validation | Query failed; zero monitors; `None`/unresolvable/invalid-UTF-8 name; duplicate identity; negative/overflowing count; absent output array; zero and extreme mm preserved; pixel bounds checked. |
| Canonical equality | Monitor enumeration permutation; output permutation/duplicates; all individual semantic fields changed one at a time; rename as remove/add. |
| Epoch/publication | First commit 1; equal candidate unchanged; one semantic change +1; failure unchanged; old object freed only after successful swap/reconciliation staging. |
| Refresh policy | First event; quiet extension; exact quiet boundary; hard cap; positive timeout ceiling; due reset; equal refresh clears dirty; events during refresh re-arm one later burst. |
| Serialization | Exact Status v4 keys/types; deterministic order; empty output array; zero mm; escaped names; strict Python `json.loads()` acceptance in live tests. |

### Live-X Scenario Matrix

| Backend | Required Scenario | Authority |
|---------|-------------------|-----------|
| Xvfb | Baseline active monitor → FVWM epoch 1 → attempt create/mutate/remove → either assert state transitions if capability probe passes or record mutation subscenario unsupported and assert no false epoch | Smoke only |
| Xorg-dummy | Baseline `DUMMY0`; connect/mode/position/mm for `DUMMY1`; primary change; user Monitor create/remove; automatic Monitor geometry/mm mutation; multi-output membership; reversed membership order no epoch; rapid burst one epoch | Mandatory authority |
| Xorg-dummy + fault shim | Initial good epoch; fail exactly next `XRRGetMonitors`; verify diagnostic and unchanged JSON/epoch; trigger another event; verify successful recovery and one subsequent commit | Mandatory authority |

### Sampling Rate

- **Per task commit:** narrowest target through `tests/run_in_linux.py`
- **Per wave merge:** wrapped full native suite
- **Dedicated live-X CI:** direct `meson test -C builddir --suite live-x --print-errorlogs` after the job installs the four audited packages
- **Phase gate:** native full suite, sanitizer suite, Xvfb smoke, and authoritative
  Xorg-dummy suite green before `/gsd-verify-work`.

### Planned Test Artifacts (created TDD-first; no separate Wave 0)

- [ ] `tests/test_monitor_topology.c` — TOP-01 snapshot/epoch/validation/allocator tests.
- [ ] `tests/test_fscreen_topology.c` — call-scoped RandR adapter and exhaustive legacy-staging failure tests.
- [ ] `tests/test_topology_refresh_policy.c` — TOP-03 deterministic time tests.
- [ ] `tests/test_topology_event_adapter.c` — exact event classes, timeout selection, and changed-only side-effect decisions.
- [ ] `tests/run_in_linux.py` — Linux-direct/Darwin-Podman linux/amd64 verification router.
- [ ] `tests/live_x/live_x_harness.py` — stdlib lifecycle, predicates, scenarios,
  state comparison, diagnostics.
- [ ] `tests/live_x/xorg-dummy.conf` — deterministic modes and virtual screen.
- [ ] `tests/live_x/xrr_query_fault.c` — test-only Linux query failure.
- [ ] `tests/meson.build` registrations and `meson.options` opt-in live-X toggle.
- [ ] `.github/workflows/ccpp.yml` dedicated native-Linux `live-x` job with apt
  install and failure artifact upload.
- [ ] `doc/fvwm3_manpage_source.adoc` Status v4 / `PrintInfo Topology` contract
  examples should be verified by the serializer test fixture.

## Security Domain

The configuration has quoted `"security_enforcement": true` and
`"security_asvs_level": 1`. [VERIFIED: `.planning/config.json:20-49`]
ASVS is web-application-oriented, so only its input-validation/encoding and
logging principles map directly to this local native X11 phase. [CITED:
https://github.com/OWASP/ASVS/tree/v4.0.3/4.0/en]

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No authentication feature or network service is introduced; preserve the existing X server trust boundary. [VERIFIED: phase boundary in `01-CONTEXT.md:6-12`] |
| V3 Session Management | no | RandR epoch is an in-process generation, not an authenticated user session. [VERIFIED: `01-CONTEXT.md:39-54`] |
| V4 Access Control | no new control | The phase reads the already-connected X display and does not widen X access; nested servers disable TCP. [VERIFIED: `AGENTS.md:57-64`] |
| V5 Input Validation / Encoding | yes | Validate all X reply counts, dimensions, identities, output copies, allocation results, UTF-8 names, and JSON encoding before publication. [VERIFIED: X.Org libXrandr 1.5.5 `src/XrrMonitor.c:51-150`; `libs/cJSON.c:906-974`] |
| V6 Cryptography | no | No credentials, transport encryption, signatures, or random security tokens are added. [VERIFIED: phase boundary in `01-CONTEXT.md:6-12`] |

### Known Threat Patterns for Native XRandR/C Harness

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Malformed/hostile X reply counts or dimensions | Tampering / Denial of Service | Checked arithmetic, detached allocation, structural validation, and last-good retention before pointer swap. |
| Event storm | Denial of Service | One dirty state, quiet-period coalescing, maximum-latency cap, one query per due refresh, existing X-event budget. [VERIFIED: `fvwm/event_loop_policy.c`; `fvwm/events.c:4266-4285`] |
| Invalid Atom-name bytes in JSON/log output | Tampering | Validate UTF-8 and serialize via cJSON; retain numeric Atom identity; never interpolate names into shell commands. |
| Shell/argument injection in harness | Elevation / Tampering | `shell=False`, argv arrays, validated numeric display from `-displayfd`, private temp paths, no user-provided command fragments. |
| Nested X server exposed over TCP | Information Disclosure / Elevation | Always use `-nolisten tcp`; do not use `-ac` for remote access. The Xserver manual warns that `-ac` disables host access controls. [CITED: https://www.x.org/releases/current/doc/man/man1/Xserver.1.xhtml] |
| Test fault switch reachable in production | Denial of Service | Build/use an isolated preload shim only for the spawned test process; no production command or persistent environment trigger. |
| Lost child process after timeout | Denial of Service | New process session, bounded terminate/wait/kill, reverse-order cleanup in `finally`, and artifact capture before cleanup. |

## Sources

### Primary (HIGH confidence)

- X.Org xorgproto 2024.1 release `randrproto.txt` — RandR 1.5 Monitor model,
  fields, Get/Set/Delete requests, multi-monitor output membership, and root
  `ConfigureNotify` behavior.
- X.Org libXrandr 1.5.5 release `src/XrrMonitor.c` — current
  `XRRGetMonitors()` count initialization, bounds checks, allocation/failure,
  deep output layout, and result semantics; published 2026-01-26.
- X.Org xrandr 1.5.3 release man/source — Monitor CLI grammar; published
  2024-11-07.
- X.Org xf86-video-dummy 0.4.1 release source — virtual outputs, mode-driven
  connection, and mm properties; published 2023-05-07.
- Repository source opened this session: `libs/FScreen.c`, `libs/FScreen.h`,
  `fvwm/events.c`, `fvwm/eventmask.h`, `fvwm/fvwm3.c`, `fvwm/builtins.c`,
  `tests/meson.build`, `.github/workflows/ccpp.yml`, and associated docs.

### Secondary (MEDIUM confidence)

- https://mesonbuild.com/Unit-tests.html — test args/env/suites/timeouts,
  nonparallel behavior, skip status, and logs.
- https://pubs.opengroup.org/onlinepubs/9799919799/functions/clock_getres.html —
  POSIX Issue 8 monotonic-clock contract.
- https://www.x.org/releases/current/doc/man/man1/Xvfb.1.xhtml — Xvfb's promised
  dumb-framebuffer test scope.
- Debian trixie package metadata and Podman x86_64 probes — exact available
  package versions and observed Xvfb/Xorg-dummy behavior.
- https://github.com/OWASP/ASVS/tree/v4.0.3/4.0/en — ASVS v4 category mapping.

### Tertiary (LOW confidence)

- The initial 50 ms / 250 ms tuning recommendation and cross-distribution Xvfb
  capability expectation; both are explicitly logged as assumptions and must be
  validated in implementation CI.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — existing project stack plus exact upstream and Debian
  versions were verified from source indexes and live package probes.
- Architecture: HIGH — derived from opened repository control flow and the
  authoritative RandR 1.5 protocol/library implementation.
- Pitfalls: HIGH for event/failure/order issues; MEDIUM for backend portability
  because Xvfb behavior was tested on one current Debian package set.
- Validation architecture: MEDIUM-HIGH — Meson/Python patterns are established,
  and live server behavior was probed; the final preload shim remains unproven.

**Research date:** 2026-08-29
**Valid until:** 2026-09-28 (re-check Debian package versions and Xvfb capability
if the build image changes)
