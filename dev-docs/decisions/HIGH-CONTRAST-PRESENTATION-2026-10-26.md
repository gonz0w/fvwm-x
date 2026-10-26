# High-contrast presentation

THEME-02 #214, R1: four related outcomes extend the accepted color/wallpaper
composition without introducing a second theme engine. One `contrast` preference
(`normal`/`high`, default `normal`) belongs to the existing appearance group.

## Observable contract

Composition version 2 preserves the fourteen existing colorset names/IDs and
normal-mode output. High dark uses black surfaces/raised regions and white
normal/disabled text and borders; high light reverses them. The selected palette
accent remains when its full-precision sRGB contrast against the projected surface
is at least 4.5:1; otherwise use the opposite black/white. Selected text chooses
black or white by greatest contrast. Text/surface is 21:1; accent/surface and
selected text/accent meet at least 4.5:1. This also exceeds the project's 3:1
meaningful-state target for those projected accent-backed states. Decorative
same-color fills are not text pairs. #75 still owns complete native-state,
keyboard/assistive-input and shipped-theme acceptance.

Explicit black/white surfaces with guarded palette accents are simpler than
iterative RGB adjustment or new theme assets, and retain palette identity where
it meets the threshold. Validate all colors/choices before returning owned roles;
malformed hidden colors remain errors. No client content or wallpaper pixels are
recolored by this projection.

Module overrides resolve over saved requests first. One pure presentation
resolver then applies high contrast as the strongest opaque constraint: panels
and menus become opaque, while saved/effective requested transparency and opacity
stay intact. Returning to normal restores those requests. Wallpaper retention,
independent wallpaper choices and motion retain their current ownership.

Registry/editor/menu/IPC report requested values separately from `resolved`
presentation, `resolution` and `runtime_constraint`. High contrast reports false
transparency and 100 launcher opacity with `high-contrast`; normal-mode disabled
transparency retains `transparency-disabled` masking for opacity. Composition
version is 2, private preference schema is 4, and IPC schema stays 1. Query fields
are stable within their preference generation and do not acknowledge final paint.

## Preview and persistence ownership

The existing controller refuses both saved and runtime-only opacity previews in
high contrast, including a grouped draft selecting it. Admission, delayed inspect
and action/reply boundaries recheck the same policy. A successfully committed or
reloaded high-contrast transition abandons pending/active preview work through the
existing core lease path before opaque theme commands. Failed validation or
persistence leaves the preview alone. The core's generation/recovery authority is
unchanged; scalar opacity save does not replay a whole theme. Existing normal-mode
runtime preview behavior is preserved.

Schema 4 requires the eleven registered preferences plus retained wallpaper.
Schemas 1, 2 and 3 require exactly their historical fields; they gain contrast
normal, with existing opacity/follow defaults as appropriate. Load never rewrites;
next successful save writes 4. Unknown/invalid/oversized data remains preserved.
Unversioned data retains its original normalization. Older helpers cannot read 4.
No supported FVWM directive, binding or configuration file needs a rewrite.

## Boundaries and verification

No C/module protocol, dependency, service, asset package, compositor, toolkit or
font/density/reflow contract changes. Existing Buttons/Form colorset repaint is
used. Fonts and density need real module recreation/layout support; #212/#214
retain that work, including the existing enlarged fixed-width bar clipping.
Parked Backer/pathname work remains excluded.

Use one affected Go/race and native Linux checkpoint for numerical projection,
normal parity, schema/refusal/ownership, request restoration, preview races,
opaque controls and independent wallpaper. Inspect three real configurations and
native UI captures. The due PR253→PR264 observation uses preserved binaries and
keeps partial telemetry separate from complete release/profile budgets. Broader
THEME-02 and six-target/theme acceptance remain open.
