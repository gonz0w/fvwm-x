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
Export lifetime follows the Y2 durable-artifact contract below; automatic
retirement of old disk copies is not provided.

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

## A5 explicit refresh and artifact ownership — 2026-11-02

An explicit native tray Refresh or Reload selects a fresh source acquisition
when the last pending configuration snapshot finishes, including an unchanged
icon tuple. Requests coalesce in one pending bit; old results and new work are
refused while it is pending. The existing nonwrapping acquisition fence and
cell/menu invalidation run before later result admission. Automatic snapshots
do not refresh unchanged sources. Refresh preserves helper-retry policy; Reload
remains the existing recovery operation. Exhaustion and retirement failure stay
blocking. There is no new watcher, process, package or renderer.

Source/finder/request caches remain acquisition-local. Regenerating an exported
application/directory menu refreshes source lookup; changed bytes produce new
content identities, leaving preceding exported paths intact. Worker exit is not
reference release: retained core menu pictures can load again at another scale,
and exported files can be read later or copied. No age-, mtime- or generation-only
sweep is selected. A5 left #215 export lifetime acceptance open pending an owned acknowledged
menu mode or an explicit exported-artifact lifetime contract. Y2 selects the
latter below; the earlier checkpoint did not establish that choice.

Private consumers already have narrower ownership: tray batch files survive
through result consumption and retire afterwards; pinboard replacements retire
obsolete generations at acknowledged native boundaries, retaining files when
shutdown cannot confirm release. Preserve these mechanisms rather than adding a
shared collector. The A5 source/compile checkpoint queues its new generation,
held-batch refresh and retention cases for A5–A8 combined runtime acceptance.


## Y2 exported-artifact lifetime contract — 2026-11-07

Select A5's explicit exported-artifact contract for application/directory menus.
Generated content-addressed images are durable outputs referenced by exported
menu commands/files, not temporary worker files. Publication transfers their
retention obligation to the owner of those exports. Regeneration can create new
identities but cannot release old references; worker exit, elapsed age and source
replacement are not permission to remove them. Existing absolute-path output
and FVWM configuration syntax remain unchanged.

Keep each image while any live menu, saved/copied menu file or other exported
consumer can load its path. Manual retirement requires retiring every such
reference first, then removing only images known to belong to that retired set.
The application cannot infer when copied exports disappear. It does not sweep
shared directories or promise automatic garbage collection. Per-acquisition
source, request and output limits remain enforced; there is no cumulative disk
quota across arbitrarily many retained exports. This is distinct from bounded
resident memory and must not be described as a total disk bound.

Tray and pinboard retain their existing acknowledgement-based private-generation
retirement. This contract resolves A5's exported-lifetime choice without a new
collector, protocol, store or mandatory package. The combined #214/#215
checkpoint checks changed source identities and retained earlier export bytes;
its passing result is required before treating the new checkpoint as qualified.
