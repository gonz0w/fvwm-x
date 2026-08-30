# Domain Pitfalls: Per-Monitor HiDPI in FVWM-X

**Domain:** Mixed-DPI support in an existing X11 window manager
**Researched:** 2026-08-29
**Overall confidence:** MEDIUM — the protocol and toolkit contracts are well documented, but several prevention policies are product decisions that must be validated against live X servers and real clients.

## Bottom Line

The dangerous implementation is “read `pixels / millimetres`, attach a float to each monitor, and multiply every rectangle whenever `fw->m` changes.” It fails because XRandR topology is asynchronous, physical dimensions can be absent or bogus, X11 exposes integer root/client geometry rather than a shared logical coordinate system, ICCCM size hints constrain client pixels, and most X11 toolkit DPI inputs are global to an X screen rather than per RandR monitor.

FVWM-X should instead establish four explicit contracts before scaling UI:

1. **An immutable, generation-tagged monitor snapshot** built and validated in one place.
2. **A deterministic scale-source policy** with override precedence, rejection reasons, quantization, and a safe fallback.
3. **A canonical logical client-size record** that is never reconstructed from repeatedly rounded device geometry.
4. **A deterministic monitor-ownership and transition state machine** that applies at most one scale transition per committed crossing.

Existing code makes this sequencing especially important. `libs/FScreen.c` currently stores only monitor `x/y/w/h`, primary state, and the first output; it drops `XRRMonitorInfo` millimetres, automatic state, and additional outputs. `FScreenSelect()` selects only screen/output change masks, while RandR 1.5 monitor creation/deletion is specified to generate root `ConfigureNotify`. Every RandR event immediately calls `scan_screens()`. `FindScreenOfXY()` wraps coordinates modulo the root dimensions and chooses the first containing monitor in red-black-tree order. Those are acceptable legacy assumptions, but each becomes a mixed-DPI correctness hazard.

## Critical Pitfalls

### 1. Treating EDID or RandR Millimetres as Trustworthy DPI

**What goes wrong:** A zero, fabricated, stale, or implausible physical size produces division by zero, a huge scale, tiny UI, asymmetric X/Y scaling, or a scale change after reconnecting the same display. KVMs, docks, projectors, tiled displays, and manually defined RandR monitors are frequent boundary cases.

**Why it happens:** RandR explicitly permits output millimetres to be `(0, 0)` when physical size is unknown or not fixed. The Linux kernel documents monitors sending no or bogus EDID and KVMs substituting their own EDID. A RandR 1.5 monitor may combine several outputs or have manually supplied dimensions, so “first output EDID” is not necessarily the logical monitor's size.

**FVWM-X exposure:**
- `monitor_set_coords()` currently discards `rrm.mwidth` and `rrm.mheight`.
- A monitor stores only the first `rrm.outputs` entry, which is insufficient for tiled or client-defined multi-output monitors.
- `monitor_mark_changed()` cannot detect physical-size-only or output-membership changes.

**Warning signs / detection:**
- Computed X and Y DPI differ materially.
- Any pixel or millimetre dimension is zero or negative after conversion.
- Auto scale is outside the supported desktop range or changes while pixel geometry is unchanged.
- The same runtime monitor name reconnects with a very different scale.
- A multi-output monitor is assigned the DPI of one tile.

**Prevention:**
1. Preserve raw monitor millimetres, full output membership, `primary`, and `automatic` in the topology snapshot. Keep a validity/rejection enum; never silently rewrite bad raw data.
2. Resolve scale in this strict order: **explicit per-monitor config override → validated RandR monitor DPI → configured global fallback → 1.0**.
3. Validate both axes before division. Recommended initial policy: reject DPI outside `50..400`, reject X/Y disagreement above 20%, reject zero dimensions, and reject arithmetic overflow. Keep these named policy constants, not scattered literals.
4. Calculate one isotropic DPI from both valid axes (for example, the geometric mean), derive `dpi / 96`, clamp automatic scale to the supported product range, then quantize once to supported steps (quarter steps are a defensible v0.1 choice). Explicit overrides may bypass auto-detection bounds but must still obey finite/range checks.
5. Never parse EDID in the geometry path. RandR monitor data is the runtime input; EDID/output details are diagnostics and possible identity hints, not unquestioned geometry truth.
6. Emit one rate-limited diagnostic containing source, raw px/mm, computed DPI, rejection reason, and chosen fallback.

**Test scenarios:**
- `0x0`, one-axis-zero, 1 mm, enormous mm, overflow-edge, 30 DPI, 800 DPI, and 2:1 X/Y mismatch.
- Normal 96/144/192 DPI and exact quantization boundaries just below/at/above each step.
- Two outputs forming one monitor, cloned outputs, an outputless manually defined monitor, projector-style unknown mm, and same-name reconnect with changed mm.
- Property test: every accepted input yields a finite supported scale; every rejected input chooses the documented fallback.

**Recommended phase placement:** **Scale Policy phase**, immediately after the topology snapshot and before any rendering or geometry consumer.

---

### 2. Mixing Root Pixels, Client Pixels, Frame Pixels, and Logical Units

**What goes wrong:** Windows drift on round trips, decorations are counted twice, maximize leaves gaps, a southeast-gravity window moves from its anchored corner, or EWMH properties contain private logical units clients cannot interpret.

**Why it happens:** X11 protocol geometry is integral pixels. ICCCM hints describe client-area pixel constraints. EWMH desktop geometry, work areas, struts, and frame extents are root-window pixel contracts. A per-monitor logical coordinate system is therefore an FVWM-X internal abstraction, not a replacement wire coordinate system.

**Warning signs / detection:**
- A 1x → 1.25x → 1x round trip changes size by more than one pixel or accumulates drift on repetition.
- Client `ConfigureNotify` reports frame rather than client coordinates or dimensions.
- `_NET_FRAME_EXTENTS` disagrees with actual scaled borders.
- `_NET_WORKAREA`, struts, or desktop geometry are divided by monitor scale.
- Code multiplies `x`, `y`, `width`, `height`, and each border independently.

**Prevention:**
- Define coordinate-bearing types or strongly named APIs for `root_px`, `client_px`, `frame_px`, and internal `logical_fp`; do not pass anonymous `rectangle` values across scale boundaries.
- Store canonical logical **client-area** width/height for normal resizable windows. Decorations are WM-owned and recomputed from the destination scale. Do not define the canonical size as the current outer frame.
- Keep root positions in root pixels. Preserve a documented anchor (pointer grab point during interactive move; ICCCM gravity/reference point for programmatic resize) while changing size.
- Use fixed-point or rational arithmetic with 64-bit intermediates. Transform rectangle edges through one shared helper and derive width from transformed endpoints. Specify one tie-breaking rule; never rely on casts from floating point.
- Clamp and reject before narrowing. Xlib notes that protocol coordinates/sizes are 16-bit quantities and oversized coordinates may be silently truncated.
- Keep all ICCCM/EWMH properties in root/client pixels. The logical record is private FVWM-X state.

**Test scenarios:**
- Round trips among `1.0`, `1.25`, `1.5`, `1.75`, and `2.0`, repeated 1,000 times from odd and even sizes.
- Negative origins, INT16 boundaries, one-pixel rectangles, nonzero client border width, asymmetric decorations, and every ICCCM gravity.
- Verify actual frame/client geometry against synthetic `ConfigureNotify` and `_NET_FRAME_EXTENTS`.
- Maximize, fullscreen, shade, iconify/restore, and restart with each scale pair.

**Recommended phase placement:** **Geometry Foundation phase**, before cross-monitor resize and before scaled borders are wired into `move_resize.c` or `borders.c`.

---

### 3. Cross-Monitor Resize Oscillation and Seam Thrashing

**What goes wrong:** A window repeatedly changes size near a seam. Resizing to the destination scale changes overlap or center position, ownership flips back, and the inverse resize repeats. A client that responds to `ConfigureNotify` with its preferred old size can create a second feedback loop.

**Why it happens:** Monitor selection, scale application, and post-resize monitor selection are often implemented as independent stateless calculations. Overlapping monitors and dead areas make “the monitor containing the center” ambiguous. Fractional rounding makes exact boundary equality unstable.

**FVWM-X exposure:** `FindScreenOfXY()` returns the first containing monitor in tree order and uses traversal order for equal-distance dead-area ties. Current assignments to `fw->m` occur in placement, movement, update, and maximize paths, so adding scaling independently at those sites would create several transition authorities.

**Warning signs / detection:**
- Alternating monitor IDs or configure sizes in consecutive events.
- More than one WM-initiated resize for the same pointer move/topology generation.
- High `ConfigureRequest`/`ConfigureNotify` traffic while the pointer is stationary.
- Ownership changes when only the WM's own border size changed.

**Prevention:**
1. Centralize ownership. For an interactive move, the pointer's monitor determines the pending destination; for noninteractive geometry, choose greatest frame intersection, then incumbent, then primary, then stable monitor identity.
2. Determine the destination from **pre-transition** geometry, commit it, apply scale once, and do not immediately feed the resulting geometry back into selection.
3. Store `(topology_generation, monitor_id, scale, transition_serial)` on each window. A transition with the same tuple is a no-op.
4. Add incumbent hysteresis for ambiguous overlap/seam cases. Use a named logical margin and convert it once; do not sprinkle `+1` seam fixes through movement code.
5. Freeze the destination for one interactive operation. Re-evaluate only on pointer movement into another monitor, operation completion, or a newer topology generation.
6. If a client immediately requests a different legal size, do not fight it indefinitely. Respect fixed-size hints and provide a per-window/style opt-out from logical-size preservation for legacy or self-scaling clients.

**Test scenarios:**
- Drag slowly and rapidly across both directions of every scale pair; stop exactly one pixel on each side of the seam.
- Windows wider than either monitor, 50/50 overlap, overlapping monitors, cloned monitors, L-shaped layouts, and gaps.
- A client that echoes every configure size, one that requests its prior size, and one that changes `WM_NORMAL_HINTS` during the transition.
- Assert a bounded transition count and no alternating monitor sequence with a stationary pointer.

**Recommended phase placement:** **Logical-Size Migration phase**. Do not add preservation opportunistically to existing `fw->m` assignments.

---

### 4. Rebuilding Topology In Place During RandR Event Storms

**What goes wrong:** Hotplug leaves windows attached to disabled monitors, dereferences stale monitor/output state, performs the reconciliation several times against intermediate layouts, exits because a query temporarily returns no monitor, or stalls the single-threaded event loop with repeated round trips.

**Why it happens:** RandR changes are dynamic and asynchronous. Selecting for notifications can itself produce an immediate event. One user action can generate screen, CRTC, output, resource, root configure, and property events. RandR timestamps exist specifically because cached configuration becomes stale.

**FVWM-X exposure:**
- `dispatch_event()` rescans and broadcasts synchronously for every `RRScreenChangeNotify` and every `RRNotify`.
- `FScreenSelect()` omits CRTC/resource masks; RandR 1.5 `RRSetMonitor`/`RRDeleteMonitor` explicitly generate root `ConfigureNotify`.
- `scan_screens()` mutates the global tree and monitor objects while discovering changes. A post-startup `XRRGetMonitors()` failure/zero result can mark every monitor disabled.
- Many windows hold raw `struct monitor *` pointers, while monitor lifecycle and per-monitor desktop state are global and order-sensitive.

**Warning signs / detection:**
- Multiple module broadcasts and EWMH rewrites for one topology action.
- A transient zero-monitor snapshot is committed.
- Window ownership points at `MONITOR_DISABLED` after reconciliation.
- Scale or primary changes depend on event ordering.
- Desktop freezes while docking/undocking.

**Prevention:**
- Event handlers should only mark topology dirty and capture the newest event time. Rebuild once at the next event-loop safe point after the current bounded X-event batch; coalesce same-cycle notifications without a sleep-based debounce.
- Build a complete immutable candidate snapshot off to the side, validate it, then atomically swap the pointer/generation. Reconcile every window from old → new exactly once. Never expose a half-mutated monitor tree.
- Treat query failure separately from a valid empty result. Keep the last good snapshot and retry on the next event/deadline; use a root-sized emergency monitor only under an explicit degraded-mode policy. Never exit the WM because live topology is temporarily unavailable.
- Subscribe to all relevant RandR notifications and handle root `ConfigureNotify` for monitor-object changes. Make refresh idempotent because duplicate notifications are valid.
- Copy Xlib reply data into owned storage and free the reply immediately. Do not retain `XRRMonitorInfo` pointers or `outputs` arrays.
- Identify snapshots by RandR monitor-list timestamp plus an FVWM generation. If a newer event is already queued after a query, allow one subsequent refresh rather than blocking with `XSync`.
- Reconcile removed owners deterministically: greatest intersection with surviving monitors, then primary. Cancel/rebase interactive operations whose source monitor disappeared; clamp inaccessible windows into a surviving work area.

**Test scenarios:**
- Add/delete/redefine a RandR 1.5 monitor; change primary; alter only mm; alter only output membership; deliver duplicate and reordered notifications.
- Remove the monitor under an active move, resize, menu, pointer grab, fullscreen window, maximized window, and focused window.
- Simulate query failure between two valid snapshots and assert that the last good generation remains authoritative.
- Burst 100 topology notifications and assert one bounded reconcile per event-loop cycle, no stale owners, and continued module/X-event fairness.

**Recommended phase placement:** Split across two phases: **Topology Snapshot + Live-X Harness first**, then **Hotplug Window Reconciliation** after geometry-transition semantics exist.

---

### 5. Treating X Resources or XSETTINGS as Per-Monitor State

**What goes wrong:** FVWM changes `Xft.dpi` when the pointer or focused window crosses a monitor, causing unrelated applications to relayout, new and old clients to disagree, or FVWM fonts to be scaled twice. Runtime resource changes may not be observed consistently.

**Why it happens:** `RESOURCE_MANAGER` is global to an X display (screen 0), `SCREEN_RESOURCES` is per X **screen**, and XSETTINGS uses one manager selection per X screen. RandR monitors are not X screens. Qt documents its normal X11 DPI precedence as global XSETTINGS `Xft/DPI`, global X resource `Xft.dpi`, then 96 DPI.

**Warning signs / detection:**
- Code writes `RESOURCE_MANAGER`, `SCREEN_RESOURCES`, or XSETTINGS on monitor focus/crossing.
- FVWM font size is multiplied by monitor scale while Xft point-size resolution also consumes an elevated global DPI.
- Restarted applications render at a different size from already-running applications.
- A root property change causes every stored logical window size to be recomputed.

**Prevention:**
- FVWM-X must **not** publish per-monitor scale by rewriting global DPI resources and must not become an XSETTINGS manager for this feature.
- Treat `Xft.dpi`/`Xft/DPI` only as an optional global user preference or fallback, never as monitor physical DPI. Document its exact position in the scale-source chain.
- For WM-owned fonts, choose one final sizing path: resolve the target pixel size once, then construct the Xft/fontconfig request so hidden DPI conversion cannot multiply it again. Do not scale both point size and DPI.
- If FVWM reads X resources dynamically, watch the relevant root property, parse into a candidate config, and apply one generation update. Explicit FVWM monitor overrides remain higher precedence.
- Keep configuration matching separate from monitor discovery. A malformed resource must not invalidate topology.

**Test scenarios:**
- No resources, `Xft.dpi` at 96/144/192/malformed/negative/huge, XSETTINGS present/absent/replaced, and runtime `xrdb` replacement.
- Verify monitor crossing does not change root resource properties or trigger global client relayout.
- Compare actual Xft glyph metrics at each monitor scale to catch double application.

**Recommended phase placement:** **Configuration + Scale Policy phase**, with font-metric verification repeated in the UI Scaling phase.

---

### 6. Assuming the Window Manager Controls Client DPI

**What goes wrong:** FVWM scales client content, fights a toolkit's own resize, or promises physical-size preservation that an X11 client cannot honor. Qt/GTK applications, legacy bitmap-font clients, games, terminals with resize increments, and clients with environment overrides all react differently.

**Why it happens:** X11 has no universal per-window/per-RandR-monitor DPI contract. Toolkits own rendering and may consume global settings, physical DPI only when explicitly enabled, environment overrides, or application-specific policy. A WM can request client pixel geometry; it cannot safely resample arbitrary client rendering or infer the client's internal device-independent size.

**Warning signs / detection:**
- Use of XRender/Compositor transforms to scale arbitrary client pixels in this milestone.
- Rewriting client DPI properties or environment after process launch.
- Repeated client configure requests undoing the WM's cross-monitor size.
- Fixed-size clients being resized in violation of `WM_NORMAL_HINTS`.

**Prevention:**
- Keep the v0.1 contract narrow: scale FVWM-owned decorations/fonts/icons/cursors/hit targets and adjust legal client geometry; never resample arbitrary client pixels.
- Describe logical-size preservation as an FVWM geometry policy, not guaranteed client-content DPI awareness.
- Honor client min/max/base/increment/aspect constraints after calculating the ideal destination client size. If exact preservation is impossible, choose the nearest legal size deterministically and record that the transition was constrained.
- If min equals max, preserve the client pixel size and scale only WM-owned extents. Provide a style/config opt-out for applications that manage their own monitor transition.
- Process at most one WM scale transition per committed crossing. Subsequent client requests are normal ICCCM input, not a reason to reapply the same transition.

**Test scenarios:**
- Purpose-built clients with no hints; min/max; fixed size; resize increments; aspect limits; changing hints; and configure-request feedback.
- Qt with `QT_SCALE_FACTOR`, `QT_SCREEN_SCALE_FACTORS`, and physical-DPI opt-in; GTK with and without XSETTINGS; Xterm-like increment-constrained clients.
- Confirm client pixels are never transformed by the WM and that opt-out behavior is stable.

**Recommended phase placement:** Define the contract in **Scale Policy**, implement it in **Logical-Size Migration**, and verify with live clients before milestone completion.

---

### 7. Violating ICCCM/EWMH While “Scaling Everything”

**What goes wrong:** Resize increments are broken, fullscreen restores to scaled fullscreen geometry rather than prior normal geometry, maximized windows cover panels, struts are scaled twice, or gravity/reference points move when frame extents change.

**Geometry contracts that must remain pixel-based:**

| Contract | Required interpretation | Mixed-DPI failure to avoid |
|---|---|---|
| `WM_NORMAL_HINTS` | Client-pixel min/max/base/increments/aspect/gravity | Scaling the hints themselves; violating fixed/increment sizes |
| Synthetic `ConfigureNotify` | Client top-left in root coordinates and client width/height | Reporting frame geometry or private logical units |
| `_NET_FRAME_EXTENTS` | Actual WM border widths in root pixels | Stale 1x values after destination-scale border rebuild |
| `_NET_WORKAREA` | Root-pixel area per desktop | Publishing per-monitor logical coordinates |
| `_NET_WM_STRUT_PARTIAL` | Client-supplied root coordinates measured from screen edges | Scaling incoming struts or treating them as monitor-relative |
| Maximized state | Destination monitor work area, honoring reservations | Generic multiplicative resize instead of state recomputation |
| Fullscreen state | Entire destination monitor, no decorations; restore prior geometry | Including work-area struts or losing canonical normal geometry |

**Prevention:**
- Put size-hint solving after ideal scale conversion and before frame construction. Use one solver for interactive, command, maximize/restore, and crossing paths.
- Preserve the ICCCM reference point according to current gravity. Applications may change gravity while mapped; consume a coherent hints snapshot.
- Treat maximized/fullscreen as derived geometry states. On monitor change, recompute from destination monitor/work area instead of scaling the current derived rectangle.
- Preserve normal geometry separately while maximized/fullscreen. Restore through the normal destination-scale conversion exactly once.
- Consume struts verbatim as root pixels, recompute per-monitor intersections, and publish EWMH values only after topology/scale reconciliation commits.
- Update frame extents and send coherent configure notifications after decoration scale changes.

**Test scenarios:**
- All gravities with size-only and position+size requests.
- Base size plus increments where the ideal scaled size falls exactly between legal values; min/max and aspect conflicts.
- Panel struts on outer and internal monitor edges, partial struts, negative/overlapping monitor layouts, maximize/fullscreen migration, and restore.
- Change hints and struts during a crossing and verify one coherent final state.

**Recommended phase placement:** Core semantics belong in **Geometry Foundation**; maximize/fullscreen/work-area integration belongs in **Logical-Size Migration**; final EWMH assertions belong in **Hotplug Reconciliation**.

## Moderate Pitfalls

### 8. Scaling Only What Is Easy to See

**What goes wrong:** Title text grows but buttons, border grabs, menu spacing, icons, cursors, pan frames, edge resistance, and hit targets remain 1x or are scaled twice by separate call sites.

**Prevention:** Create one resolved WM metrics object per `(theme/config generation, scale bucket)`. It owns font metrics, border/title dimensions, icon target sizes, menu spacing, cursor size, and interaction thresholds. Consumers request resolved pixels; they do not multiply config values themselves. Enforce minimum one-pixel visible strokes and minimum usable hit targets after rounding.

**Detection/tests:** Screenshot/pixel assertions are secondary. Primary checks should inspect resolved metrics, actual frame extents, button/hit rectangles, pointer-grab cursor changes, and hit-testing at every edge for each scale bucket.

**Phase placement:** **WM-Owned UI Scaling**, after scale and geometry foundations.

### 9. Assuming Xcursor Automatically Follows the Monitor

**What goes wrong:** The frame is 2x but resize/move/grab cursors remain 1x, or cursor size changes only after restart. Xcursor chooses the closest image to a display-global default unless the caller requests/assigns sizes deliberately.

**Prevention:** Cache cursor resources by theme and scale bucket, assign them to WM-owned windows, and update the active pointer-grab cursor when an interactive operation crosses monitors. Free old cursor generations only after no window/grab references them.

**Phase placement:** **WM-Owned UI Scaling**; crossing behavior verified again in **Logical-Size Migration**.

### 10. Using Monitor Name, Output XID, or EDID Serial as a Perfect Identity

**What goes wrong:** Overrides attach to the wrong display after docking, duplicate EDIDs collide, output names change, or a deleted/recreated monitor inherits stale per-monitor runtime state.

**Prevention:** Separate runtime identity from config matching. Runtime monitor identity is snapshot-local plus generation. Configuration matching should be explicit and observable: exact configured RandR monitor name first, then an optional documented output-set match, then fallback. Never assume EDID serial is present or unique. On ambiguous matches, reject the automatic match and log candidates rather than guessing.

**Phase placement:** **Topology Snapshot + Configuration**.

### 11. Conflating Virtual-Desktop Wrapping with Physical Monitor Selection

**What goes wrong:** Negative coordinates or dead areas wrap to the opposite side of the root, and overlap ownership depends on tree order. This can select the wrong scale before placement or maximize.

**FVWM-X exposure:** `FindScreenOfXY()` applies modulo `DisplayWidth/DisplayHeight` before containment and picks the first monitor in RB order. That behavior mixes virtual-page normalization with monitor hit testing.

**Prevention:** Split APIs: one explicitly normalizes virtual-page coordinates; another performs literal root-rectangle monitor selection with deterministic overlap/dead-area rules. Never modulo in the latter. Validate monitor rectangles with 64-bit endpoint arithmetic and clip only where an X/EWMH wire contract requires it.

**Phase placement:** **Geometry Foundation**, before ownership transitions.

## Required Observability

Without observability, mixed-DPI failures appear as random jumps. Add debug output or a diagnostic command exposing:

- Topology generation and RandR timestamp.
- Every monitor's name, full output set, raw geometry, raw mm, primary/automatic flags, validation result, scale source, and resolved scale.
- Every window's incumbent monitor, canonical logical client size, current client/frame pixel geometry, last transition serial, and constrained-size reason.
- Dirty-event counts, snapshot rebuild count, discarded/failed snapshots, and window reconciliation count.
- A rate-limited warning for oscillation detection (for example, repeated monitor alternation without pointer/topology change).

These fields should be available to tests without parsing human log prose where practical.

## Recommended Test Strategy

### Pure Native Tests (Required Before X Wiring)

Follow the repository's existing pattern: extract narrow `.c/.h` policy units, use plain `assert`, local fixtures, and register each executable in `tests/meson.build`.

1. **Scale validation:** px/mm boundaries, mismatch, quantization ties, override precedence, malformed config, and fallback reasons.
2. **Rectangle conversion:** endpoint rounding, overflow, negative origins, round-trip invariants, and no cumulative drift.
3. **Monitor ownership:** seam, overlap, gap, primary/incumbent tie breaks, disabled monitors, and deterministic ordering independent of input list order.
4. **Transition state machine:** idempotence by generation/serial, hysteresis, frozen interactive destination, constrained clients, and client feedback.
5. **ICCCM solver:** min/max/base/increments/aspect/gravity combinations, including contradictory or malformed hints handled safely.
6. **Snapshot diff/reconcile plan:** add/remove/rename/redefine/primary/mm/output-set changes and query-failure retention.

Use table-driven fixtures and property-style loops. Tests should assert not just output geometry but scale source, rejection reason, selected owner, transition count, and emitted actions.

### Live-X Integration Tests (Milestone Gate)

The current suite starts no X server, WM, or client; unit tests alone cannot verify event masks, root properties, reparented geometry, grabs, or actual notifications.

Build a deterministic Linux-only harness that starts a disposable X server, FVWM-X with a minimal config, and a purpose-built Xlib client. Prefer event/deadline waits and property polling with hard timeouts; do not use arbitrary sleeps.

Minimum scenarios:

1. Create/delete/redefine RandR 1.5 monitor objects with `xrandr --setmonitor/--delmonitor`; verify FVWM receives root/RandR changes and updates one generation.
2. Define 96- and 192-DPI monitors; place and move a hinted client across both directions; assert client/frame geometry, owner, extents, and transition count.
3. Repeat with 125%/150% scales and odd sizes to expose rounding drift.
4. Change primary and rearrange monitors while normal, maximized, fullscreen, and iconified windows exist.
5. Delete the active monitor during move/resize and while a menu/pointer grab is active; assert bounded recovery and responsiveness.
6. Exercise overlap, gap, outputless monitor, multi-output monitor, negative coordinates if accepted by the server, and a monitor wider than protocol-safe client coordinates.
7. Run a client that changes `WM_NORMAL_HINTS` and one that counters WM resize requests; assert no configure storm.
8. Replace `RESOURCE_MANAGER`/XSETTINGS inputs and verify precedence without per-monitor global-resource writes.
9. Assert `_NET_WORKAREA`, `_NET_FRAME_EXTENTS`, fullscreen restore, maximize, and synthetic `ConfigureNotify` values in root/client pixels.
10. Burst topology changes while flooding benign X events/module work; assert event-loop fairness remains bounded.

`Xvfb` plus RandR monitor objects can cover monitor-list and many geometry contracts. It does **not** prove real connector/EDID hotplug. Add a native-Linux scenario using a controllable Xorg dummy/vkms-style environment if CI permits; otherwise mark real output connect/disconnect and bogus EDID as an explicit manual or dedicated-runner gate rather than pretending Xvfb covers it.

## Phase-Specific Warning Matrix

| Recommended phase | Must resolve before exit | Failure if deferred | Confidence |
|---|---|---|---|
| 1. Topology Snapshot + Live-X Harness | Complete monitor model, relevant event subscriptions, immutable generations, query-failure behavior, basic monitor-object integration test | Every later scale/geometry feature consumes stale or incomplete state | HIGH for code observation; MEDIUM for final event policy |
| 2. Scale Policy + Configuration | Override precedence, mm validation, fallback, quantization, identity matching, X resource boundary | Bad EDID can make the desktop unusable; font/client scaling becomes incoherent | MEDIUM |
| 3. Geometry Foundation | Typed coordinate spaces, canonical logical client size, rounding, ownership, ICCCM solver, gravity | Cross-monitor behavior accumulates drift and violates client contracts | MEDIUM |
| 4. WM-Owned UI Scaling | One metrics pipeline for borders/fonts/icons/cursors/hit targets; frame extents | Visually inconsistent UI and hidden double scaling | MEDIUM |
| 5. Logical-Size Migration | Transition state machine, hysteresis, client opt-out, maximize/fullscreen semantics | Oscillation, configure storms, lost restore geometry | MEDIUM |
| 6. Hotplug Reconciliation + Compatibility | Removed-owner recovery, active-operation cancellation/rebase, EWMH atomic update, soak/client matrix | Windows become inaccessible or global state points to disabled monitors | MEDIUM |

**Ordering rule:** Do not combine phases 2–5 in one broad change to `move_resize.c`/`borders.c`. The codebase map already identifies those files as large and tightly coupled. Each phase should first land pure policy and tests, then wire one X-visible behavior through the single authoritative path.

## Sources

Confidence labels use the research seam's provider classification. Direct official/canonical documents were cross-checked where a claim is critical; overall confidence remains MEDIUM because live-server behavior and FVWM policy still require validation.

- **XRandR protocol 1.5 (canonical X.Org source)** — monitor semantics, unknown millimetres, dynamic topology, event masks, timestamps, monitor requests: https://cgit.freedesktop.org/xorg/proto/randrproto/plain/randrproto.txt — **MEDIUM**
- **xrandr 1.5.3 manual (current Debian rendering of upstream manual)** — monitor geometry, transforms, panning, and the lack of physical meaning for global screen DPI in mixed-DPI layouts: https://manpages.debian.org/testing/x11-xserver-utils/xrandr.1.en.html — **MEDIUM**
- **XRandR/Xlib manual** — hotplug notifications, timestamp freshness, Xlib configuration updates: https://cgit.freedesktop.org/xorg/lib/libXrandr/plain/man/Xrandr.man — **MEDIUM**
- **ICCCM 2.0** — client configuration, normal hints, resize, and gravity contracts: https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html — **MEDIUM**
- **XSetWMNormalHints / XSizeHints (libX11 1.8.13)** — min/max/base/increments/aspect/gravity fields: https://manpages.debian.org/testing/libx11-doc/XSetWMNormalHints.3.en.html — **MEDIUM**
- **EWMH 1.5 root properties** — desktop geometry and work area: https://specifications.freedesktop.org/wm-spec/latest/ar01s03.html — **MEDIUM**
- **EWMH 1.5 application properties** — fullscreen, struts, and frame extents: https://specifications.freedesktop.org/wm-spec/latest/ar01s05.html — **MEDIUM**
- **EWMH implementation notes** — gravity and synthetic `ConfigureNotify` geometry: https://specifications.freedesktop.org/wm-spec/latest/ar01s09.html — **MEDIUM**
- **Xlib manual** — integral pixel coordinates, protocol-width truncation, asynchronous requests, and resource manager scope: https://www.x.org/releases/current/doc/libX11/libX11/libX11.html — **MEDIUM**
- **xrdb manual** — `RESOURCE_MANAGER` display-global and `SCREEN_RESOURCES` per-X-screen scope: https://www.x.org/releases/current/doc/man/man1/xrdb.1.xhtml — **MEDIUM**
- **XSETTINGS specification** — one manager per X screen, atomic property refresh, and runtime update model: https://specifications.freedesktop.org/xsettings-spec/latest/ — **MEDIUM**
- **Qt 6.11 High DPI documentation** — X11 global DPI source precedence, client overrides, device-independent geometry, and scale rounding: https://doc.qt.io/qt-6/highdpi.html — **MEDIUM**
- **GTK 3.24 Settings documentation** — XSETTINGS-backed per-X-screen settings and application override behavior: https://docs.gtk.org/gtk3/class.Settings.html — **MEDIUM**
- **Xcursor manual** — nominal cursor sizes and display-default selection: https://www.x.org/releases/current/doc/man/man3/Xcursor.3.xhtml — **MEDIUM**
- **Linux kernel EDID documentation** — absent/bogus EDID and KVM substitution are real supported failure cases: https://docs.kernel.org/admin-guide/edid.html — **MEDIUM**

## Research Gaps Requiring Phase Validation

- Exact automatic-DPI acceptance range, quantization steps, and hysteresis margin are recommended starting policies, not X11 standards; calibrate them with target hardware and user expectations.
- Xvfb coverage of RandR 1.5 monitor objects must be confirmed in the chosen CI image. Real output hotplug/EDID injection needs a separate Linux mechanism.
- The existing font-loading path must be traced during the UI phase to prove whether configured Xft point sizes already consume global DPI and to prevent double scaling.
- Module ABI/config exposure for scale and topology generations is deliberately not specified here; compatibility breaks are allowed, but one authoritative representation is required before broadcasting it.
