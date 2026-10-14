# Feature Landscape: Mixed-DPI Support in FVWM-X

**Domain:** Per-monitor HiDPI behavior in an existing non-compositing X11 window manager
**Milestone:** FVWM-X v0.1
**Researched:** 2026-08-29
**Overall confidence:** MEDIUM

## Scope and Product Contract

For this milestone, “mixed-DPI support” should mean that each XRandR 1.5
monitor has one explicit, inspectable scale and that FVWM-X applies it to
window-manager-owned pixels and geometry. It should **not** promise that FVWM-X
can rerender arbitrary X11 client content. X11 has root-pixel geometry and no
standard per-monitor logical-DPI setting; Qt's current X11 documentation calls
out exactly this mismatch. This makes FVWM-X's strongest credible promise:

> FVWM-owned UI remains physically usable, managed windows retain their chosen
> logical size when their owning monitor changes, and X11 protocol geometry
> remains correct through topology changes.

“Table stakes” below means required for that stated contract, not behavior that
every current X11 WM already implements. Source-backed claims are MEDIUM
confidence because the research seam selected Jina (MEDIUM) and official pages
were retrieved through its available WebFetch fallback. Product recommendations
that choose thresholds or policy beyond the standards are explicitly identified.

## Table Stakes

Missing any item in this section makes mixed-DPI support feel incomplete or
unsafe.

| Feature | User-visible behavior / acceptance scenario | Why expected | Complexity | Confidence |
|---|---|---|---|---|
| **One authoritative active-monitor topology** | FVWM-X treats each active XRandR 1.5 `Monitor`, not each connector, as one user-visible area. A layout with negative coordinates, rotated outputs, exact clones, gaps, or overlapping monitors starts without duplicate ownership or lost areas. Primary status is honored but is never mistaken for “the only monitor.” | RandR 1.5 introduced Monitor specifically as the logical viewable area above outputs/CRTCs. All later scale and ownership decisions need stable monitor identity and geometry. | High | MEDIUM — RandR protocol |
| **Deterministic per-monitor effective scale** | Every active monitor resolves to exactly one finite positive scale. A monitor-specific explicit override wins over auto detection. Auto uses validated monitor pixel/mm data against a 96-DPI baseline, quantized to a supported step; missing, zero, contradictory, or implausible physical size falls back safely to 1x rather than producing extreme UI. Reconnecting the same named monitor reapplies its override. | Users need deterministic control because X11 exposes global logical DPI but only per-monitor physical DPI; physical dimensions may legally be unknown. | Medium | MEDIUM — RandR + Qt + AwesomeWM |
| **Scale introspection and reason reporting** | A user command/module query shows, per monitor: identity, root-pixel rectangle, primary/automatic status, physical mm, calculated DPI or invalid reason, configured mode (`auto` or explicit), and effective scale. Invalid overrides are rejected with a visible diagnostic; there is no silent coercion. | Auto scaling cannot be trusted or debugged if users cannot see its inputs and precedence. This is especially important with bad EDID data. | Low–Medium | MEDIUM — recommendation grounded in source limitations |
| **Scale-aware decorations and core UI** | On a 2x monitor, titlebars, border art, title fonts, buttons, menus, iconified-window UI, move/resize feedback, and other core-owned visual surfaces are approximately twice their 1x physical dimensions while retaining the same logical layout. On 1.5x, all dimensions use one documented rounding policy and adjacent pieces meet without gaps. | A HiDPI WM that scales only fonts or only borders still has tiny controls, clipping, or mismatched chrome. | High | MEDIUM — Qt high-DPI model + project scope |
| **Scale-aware interactive targets and cursors** | Resize handles, title-button hit boxes, edge/pan zones, snapping distances, and drag thresholds are expressed in logical units and converted for the active monitor. A thin visible border may retain a larger invisible hit target. WM-owned cursors reload/select the closest theme size when the pointer's WM context changes monitor; cursor hot spots remain aligned. | Usability depends on target size, not just visual size. Xcursor themes support multiple nominal sizes and closest-size selection. | High | MEDIUM — Xcursor + recommendation |
| **Exactly-once scale application** | Fonts, SVG/PNG assets, border widths, and hit targets are scaled once. A 12-unit title font at 2x does not become 4x because global Xft DPI and FVWM monitor scale were both applied. Raster assets choose the nearest adequate source and downsample if needed rather than enlarging a low-resolution icon unnecessarily. | Double scaling and blurry assets are common integration failures when logical font DPI and geometric scaling coexist. | Medium | MEDIUM — Qt asset/DPI model |
| **Stable monitor ownership during movement** | Every window has one owning monitor under a documented rule based on its frame/reference point and current ownership. Crossing a boundary causes at most one ownership/scale transition; hovering on the boundary, overlap ties, or one-pixel pointer jitter does not alternate scale every event. During an interactive drag, the grabbed frame point stays under the pointer. | Per-monitor scale is unusable if ownership chatters or the window jumps away from the cursor. Overlapping RandR monitor rectangles are legal. | High | MEDIUM — RandR permits overlap; handoff policy is product recommendation |
| **Logical-size preservation on scale handoff** | A normal client with an 800×600 client area on 1x becomes approximately 1600×1200 on 2x after ownership changes, subject to ICCCM constraints and available work area; moving it back returns exactly to 800×600 rather than drifting. Canonical logical dimensions, not the last rounded pixel dimensions, are preserved. Decorations are recomputed at the destination scale. | Stable perceived window size is the milestone's central user promise and follows the device-independent sizing model used by modern toolkits. | High | MEDIUM — Qt model + project decision |
| **ICCCM-constrained target sizing** | Scale-preserving resize first computes the desired logical target, then clamps min/max, quantizes base-plus-increment, honors aspect limits, and preserves the gravity reference point. A fixed-size client (`min == max`) is not forcibly resized; FVWM scales only its frame and keeps it visible. A terminal with 8×16 increments lands on a valid grid. | Resizing to an invalid client size breaks terminals, fixed dialogs, and gravity-sensitive placement. | High | MEDIUM — ICCCM + EWMH |
| **Correct maximize, fullscreen, placement, and work areas** | Maximize fills the destination monitor's work area after root-coordinate struts; fullscreen fills the exact monitor and has no decoration; leaving fullscreen restores the prior logical normal geometry. Initial placement and “move to monitor” commands choose a monitor first, then scale and constrain geometry. No window is placed into a virtual-desktop gap. | These are routine actions and EWMH-visible contracts. Mixed scale must not change their root-pixel meaning. | High | MEDIUM — EWMH + Qt geometry guidance |
| **Correct protocol-boundary geometry** | `_NET_FRAME_EXTENTS` matches the currently scaled frame in root pixels. `_NET_WORKAREA`, desktop geometry, struts, synthetic `ConfigureNotify`, and moveresize requests remain in X root pixels. Scale changes update affected properties before clients/pagers observe final geometry. | EWMH and ICCCM do not define FVWM logical units; publishing logical values would break panels, pagers, and clients. | High | MEDIUM — EWMH/ICCCM |
| **Atomic, non-destructive hotplug reconciliation** | A topology event is reconciled from one fresh snapshot, not from transient event order. Removing a monitor moves orphaned windows into a remaining work area, keeping them reachable and preserving normal/maximized/fullscreen state where possible. Adding a monitor does not pull windows onto it unexpectedly. A primary-only change does not resize windows whose owner and scale are unchanged. | RandR topology is asynchronous and may emit several notifications for one physical action. Hotplug must never strand windows or repeatedly reflow them. | High | MEDIUM — RandR dynamic model + recommendation |
| **Safe startup and compatibility default** | With one monitor, no scale configuration, and invalid/no physical size, FVWM-X behaves as 1x FVWM rather than changing all geometry. If RandR 1.5 monitor data is unavailable at runtime, startup continues with a clearly reported single-monitor/1x fallback or a documented hard requirement; it does not divide by zero or create zero-sized UI. | Existing users and nested-X tests frequently expose 96-DPI or missing-mm environments. | Medium | MEDIUM — RandR + product constraint |
| **First-party module consistency** | Any first-party module UI included in the v0.1 support claim (at minimum pager/buttons/identify-style UI if shipped by default) receives monitor identity/effective scale and redraws when its own monitor changes. Module UI is not permanently tiny while core decorations are scaled. Unsupported legacy modules continue safely and are documented as 1x. | FVWM presents modules as part of the desktop; visibly inconsistent bundled UI weakens the product claim. Compatibility breaks are allowed, but silent protocol mismatch is not. | High | MEDIUM — project architecture + recommendation |

### Recommended Auto-Scale Contract

This is an opinionated product policy, not an X11 standard:

1. A valid explicit monitor override wins.
2. Otherwise compute physical DPI from the XRandR Monitor rectangle and
   physical millimeters when both axes are valid and reasonably consistent.
3. Convert against a 96-DPI baseline and quantize to 0.25 steps. Quarter steps
   cover common 125/150/175% choices and Qt documents 25% increments as a good
   practical compromise.
4. Clamp auto-selected scale to a conservative supported range (recommended
   1x–3x for v0.1). Values outside it indicate bad metadata and fall back to 1x
   with a diagnostic; they must not silently clamp to a giant or tiny UI.
5. Treat global `Xft.dpi` as a global logical-font input, not as evidence that
   every physical monitor has the same density. Ensure it is not multiplied
   twice into FVWM-owned font sizing.

The exact “valid physical DPI” thresholds should be constants covered by unit
tests, not ad-hoc checks scattered through drawing code.

## Differentiators

These are not all required to establish the basic contract, but they would make
FVWM-X unusually dependable among X11 window managers.

| Feature | Value proposition / acceptance scenario | Complexity | Recommended timing | Confidence |
|---|---|---|---|---|
| **Reversible, drift-free cross-monitor sizing** | Repeatedly move a constrained window 100 times between 1.25x and 2x monitors; its stored logical size and final 1.25x pixel size are identical to the start. Rounding residue is retained in canonical state rather than accumulated. | High | v0.1 core differentiator | MEDIUM — recommendation |
| **Predictable fractional WM scale** | Explicit 1.25x, 1.5x, and 1.75x factors scale all FVWM-owned dimensions, fonts, assets, and hit regions using centralized rounding. Screenshots show no one-pixel seams in titlebar/button assembly and no clipped text. Client pixels are still not compositor-scaled. | High | After integer 1x/2x path is correct | MEDIUM — Qt recommends integer/quarter-step discipline |
| **Topology edge-case determinism** | Exact clones, partial overlaps, nested monitor objects, negative origins, and output reorderings produce stable monitor IDs and the same ownership tie-break every run. Conflicting scale declarations for outputs inside one logical Monitor produce one diagnostic rather than two scales for the same pixels. | High | v0.1 before hotplug sign-off | MEDIUM — RandR Monitor semantics |
| **Live scale change without restart** | Changing one monitor from 1.5x to 2x recomputes its WM UI and eligible windows once, preserves logical normal geometry, and leaves other monitors untouched. Active move/resize either finishes under the old snapshot or restarts cleanly; it never mixes scales mid-operation. | High | Late v0.1 if configuration reload already supports it | MEDIUM — recommendation |
| **User-selectable handoff policy** | Users can choose “preserve logical size on monitor change” or “preserve raw client pixels” globally/per style for legacy applications. The default remains logical preservation. A fixed/pixel-sensitive application can opt out without disabling scaled decorations. | Medium–High | Post-core, if real compatibility cases demand it | LOW–MEDIUM — product hypothesis |
| **Scale-aware first-party module protocol** | Core broadcasts monitor add/remove/change and effective-scale changes as versioned module packets. New modules redraw immediately; old modules ignore unknown packets or are rejected explicitly. `FvwmPager` uses physical root geometry for the map but destination-monitor scale for its own controls/text. | High | v0.1 only for declared supported modules; broaden later | MEDIUM — architecture-driven |
| **Explainable recovery decisions** | After hot-unplug, an inspection command reports that window X moved from removed monitor A to B, whether it was clamped, and whether ICCCM hints prevented logical preservation. | Medium | Post-MVP polish | LOW–MEDIUM — product hypothesis |
| **Deterministic simulation mode for users/tests** | A command or nested-X fixture can inject monitor rectangles, millimeters, and scales without physical hardware, then expose the resulting ownership/geometry. The same fixture covers hotplug and invalid EDID behavior. | Medium | Build alongside policy, not after it | MEDIUM — project verification requirement |

## Anti-Features

These should be explicitly excluded from the v0.1 promise. Several are tempting
shortcuts that would create blur, protocol incompatibility, or rewrite risk.

| Anti-feature | Why avoid | What to do instead | Confidence |
|---|---|---|---|
| **Arbitrary client-content scaling** | A non-compositing WM does not own client rendering. X11 has no standard per-monitor logical-DPI channel that makes every toolkit rerender, and compositor supersampling/downsampling is explicitly out of scope. | Resize eligible client windows to preserve logical outer size; scale only FVWM-owned chrome; document that client rendering remains application-controlled. | MEDIUM — Qt X11 limitation + archived GNOME compositor design |
| **Using `xrandr --scale`/CRTC transforms as the WM's HiDPI model** | RandR scale is an output image transform, not a logical UI scale. It can blur the whole output, alter framebuffer geometry, complicate pointer/input mapping, and affect every client. | Keep output transforms as externally configured topology input; build monitor scale as independent WM policy. | MEDIUM — RandR/xrandr docs |
| **Mutating global `Xft.dpi`, `Xft/DPI`, or toolkit environment variables per monitor** | These are global process/server inputs on X11. Rewriting them cannot reliably target the window currently on one monitor and may cause applications to double-scale or restart inconsistently. | Keep FVWM monitor scale internal and explicit; expose it to first-party modules through FVWM state/protocol. | MEDIUM — Qt X11 priority table |
| **Trusting physical millimeters unconditionally** | RandR permits zero when size is unknown; projectors and EDID bugs can report misleading values. Exact physical DPI is also not always the user's preferred logical scale. | Validate both axes, quantize/clamp, permit explicit override, and expose the fallback reason. | MEDIUM — RandR + Qt |
| **Choosing one global scale from the primary or highest-DPI monitor** | It makes one side of every mixed-DPI setup wrong and defeats the milestone. Primary is an ordering/menu preference, not a density authority. | Resolve scale independently for each RandR Monitor. | MEDIUM — RandR primary semantics |
| **Publishing EWMH/ICCCM geometry in logical units** | Panels, pagers, and applications interpret these values in root coordinates. Logical values would corrupt work areas, frame extents, configure notifications, and fullscreen/maximize behavior. | Convert to/from root pixels at all X protocol boundaries. | MEDIUM — EWMH/ICCCM |
| **Continuous rescale on every overlap/pointer event** | A window spanning two monitors would jitter, issue configure storms, and potentially oscillate between rounded sizes. | Maintain explicit owner state, a stable tie-break/hysteresis rule, and one transition per ownership change. | MEDIUM — recommendation |
| **Repeatedly deriving logical size from the current rounded pixel size** | Fractional round trips accumulate error, so a window grows or shrinks after repeated moves. | Store canonical logical normal geometry and derive physical target geometry from it each time. | MEDIUM — high-DPI model + recommendation |
| **Forcing scale-preserving resize through ICCCM constraints** | Fixed-size, increment-constrained, and aspect-constrained clients may malfunction or fight the WM. | Apply hints after scaling, preserve gravity, and report when exact logical size is impossible. | MEDIUM — ICCCM/EWMH |
| **Scaling visible borders but not input regions (or vice versa)** | Tiny resize targets remain unusable, or invisible giant targets steal clicks from clients. | Define visual and interactive metrics separately in logical units, scale both intentionally, and test boundary clicks. | MEDIUM — recommendation |
| **Independent scales for cloned outputs that present the same pixels** | One root-pixel region cannot simultaneously have two coherent WM scales; ownership becomes ambiguous. | Attach scale to the RandR Monitor logical area. Reject/warn on conflicting output-level selectors within one Monitor. | MEDIUM — RandR 1.5 monitor model |
| **Restoring unplugged windows to stale absolute coordinates** | The old monitor may no longer exist, leaving windows unreachable in root-space gaps. | Rehome to an active work area using stored logical geometry and a deterministic nearest/primary fallback. | MEDIUM — recommendation |
| **Integrated compositor or Wayland abstraction work** | This multiplies scope and is unnecessary for reliable WM-owned scaling on X11. | Remain compatible with external compositors and keep the milestone's X11 root-pixel contract. | HIGH — explicit project constraint |
| **Configuration-language redesign bundled with scaling** | It delays the runtime policy and makes failures hard to attribute. | Add the minimum monitor-scale expression to the existing command/config path; redesign syntax only after the runtime model is proven. | HIGH — explicit project constraint |

## Behavioral Decisions That Must Be Frozen Early

These are observable and therefore cannot remain accidental implementation
details:

1. **Monitor identity:** configure against RandR Monitor name, with a documented
   fallback for automatically named monitors. Output name may be an alias only
   when it resolves unambiguously to one Monitor.
2. **Ownership:** use one stable frame-based rule; retain the current owner on
   ties and use primary/output order only as a final deterministic fallback.
3. **Transition timing:** commit scale once when ownership changes, not on every
   intermediate overlap event. Interactive pointer anchoring must survive the
   resize.
4. **Canonical geometry:** store normal logical client geometry separately from
   current physical frame geometry and from maximized/fullscreen geometry.
5. **Rounding:** use one signed-safe conversion policy for positions, sizes,
   extents, and hit regions. Negative coordinates must round symmetrically.
6. **Constraint failure:** ICCCM validity and reachability win over exact logical
   preservation; expose that exception rather than looping resize requests.
7. **Hotplug destination:** prefer the surviving monitor with greatest overlap;
   otherwise nearest active work area; otherwise primary; never stale list order.

## Feature Dependencies

```text
XRandR 1.5 snapshot + stable Monitor identity
  → deterministic topology (including overlap/clone/negative-coordinate policy)
    → per-monitor scale resolution + validation + introspection
      → logical↔root-pixel conversion and centralized rounding
        ├─→ scaled decorations/fonts/assets/hit targets/cursors
        ├─→ EWMH frame extents/work areas/protocol boundaries
        └─→ first-party module scale propagation

ICCCM size hints + gravity helpers
  → canonical logical normal geometry
    → stable monitor ownership/handoff
      → logical-size preservation
        → maximize/fullscreen restoration
          → scale-change and hotplug reconciliation

Atomic topology snapshot
  + ownership/handoff
  + work-area computation
  + canonical logical geometry
    → non-destructive monitor removal/reconnection
```

Critical ordering consequences:

- Do not scale drawing code before there is one monitor-scale accessor; otherwise
  every UI path will invent fallback and rounding behavior.
- Do not implement cross-monitor resize before canonical logical geometry and
  ICCCM quantization exist; deriving from rounded physical geometry causes drift.
- Do not implement hotplug by calling ordinary move/maximize paths repeatedly for
  each RandR event; reconcile one complete topology snapshot.
- Module packet changes follow the core scale model, but module consumers should
  be upgraded before claiming their UI as HiDPI-supported.

## MVP Recommendation

Prioritize in this order:

1. **Topology and observable scale policy** — authoritative Monitor snapshot,
   explicit override, validated auto fallback, 1x safety, and inspection output.
2. **Central conversions plus WM-owned UI** — prove 1x/2x and common fractional
   factors across decorations, fonts, icons, menus, hit targets, and cursors.
3. **Geometry correctness** — EWMH/ICCCM boundaries, work areas, placement,
   maximize/fullscreen, canonical logical geometry, and drift-free handoff.
4. **Topology change behavior** — atomic hotplug/reconfiguration, orphan rehome,
   scale changes, primary changes, overlap/clone/negative-coordinate cases.
5. **Declared first-party modules and polish** — scale packets, redraw behavior,
   diagnostics, and simulation/integration scenarios.

Defer unless core behavior is already stable:

- Per-application opt-out policy: useful, but first validate real incompatible
  clients against the default logical-preservation rule.
- Arbitrary scale percentages: support disciplined 0.25 steps first.
- Automatic restoration of windows to a reconnected monitor: surprising unless
  a session-placement policy is designed separately.
- Any client-pixel supersampling, compositor integration, or toolkit environment
  injection: outside the product contract.

## Test Matrix for the Feature Claim

The release claim should require these externally visible scenarios:

| Scenario | Required result |
|---|---|
| 1920×1080@1x beside 3840×2160@2x | Chrome has equal logical size; eligible 800×600 client becomes 1600×1200 on 2x and returns exactly to 800×600. |
| 1x beside 1.5x with terminal resize increments | Destination size is the nearest valid base-plus-increment size; repeated round trips do not drift. |
| Fixed-size dialog moved to 2x | Client size remains fixed, frame/UI scales, dialog stays reachable, and no configure loop occurs. |
| Maximized window moved between scales | It fills each monitor's current strut-adjusted work area; unmaximize restores destination-scaled canonical normal geometry. |
| Fullscreen enter/leave on 2x | Fullscreen equals exact monitor root rectangle with zero decoration; leave restores prior logical normal geometry. |
| Negative-origin monitor | Placement, frame extents, hit testing, and round-trip conversions are correct for negative X/Y. |
| Exact clone and partial overlap | One stable owner/scale is selected; no duplicate monitor transition or boundary chatter occurs. |
| Invalid/zero physical millimeters | Auto scale is 1x with an inspectable reason; no divide-by-zero or extreme UI. |
| Unplug current owner during drag and while idle | One final reconciliation occurs; window remains reachable; operation ends safely; event loop remains responsive. |
| Primary changes without geometry/scale change | Panels/placement preference update as applicable, but existing windows do not resize merely because primary changed. |
| Scale config reload on one monitor | Only that monitor's owned UI/windows update once; unaffected monitors retain exact geometry. |
| External compositor running vs absent | FVWM-owned scale/geometry behavior is the same; no compositor dependency is introduced. |

## Sources

### Official / Current Primary Sources

- X.Org RandR protocol 1.5 — Monitor model, physical dimensions, primary,
  overlap/outputs, asynchronous changes:
  https://cgit.freedesktop.org/xorg/proto/randrproto/plain/randrproto.txt
- X.Org `xrandr(1)` — global DPI physical-size behavior and per-output projective
  transforms/scale:
  https://www.x.org/releases/current/doc/man/man1/xrandr.1.xhtml
- Qt 6.11 High DPI — device-independent geometry, stable size across screens,
  X11 DPI source limitations/precedence, asset selection, and scale rounding:
  https://doc.qt.io/qt-6/highdpi.html
- Extended Window Manager Hints 1.5 — desktop/work area and root geometry:
  https://specifications.freedesktop.org/wm-spec/latest/ar01s03.html
- Extended Window Manager Hints 1.5 — maximize/fullscreen, struts, icons, and
  frame extents:
  https://specifications.freedesktop.org/wm-spec/latest/ar01s05.html
- EWMH implementation notes — fixed-size windows, gravity, synthetic geometry:
  https://specifications.freedesktop.org/wm-spec/latest/ar01s09.html
- ICCCM 2.0 — `WM_NORMAL_HINTS` min/max, base/increment, aspect, and gravity:
  https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html#WM_NORMAL_HINTS_Property
- X.Org Xcursor — multi-size themes, closest-size selection, default display
  cursor size:
  https://www.x.org/releases/current/doc/man/man3/Xcursor.3.xhtml
- AwesomeWM current source — explicit per-screen DPI/application and fallback
  precedent:
  https://raw.githubusercontent.com/awesomeWM/awesome/master/lib/beautiful/xresources.lua

### Historical Corroboration (not authoritative for current product behavior)

- Archived GNOME fractional-scaling design — illustrates why arbitrary
  fractional client-content scaling is compositor work:
  https://wiki.gnome.org/Initiatives/FracionalScaling

## Confidence Notes and Remaining Gaps

| Area | Confidence | Gap to resolve during phase planning |
|---|---|---|
| RandR Monitor/topology semantics | MEDIUM | Primary source is stable but old; validate actual Xorg/Xvfb behavior for monitor change event sequences and automatic monitor names. |
| X11 logical-DPI limitations | MEDIUM | Qt documents toolkit behavior clearly; GTK and legacy application behavior remains heterogeneous and should be tested, not generalized. |
| EWMH/ICCCM geometry obligations | MEDIUM | Standards are authoritative, but FVWM's virtual desktops/per-monitor desks require code-specific mapping tests. |
| Auto-scale thresholds | MEDIUM-LOW | Quantization and safe range are product policy; select concrete invalid-DPI/aspect thresholds with fixtures from real EDID failures. |
| Logical-size preservation UX | MEDIUM | Strong modern precedent and explicit project decision, but pixel-sensitive/fixed-size clients need an empirical compatibility corpus. |
| Per-monitor cursor behavior | MEDIUM-LOW | Xcursor provides suitable sizes but its default is Display-scoped; verify which FVWM-owned windows can swap cursor resources without flicker. |
| First-party module scaling | MEDIUM | Architecture supports protocol evolution, but v0.1 must explicitly name supported modules and packet compatibility behavior. |

What may still be missing: toolkit-specific reactions to WM-driven resizes,
rotated-monitor physical-DPI calculation, remote X servers, and interaction with
existing FVWM styles that intentionally use pixel-exact dimensions. These need
phase-specific integration research rather than broader feature expansion.
