# Slate SVG artwork — focused native Linux checkpoint

Date: 2026-10-06. Owner: THEME-01 / DESKTOP-01.
Status: implemented, focused native Linux checks passed; complete six-target
theme qualification remains pending. The user explicitly requested adoption of
the richer SVG concepts and screenshots from the running desktop. See the
[decision](../decisions/THEME-SVG-ARTWORK-2026-10-06.md).

## Changes and scope

377 self-contained SVG assets replace the shipped geometric PNG artwork:
368 icons/palette strips, eight wallpapers and one FVWM-X session badge.
The twelve small symbols use 24-pixel native dimensions; seven Session symbols
use 40 pixels, with all eight palette/mode combinations and normal, selected
and unavailable states. Rounded badges, semantic palette accents and subtle
gradients provide the approved richer direction. Geometry remains simple at
small sizes; labels/actions are unchanged.

The existing stdlib generators produce committed SVG, without login-time
generation, embedded fonts, external images, scripts or filters. Meson installs
SVG paths and configures the desktop helper from SVG feature availability,
independently of PNG. The X session desktop entry points to the SVG badge.
Wallpaper options request actual viewport pixel dimensions and topology
refresh invalidates the old wallpaper dimensions. Existing native titlebar
vectors remain native; screenshots and arbitrary raster-image support retain
PNG where appropriate. The global codec/dependency defaults are unchanged.

Launcher rows retain unchanged icon paths, avoiding repeated synchronous
decoding during queries. Palette/state changes still replace the image;
reopening and explicit theme reload invalidate the retained path state.
Existing result generation, stale-click refusal, FIFO query transport and
command ordering are preserved. This is a bounded default-helper improvement,
not activation of the unfinished asynchronous core adapter.

## Environment and isolation

Linux ARM64 Ubuntu 24.04 in the existing `slate-review-oct04` Podman container,
GCC 13.3.0, Meson 1.12.1, librsvg 2.58.0, Cairo 1.18.0. Source and builds are
isolated under `/tmp/fvwm-slate-svg-20261006`. The main visual profile explicitly
enables SVG/Cairo and disables PNG. The separate fallback profile disables both
SVG and PNG. No guest/system packages were changed, and no existing desktop,
build, VM or view service was stopped or overwritten.

The task-owned desktop was run on Xvfb `:101`, with native FVWM/modules, two real
xterms and optional Picom modern XRender. Screenshots use the existing X11
capture helper and Composite overlay. They are actual running native desktop
pixels, not browser mockups. The existing review display `:99` and localhost
6080 service remain independent.

After screenshot/evidence capture, only the task-owned review supervisor was
stopped by its exact argument identity; it shuts down its own X server,
compositor, window manager and sample terminals. The original review remains
running. Isolated source/build/install directories and evidence are retained.

391 final theme/generator/check/AsciiDoc source hashes match the Linux copy.
Installed SVG files match all 377 source assets byte for byte. The previous
PNG assets and generators are preserved in a task-owned rollback archive.
Only the theme-specific fingerprint scope is claimed, not a new whole-project
or shared asynchronous-image qualification.

## Passing evidence

Evidence directory: `builddir/slate-svg-20261006/` (task-owned, ignored).

| Check | Actual result |
| --- | --- |
| Desktop / launcher / Session policies | 19 + 15 + 17 = 51 pass on macOS and native Linux; includes self-contained SVG inventory, renderer-absent behavior, unchanged-icon retention, palette replacement and fresh-overlay reload |
| Native SVG protocol/client/render regressions | Three distinct entries pass with the built worker; shared worker implementation is unchanged by this theme batch |
| Full `desktop-baseline-xvfb` scenario | Pass in 66.22 seconds, including live keyboard/fuzzy search, clipboard, real result click, stale/absent-reader handling, Session actions/cancellation, all palette/mode Appearance controls, comfortable/compact, desktop controls and two real RandR monitors |
| Main profile build/install | GCC build and staged installation pass with `-Dsvg=enabled -Dcairo=enabled -Dpng=disabled`; 377 SVG assets, zero geometric PNG assets |
| All packaged SVG renders | 377 worker renders pass with five-second subprocess deadlines and real 1024 MiB address-space limits; typed header, nonempty pixels, dimensions and output length checked |
| Additional scaling | 12 representative worker renders pass at 1.25x, 1.5x and 2x, in addition to all 1x renders; normal/selected/disabled plus light/dark samples |
| Meaningful glyph color pairs | 48 endpoint/state pairs pass 3:1 against surface/raised/accent backgrounds; `artwork-evidence.json` records unrounded ratios |
| Minimal fallback build | Separate `-Dsvg=disabled -Dpng=disabled` build passes all 51 policy checks and native Session-panel actions, keyboard/pointer cancellation, light/dark, small-screen fallback and readable bar/control geometry |
| Source/install parity | 391 theme/check/doc hashes match Linux source; 377 installed SVGs match source bytes |
| Patch whitespace | Scoped `git diff --check` passes |

The primary retained logs are `linux-build.log`, `linux-focused.log`,
`linux-desktop-recheck.log`, `linux-launcher-retention.log`, `linux-no-svg.log`,
`linux-install.log`, `native-artwork.log` and `source-parity.log`.
`desktop-evidence.json`, `no-svg-desktop-evidence.json`,
`artwork-evidence.json` and `source-fingerprints.json` retain scope/results.

Actual screenshots: `desktop-svg-dark.png`, `session-svg-dark.png` and
`appearance-svg-light.png`. The full desktop fixture also captures all eight
Appearance palettes and both densities within its isolated artifact directory.

## Retained failures and repairs

The first live wallpaper request used the loader's internal option prefix
rather than the public filename suffix; the file lookup rejected it. The helper
now appends viewport dimensions using public SVG filename syntax.

The initial icon viewBox was 64 with native width/height 24 or 40. FVWM's legacy
SVG dimension contract treated the viewBox as native size, producing oversized
bar images. Assets now have matching native dimensions/viewBox, with original
geometry in a scaled group. All 377 actual worker output dimensions pass.
No shared decoder compatibility behavior was changed.

Two initial full-desktop attempts fail at declared-application-action clicking:
the native query field changes while repeated icon decoding leaves the old
result rows pending. Unchanged-icon retention removes those repeated requests;
the final unchanged test, including that actual click, passes. Prior failing
logs and screenshot are retained; no wait or stale-generation protection was
removed to obtain the result.

## Remaining qualification

This does not close the complete THEME-01 review or IMAGE-01. Core SVG waiting
remains synchronous pending the separately owned asynchronous activation.
The new theme has not completed all six platform/compiler/install/default-X
gates, enlarged-text review, physical mixed-DPI/GPU/input checks, full effective
transparency contrast measurements or assistive-provider qualification.
Worker scale renders demonstrate renderer geometry, not universal module UI
scale coverage. Native Linux release sanitizer CI remains unrun.

AsciiDoc user documentation and the installed session entry are updated. This
profile does not generate man pages; final rendered/manual-install evidence
belongs to its subsequent documentation/platform qualification. Earlier source
and security checkpoints remain scoped to their own reports.
