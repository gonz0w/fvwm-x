# Slate Appearance and interactive review — 2026-10-04

This follow-up adds the native Appearance panel and bundled symbols, tests the
previous shadow artifact, and starts a real desktop for user-controlled review.
No C implementation or toolkit theme is changed by this work. The baseline
continues to require no new desktop runtime library, icon font, compositor or
system icon theme.

## Shipped behavior

- The bar's palette button, Quick palette row and launcher Appearance action
  toggle a native, centered 640x480 FvwmButtons panel. Four cards show their own
  background, surface, border and accent colors, with text and color selection.
- Light, Dark, Automatic, schedule, spacing, transparency, native motion,
  wallpaper retention and task-list controls apply and persist through the
  existing desktop helper. The panel remains open across spacing changes.
- Dedicated colorsets 30..37 keep palette previews independent. Initial module
  configuration includes saved labels and artwork before mapping; dynamic
  ChangeButton updates do not have to race FvwmButtons' initial subscription.
- Utility-panel pointer focus and explicit focus/raise click forwarding avoid
  consuming the first button click. Appearance definitions are read before each
  opening, rather than reread during a spacing change. No global client focus
  policy is changed.
- Twelve original 18x18 symbols have normal and selected ink in all eight
  palette variants; eight palette strips show four color roles. The resulting
  200 PNGs ship with the theme. The stdlib-only generator is a development tool.
  Builds without PNG support retain written labels and functional controls.
- Bar, Quick and launcher result icons share the same geometry. The launcher
  symbol rail has separate widget identifiers and does not cover result text.
  Application/toolkit content retains the user's own colors and fonts.

## Compositor comparison

Picom 12.5 was built from its official release source in the native ARM64 Linux
review container. Modern XRender, modern GLX, and GLX without fades completed
the full desktop/monitor gate and remained alive through final teardown.
Legacy XRender failed XRender picture allocation (BadDrawable), then exited
with status -6 during the same overlapping-window scenario. This reproduces
its earlier AMD64-emulated failure on native hardware; it is an unstable
compositor path, not evidence that FVWM frame borders need redesign.

GLX remained alive, but XGetImage of its Composite overlay retained old bar
pixels in this headless setup. Direct XGetImage of the bar showed current time
and CPU readings while the captured overlay held older values. Disabling
compositor damage did not resolve that capture discrepancy. Modern XRender
keeps the viewer's framebuffer current, so the optional profile and live
review use XRender without --legacy-backends. Physical GPU GLX behavior has
not been established by these headless captures.

The optional profile retains broad 20-pixel shadows at 28% opacity, an
8-pixel downward visible offset, opaque client content and rectangular hit
regions. Short compositor fades are independent of the native Motion setting.
The compositor is neither installed nor launched by the baseline.

## Verification

The policy suite has 17 desktop and 14 launcher checks. Added checks cover
palette-preview roles, all symbol paths, unique native widget identifiers,
and result labels staying outside the icon rail.

Real XTest pointer input selects four palettes in all three modes, checks
saved selection and artwork in the first frame, retains the panel across both
spacing settings, changes/restores four toggles, opens the time form and closes
the panel. Eight palette captures are actual native X images. The gate orders
commands through the helper and round-trips through FvwmButtons before sending
a subsequent click; a core Echo alone does not establish widget readiness.
It also waits for the new bar height after spacing changes. Pointer presses
use a deliberate 200 ms dwell in this setup gate, rather than a 50 ms synthetic
click while emulated startup/repainting is still pending; this is not a latency
benchmark. Initial failures
and module event traces are retained separately; final binaries have
no diagnostic instrumentation.

Both Linux architectures passed the full desktop and two-output Xorg-dummy
regression, including the launcher, clipboard, status widgets, task list,
sound fixture, palette/schedule, density and window controls. The final native
ARM64 compositor gate uses modern XRender and checks liveness at completion.
BSD checks use task-owned QEMU snapshots and real Xvfb/module processes on
FreeBSD and OpenBSD, ARM64 and AMD64.

| Target | Policy | Final Appearance / density / window controls | Staged assets |
| --- | --- | --- | --- |
| Linux ARM64 | 31 pass | Pass; full desktop + two-output Xorg also pass | 200 icons + panel |
| Linux AMD64 (CPU emulation) | 31 pass | Pass; full desktop + two-output Xorg also pass | 200 icons + panel |
| FreeBSD ARM64 | 31 pass | Xvfb pass | 200 icons + panel |
| OpenBSD ARM64 | 31 pass | Xvfb pass | 200 icons + panel |
| FreeBSD AMD64 (CPU emulation) | 31 pass | Xvfb pass | 200 icons + panel |
| OpenBSD AMD64 (CPU emulation) | 31 pass | Xvfb pass | 200 icons + panel |

Thirteen theme/test source fingerprints and the complete 200-image asset tree
match across all final targets. Transfer-only macOS metadata files are excluded
from that runtime asset inventory and are not part of the Meson install list.
The final panel-focused follow-up covers the later utility focus and template
reread changes; the preceding full Linux runs cover the complete desktop.
Native BSD diagnostic edits were restored before the passing OpenBSD gate.
Only the four task-owned QEMU snapshots and the AMD64 test container were
stopped after evidence collection; the native interactive desktop remains live.

## Interactive review

The test-only supervisor is `tests/start_desktop_review.py`. It runs real FVWM,
its native modules and two xterms on a disposable X server. x11vnc and noVNC
carry mouse/keyboard input and the actual composited framebuffer; this is not
a browser mockup. Those viewer tools and Picom exist only in the test container.
The host listener is mapped to 127.0.0.1:6080. Preferences and terminals use a
separate review home. Closing a sample terminal leaves the desktop running.

Open http://127.0.0.1:6080/vnc.html?autoconnect=1&resize=scale and click inside
its desktop to send input. F2 opens the launcher, F3 toggles Appearance, and
F4 opens the overview. These three shortcuts are review-only alternatives
when the browser/host intercepts Super keys; the shipped launcher retains
Super+Space. Drag title bars, resize edges and use the native title controls.
The live review stays running after delivery for the user's exploration.

Evidence is under `builddir/slate-verification/appearance-review/`, with
backend comparisons, native captures, platform logs and source fingerprints.
This is targeted desktop verification. Native sanitizer CI and physical
GPU/DPI/input hardware are separate release checks.
