# Project Research Summary

**Project:** FVWM-X v0.1
**Domain:** Per-monitor HiDPI foundations for a single-threaded, non-compositing X11 window manager
**Researched:** 2026-08-29
**Confidence:** MEDIUM

## Executive Summary

FVWM-X v0.1 is an incremental modernization of FVWM3 that makes WM-owned UI and managed-window geometry dependable on mixed-density XRandR layouts. Experts should build this without inventing a second desktop coordinate system: X11, ICCCM, EWMH, client hints, module geometry, and RandR rectangles remain integer root/client pixels. Per-monitor scale is private FVWM policy attached to each logical RandR 1.5 Monitor, used only to resolve FVWM-owned metrics/resources and to preserve a stable perceived client size during monitor transitions. Arbitrary client-content resampling, global DPI mutation, output transforms as a scaling model, a compositor, a toolkit, and Wayland are all explicitly outside the milestone.

The recommended implementation is a six-stage extension of the existing C11/Xlib architecture: establish an owned, generation-tagged monitor snapshot and live-X test harness; add a pure rational scale policy with explicit overrides and safe automatic fallback; separate root/client/frame/UI coordinate contracts and establish deterministic ownership plus a drift-free client-size basis; make WM resources and metrics scale-aware; centralize cross-monitor transitions; then compose these foundations into bounded hotplug reconciliation and versioned first-party module support. The reference build should require XRandR 1.5+, Xft/fontconfig, and Xcursor, with XRender, PNG, and SVG explicitly enabled in a HiDPI CI profile. Meson, plain C tests, Python-stdlib orchestration, Xvfb, and Xorg dummy are sufficient for verification.

The dominant risks are not rendering quality but state and protocol correctness: bad physical dimensions can select dangerous scales; event storms can expose partial topology; repeated conversion can accumulate drift; independent ownership decisions can oscillate at seams; global font/cursor settings can double-scale resources; and naive resizes can violate ICCCM/EWMH. Prevent these by validating complete candidates before commit, retaining the last good topology on query failure, making scale and ownership single-authority decisions, storing an exact client-size basis, applying named rational rounding once, preserving all protocol geometry in pixels, acquiring new resources before releasing old ones, and gating completion on native-Linux live-X scenarios.

## Key Findings

### Recommended Stack

Keep the current stack and add policy, not infrastructure. No new runtime library is required for scale arithmetic or topology policy. The existing X11 wrappers and rendering paths already expose the needed mechanisms; the work is to establish authoritative state, explicit target sizes, complete cache identities, and safe invalidation.

**Core technologies:**

- **C11:** pure scale, snapshot, ownership, transition, and testable geometry policy; matches the repository and avoids migration risk.
- **Xlib and X11 protocols:** sole window-system backend; all X-facing geometry remains integer pixels.
- **XRandR/libXrandr 1.5+:** authoritative logical Monitor topology, geometry, physical-size input, output membership, and change notifications. Use `XRRGetMonitors`, not Xinerama or connector-only heuristics.
- **ICCCM 2.0 and EWMH 1.5:** non-negotiable client-size, gravity, work-area, maximize/fullscreen, frame-extents, and notification contracts.
- **Xft plus fontconfig:** open monitor-specific font patterns using final pixel size or pattern-local DPI. Never mutate display-global `Xft.dpi` as windows cross monitors.
- **libXcursor >=1.2.0:** load themed/file cursors at an explicit target size and bind them to WM contexts. Require it in the supported FVWM-X release and CI profile; a minimal build may retain a documented degraded fallback.
- **Existing XRender/image/librsvg paths:** render images from source to explicit destination dimensions and key caches by target size. Prefer XRender for alpha/filtering but do not require a compositor.
- **libpng >=1.6 in the supported profile; librsvg >=2.52 if the SVG path is changed:** modernize floors only where v0.1 exercises those paths; SVG is not a blocker for the scale foundation.
- **Meson >=1.5.1, plain C assertions, and Python 3 stdlib:** preserve one test runner and avoid introducing pytest or another framework.
- **Xvfb plus Xorg dummy and xrandr:** use Xvfb for monitor-object smoke tests and Xorg dummy for authoritative output/CRTC/mm/topology scenarios; Xephyr is a manual debugging aid, not the proof environment.

**Supported reference build policy:** explicitly enable Xcursor, XRender, PNG, and SVG in one native-Linux CI job so dependency autodetection cannot silently omit the acceptance path. Keep sanitizer and compiler matrices for pure policy tests.

### Expected Features

The credible v0.1 promise is: FVWM-owned UI stays physically usable, eligible managed windows preserve their chosen logical size when ownership changes, and protocol geometry stays correct through topology changes. It is not a promise to make arbitrary X11 applications per-monitor-DPI-aware.

**Must have (table stakes):**

- One authoritative RandR 1.5 Monitor topology with complete geometry, mm, identity, flags, and output membership, including negative coordinates, overlaps, clones, gaps, and reorderings.
- Exactly one deterministic effective scale per active Monitor: explicit override first, validated automatic policy second, safe fallback last, with inspectable source and rejection reason.
- Scale-aware titlebars, borders, fonts, buttons, menus, icons, move/resize feedback, hit targets, edge/pan zones, snapping thresholds, and WM cursors, all scaled exactly once.
- Deterministic window ownership with incumbent tie-breaking/hysteresis and at most one scale transition per committed crossing.
- Drift-free perceived client-size preservation based on canonical intent, with ICCCM min/max/base/increment/aspect/gravity constraints taking precedence over exact preservation.
- Correct placement, work areas, maximize, fullscreen, restore, frame extents, synthetic `ConfigureNotify`, and module/EWMH pixel semantics.
- Atomic, non-destructive topology reconciliation that coalesces event bursts, retains the last good state after query failure, and rehomes removed-monitor windows without making them unreachable.
- Safe 1x startup/fallback behavior and diagnostics when monitor data is missing or invalid.
- Deterministic native unit tests and live-X integration coverage for the full feature claim.
- Explicitly named first-party modules that receive scale metadata and redraw correctly; unclaimed legacy modules remain safe and documented as 1x.

**Should have (v0.1 differentiators):**

- Reversible cross-monitor sizing with no drift after repeated fractional round trips.
- Quarter-step WM scale factors (1.25x, 1.5x, 1.75x) after the 1x/2x path is proven, without scaling client pixels.
- Deterministic topology behavior for exact clones, overlaps, output reorderings, and negative origins.
- Live per-monitor scale reload if the existing configuration reload path can support one coherent generation.
- Structured diagnostics/simulation exposing topology epoch, scale inputs/source, owner, size basis, transition serial, and constraint reason.
- Additive/versioned scale metadata for the declared first-party module set.

**Defer beyond v0.1 unless demanded by compatibility evidence:**

- Per-application “preserve raw pixels” opt-out policy beyond the minimum escape hatch needed for observed clients.
- Arbitrary scale percentages outside disciplined quarter-step buckets.
- Automatic restoration to a reconnected monitor and richer recovery explanations.
- Broad legacy-module conversion beyond the explicitly supported first-party set.
- Client-content supersampling, compositor integration, toolkit environment injection, global DPI rewrites, Wayland, and configuration-language redesign.

### Architecture Approach

Attach derived scale policy to a validated RandR Monitor snapshot and introduce four explicit layers: topology snapshot/adapter, pure scale and ownership policy, a generation-safe transition coordinator, and resolved scale-aware UI resources. Keep the existing authoritative X state single-threaded. Event handlers only mark topology dirty; a safe-point coordinator builds and validates a candidate, diffs it against the committed epoch, projects it into stable runtime monitor nodes, and queues affected windows in bounded batches. Every scale-sensitive API must name its coordinate space, and every owner change must pass through one transition authority.

**Major components:**

1. **Rational scale primitive (`libs/fvwm_scale.[ch]`)** — reduced fractions, checked 64-bit conversion, named floor/ceil/nearest rounding, mm/DPI validation, quantization, and rejection/source reporting; no Xlib or global state.
2. **Monitor snapshot/diff policy (`libs/monitor_snapshot.[ch]` plus `FScreen`)** — deep-copy all RandR Monitor facts, resolve stable configuration identity, validate candidates, classify diffs, retain last-good state, and assign epochs.
3. **Topology coordinator (`fvwm/topology.[ch]`)** — coalesce notifications, commit candidates, manage stable/retired monitor-node lifetimes, queue generation-tagged work, and broadcast only committed state.
4. **Ownership and geometry foundation** — literal root-rectangle selection (never virtual-page modulo), canonical `{client_px, capture_scale}` size basis, ICCCM solver integration, anchor/gravity preservation, and state-specific target planning.
5. **Resolved UI metrics and resource adapters** — immutable pixel bundles per style/config generation and normalized scale; scale-complete font, picture, cursor, and metrics cache keys; acquire-new-before-release-old lifecycle.
6. **Window scale transition authority (`fvwm/window_scale.[ch]`)** — idempotent owner/scale commits, pending/applied epochs, pointer/gravity anchoring, frame/client X configuration, redraw, extents, and module publication.
7. **Additive module protocol** — scale/topology metadata without reinterpreting existing geometry packets; update C headers, packet translation, Perl constants, and selected modules together.

**Required patterns:** candidate-then-commit; generation-tagged deferred work; immutable effective scale within a generation; explicit boundary conversion; one metrics bundle; scale-complete cache keys; pre-transition owner selection; acquire resources before releasing old generations; publish only actual applied pixels.

### Resolved Research Contradictions

- **Automatic-scale fallback chain:** use `explicit monitor override -> validated RandR Monitor DPI -> explicitly configured FVWM global scale fallback, if provided -> 1/1`. Global `Xft.dpi`, XSETTINGS, root DPI, or the primary monitor must not become implicit per-monitor physical scale. For fonts, a user-specified pattern DPI may be a font input but monitor scale is still applied exactly once.
- **“Canonical logical dimensions” versus a pixel basis:** v0.1 should store the exact pair `{client pixel size, capture scale}` rather than introduce a global fractional logical coordinate grid. The pair is the canonical logical equivalent because target pixels are always derived as `basis_px * target_scale / capture_scale`; WM-driven transitions never rewrite it.
- **Geometry versus UI implementation order:** pure coordinate, ownership, and ICCCM geometry policy must precede UI wiring, but X-visible cross-monitor resize must follow UI resources because destination decoration extents and font metrics are inputs to final frame geometry. This yields separate Geometry Foundation and Logical-Size Transition phases.
- **Immutable snapshot versus existing raw monitor pointers:** build immutable candidate snapshots, but for v0.1 commit them through stable runtime monitor nodes and retire removed nodes until references drain. A wholesale runtime snapshot-pointer swap is deferred until direct `struct monitor *` coupling is removed.
- **Fractional scaling scope:** quarter-step fractional scaling applies to FVWM-owned UI and legal window geometry only. Fractional client-content resampling remains out of scope. Prove 1x/2x first, then enable 1.25x/1.5x/1.75x.
- **Xcursor optionality:** retain a compile-time fallback for minimal builds, but make libXcursor mandatory for the supported v0.1 profile and acceptance CI because reliable themed per-monitor cursor sizing is table stakes.
- **Module compatibility:** existing geometry packets remain pixels. Compatibility breaks are allowed, but scale data must be additive/versioned and the exact v0.1 supported module set must be frozen before implementation rather than implicitly claiming all modules.

### Critical Pitfalls

1. **Trusting physical millimetres** — preserve raw values and full output membership; reject zero, implausible, overflowed, or materially inconsistent axes; quantize once and report why fallback was chosen. Initial thresholds such as 50–400 DPI and 20% axis disagreement are hypotheses to calibrate, not standards.
2. **Mixing coordinate spaces** — keep root, frame, client, logical UI, and size-basis values explicit; never scale global positions, client hints, struts, or EWMH geometry. Scale adjacent edges before deriving lengths and clamp before narrowing to protocol fields.
3. **Repeatedly scaling rounded geometry** — derive each target from the stable client-size basis and destination metrics, never from the last rounded frame. Recompute maximize/fullscreen from destination monitor/work area rather than multiplying derived rectangles.
4. **Ownership oscillation and multiple transition authorities** — select from pre-transition geometry, use incumbent/primary/stable-ID ties and interaction hysteresis, freeze the destination for an operation, and make duplicate `(epoch, monitor, scale, serial)` work a no-op.
5. **In-place topology mutation during event storms** — handlers mark dirty; safe-point code deep-copies, validates, diffs, and commits once per loop budget. Keep the last good snapshot on query failure and preserve retired nodes while referenced.
6. **Global font/cursor mutation and incomplete cache keys** — never change display-global DPI/default cursor size on monitor crossing. Request final sizes explicitly and include scale/target dimensions and generation in every resource key.
7. **Violating ICCCM/EWMH while “scaling everything”** — solve client hints after ideal scale conversion, preserve gravity, leave fixed-size clients legal, keep wire properties in pixels, and publish extents/notifications only after actual X geometry is applied.
8. **Scaling only visible chrome** — resolve fonts, images, borders, cursors, hit regions, drag thresholds, menu spacing, edge behavior, and first-party module UI from one metrics pipeline; test interaction rectangles, not screenshots alone.

## Implications for Roadmap

The combined research supports six dependency-ordered phases. Each phase should land pure policy and tests before wiring X-visible effects. Do not collapse phases 2–5 into broad edits to `move_resize.c` and `borders.c`.

### Phase 1: Authoritative Topology Snapshot and Live-X Harness

**Rationale:** Every scale, ownership, and hotplug decision depends on complete, durable monitor facts and known event behavior. Establish the integration environment before relying on assumptions about packaged X servers.

**Delivers:** Deep-copied RandR Monitor candidates; name/Atom, geometry, mm, flags, complete output set, timestamps and epochs; stable/retired runtime-node lifecycle; structural validation and diff classes; last-good retention; broader RandR/root event subscriptions; dirty-event coalescing; Xvfb smoke and Xorg-dummy harness with purpose-built Xlib probes.

**Addresses:** authoritative topology, safe startup, deterministic simulation, observable monitor identity.

**Avoids:** partial monitor-tree mutation, dangling `struct monitor *`, one rescan per event, missing Monitor-object changes, and treating output order as identity.

**Research flag:** **Required.** Probe the chosen Xvfb/Xorg versions for `--setmonitor`/`--delmonitor`, root `ConfigureNotify`, notification masks, zero/query-failure behavior, negative coordinates, and output/mm support.

### Phase 2: Scale Policy, Configuration, and Diagnostics

**Rationale:** All consumers need one authoritative effective-scale accessor, precedence chain, and rejection reason before rendering or geometry code changes.

**Delivers:** `fvwm_scale` rational arithmetic; checked signed/unsigned conversions; validated px/mm DPI calculation; quarter-step quantization and supported range; monitor-name override matching; optional explicit global fallback; 1x safe default; scale source/rejection diagnostics; config generation and scale-only diff behavior.

**Addresses:** deterministic effective scale, explicit overrides, auto fallback, introspection, 1x compatibility.

**Uses:** C11, XRandR Monitor mm data, plain Meson C tests; no EDID parser and no global Xft/XSETTINGS mutation.

**Avoids:** divide-by-zero/extreme UI, primary-derived global scale, hidden Xft double scaling, and malformed config invalidating topology.

**Research flag:** **Required.** Calibrate physical-DPI range, axis disagreement, quantization ties, maximum supported auto scale, rotated-monitor treatment, and matching ambiguity using real fixtures/hardware evidence.

### Phase 3: Coordinate, Ownership, and Geometry Foundation

**Rationale:** Drift-free transitions and correct UI layout require explicit spaces, stable client intent, deterministic literal monitor selection, and one ICCCM constraint path before any crossing behavior is enabled.

**Delivers:** Root/client/frame/logical-UI API contracts; edge-based rational rounding; overflow/narrowing checks; literal root monitor selection separated from virtual-page wrapping; overlap/gap/tie policy; canonical `{client_px, capture_scale}` basis; ICCCM min/max/base/increment/aspect/gravity tests; pure transition planning for normal, fixed, maximized, and fullscreen states.

**Addresses:** stable ownership, logical-size preservation foundation, placement/work-area correctness, reversible sizing.

**Avoids:** geometry drift, decoration double counting, modulo-wrapped monitor choice, seam chatter, invalid terminal/fixed-dialog sizes, and logical units leaking into EWMH/ICCCM.

**Research flag:** **Limited; skip broad external research.** ICCCM/EWMH rules are documented. Spend planning effort on code archaeology around current virtual desktops, gravity solver, `g.normal/g.max/g.frame`, and style values that may intentionally be literal pixels.

### Phase 4: WM-Owned UI Metrics and Resources

**Rationale:** Correct transition geometry depends on destination decoration extents and resource metrics. Centralize those values before applying monitor handoffs.

**Delivers:** Immutable metrics bundles per style/config generation and scale; pattern-local/final-size Xft fonts; target-size pictures rendered from source; explicit-size Xcursor loading and rebinding; scale-complete caches; frames, borders, buttons, menus, icons, feedback, hit targets, pan/edge metrics, and `_NET_FRAME_EXTENTS`; 1x/2x proof followed by fractional buckets.

**Addresses:** scale-aware core UI, interaction targets/cursors, exactly-once scale application, predictable fractional WM scale.

**Uses:** Xft/fontconfig, supported-profile libXcursor, existing XRender/PNG/SVG/XPM paths, reference CI build with explicit feature flags.

**Avoids:** global DPI/default-cursor mutation, blurry derivative rescaling, cache aliasing across sizes, stale resources, tiny hidden hit targets, and one giant global style flush.

**Research flag:** **Required.** Trace every font/image/cursor/decoration caller and fallback build, verify point-size/global-DPI interaction, decide which style metrics become logical, and test cursor rebinding without flicker or dangling grab resources.

### Phase 5: Logical-Size Monitor Transitions

**Rationale:** With authoritative topology, tested geometry policy, and destination resources available, one coordinator can safely commit ownership and all resulting X-visible state.

**Delivers:** Pending/applied owner and epoch state; transition serial/idempotence; pre-transition selection; pointer and gravity anchors; stable size-basis conversion; one-pass ICCCM constraints; fixed-size handling; state-specific maximize/fullscreen/restore; frame/client configuration; cursor/resource swap; coherent extents, synthetic notifications, and per-window broadcasts; client-feedback protection.

**Addresses:** logical-size handoff, stable ownership during movement, maximize/fullscreen migration, live scale changes, drift-free round trips.

**Avoids:** oscillation, configure storms, multiple `fw->m` assignment authorities, stale restore geometry, fighting fixed/pixel-sensitive clients, and post-resize ownership feedback.

**Research flag:** **Required.** Validate active-drag anchoring and toolkit feedback with purpose-built clients plus representative Qt, GTK, terminal, fixed-size, and changing-hints clients. Decide the minimum v0.1 per-window/style escape hatch from observed incompatibilities.

### Phase 6: Hotplug Reconciliation, Modules, and Release Compatibility

**Rationale:** Hotplug composes every earlier invariant and exposes stale state, ordering, fairness, protocol, and compatibility defects. It should be the final behavior phase, not the place where foundations are invented.

**Delivers:** Generation-replacing bounded transition queues; urgent removed-owner recovery; reachability clamping; active move/resize/menu/grab cancellation or rebase; primary-only and same-scale no-op behavior; final EWMH ordering; versioned/additive MonitorScale metadata; scale-aware declared first-party modules; structured observability; topology-storm soak tests and the complete live-client release matrix.

**Addresses:** non-destructive hotplug, module consistency, explainable recovery, final topology edge cases, milestone verification.

**Avoids:** windows stranded in gaps, references to disabled monitors, repeated relayout per event, event-loop starvation, mixed packet geometry semantics, and unsupported implicit module claims.

**Research flag:** **Required.** Freeze the supported module set and packet compatibility strategy, choose event-loop work budgets, validate storm/event ordering on native X, and define any manual/dedicated-runner gate for real connector hotplug or bogus EDID that Xvfb/dummy Xorg cannot prove.

### Phase Ordering Rationale

- Topology facts and the live-X harness come first because later correctness cannot be inferred from unit tests or incomplete `FScreen` state.
- Scale policy precedes consumers so rendering, geometry, modules, and diagnostics share one value and fallback chain.
- Pure geometry/ownership precedes UI wiring to freeze coordinate and rounding contracts; X-visible transitions follow UI wiring because final frame extents are transition inputs.
- Hotplug and modules come last because they compose snapshot, scale, resource, geometry, transition, and event-loop behavior.
- Native C tests should land with each policy module; live-X scenarios should grow in every phase rather than appearing only as final QA.

### Release Gates

- Pure tests cover rational reduction, overflow, negative rounding, invalid mm, override precedence, snapshot diff, ownership ties, 1,000 repeated scale round trips, ICCCM combinations, transition idempotence, and cache-key separation.
- Live-X tests cover 1x/2x and fractional pairs, negative/overlapping/cloned layouts, fixed and constrained clients, active crossing, maximize/fullscreen/restore, primary-only changes, scale reload, monitor removal during grabs, query-failure retention, resource-disabled fallback, module packet semantics, and fairness under event storms.
- A release candidate must produce identical geometry behavior with and without an external compositor and must never rewrite global DPI/XSETTINGS resources as a per-monitor mechanism.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | MEDIUM | Official X.Org, Xft/fontconfig, Xcursor, ICCCM, EWMH, and rendering documentation aligns with inspected repository paths. Exact distro dependency availability and server capabilities still need configure/CI validation. |
| Features | MEDIUM | The narrow product contract follows X11 limitations and modern HiDPI precedent. Auto thresholds, handoff UX, client compatibility, and the first-party module claim are product choices requiring validation. |
| Architecture | MEDIUM | Component boundaries and existing integration points are well supported by direct code inspection. Event sequences, raw-pointer migration details, and bounded reconciliation behavior need implementation tests. |
| Pitfalls | MEDIUM | Protocol/resource hazards are well documented and corroborated by current code exposure. Prevention constants and client/toolkit reactions remain empirical. |

**Overall confidence:** MEDIUM. The direction and build order are strong; completion criteria depend on native-Linux server and client evidence rather than documentation alone.

### Gaps to Address

- **Automatic scale constants:** choose and test accepted DPI range, X/Y disagreement, quantization boundaries, and auto-scale ceiling against target hardware and known bad metadata.
- **RandR server behavior:** verify event masks, Monitor-object notifications, query failures, negative origins, and Xvfb/Xorg-dummy capabilities in the exact CI images.
- **Monitor identity:** specify automatic-name and optional output-set fallback behavior; reject ambiguous matches instead of guessing.
- **Ownership hysteresis:** select the interaction margin and define its logical-to-pixel conversion using seam/overlap usability tests.
- **Style-unit compatibility:** classify existing values as logical UI intent or literal pixels and decide the minimum explicit literal-pixel escape mechanism.
- **Client compatibility corpus:** test representative Qt/GTK/terminal/fixed/pixel-sensitive clients, counter-requests, and changing hints before finalizing default preservation/opt-out behavior.
- **Font exact-once sizing:** trace existing point/pixel patterns and global DPI inputs to prove scale is neither omitted nor multiplied twice.
- **First-party modules:** freeze which modules v0.1 supports and the additive/versioned packet representation before Phase 6 coding.
- **Real hardware coverage:** if CI cannot inject connector hotplug/bogus EDID, document a dedicated native runner or manual release gate instead of treating Xvfb as equivalent.

## Sources

All external source-backed findings are rated MEDIUM under the configured research-provider seam, even where the underlying document is an official specification. Direct repository observations have higher local confidence but do not remove the need for live-X validation.

### Official and Canonical Specifications

- X.Org RandR protocol 1.5/1.6 — logical Monitor objects, physical dimensions, output membership, transforms, notifications, and dynamic topology: https://cgit.freedesktop.org/xorg/proto/xorgproto/plain/randrproto.txt
- libXrandr manual — configuration updates, timestamps, and event handling: https://www.x.org/releases/current/doc/man/man3/Xrandr.3.xhtml
- xrandr manual — monitor objects, output transforms, and global DPI behavior: https://www.x.org/releases/current/doc/man/man1/xrandr.1.xhtml
- ICCCM 2.0 — `WM_NORMAL_HINTS`, gravity, client resizing, and synthetic configure behavior: https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html
- EWMH 1.5 — work areas, struts, moveresize, maximize/fullscreen, and frame extents: https://specifications.freedesktop.org/wm-spec/latest-single/
- Xlib C Language Interface — integer/protocol geometry, asynchronous behavior, and resource-manager scope: https://www.x.org/releases/current/doc/libX11/libX11/libX11.html
- Fontconfig Developers Reference 2.18.3 — `FC_SIZE`, `FC_PIXEL_SIZE`, `FC_DPI`, matching, and substitution: https://fontconfig.pages.freedesktop.org/fontconfig/fontconfig-devel/
- libXft manual — pattern-based font loading: https://cgit.freedesktop.org/xorg/lib/libXft/plain/man/Xft.man
- Xcursor manual — themed multi-size images, explicit target loading, and display-global defaults: https://www.x.org/releases/current/doc/man/man3/Xcursor.3.xhtml
- XSETTINGS specification — per-X-screen manager and update scope: https://specifications.freedesktop.org/xsettings-spec/latest/
- Xvfb manual — headless X server role: https://www.x.org/releases/current/doc/man/man1/Xvfb.1.xhtml
- librsvg target-size rendering API: https://gnome.pages.gitlab.gnome.org/librsvg/Rsvg-2.0/method.Handle.render_document.html

### Current Implementation Guidance and Corroboration

- Qt 6.11 High DPI documentation — X11 DPI limitations/precedence, device-independent sizing, asset selection, and rounding guidance: https://doc.qt.io/qt-6/highdpi.html
- GTK 3.24 Settings documentation — XSETTINGS-backed per-X-screen behavior: https://docs.gtk.org/gtk3/class.Settings.html
- Linux kernel EDID documentation — absent, bogus, and KVM-substituted EDID cases: https://docs.kernel.org/admin-guide/edid.html
- AwesomeWM current scale/DPI source — precedent for explicit per-screen application with fallback: https://raw.githubusercontent.com/awesomeWM/awesome/master/lib/beautiful/xresources.lua

### Repository Evidence

- `.planning/PROJECT.md` — v0.1 product contract, constraints, active requirements, and exclusions.
- `libs/FScreen.[ch]`, `fvwm/events.c`, `fvwm/update.[ch]`, `fvwm/fvwm.h`, `fvwm/geometry.c`, `fvwm/add_window.c` — current monitor, event, owner, window-state, and geometry integration points.
- `fvwm/move_resize.c`, `fvwm/placement.c`, `fvwm/ewmh.c`, `fvwm/frame.c`, `fvwm/borders.c`, `fvwm/decorations.c` — scale-sensitive geometry and UI paths.
- `libs/Flocale.c`, `libs/Fft.c`, `libs/Picture.c`, `libs/PictureImageLoader.c`, `libs/PictureGraphics.c`, `libs/Fcursor.h`, `fvwm/cursor.c` — font, image, render, and cursor paths requiring explicit size-aware cache identity.
- `fvwm/module_interface.c`, `libs/Module.h`, `libs/vpacket.h` and language bindings — module protocol coupling that must remain pixel-based and evolve synchronously.
- `tests/meson.build`, `meson.build`, and `.github/workflows/ccpp.yml` — existing build/test structure and CI integration points.

---
*Research completed: 2026-08-29*
*Ready for roadmap: yes*
