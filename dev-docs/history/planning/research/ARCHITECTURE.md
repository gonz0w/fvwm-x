# Architecture Patterns: Per-Monitor HiDPI in FVWM-X

**Domain:** Mixed-DPI support in a single-threaded, non-compositing X11 window manager
**Researched:** 2026-08-29
**Overall confidence:** MEDIUM — protocol boundaries and current integration points are well established; ownership thresholds, automatic-scale validation, and live event ordering remain product policies that need native-Linux validation.

## Architectural Decision

Implement per-monitor scaling as **derived window-manager policy attached to an authoritative RandR Monitor snapshot**. Keep every X-facing rectangle in root/client pixels. Introduce logical units only for FVWM-owned metrics and for a stable client-size basis used during monitor transitions.

Do not spread `scale * value` through `borders.c`, `move_resize.c`, and modules. Build four explicit layers:

1. **Topology adapter and snapshot** — copy all relevant `XRRMonitorInfo` data into owned, generation-tagged state.
2. **Pure scale and ownership policy** — validate physical data, resolve overrides, perform rational conversions, and select one monitor deterministically.
3. **Transition coordinator** — coalesce RandR changes and apply generation-safe window transitions in bounded batches.
4. **Resolved UI resources** — cache fonts, metrics, pictures, and cursors by scale/target size and make drawing code consume already-resolved pixels.

This is an incremental refactor of the existing event-driven monolith, not a new renderer, compositor, toolkit, or background service.

## Recommended Architecture

```text
RandR events / root ConfigureNotify       scale config / style reload
                 │                                  │
                 ▼                                  ▼
       ┌──────────────────┐               ┌───────────────────┐
       │ topology dirty   │               │ config generation │
       │ marker/coalescer │               │ and overrides     │
       └────────┬─────────┘               └─────────┬─────────┘
                └─────────────────┬──────────────────┘
                                  ▼
                    ┌─────────────────────────┐
                    │ Candidate monitor       │
                    │ snapshot (owned values) │
                    │ geometry/mm/outputs/id  │
                    └────────────┬────────────┘
                                 │ validate + diff
                                 ▼
                    ┌─────────────────────────┐
                    │ Committed topology      │
                    │ epoch + effective scale │
                    └──────┬──────────┬───────┘
                           │          │
              invalidate   │          │ publish monitor facts
                           ▼          ▼
              ┌────────────────┐  EWMH/module monitor data
              │ Window pending │
              │ transition set │
              └───────┬────────┘
                      │ bounded event-loop batches
                      ▼
       ┌──────────────────────────────────────────┐
       │ ownership → desired client size → ICCCM  │
       │ constraints → target metrics/resources  │
       │ → frame/client X configure → redraw      │
       └───────────────┬──────────────────────────┘
                       ▼
              EWMH/frame extents/module window data
```

### Architectural Invariants

| Invariant | Consequence | Confidence |
|---|---|---|
| X11 has one root-pixel geometry space per X screen. | RandR rectangles, pointer events, frame/client windows, struts, EWMH properties, and module geometry remain pixels. | MEDIUM — RandR/EWMH/ICCCM |
| A RandR 1.5 Monitor is the scale authority, not an individual output. | Cloned or tiled outputs presenting one logical area get one effective scale. Preserve the complete output set for identity and diagnostics. | MEDIUM — RandR Monitor model |
| Effective scale is immutable within a topology/config generation. | Consumers use a value plus generation; they never recalculate DPI from raw millimetres. | MEDIUM — architecture recommendation |
| Existing `struct monitor *` pointers must not dangle during refresh. | Build an immutable candidate off to the side, then project it into stable monitor nodes or retain retired nodes until no window references them. | HIGH — direct code observation |
| Current physical geometry is not canonical logical intent. | A WM-driven scale transition must not overwrite the stable client-size basis used for the next transition. | MEDIUM — drift prevention recommendation |
| Monitor selection and scale transition have one authority. | Existing call sites may request reevaluation, but only the transition coordinator commits `fw->m` and a new applied scale. | HIGH — direct code observation plus recommendation |
| Resource identity includes its requested scale or pixel size. | A font, picture, or cursor loaded for 1x cannot satisfy a 2x request merely because its source name matches. | MEDIUM — library contracts and current cache inspection |
| A topology transaction may span loop iterations, but each visible window state is self-consistent. | Keep applied owner/scale separate from pending owner/epoch and emit per-window packets only after that window's X geometry is applied. | MEDIUM — architecture recommendation |

## Component Boundaries

### New Components

| Component | Location | Responsibility | Must Not Do | Communicates With |
|---|---|---|---|---|
| Rational scale primitive | `libs/fvwm_scale.[ch]` | Reduced rational scale, checked multiplication/division, named rounding modes, px/mm validation, scale-source/rejection enums | Call Xlib, inspect global monitor state, or choose a window owner | Snapshot policy, UI metrics, geometry tests |
| Monitor snapshot/diff policy | `libs/monitor_snapshot.[ch]` | Owned monitor facts, stable identity keys, candidate validation, deterministic diff, scale resolution | Mutate `Scr`, `FvwmWindow`, EWMH, or X resources | `FScreen.c`, topology coordinator |
| Topology coordinator | `fvwm/topology.[ch]` | Dirty-event coalescing, candidate query/commit, epochs, pending-window queue, bounded reconciliation, final broadcasts | Implement drawing or duplicate geometry policy | `events.c`, `FScreen.c`, `update.c`, module/EWMH adapters |
| Monitor ownership policy | `fvwm/monitor_ownership.[ch]` or a pure subunit of `topology` | Literal root-rectangle selection, overlap/dead-area tie-breaks, incumbent preference, interactive hysteresis | Apply modulo virtual-page wrapping or resize a window | Placement, move/resize, topology reconciliation |
| Window scale transition | `fvwm/window_scale.[ch]` | Stable size basis, pending/applied owner state, transition serial, state-specific target geometry, idempotent commit | Parse configuration or directly discover RandR monitors | Geometry, frame/update, placement, move/resize |
| Resolved UI metrics | `fvwm/ui_metrics.[ch]` | Convert logical style/decor metrics to one immutable pixel bundle per scale/config generation | Let consumers rescale the result | Borders, frame, menus, icons, hit testing, pan frames |
| Scale-aware resource adapters | Existing `Flocale`, `Fft`, `Picture`, `Fcursor` wrappers | Acquire/release fonts, target-sized pictures, and cursors using scale-complete cache keys | Change process-global DPI/cursor size on monitor crossing | UI metrics, decoration/icon/menu drawing |

Names under `libs/` remain lowercase because these are project-owned policy units, not new wrappers around one external API. `FScreen` remains the RandR wrapper.

### Existing Components to Modify

| Existing component | New role / integration point | Confidence |
|---|---|---|
| `libs/FScreen.[ch]` | Copy `XRRMonitorInfo` name, geometry, millimetres, primary/automatic state, and all outputs into a candidate; commit to stable monitor records; broaden relevant notifications. | HIGH — current code inspected |
| `fvwm/events.c` | Call `XRRUpdateConfiguration()` where required, mark topology dirty, and run bounded topology work at an event-loop safe point instead of rescanning/broadcasting per event. | HIGH |
| `fvwm/fvwm.h` | Add per-window applied/pending topology state and stable client-size basis; retain `g.normal`, `g.max`, and `g.frame` as physical geometry. | HIGH |
| `fvwm/update.[ch]` | Replace `update_fvwm_monitor()` as a direct assignment authority with a reevaluation request/transition entry point; add scale invalidation flags. | HIGH |
| `fvwm/geometry.[ch]` | Add explicit conversion helpers and reuse `gravity_constrain_size()` after ideal target size is computed. | HIGH |
| `fvwm/add_window.c` | Choose initial owner before scale-dependent fonts/decorations; establish the first stable client-size basis after hints and placement. | HIGH |
| `fvwm/frame.c`, `borders.c`, `decorations.c` | Consume resolved metrics and resources; do not derive scale independently. | HIGH |
| `fvwm/move_resize.c` | Track interactive destination and pointer anchor; request one transition when ownership commits; freeze generation during one operation. | HIGH |
| `fvwm/placement.c`, `ewmh.c`, `virtual.c` | Use literal monitor selection and physical work areas; recompute maximize/fullscreen from destination state rather than scaling derived rectangles. | HIGH |
| `libs/Flocale.c`, `libs/Fft.c` | Accept normalized scale/target size and include it in font cache identity. | HIGH — current name-only cache inspected |
| `libs/Picture.c` and image loaders | Include target width/height or normalized scale in cache identity and rerender from source. | HIGH — current path/attributes key inspected |
| `fvwm/cursor.c`, `libs/Fcursor.h` | Load explicit-size Xcursor resources and bind by WM window/pointer context. | MEDIUM — API verified, exact wrapper path needs phase tracing |
| `fvwm/module_interface.c`, `libs/Module.h`, `libs/vpacket.h`, Perl constants/consumers | Add scale metadata without changing existing geometry units; synchronize all protocol consumers. | HIGH — protocol coupling inspected |

## Authoritative State Models

### Scale Value and Resolution

Use exact rational scale for equality and conversion. `double` may be used temporarily while calculating physical DPI, but it must not become stored geometry state.

```c
struct fvwm_scale {
	uint32_t num;
	uint32_t den;
};

enum fvwm_scale_source {
	SCALE_FROM_OVERRIDE,
	SCALE_FROM_MONITOR_MM,
	SCALE_FROM_GLOBAL_FALLBACK,
	SCALE_FROM_SAFE_DEFAULT
};

struct monitor_scale_state {
	struct fvwm_scale automatic;
	struct fvwm_scale override;
	struct fvwm_scale effective;
	enum fvwm_scale_source source;
	enum fvwm_scale_rejection rejection;
	uint64_t generation;
};
```

The policy module owns validation, quantization, and fallback. Rendering code sees only `effective`. Keep raw values and rejection reason for diagnostics.

### Monitor Snapshot

```c
struct monitor_snapshot_item {
	char *name;                  /* configured identity */
	Atom name_atom;              /* runtime RandR identity */
	bool primary;
	bool automatic;
	int64_t x, y, width, height; /* validated root pixels */
	uint32_t mm_width, mm_height;
	RROutput *outputs;           /* owned complete array */
	size_t output_count;
	struct monitor_scale_state scale;
};

struct monitor_snapshot {
	uint64_t epoch;
	Time randr_timestamp;
	struct monitor_snapshot_item *items;
	size_t count;
};
```

Do not retain pointers into `XRRGetMonitors()` replies. Copy, validate, then free the reply. Treat query failure differently from a valid candidate; retain the last good snapshot on failure.

The practical migration path is a **candidate snapshot plus stable runtime projection**:

- `monitor_snapshot` is immutable after validation.
- Existing `struct monitor` nodes remain stable and receive committed value updates.
- Removed monitors become retired/disabled, not freed while referenced.
- New code uses snapshot epoch and stable identity rather than coordinate-sorted `number`.
- Once direct raw pointers are eliminated, a later milestone may swap immutable runtime snapshots wholesale; v0.1 does not need that rewrite.

### Per-Window Scale State

Keep `fw->m` as the owner whose geometry/resources are currently applied, preserving compatibility with existing consumers. Add a pending target rather than changing `fw->m` before X-visible work completes.

```c
struct window_size_basis {
	uint32_t client_width_px;
	uint32_t client_height_px;
	struct fvwm_scale capture_scale;
	bool valid;
};

struct window_scale_state {
	struct monitor *pending_monitor;
	struct fvwm_scale applied_scale;
	uint64_t applied_epoch;
	uint64_t pending_epoch;
	uint64_t transition_serial;
	unsigned invalidation;
	struct window_size_basis normal_client;
};
```

The size basis stores an exact client-pixel size plus the scale at which user/client intent last established it. A target size is computed as:

```text
target_px = round(basis_px × target_scale ÷ basis_capture_scale)
```

WM-driven monitor transitions do not rewrite the basis. A genuine interactive resize, accepted client configure request, or explicit size command establishes a new basis. This avoids cumulative drift without introducing a global logical coordinate grid.

## Coordinate-Space Contract

| Space | Meaning | Storage/API rule |
|---|---|---|
| `root_px` | X root-window position and monitor rectangle | Signed physical pixels; never divided by monitor scale; use literal coordinates for monitor selection. |
| `frame_px` | Outer reparenting-frame rectangle | Physical pixels in `g.frame`, `g.normal`, and `g.max`; decorations included exactly once. |
| `client_px` | Client window width/height and ICCCM constraints | Physical client pixels; min/max/base/increments/aspect are not scaled as WM metrics. |
| `logical_ui` | FVWM-owned border, title, icon, menu, cursor, and hit-target intent | Converted once through `fvwm_scale` into a resolved metrics/resource bundle. |
| `logical_size_basis` | Stable perceived client-size intent | Exact `{client_px, capture_scale}` pair; private to FVWM-X. |
| `physical_mm` | RandR physical-size input | Diagnostics and automatic-scale policy only; never geometry. |

Required conversion rules:

1. Scale rectangle **edges**, then derive widths/heights, when adjacent UI pieces must meet. This prevents independent rounding gaps.
2. Use explicit `floor`, `ceil`, and `nearest` helpers with 64-bit checked intermediates. Do not rely on C casts, especially with negative values.
3. Clamp before narrowing to Xlib/protocol-sized fields and reject impossible geometry rather than wrapping.
4. Preserve root position or a named anchor while changing size; do not scale global `x/y` as if the desktop were logical.
5. Publish only actual applied pixel values to ICCCM, EWMH, and modules.

## Topology Transaction and Data Flow

### Event Intake

The current path rescans and broadcasts immediately for each `RRScreenChangeNotify` and `RRNotify`. Replace that with:

1. For `RRScreenChangeNotify` and relevant root `ConfigureNotify`, call `XRRUpdateConfiguration()` while the event is available.
2. Mark `topology_dirty` and retain the newest event/config timestamp.
3. At an event-loop safe point, nonblockingly coalesce already-queued topology events up to a budget.
4. Perform at most one complete `XRRGetMonitors()` snapshot query per loop turn.
5. If another notification arrives during/after the query, leave dirty set for one later refresh; never spin or sleep-debounce.

Subscribe to screen, CRTC, output, and resource change notifications supported by the required RandR level, and treat root `ConfigureNotify` as a topology trigger for RandR Monitor object changes.

### Candidate Commit

```text
query reply
  → deep copy
  → structural validation
  → physical-DPI validation and override resolution
  → deterministic identity match
  → diff against committed epoch
  → update stable monitor nodes
  → publish new epoch
  → enqueue affected windows
```

A diff classifies geometry, identity/output membership, primary, physical-size, and effective-scale changes separately. A primary-only change must not trigger window rescaling. A physical-size change that quantizes to the same effective scale does not invalidate UI resources.

### Bounded Window Reconciliation

Topology commit is cheap state work; X configuration and redraw are processed in bounded batches. The queue stores window identity plus target epoch, not an unsafe iterator into the mutable window list.

- Prioritize windows whose owner was removed, focused/grabbed windows, then visible windows, then hidden/iconified windows.
- A newer epoch replaces stale pending work; generation checks make old queue entries no-ops.
- Before an operation mutates a pending window, `window_scale_ensure_current(fw)` may reconcile that one window synchronously.
- Disabled monitor nodes remain valid until no applied window references them.
- Emit a window configure packet only after that window's frame/client geometry and resource generation are applied.
- Emit a final topology-settled diagnostic/event when the pending queue for the epoch reaches zero.

This preserves responsiveness without pretending a multi-window X relayout is atomic at the wire level.

## Window Ownership and Scale Transition

### Deterministic Ownership

Replace `FindScreenOfXY()` for scale ownership with a literal-root policy that does not modulo coordinates through virtual desktop dimensions.

Recommended ordering:

1. During interactive move/resize, use the pointer monitor after crossing a named hysteresis margin; freeze that destination for the current motion step.
2. Otherwise choose the monitor with greatest intersection with the **pre-transition frame**.
3. On an exact tie, retain the incumbent active monitor.
4. Then prefer primary.
5. Finally use stable monitor identity, never RB traversal order or transient monitor number.
6. For no intersection, choose nearest rectangle using 64-bit endpoint distance, then the same tie-breaks.

Scale application must not immediately feed its enlarged/shrunken result back into owner selection. The chosen target is committed for that transition serial.

### Transition Algorithm

For one window and one target epoch:

1. Reject duplicate `(target monitor, target scale, epoch)` work.
2. Capture the pre-transition frame, state, ICCCM gravity, and interaction anchor.
3. Select target metrics/resources for the destination scale before releasing old resources.
4. For a normal resizable window, derive ideal target **client** size from the stable basis.
5. For a fixed-size client, retain legal client pixels and scale only WM-owned extents.
6. Run the ideal frame/client result through existing `gravity_constrain_size()` once, preserving min/max/base/increments/aspect/gravity.
7. For maximized state, recompute from the destination work area. For fullscreen, use the exact destination monitor rectangle with no decoration. Never scale those derived rectangles.
8. Reposition around the named anchor: ICCCM gravity for programmatic transitions; the logical grabbed point for interactive movement.
9. Apply frame/client X geometry, rebuild decoration/input windows, update cursor bindings, and redraw.
10. Update `_NET_FRAME_EXTENTS`, synthetic configure state, and module data from actual applied pixels.
11. Commit `fw->m`, `applied_scale`, `applied_epoch`, and transition serial; then release old generation resources.

If exact perceived size conflicts with ICCCM validity or reachability, validity and reachability win. Record the constraint reason and do not retry the same transition in response to the resulting geometry.

## Resolved Metrics and Resource Caches

### One Metrics Bundle

Create one immutable resolved bundle per `(decor/style generation, normalized scale)` containing at least:

- title/border/corner/button dimensions;
- title/icon font handles and measured offsets;
- visible strokes and separate interactive hit widths;
- icon/mini-icon target limits;
- menu spacing and move/resize feedback metrics;
- edge/pan thresholds and cursor target size.

Consumers receive pixel values. They do not multiply by scale again.

### Cache Keys

| Resource | Required key fields | Invalidation |
|---|---|---|
| Font | normalized font specification, effective scale or final pixel size, rotation, fontconfig generation | scale, font config, font name, rotation |
| Picture/icon | source path/stamp, picture attributes, target width/height, renderer options | source change, target size, visual/depth as applicable |
| Cursor | shape/name/file, theme, target pixel size, render capability, cursor generation | scale, theme, cursor config |
| UI metrics | decor/style identity, normalized scale, config generation | scale or relevant style/decor change |

Acquire-new-before-release-old is mandatory because windows and active grabs may still reference old X resources. Existing reference counts are the right lifecycle pattern; expand cache identity rather than bypassing the cache.

For Xft, use a pattern-local final pixel size or DPI and ensure monitor scale is applied exactly once. For Xcursor, load an explicit target size; `XcursorSetDefaultSize()` is display-global and is not the per-monitor mechanism.

## Invalidation Model

Add scale-specific positive-syntax flags alongside `update_win`, but keep topology work distinct from broad style flushes so one monitor change does not grab/redecorate every window.

```text
OWNER
  → SIZE_BASIS_TARGET / DERIVED_STATE_GEOMETRY
  → METRICS
  → FONT + PICTURE + CURSOR
  → FRAME_LAYOUT + HIT_REGIONS
  → X_GEOMETRY
  → REDRAW
  → EWMH + MODULE_BROADCAST
```

| Change | Affected invalidation | Explicit non-effects |
|---|---|---|
| Monitor position/size only | ownership reevaluation, work areas, derived maximize/fullscreen geometry | No font/picture reload when effective scale is unchanged |
| Effective scale change | metrics/resources, normal transition target, frame layout, hit regions, extents | Do not scale root coordinates or client hints |
| Raw mm change with same scale bucket | diagnostics/generation metadata only | No redraw or client resize |
| Primary-only change | placement preference, monitor/EWMH/module facts | Existing normal windows do not resize |
| Strut/work-area change | affected work areas and maximized windows | Normal-size basis and resource scale unchanged |
| Decor/font/theme generation | metrics and named resource caches on relevant windows | Topology epoch unchanged |
| Monitor removal | urgent owner reassignment, reachability, state-specific geometry | Never restore stale absolute coordinates into a gap |

## Protocol and Module Boundaries

Existing module geometry remains root/frame/client pixels. Do not reinterpret `ConfigWinPacket.frame_x`, `frame_width`, `M_NEW_PAGE`, or monitor rectangles as logical units.

Recommended additive protocol:

- Include an initial `MonitorScale` record for each monitor containing stable name, rational effective scale, source, and topology epoch.
- Emit a scale-changed extended message after a monitor's new scale is committed.
- Continue emitting per-window configure packets only after each affected window is applied.
- Update `libs/Module.h`, `libs/vpacket.h`, C modules, FvwmMFL translation, and Perl constants together.
- First-party module windows resolve their own owning monitor and use the scale metadata for their UI; pager map geometry remains physical root geometry.

A dedicated versioned packet is preferable long-term. A separate additive `M_CONFIG_INFO` record is acceptable for the first vertical slice if old modules safely ignore unknown records. Do not append fields to a native binary packet without synchronized consumer changes and tests.

## Patterns to Follow

### Pattern 1: Candidate-Then-Commit

**What:** Build and validate complete state before touching authoritative monitor nodes.
**When:** RandR refresh, config reload, or automatic-scale recalculation.

```c
candidate = monitor_snapshot_query(dpy);
if (candidate == NULL || !monitor_snapshot_validate(candidate)) {
	monitor_snapshot_discard(candidate);
	return KEEP_LAST_GOOD;
}
diff = monitor_snapshot_diff(committed, candidate);
topology_commit(candidate, diff);
topology_enqueue_windows(diff);
```

### Pattern 2: Generation-Tagged Deferred Work

**What:** Every queued window transition names its target epoch; stale work is discarded.
**When:** Topology bursts, scale reload, theme reload.

```c
if (work.epoch != topology_current_epoch())
	return; /* superseded */
if (fw->scale.applied_epoch == work.epoch &&
    fw->m == work.target_monitor)
	return; /* idempotent */
window_scale_apply(fw, &work);
```

### Pattern 3: Explicit Boundary Conversion

**What:** Functions state whether inputs are logical UI units, client pixels, frame pixels, or root pixels.
**When:** Every new scale-sensitive API.

Prefer `ui_length_to_px(scale, logical, ROUND_NEAREST)` and `client_basis_to_target_px(...)` over generic multiplication helpers at call sites.

## Anti-Patterns to Avoid

### Mutating the Monitor Tree While Discovering It

**Why bad:** Partial state leaks to raw monitor pointers, EWMH, and modules; transient query failures can disable everything.
**Instead:** Deep-copy a candidate, validate, diff, then commit.

### Reusing `FindScreenOfXY()` for Scale Ownership

**Why bad:** It modulo-wraps coordinates and resolves overlaps/ties by data-structure order.
**Instead:** Use literal root rectangles and a documented incumbent/primary/stable-ID policy.

### Treating `fw->m` Assignment as a Harmless Field Update

**Why bad:** At mixed scale, changing owner implies font, decoration, hit region, cursor, geometry, EWMH, and module work.
**Instead:** Route all owner changes through the transition state machine.

### Scaling Current Frame Geometry Repeatedly

**Why bad:** Decorations are counted into client intent and fractional rounding accumulates.
**Instead:** Derive client size from the stable basis and derive frame extents from destination metrics.

### Global DPI or Cursor Mutation

**Why bad:** Two monitors are visible simultaneously, while `Xft.dpi` and Xcursor default size are display-global.
**Instead:** Use per-pattern font sizing and explicit-size cursor resources.

### One Giant Style Flush for Every Scale Event

**Why bad:** `flush_window_updates()` grabs the server and loops over every window. It defeats affected-monitor invalidation and bounded work.
**Instead:** Reuse its positive flag style at per-window granularity under the topology queue; reserve full flushes for true global style changes.

## Scalability and Responsiveness

| Concern | Small desktop (≤20 windows) | Busy desktop (≈200 windows) | Stress case (1000+ windows/events) |
|---|---|---|---|
| Topology query | One complete query per coalesced loop turn | Same; never one query per notification | Budget notification drain and keep dirty for next turn |
| Ownership planning | Linear pure calculation is acceptable | Build queue once from diff | Generation-replace stale queue; avoid repeated full scans |
| X relayout | Usually completes in one batch | Fixed window/time budget per loop turn | Prioritize visible/orphaned windows; lazy ensure on interaction |
| Resource creation | Cache by scale bucket | Shared refcounted bundles | Bound generations; retire after references drop |
| Module output | Per-monitor facts plus changed windows | Existing bounded nonblocking queues | Coalesce superseded configure/scale messages where protocol permits |

No monitor operation should perform filesystem discovery, subprocess execution, sleeps, or unbounded server grabs in the main loop. X resource creation remains on the authoritative thread, but is amortized through cache sharing and bounded transitions.

## Testing Architecture

### Pure Native Tests

Add Meson-registered C tests before X wiring:

1. Rational reduction, checked conversion, negative rounding, overflow, and endpoint transforms.
2. px/mm validation, axis disagreement, fallback/source reason, quantization, and override precedence.
3. Snapshot copy/diff for add/remove/reorder/primary/mm/output-membership changes.
4. Literal ownership for seams, overlaps, gaps, clones, incumbent ties, negative origins, and input-order independence.
5. Stable size-basis round trips across 1x/1.25x/1.5x/1.75x/2x, including odd dimensions and 1000 repetitions.
6. Transition idempotence, epoch supersession, fixed-size clients, hints, gravity, and state-specific geometry.
7. Cache-key separation and acquire-before-release lifecycle.

### Live X11 Integration

Use a native-Linux harness with Xvfb for monitor-object smoke tests and Xorg dummy output for authoritative output/CRTC/mm scenarios. Purpose-built Xlib clients should expose known hints and report actual ConfigureNotify/frame/extents values.

Gate the milestone on:

- one coalesced epoch for duplicate/burst notifications;
- safe retention of the last good topology after query failure;
- 1x↔2x and fractional round trips without drift;
- active drag crossing with pointer anchor preserved and no ownership oscillation;
- fixed/increment/aspect/gravity-constrained clients;
- maximize/fullscreen/restore and strut-aware work areas;
- unplug of owner during idle, move, resize, menu, and pointer grab;
- correct module monitor scale and unchanged pixel geometry semantics;
- event-loop/module fairness during topology storms.

## Dependency-Ordered Implementation Phases

1. **Topology Snapshot and Live-X Harness**
   - Deep-copy complete RandR Monitor facts, add epochs/diffs, broaden event intake, retain last good state, and establish stable monitor-node lifecycle.
   - Why first: every later scale or transition decision depends on trustworthy identity and change classification.

2. **Scale Policy and Configuration**
   - Land rational arithmetic, px/mm validation, quantization/fallback, explicit override precedence, diagnostics, and monitor scale state.
   - Why second: consumers need one authoritative accessor and reason before any visual code changes.

3. **Geometry and Ownership Foundation**
   - Separate literal root selection from virtual-page wrapping, add stable client-size basis, conversion/rounding APIs, and pure transition planning around ICCCM constraints.
   - Why before rendering: scaled frame extents and cross-monitor behavior otherwise encode incompatible coordinate assumptions.

4. **WM-Owned UI Resources**
   - Add resolved metrics and scale-complete font/picture/cursor caches; wire frames, borders, menus, icons, hit targets, and frame extents.
   - Prove 1x/2x first, then supported fractional buckets.

5. **Logical-Size Monitor Transitions**
   - Centralize owner commit, pointer/gravity anchors, normal/maximized/fullscreen behavior, transition serials, and per-window broadcasts.
   - Why after UI resources: target decoration extents and fonts are inputs to correct final frame geometry.

6. **Hotplug Reconciliation, Modules, and Compatibility**
   - Bounded queue policy, removed-owner recovery, active-operation cancellation/rebase, additive module scale metadata, first-party module UI, soak/client matrix, and final observability.
   - Why last: hotplug composes all earlier invariants and is where stale pointers, partial resources, and protocol ordering become visible.

## Phase Research Flags

| Phase | Deeper research needed? | Reason |
|---|---|---|
| Topology snapshot | Yes | Confirm packaged Xorg/Xvfb event sequences for `--setmonitor`, `--delmonitor`, root ConfigureNotify, and resource masks. |
| Scale policy | Yes | Calibrate invalid-DPI thresholds, X/Y disagreement, supported range, and quantization against target hardware data. |
| Geometry foundation | Limited | Core ICCCM rules and existing solver are known; focus on extracting testable contracts from current virtual-desktop behavior. |
| UI resources | Yes | Trace all font/image/cursor call sites and fallback builds to prove exactly-once scaling and safe cache ownership. |
| Window transitions | Yes | Validate client feedback behavior and interactive anchor UX with real toolkits and constrained test clients. |
| Hotplug/modules | Yes | Specify protocol versioning and test event-loop budgets under native X server storms. |

## Sources

All external source-backed claims are **MEDIUM confidence** under the configured research-provider seam after cross-checking official/current documents against repository code. Architecture choices marked as recommendations require phase tests.

- X.Org RandR protocol — dynamic screen model, screen/CRTC pixel geometry, physical millimetres, transforms, and notifications: https://www.x.org/releases/current/doc/randrproto/randrproto.txt
- libXrandr manual — `XRRUpdateConfiguration`, screen-change notification, timestamps, and Xlib cache behavior: https://www.x.org/releases/current/doc/man/man3/Xrandr.3.xhtml
- Current Debian libXrandr 1.5.4 manual rendering: https://manpages.debian.org/testing/libxrandr-dev/Xrandr.3.en.html
- Fontconfig Developers Reference 2.18.3 — `FC_SIZE`, `FC_PIXEL_SIZE`, `FC_DPI`, patterns, and default substitution: https://fontconfig.pages.freedesktop.org/fontconfig/fontconfig-devel/
- Current Debian libXft manual — pattern-based font opening and display defaults: https://manpages.debian.org/testing/libxft-dev/XftFontOpen.3.en.html
- Xcursor manual — nominal-size selection, explicit image loading, and display-global default size: https://www.x.org/releases/current/doc/man/man3/Xcursor.3.xhtml
- Current Debian libXcursor 1.2.3 manual rendering: https://manpages.debian.org/testing/libxcursor-dev/XcursorSetDefaultSize.3.en.html
- ICCCM 2.0 — client resize, gravity, and `WM_NORMAL_HINTS`: https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html
- Current libX11 `XSizeHints` manual — min/max/base/increments/aspect/gravity fields: https://manpages.debian.org/testing/libx11-doc/XGetWMNormalHints.3.en.html
- EWMH 1.5 — work areas, moveresize, maximize/fullscreen, and frame extents: https://specifications.freedesktop.org/wm-spec/latest-single/
- Repository evidence: `libs/FScreen.[ch]`, `fvwm/events.c`, `fvwm/update.[ch]`, `fvwm/fvwm.h`, `fvwm/geometry.c`, `fvwm/add_window.c`, `libs/Flocale.c`, `libs/Fft.c`, `libs/Picture.c`, `fvwm/cursor.c`, `fvwm/module_interface.c`, and `libs/vpacket.h`.

## Remaining Gaps

- The exact automatic-scale acceptance thresholds and ownership hysteresis margin are policy, not standards.
- RandR Monitor event behavior and temporary zero/query-failure behavior need verification on the CI server versions chosen by the project.
- The first-party module support set and protocol compatibility strategy must be frozen before module implementation.
- Toolkit reactions to WM-driven cross-scale resize are heterogeneous; fixed-size and client-counter-request behavior need a representative live-client corpus.
- Existing style values may intentionally be pixel-exact. The configuration phase must decide which values become logical UI units and how users opt into literal pixels where necessary.
