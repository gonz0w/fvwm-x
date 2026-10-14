# Default desktop verification — 2026-10-04

Slate is the sole shipped default session layout. Slate, Dune, Grove, and Ink
share the same configuration and each provide light and dark colors and an
original geometric wallpaper. The old default icons, wallpaper chooser,
confirmation/copy scripts, clock script, and tray configuration were removed.
Module fallback configurations and existing personal-config lookup remain
compatible.

The default uses FvwmButtons, FvwmBacker, FvwmForm, FvwmEvent, native menus,
FvwmPager, and the new stdlib-only FvwmDesktop helper. Comments describe stable
colorset roles and the personal override hook. Preferences are private,
atomic JSON; automatic appearance is session-owned and needs no cron entry.
No GTK/Qt configuration or additional runtime package is installed.

## Evidence obtained

| Environment | Result |
| --- | --- |
| Linux AMD64 binaries on macOS through Podman emulation | Build, install, 25 native tests, desktop Xvfb, compatibility Xvfb, and graphics error regression passed |
| FreeBSD 15.1 ARM64 under QEMU/HVF | Native desktop helper policies, actual CPU/RAM counters, rebuilt graphics library/modules, desktop Xvfb, and graphics regression passed |
| FreeBSD 15.1 AMD64 under QEMU/TCG | Same checks passed |
| OpenBSD 7.9 ARM64 under QEMU/HVF | Same checks passed, including absent-battery detection |
| OpenBSD 7.9 AMD64 under QEMU/TCG | Same checks passed |

The desktop checks verify the 1440×36 primary bar, all four palette repainting
paths, light/dark and schedule settings, actual opaque/tinted bar pixels,
launcher dismissal, shell-free fixture application launch, submitted search,
calendar, screenshot capture while the core serves Echo, and correct screenshot
RGB at 16-bit and 24-bit X depths. They save actual native-menu captures for all
eight palette variants. Python policy tests cover schedule boundaries and
midnight, untrusted settings, private persistence, menu-title sanitation,
desktop-entry execution and user masking, text contrast, BSD counter formats,
absent batteries, and wallpaper retention across restarts.

The Linux installed default contains only config, desktop.fvwm, palettes.json,
the session badge, and eight wallpapers. FvwmDesktop installs as an executable
module. Old default-script install symlinks and translation input references
were removed.

Raw evidence and review images are under the ignored
`builddir/slate-verification/`. The final BSD evidence lives in each named
architecture directory under `bsd/`; earlier diagnostic logs in the parent
directory record failures before the fixes. Guests used disposable disk
snapshots and separate desktop fixtures. Saved VM disks were left intact.

## Graphics regression fixed

Rapid palette replacement exposed two existing library errors. The temporary
X error-handler helper installed the previous callback instead of the
requested callback. Buffered root transparency also restored the caller's
handler before freeing a GC whose server-side creation could have failed
after wallpaper replacement.

The helper now installs the requested callback, and transparent drawing drains
GC cleanup before restoring the caller. The real-X regression checks actual
server error handling, delegation, and restoration. Its linker wrapper expires
a root pixmap after geometry validation and before GC creation, deterministically
exercising the draw and cleanup path. A verification-only binary using the
pre-fix Colorset implementation fails this test; the corrected binary passes.
The new desktop and this regression pass on all four BSD targets above.

## Reproduction and limits

After compiling with `-Dlive-x-tests=true`, run:

```sh
meson test -C builddir desktop-policy desktop-baseline-xvfb \
    x-error-handler-xvfb compatibility-xvfb --print-errorlogs
```

These checks cover nested Xvfb rendering, rather than physical GPU, mixed-DPI
hotplug, external compositors, sound hardware, power permissions, or secure
authentication. This desktop change was not separately rerun on Linux ARM64
or under native sanitizers. BSD reused native source/build snapshots and
rebuilt the affected graphics code; it did not repeat the full release install
matrix.

Transparency uses FVWM wallpaper tint and snapshot menus. Native motion controls
do not provide live compositor blur or client fades. Lock and sound controls use
available backends and disable unavailable actions. The helper's messages are
desktop-action notices, not a D-Bus application-notification service. Files,
calendar, and display information use native menus or base tools; a graphical
monitor editor, bundled secure locker, full notification service, and GTK/Qt
integration remain separate work.

## Shadow and spacing review follow-up

The baseline now uses 32-pixel titles, two-pixel resize/focus borders, a visible
one-pixel menu keyline and a configurable 16-pixel working-area inset. Explicit
EWMH working-area styles make maximization honor the inset. Auto-detected base
X terminals receive 14 pixels of internal padding; explicit user terminal
commands retain their supplied arguments.

An optional, commented Picom profile lives in
`contrib/compositors/slate-picom.conf`. Nothing installs or starts a compositor
as part of the baseline. Review packages were installed only in a disposable
Linux AMD64 Podman container. Existing native WM/module binaries were copied
from the verification volume before running the updated config and helper.

Ten desktop policy tests pass on macOS and Linux. The real Xvfb desktop harness
passes the existing service, palette, opacity, capture-format and parser checks,
plus a Picom selection check, a localized captured-shadow pixel comparison and
maximized-frame geometry `[16, 52, 1409, 824]` on a 1440x900 display. Terminal
size increments/frame accounting permit up to two pixels of rounding.

The review used Picom 12.5, XRender and `--legacy-backends`. The newer XRender
path produced faint/missing shadows and repaint artifacts in this emulated
software-rendering environment; the compatibility path produced clean window
and menu shadows. Physical GPU rendering and BSD compositor behavior were not
rerun for this appearance follow-up. No C code changed in this follow-up.

Screenshots now read the Composite overlay when a compositor owns the screen;
the system Xcomposite library is loaded only in that case. Non-composited
capture still needs only Xlib. A missing Composite library produces a useful
capture error instead of silently dropping compositor effects. The preview
container required `libxcomposite1` alongside Picom for this capture path.

All eight palettes have actual composited window/launcher captures under
`builddir/slate-verification/shadows/`, with logs, `evidence.json`, a pre-Picom
control and the separate maximized view. Reproduce with:

```sh
python3 tests/live_x/desktop_live.py --build builddir \
    --artifacts-dir builddir/slate-verification/shadows \
    --picom-config contrib/compositors/slate-picom.conf \
    --picom-legacy-backends
```

## Native bar and quick-settings follow-up

The baseline now separates CPU, RAM, network, sound and real battery readings
into clickable FvwmButtons cells. A native panel anchors below Quick and includes
Light/Dark/Auto, transparency, motion, task-list selection, service shortcuts,
and a palette-aware FvwmScript volume slider. FvwmIconMan provides the optional
embedded task list. Its native actions restore/raise/focus, minimize, or open
window operations. The focused-title widget is available in custom layouts.

A native form saves ordered left/center/right widget lists. Startup module
options override saved sections. The bounded layout policy keeps cells inside
narrow/wide displays, preserves the center when space permits, hides absent
batteries and disabled tasks, and removes optional cells before navigation.
Topology/layout changes replace only owned modules. KillModule uses executable
and alias together; the helper refreshes newly mapped panels so startup messages
cannot leave selection or slider state stale. Numeric FvwmScript signal labels
use the required space before the colon. The quick panel accepts focus for
Escape; Close and Quick also dismiss it.

Fifteen desktop policy tests cover layout bounds from 1 to 3840 pixels,
untrusted/duplicate widget identifiers, Linux/BSD mixer readings and argument
arrays, unavailable devices, bounded/sanitized title events, and the previous
appearance/application/settings policies. The real-X gate replaces individual
metric labels and observes subsequent live repainting, clicks Light/Dark,
checks the selected appearance color, dismisses a focused panel with Escape,
checks actual task-list reparenting and minimized-client restoration, saves a
custom layout and checks a 640-pixel bar before restoring the default. Existing
palette, launcher, service, screenshot, parser and shadow checks remain.

The Linux AMD64 WM and used modules were built from an isolated copy of the
current source. Meson configuration and the install manifest include bar.fvwm,
SlateVolume and FvwmDesktop. Linux policy, native control and composited review
checks pass. Slider/mute/unmute writes use a session-private deterministic mixer
fixture: these verify UI and protocol behavior, not physical audio hardware.
The BSD gates use real base-tool counters and report unavailable VM audio;
portable backend parsing and writes have unit coverage.

Saved BSD disk images predate the earlier Colorset/XError fixes. Reusing their
old libraries reproduced BadGC during rapid palette replacement on OpenBSD.
The existing fixes were transferred and the WM/used modules relinked in
snapshot guests before final verification. Saved disks were left intact.

Final evidence is stored in builddir/slate-verification/bar/. Linux native
captures include all eight palettes and the separate quick-settings and task
views; the composited review uses optional Picom 12.5's legacy XRender backend.
The volume value in those review captures comes from the test mixer. Physical
GPU, audio hardware and native Linux sanitizer coverage remain CI/hardware work.
GTK/Qt, a tray, full application notifications, a bundled secure locker and a
graphical monitor editor remain separate work.

```sh
python3 tests/test_desktop.py
python3 tests/live_x/desktop_live.py --build builddir \
    --artifacts-dir builddir/slate-verification/bar \
    --sound-fixture --picom-config contrib/compositors/slate-picom.conf \
    --picom-legacy-backends
```

The final native bar gate and all fifteen policies pass on FreeBSD 15.1 and
OpenBSD 7.9 for both AMD64 and ARM64. Their final-run logs, native counter
samples, screenshots and outcome manifests are under bar/bsd/<os>-<arch>/.
The test waits for the panel's completed hidden startup state before opening
it; observing its child window alone can precede the native panel withdrawal.
Earlier diagnostic logs were excluded from the final-run archives.
