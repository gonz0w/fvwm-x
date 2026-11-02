# Desktop component metrics

2026-11-02 · THEME-02 #214 · Work in progress

The desktop settings store gains `desktop_font`, `desktop_density`, and
`desktop_border`. Empty font inherits configured `infostore.slate_font`; density
and border default to inherit. Module overrides retain precedence. The font is
at most 128 bytes of valid printable UTF-8, with no leading or trailing space,
backslash, double quote, dollar, percent, ampersand, controls, U+2028 or U+2029.
Reject values that cannot survive a native command literal rather than silently
stripping characters.

The pure desktop-metrics projection is version 1, separate from color
composition version 2, preference-file version 10 and IPC schema version 1.
Menu item/title spacing and form text padding are 8/10/12 for inherit or
comfortable, 4/6/6 for compact and 12/14/16 for spacious. Launcher border
width is 1 for inherit or thin and 3 for strong. Existing scaling applies; the
projection adds no DPI multiplier.

The controller applies these values only to `SlateLauncherStyle` after the
existing style copy, and to future controller-owned Slate forms through their
font, input font, button font and vertical text padding. Existing form resources
remain until reopen. Initial inherit adds no override. A transition from an
explicit value to inherit restores the named shipped-profile baseline only; it
does not restore arbitrary outside commands. Panel, tray, global decoration and
default font overrides are outside this slice.

The compact `group:desktop` editor uses the existing store and save/apply
path; these keys have no PreviewKey. Publication waits for existing preview
recovery and a conflict blocks it. Metadata distinguishes requested, effective,
source and resolution, launcher submission, and future owned-form start; it
is not a rendered acknowledgement. Exact schemas 1 through 9 load without a
rewrite; the next save writes schema 10. Older helpers cannot read schema 10.
No new package or clipboard enablement is included.

Affected compilation and source/static review are planned for this batch.
Native runtime checks and three materially different theme/enlarged-text
captures remain queued for the shared appearance checkpoint; no new runtime or
screenshot evidence is claimed here.
