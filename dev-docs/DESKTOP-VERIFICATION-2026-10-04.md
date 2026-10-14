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
