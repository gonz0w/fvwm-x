# Technology Stack: Mixed-DPI Foundations

**Project:** FVWM-X v0.1
**Domain:** X11 window manager with per-monitor HiDPI
**Researched:** 2026-08-29
**Overall confidence:** MEDIUM — recommendations are cross-checked against official X.Org specifications/manuals and the current FVWM3 codebase; the research-provider seam classifies web findings as MEDIUM.

## Recommendation

Do **not** add a GUI toolkit, compositor, scene graph, or alternate window-system backend. Extend the current C11/Xlib stack around five existing facilities:

1. **libXrandr 1.5+** remains authoritative for monitor topology and physical-size inputs.
2. **A small internal rational-scale policy layer** owns automatic/override/effective scale and all rounding; no third-party math library is needed.
3. **Xft + fontconfig** open fonts from monitor-specific patterns, with per-pattern DPI or pixel size rather than a process-global `Xft.dpi` mutation.
4. **libXcursor** loads a cursor image set at an explicit target size; make it part of the supported FVWM-X profile.
5. **Existing XRender, image, and librsvg paths** render to explicit target pixel sizes and key caches by scale/size.

The X server still exposes one root-window pixel coordinate space. Scale is metadata and policy owned by FVWM-X; it must not reinterpret X11, ICCCM, EWMH, or module geometry as device-independent coordinates.

## Recommended Stack

### Core Runtime and Standards

| Technology | Recommended version/standard | Status | Purpose | Why |
|------------|------------------------------|--------|---------|-----|
| C | C11 | Keep | Scale arithmetic, policy, caches, tests | Matches the repository and needs no runtime or language migration. |
| Xlib | X11R7/current distro | Keep | Root-window geometry, windows, events, cursors | X11 remains the sole backend. Root coordinates are integer pixels. |
| RandR protocol | 1.5 minimum | Keep required | Logical monitor objects, output association, physical dimensions, topology events | `XRRGetMonitors` is the correct logical-monitor API and is already used. |
| libXrandr | `>=1.5` (1.5.5 current upstream release observed) | Keep required | C binding for RandR | Existing floor is sufficient for `XRRMonitorInfo`; no replacement library is needed. |
| xorgproto | RandR 1.5+ (2025.1 current release observed) | Transitive/build | Protocol definitions and event masks | Provides the monitor and notification definitions consumed through Xrandr headers. |
| ICCCM | 2.0 | Keep contract | Client size hints and synthetic configure behavior | Scaled client resizing must still honor `WM_NORMAL_HINTS`. |
| EWMH | 1.5 | Keep contract | Work areas, frame extents, moveresize, fullscreen/maximize | Published geometries remain root-window pixels. |

### Rendering and Resources

| Technology | Recommended version | Status | Purpose | Why |
|------------|---------------------|--------|---------|-----|
| libXft | Existing required dependency; 2.3.9 current release observed | Adapt | Render WM-owned text | Already integrates fontconfig and XRender; supports pattern-specific sizing. |
| fontconfig | Existing required dependency; 2.18.3 current docs | Adapt | Match fonts and express `FC_SIZE`, `FC_PIXEL_SIZE`, and `FC_DPI` | Per-pattern properties avoid a single global DPI value. |
| libXcursor | `>=1.2.0` (1.2.3 current release observed) | Promote to supported profile | Themed, animated, explicitly sized cursors | Cursor files contain nominal sizes and the image APIs accept a target size. |
| libXrender | Existing optional dependency; 0.9.12 current release observed | Enable in reference/CI profile | Alpha compositing and high-quality scaled pixmaps/cursors | Reuse existing `FRender` and `PGraphicsRenderPicture`; do not build a renderer. |
| libpng | Raise supported-profile floor to `>=1.6` | Keep optional globally; enable in reference profile | Decode PNG assets | The current `>=1.2` floor is obsolete; the default configuration expects PNG. |
| librsvg + Cairo | If SVG is enabled, prefer librsvg `>=2.52` | Keep optional | Render vector assets directly at target size | The code already has a modern `rsvg_handle_render_document` path guarded at 2.52. SVG must be rasterized at the final monitor-sized dimensions, not scaled from a 1x bitmap. |
| XPM | Existing optional dependency | Keep fallback | Legacy bitmap assets | Retain compatibility, but do not rely on XPM for crisp HiDPI output. |

### Verification Stack

| Tool | Version policy | Purpose | Recommendation |
|------|----------------|---------|----------------|
| Meson test | Existing Meson `>=1.5.1` | Native unit/integration registration | Keep as the only test runner. |
| C assertion tests | C11 | Pure scale, rounding, geometry, and transition policy | Add small executables beside existing tests; no C test framework is needed. |
| Python 3 stdlib | Existing required interpreter | Launch servers, wait for readiness, set topology, collect diagnostics | Use only as an integration harness; do not add pytest. |
| Xvfb | Distro Xorg version | Headless X server for smoke tests and logical monitor-object scenarios | Use for fast CI where RandR monitor creation is supported by the packaged server. |
| Xorg + dummy driver | Xorg distro version; xf86-video-dummy 0.4.1 current release observed | Deterministic multi-output/CRTC, physical-size, negative-coordinate, overlap, and hotplug-like scenarios | Use for the authoritative topology integration suite. |
| Xephyr | Distro Xorg version | Developer-visible nested X server | Useful for manual debugging, not the only CI topology backend. |
| xrandr CLI | 1.5+ (1.5.4 current release observed) | Configure/query outputs and RandR 1.5 monitor objects | Test fixture tool only; FVWM-X runtime must call libXrandr directly. |
| Small Xlib probe clients | Built in `tests/` | Create constrained clients, inspect frame geometry, send events | Prefer structured probes over parsing `xwininfo` output for assertions. |

## Dependency Decisions

### Keep the Runtime Dependency Set Small

No new runtime library is needed for scale policy. `XRRMonitorInfo`, fontconfig patterns, Xcursor image APIs, and the existing picture/render pipeline already expose the required mechanisms. The missing work is state modeling, explicit target sizing, cache identity, and topology-safe invalidation.

Recommended Meson policy:

- Keep `xrandr >=1.5`, Xft, and fontconfig required.
- Make `-Dxcursor=enabled` mandatory for the supported FVWM-X release and CI profile. A minimal build may retain the compile-time fallback, but it should be documented as lacking reliable themed per-monitor cursor scaling.
- Keep direct XRender, PNG, and SVG switches available, but add one HiDPI CI job that explicitly enables Xcursor, XRender, PNG, and SVG. This prevents automatic dependency detection from silently skipping the acceptance path.
- Raise libpng's accepted floor to 1.6 when modernizing the image dependency declarations.
- If the SVG path is changed during v0.1, raise librsvg's floor to 2.52 and remove the legacy rendering branch. If it is not changed, do not make SVG a blocker for the monitor-scale foundation.

### Do Not Add These Technologies

| Excluded technology/approach | Why not | Use instead |
|------------------------------|---------|-------------|
| GTK, Qt, GDK, or a new widget toolkit | Would duplicate/rewrite FVWM's frames, menus, and module UI while not solving X11 client scaling. | Scale existing WM metrics and X resources. |
| A compositor, OpenGL/Vulkan renderer, or scene graph | Out of scope and introduces ownership/synchronization complexity. | External compositor plus existing XRender/image paths. |
| Wayland, wlroots, or native Xwayland integration | Contradicts the X11-only milestone. | Xlib/XRandR 1.5+. |
| EDID parsing or `libdisplay-info` | EDID is not needed for the initial policy and physical data can still be missing or misleading. | RandR monitor/output millimeters, strict validation, explicit user override, safe 1x fallback. |
| Process-global `Xft.dpi`, `XCURSOR_SIZE`, or X resource rewrites | One value cannot represent simultaneous mixed-density monitors and would affect unrelated clients/resources. | Per-pattern font sizing and explicit-size Xcursor image loading. |
| RandR output transforms as the WM's scale abstraction | Transforms alter framebuffer-to-output presentation but do not create per-monitor logical X11 coordinates; they can also blur output. | Keep root geometry in pixels and scale WM-owned resources deliberately. |
| Floating-point coordinates throughout the WM | Creates cumulative rounding drift and destabilizes exact X11 geometry. | Reduced rational scale with 64-bit intermediate arithmetic and explicit rounding. |
| pytest or another test framework | Existing tests are small Meson-driven native programs; another framework adds packaging without improving coverage. | C unit tests and a Python-stdlib X-server harness. |

## Internal Scale Primitive

Implement scale as a project-owned primitive, not as `double` fields scattered through drawing code:

```c
struct fvwm_scale {
	uint32_t numerator;
	uint32_t denominator;
};
```

The scale module should provide checked helpers for:

- reducing and validating a scale;
- scaling signed positions and unsigned lengths with 64-bit intermediates;
- floor, ceil, and nearest rounding with named call sites;
- inverse conversion from root pixels to logical WM units;
- comparing scales without floating-point equality;
- saturating before conversion to X11 `int`, `unsigned int`, `short`, or `CARD16` fields.

For v0.1, automatic policy should return bounded discrete scales and fall back to `1/1` when physical dimensions are absent or implausible. The representation may support rational explicit overrides, but automatic fractional client-content scaling remains out of scope.

**Do not repeatedly scale already-rounded geometry.** Store logical intent or the last authoritative unscaled dimensions and derive target pixels from that value on each transition.

## Integration by Subsystem

### 1. RandR Topology and Monitor State

`libs/FScreen.c` already calls `XRRGetMonitors(..., True, ...)`, but `screen_info` retains only geometry and the first `RROutput`. Extend the monitor snapshot to retain:

- monitor Atom/name and `automatic`/`primary` flags;
- pixel rectangle (`x`, `y`, `width`, `height`);
- physical width/height in millimeters;
- **all** associated outputs, not only `*rrm.outputs`;
- automatic scale, optional configured override, and effective scale;
- scale source/status (`override`, `physical`, `fallback`) for diagnostics;
- topology/scale generation used by resource caches and deferred updates.

Use the RandR monitor name as the primary configuration identity and output names as deterministic fallback selectors. Do not derive persistent identity from monitor array order or the current coordinate-sorted monitor number.

Expand `FScreenSelect()` from its present screen/output mask to include:

```c
RRScreenChangeNotifyMask |
RRCrtcChangeNotifyMask |
RROutputChangeNotifyMask |
RRResourceChangeNotifyMask
```

Also add `StructureNotifyMask` to `XEVMASK_ROOTW` and route root-only `ConfigureNotify` to a topology refresh: RandR 1.5 `RRSetMonitor`/`RRDeleteMonitor` changes are specified to generate root `ConfigureNotify`, while the current root mask contains only `SubstructureNotifyMask`. Keep `XRRUpdateConfiguration()` for `RRScreenChangeNotify`, then re-query a complete snapshot. Coalesce event bursts and publish one validated diff; do not relayout once per low-level RandR event.

Automatic DPI input is:

```text
dpi_x = pixel_width  * 25.4 / millimeter_width
dpi_y = pixel_height * 25.4 / millimeter_height
```

The policy must reject zero dimensions, extreme values, and materially inconsistent axes before bucket selection. Explicit monitor configuration always wins; invalid metadata falls back safely rather than guessing from total root-window DPI.

### 2. Fonts: Xft + fontconfig Patterns

Adapt `FftGetFont()` rather than introducing another text stack.

Recommended pattern flow:

1. Parse the configured name with `XftNameParse()` as today.
2. If the source pattern explicitly contains `FC_PIXEL_SIZE`, replace it with `logical_pixel_size × effective_scale`.
3. Otherwise retain `FC_SIZE` and set **pattern-local** `FC_DPI` to `base_dpi × effective_scale`, where `base_dpi` is an explicit pattern DPI when present and 96 otherwise; fontconfig then performs point-to-pixel conversion.
4. Match and open through the existing `XftFontMatch()` / `XftFontOpenPattern()` path so user fontconfig substitutions, hinting, antialiasing, and fallback continue to work.
5. Cache by normalized font specification, effective scale, rotation, and font-configuration generation.

Do not use deprecated `FC_SCALE` as the main mechanism. Do not call `XftDefaultSet()` to swap a display-global DPI as pointer focus changes. A global mutation can make two simultaneously visible titlebars disagree and makes cache behavior order-dependent.

The fallback font cache in `libs/Fft.c` hashes `FcPattern`; ensuring scale-specific values are present in the source pattern naturally separates fallback matches. `FftFontType` ownership must close every scale/rotation variant with `XftFontClose()`.

### 3. Cursors: Explicit Xcursor Size

The current cursor file path calls:

```c
XcursorFilenameLoadImages(path, XcursorGetDefaultSize(dpy))
```

Change the FVWM wrapper/API to accept a requested pixel size derived from the target monitor. Use:

- `XcursorFilenameLoadImages(path, target_px)` for configured files;
- `XcursorShapeLoadImages(shape, theme, target_px)` or `XcursorLibraryLoadImages(name, theme, target_px)` for standard/themed cursors;
- `XcursorImagesLoadCursor()` to create the X cursor resource.

Add wrappers for the selected theme/library APIs in `libs/Fcursor.h` rather than exposing third-party calls throughout `fvwm/cursor.c`.

Cache cursor resources by `{shape-or-file, theme, target_px, render capability, generation}`. Destroy replaced resources only after windows have been rebound. Because cursors are attached to X windows rather than monitor regions:

- rebind frame/menu/pan-frame cursors when their target monitor changes;
- select move/resize cursors from the pointer's current monitor during an active operation;
- dynamically rebind any root-wide cursor when pointer ownership crosses monitors.

`XcursorSetDefaultSize()` remains useful only as a global fallback; it is not the per-monitor mechanism.

### 4. Icons, Pixmaps, and Decorations

Keep existing decoders and `PGraphicsRenderPicture()`; pass explicit destination pixel dimensions computed from logical metrics and monitor scale.

- Vector assets: rasterize with librsvg at final target dimensions.
- Raster assets: decode once, then scale from the source image to the requested destination; never scale a cached 1x derivative again for 2x.
- Picture/icon caches: include requested width/height or effective scale in the key. A pathname alone is no longer a complete identity.
- Decoration metrics: centralize title height, border width, button size, icon size, menu spacing, and hit-target conversion through the scale helper.
- Re-render from source on scale changes; do not stretch the already rendered frame pixmap.

XRender should be preferred when available for alpha and filtering, but monitor/geometry correctness must remain testable without assuming a compositor.

### 5. Geometry and Client Contracts

No library can make X11 client surfaces intrinsically per-monitor DPI-aware. Keep these boundaries explicit:

- RandR monitor rectangles, `ConfigureRequest`, pointer coordinates, EWMH work areas, frame extents, module packet geometry, and X window sizes are root/window **pixels**.
- Logical dimensions are internal policy values used to derive those pixels.
- When preserving perceived logical client size across monitor scales, compute one ideal target size from the stable logical basis, then pass it through the existing ICCCM constraint path.
- Respect `PMinSize`, `PMaxSize`, `PBaseSize`, `PResizeInc`, `PAspect`, and gravity; fixed-size clients may not be forced to an invalid scaled size.
- Continue ordinary synthetic `ConfigureNotify` behavior after a WM-driven constrained resize.

Do not silently change the existing module ABI to logical coordinates in v0.1. If scale metadata is later exposed to modules, add an explicit field/message version while preserving pixel geometry semantics.

## Testing Strategy

### Pure Native Tests

Add Meson-registered C tests for:

- millimeter/DPI validation and safe fallback;
- override precedence and stable monitor-name matching;
- rational scale reduction and overflow-safe rounding;
- negative root coordinates and boundary ownership;
- logical-to-pixel-to-logical transition invariants;
- ICCCM increment/base/min/max/aspect constraint interaction;
- topology diff decisions and generation changes;
- cache keys separating 1x/2x and target pixel sizes.

Keep these tests free of Xlib calls so GCC, Clang, ASan, and UBSan can exercise them deterministically.

### Headless X11 Integration

Add a Linux-only Meson suite with a Python-stdlib harness that:

1. allocates a free display and temporary Xauthority/config directory;
2. starts Xvfb for smoke/logical-monitor tests or Xorg with the dummy driver for output/CRTC tests;
3. waits for the X socket instead of sleeping a fixed duration;
4. uses `xrandr` to build scenarios;
5. starts FVWM-X with an isolated configuration and log;
6. drives small purpose-built Xlib clients with known `WM_NORMAL_HINTS`;
7. asserts monitor scale diagnostics, frame/client geometry, resource sizes, and survival after changes;
8. terminates every child and prints server/FVWM logs on failure.

Required scenarios:

- 1x + 2x side-by-side monitors;
- negative monitor coordinates;
- overlapping and mirrored monitor rectangles;
- primary change without geometry change;
- output/monitor removal while windows are assigned to it;
- zero/implausible physical millimeters and explicit override;
- constrained and fixed-size clients crossing scales;
- repeated hotplug/configuration bursts without duplicate relayout loops;
- Xcursor/XRender disabled fallback build, plus the fully enabled reference build.

Use Xephyr for interactive developer inspection, but not as proof of multi-output semantics. Use Xorg dummy for tests that specifically require separate output IDs, CRTC changes, and physical-size metadata.

## Implementation Map

| Existing area | Required adaptation | New dependency? |
|---------------|---------------------|-----------------|
| `libs/FScreen.[ch]` | Full monitor snapshot, physical size, output list, scale state/generation, broader notification masks | No |
| `fvwm/events.c` | Coalesced topology refresh and bounded deferred relayout | No |
| New pure scale-policy module under `libs/` | Rational arithmetic, validation, automatic policy, conversion helpers | No |
| `libs/Fft.c`, `libs/FftInterface.h`, `libs/Flocale.c` | Scale-aware patterns and font cache identity | No |
| `libs/Fcursor.h`, `fvwm/cursor.c`, `libs/PictureImageLoader.c` | Explicit requested size, themed/shape loaders, per-size cache/rebinding | libXcursor promoted in supported profile |
| `libs/Picture.c`, `libs/PictureGraphics.c`, image users | Target-size/scale cache keys and source-based rerender | No |
| `fvwm/borders.c`, `fvwm/menus.c`, `fvwm/move_resize.c`, `fvwm/placement.c`, `fvwm/ewmh.c` | Consume centralized scaled metrics while preserving pixel protocol boundaries | No |
| `tests/meson.build`, new X11 harness/probes | Unit and live topology coverage | Xvfb/Xorg dummy/xrandr as test-only packages |
| `.github/workflows/ccpp.yml` | Explicit fully enabled HiDPI job on native Linux | No runtime dependency |

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Topology | XRandR 1.5 monitor objects | Xinerama screens or output-only heuristics | Xinerama lacks scale/physical/output detail; outputs do not necessarily equal logical monitors. |
| Scale representation | Reduced rational, integer pixel outputs | `double` scale throughout geometry | Exact comparisons and repeated transitions become unstable. |
| Font sizing | Per-pattern `FC_DPI`/`FC_PIXEL_SIZE` | Global `Xft.dpi` | Cannot represent two simultaneous monitor scales. |
| Cursor sizing | Explicit-size Xcursor images | `XcursorSetDefaultSize` or `XCURSOR_SIZE` | Both are display/process global. |
| Icon scaling | Existing source loaders + target rendering | New toolkit/image framework | Existing stack already decodes and renders required formats. |
| Integration display | Xorg dummy for authoritative multi-output tests | Xephyr only | Xephyr is excellent for visibility but does not provide the most controllable output topology. |
| Test orchestration | Meson + C + Python stdlib | pytest/Bats/custom daemon | Adds dependencies without a need for discovery fixtures or plugin ecosystems. |

## Development Packages

Illustrative supported-profile packages; exact names vary by distribution:

```bash
# Debian/Ubuntu family
apt install \
  libx11-dev libxrandr-dev libxft-dev libfontconfig1-dev \
  libxcursor-dev libxrender-dev libpng-dev librsvg2-dev \
  xvfb x11-xserver-utils xserver-xorg-core xserver-xorg-video-dummy

# Configure an explicit reference build rather than relying on automagic.
meson setup builddir \
  -Dxcursor=enabled -Dxrender=enabled -Dpng=enabled -Dsvg=enabled
meson compile -C builddir
meson test -C builddir --print-errorlogs
```

Keep the existing regular GCC/Clang and sanitizer matrices. Add the live X11 suite only on native Linux; local macOS/Podman emulation remains suitable for pure C policy tests, not authoritative X server behavior.

## Roadmap Implications

1. **Scale primitive and monitor metadata first.** Every later subsystem needs one stable effective scale and exact conversion rules.
2. **Transactional topology refresh second.** Resource invalidation and geometry transitions are unsafe until monitor diffs and generations are reliable.
3. **Fonts, cursors, icons, and decoration metrics third.** These can independently consume the scale API and establish cache conventions.
4. **Window transition and placement geometry fourth.** This depends on authoritative monitor ownership and scaled decoration extents.
5. **Headless topology integration throughout, with full scenarios last.** Unit-test policy as it is extracted; gate completion on dummy-Xorg mixed-scale transitions.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| RandR APIs and event semantics | MEDIUM | Official RandR 1.6 protocol text (including 1.5 monitor requests) cross-checked with current `FScreen.c`. |
| Xft/fontconfig sizing | MEDIUM | Official Xft manual and Fontconfig 2.18.3 property/reference docs cross-checked with `Fft.c`. |
| Xcursor explicit sizing | MEDIUM | Official libXcursor manual cross-checked with `Fcursor.h` and `PImageLoadCursorFromFile`. |
| X11/ICCCM/EWMH coordinate boundary | MEDIUM | Official ICCCM 2.0 and EWMH 1.5 plus RandR transform semantics. |
| Nested/dummy X test split | MEDIUM | Official server manuals establish roles; exact packaged-server RandR capabilities must be probed in the phase's CI image. |
| Dependency floors/current releases | MEDIUM | Upstream release indexes observed on the research date; distribution availability still needs configure-time validation. |

## Sources

All source-backed findings below carry **MEDIUM** confidence under the configured research seam.

- X.Org RandR protocol 1.6, including monitor objects added in 1.5: https://cgit.freedesktop.org/xorg/proto/xorgproto/plain/randrproto.txt
- libXrandr upstream release index: https://xorg.freedesktop.org/releases/individual/lib/
- xorgproto release index: https://xorg.freedesktop.org/releases/individual/proto/
- libXft manual: https://cgit.freedesktop.org/xorg/lib/libXft/plain/man/Xft.man
- Fontconfig Developers Reference 2.18.3: https://fontconfig.pages.freedesktop.org/fontconfig/fontconfig-devel/
- libXcursor manual: https://cgit.freedesktop.org/xorg/lib/libXcursor/plain/man/Xcursor.man
- ICCCM 2.0: https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html
- EWMH 1.5: https://specifications.freedesktop.org/wm-spec/latest-single/
- Xlib C Language Interface: https://www.x.org/releases/current/doc/libX11/libX11/libX11.html
- Xvfb manual: https://www.x.org/releases/current/doc/man/man1/Xvfb.1.xhtml
- Xephyr manual source: https://cgit.freedesktop.org/xorg/xserver/plain/hw/kdrive/ephyr/man/Xephyr.man
- xrandr manual and release index: https://www.x.org/releases/current/doc/man/man1/xrandr.1.xhtml and https://xorg.freedesktop.org/releases/individual/app/
- Xorg video-driver release index: https://xorg.freedesktop.org/releases/individual/driver/
- librsvg target-size render API: https://gnome.pages.gitlab.gnome.org/librsvg/Rsvg-2.0/method.Handle.render_document.html
- Cairo transformations: https://www.cairographics.org/manual/cairo-Transformations.html
- Repository evidence: `meson.build`, `libs/FScreen.[ch]`, `fvwm/events.c`, `libs/Fft.c`, `libs/Fcursor.h`, `libs/PictureImageLoader.c`, `libs/PictureGraphics.c`, `tests/meson.build`, and `.github/workflows/ccpp.yml`.
