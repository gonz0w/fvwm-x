# Slate live launcher verification — 2026-10-04

Super+Space now toggles native live search across applications, managed
windows and bundled desktop actions. Applications launch normally; window
results restore, raise and focus their existing client. Super+Shift+Space
retains the traditional application menu. The bar uses the same live launcher.

The commented `default-config/SlateLauncher` defines the FvwmScript widgets,
keyboard signals and result rows. `launcher.py`, installed beside FvwmDesktop,
provides matching and activation using Python's standard library. The default
configuration documents preferences and shortcut overrides; FvwmDesktop(1)
documents controls, discovery limits, persistence and customization.

Matching covers localized names, generic names, keywords, descriptions and
executable names, plus declared desktop-entry actions. Every query word must
match; exact/prefix/word matches precede fuzzy subsequences. Stable application
IDs support favorites and optional recents. Up to eight generous title and
description rows fit in a 620×552 panel; a 640×480 monitor uses 608×408 and five
rows. Palette roles supply light/dark colors without separate launcher themes.

## Transport and compatibility

Typed text travels as fixed 512-byte records over a private FIFO, separate
from FVWM commands. The helper coalesces records and bounds matching work;
only trusted actions and numeric window/result identities reach the command
channel. Application execution uses argument arrays. History stores identities,
never query text, in private atomic JSON files. Stale row-click generations are
rejected, and startup failure cleans private files before opening the fallback.
Enter resends the current field before activation and stays open when transport
is unavailable. Input backlogs drain in bounded batches before activation.
A timer retries only the newest unsent query. Retiring-window
packets are ignored until destruction, allowing subsequent XID reuse.

FvwmScript's new `SendMsgAndGet` mode 2 performs a nonblocking atomic write.
Missing readers, full pipes and oversized messages return immediately. Legacy
modes 0 and 1 retain their startup and write/reply behavior. `KeyboardFocus`
opts a named script into typing without pointer warping, Num Lock tolerant
bindings, select-all and bounded clipboard paste; other scripts retain their
existing input routing. Invalid script parsing now exits before building a
partial UI. The manual's `ChangeColorset` spelling was corrected to match the
interpreter.

## Evidence

The native gate uses the compiled WM and modules on a real nested Xvfb display,
with XTest key/mouse input and a native X selection owner. No rofi, dmenu,
clipboard utility or new GUI library is used.

| Environment | Policy checks | Native desktop/launcher gate |
| --- | --- | --- |
| Linux AMD64, Podman x86 emulation | 11 launcher + 15 desktop passed | Passed |
| Linux ARM64, Podman | 11 launcher + 15 desktop passed | Passed |
| FreeBSD 15.1 ARM64, QEMU/HVF snapshot | 11 launcher + 15 desktop passed | Passed |
| FreeBSD 15.1 AMD64, QEMU/TCG snapshot | 11 launcher + 15 desktop passed | Passed |
| OpenBSD 7.9 ARM64, QEMU/HVF snapshot | 11 launcher + 15 desktop passed | Passed |
| OpenBSD 7.9 AMD64, QEMU/TCG snapshot | 11 launcher + 15 desktop passed | Passed |

Native checks exercise Super+Space, live fuzzy keyword matching, favorites,
normal application launches, declared application actions, actual result-row
clicks, private recents, minimized-window restore/focus, desktop appearance
actions, empty results, navigation, history clearing, unavailable FIFO
cancellation and rejected activation, toggle dismissal, compact layout and launch, Num Lock shortcuts,
Ctrl+A and Ctrl+V. A rejected script exits with status 1 before mapping.
Existing bar, quick settings, tasks, schedule/transparency, parser and 16/24-bit
screenshot checks remain in the gate. The palette review also starts the
swallowed volume widget offscreen with UsePPosition, preventing a floating
startup flash during asynchronous panel rebuilds. This final configuration
polish was additionally retested on Linux ARM64 and OpenBSD ARM64, with clean
review captures. All eight palette variants have actual
native launcher review captures.

Policy checks cover ranking, Unicode normalization, provider filtering,
settings/history validation, localized discovery and action argv, fragmented
and coalesced FIFO records containing literal command-like text, activation,
stale clicks, deferred activation across multiple input batches,
retirement/reopening with XID reuse, small-screen geometry, and
failed-startup cleanup/fallback. The slower OpenBSD AMD64 run exposed an
activation/reopening timing failure; its failing logs are retained and the
final gate adds transport-failure activation and paint/close barriers. A
FreeBSD AMD64 rerun exceeded the original two-second cancellation allowance
under concurrent TCG load; the fixture now allows eight seconds, still below
the legacy twenty-second startup stall. That initial timeout is retained.
`py_compile` and `git diff --check` pass. Linux install introspection confirms
SlateLauncher in the default data directory and launcher.py beside FvwmDesktop.

Run after building with `-Dlive-x-tests=true`:

```sh
python3 tests/test_launcher.py
python3 tests/test_desktop.py
python3 tests/live_x/desktop_live.py --build builddir \
    --artifacts-dir builddir/launcher-verification
```

Raw logs, screenshots, gate JSON, source archives and SHA-256 fingerprints are
under `builddir/slate-verification/launcher/`. The final compatibility-preserving
run lives in each platform's `handoff/` evidence; FreeBSD AMD64's passing
cancellation rerun is `launcher-handoff-final-artifacts/`. The final offscreen
widget follow-up is in `review-final/`, and review images are in `review/`. Earlier captures and
diagnostics are retained separately. BSD guests are disposable snapshots;
existing graphics and bounded property/selection helpers were relinked from
the workspace into their saved native source trees. The final source refresh
also includes the concurrently added FvwmScript file helper and build definition;
initial incomplete-mirror build logs are retained as first-build evidence.
This is targeted desktop
verification, not a gate for unrelated concurrent workspace changes.

## Limits

These are nested-X checks; physical GPU, sound hardware and secure lock/power
permissions are outside this launcher verification. AddressSanitizer remains
a native Linux CI responsibility. This iteration adds no icon-theme lookup,
command/path/calculator modes, system-wide file indexing or GTK/Qt theme.
Existing Run command and Files services remain independent. Compositor shadows
are optional and were not required for the launcher gate.
