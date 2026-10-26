# Shared named icons: application and directory menus

ICON-01 #215; four related stories implemented for review. The accepted XDG
resolver now takes explicit theme, name, nominal size, quarter scale, state,
override and fallback requests. Both existing menu producers use one presentation
owner. No new external library, resident service or icon package is selected.

`xdgmenu.IconRequest` validates geometry before acquisition; `ScaleQuarters=4`
means 1x. Supported requests span 1–4x in quarter steps with a rounded-up physical
extent no larger than 4096 pixels. Exact nominal size and integer directory scale
take priority over equally sized assets from another scale directory; physical
distance selects fallback sizes. This follows the size/scale distinction in the
[Icon Theme Specification](https://specifications.freedesktop.org/icon-theme/).

Normal/selected/disabled states are explicit request identities. A scoped
override can supply the state image; absent overrides use the ordinary named
image and fallback. There is no universal state-filename suffix convention.
Existing inheritance, cycles/depth, hicolor, unthemed fallback and supported
absolute filenames remain with `xdgsource.IconFinder`. Legacy `Find` remains a
wrapper; consumers do not mutate finder defaults to serve a request.

`internal/iconrender` owns one Source/finder/request-cache generation and reuses
the old PNG converter plus native SVG size qualifiers. PNG/SVG/XPM copies get
private content identities; SVG and PNG use requested physical extent, while
XPM retains source dimensions. Source bytes, requests/cache strings and published
bytes are bounded separately. A new acquisition replaces all three owners,
including negative theme results; older menu generations retain their copies.
Automatic retirement of old disk copies remains an explicit #215 gap.

Application menus add `--icon-scale`; normal defaults remain unchanged.
Directory menus opt in with `--icon-theme` and resolve only four type roles in
one deadline-bound self-worker. Role metadata cannot carry commands/actions.
Failure emits a diagnostic and leaves labeled menus. The configured submenu
callback remains authoritative and must repeat desired icon flags. The old
`--icon-t` alias still means `--icon-title`; new theme selection does not make
that alias ambiguous. Manuals own user syntax and examples.

This checkpoint does not add a Form/browser picture protocol, font engine,
thumbnails, watcher, theme package or decoder. Lucide/Papirus and the final
bundled-asset decision remain open. #54 text shaping/IME is unnecessary for these
existing native labels/images. PR262's source-bound reference subset supplies
the #209 handoff used here; missing whole-session/release metrics are not
silently accepted or made a blanket veto on unrelated shared contracts.

[Verification and cost limits](../verification/VERIFICATION-2026-10-26-NAMED-MENU-ICONS.md)
record the Linux focused checkpoint, theme captures, source review and explicit
remaining #215/profile acceptance. The directory executable grows by 2,235,073
bytes; complete release cost disposition remains with #29/#226.
