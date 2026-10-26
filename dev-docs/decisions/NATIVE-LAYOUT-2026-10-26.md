# Responsive native panel and collection layout — 2026-10-26

Owner: Cam. LAYOUT-01 #212, FILE-VIEW-01 #69 and THEME-01 #75 native adoption.
This is a bounded R1 baseline using existing text measurement and UI modules.

Deliver one shared checked row/grid policy and two concrete consumers: the
shipped SlateBar and the optional native file browser. Keep the existing
configured grid/list geometry as the default for personal configurations.
The simpler fixed geometry remains supported; these two consumers need an
explicit adaptive mode because percentage cells clip enlarged captions and
fixed collection columns cannot use a resized viewport.

`Layout content` opts a flat root Buttons row into measured minimum widths.
Measure normal/active/pressed captions and icons through the existing font/image
owners, include padding and frames, and distribute surplus by configured width
spans. If the minima cannot fit, proportionally compress positive rectangles;
keep actions reachable and clip content. Do not promise unclipped text at every
width. Draw, hit testing and keyboard focus share the prepared rectangles.
Publish an accepted dynamic group before one reflow; rejected groups preserve
both data and geometry. Unsupported nested/swallowed/panel/fixed-position shapes
retain the configured legacy layout with a diagnostic.

Keep the configured outer height authoritative. SlateBar's geometry and desktop
strut use the same `slate_height`; users selecting taller fonts/icons must choose
a sufficient height. This checkpoint does not add a second geometry authority,
automatically change arbitrary clients, or reflow personal themes by default.

`ListViewport minimum_cell_units max_columns` opts exactly one dedicated-line
Form collection into resize geometry. Use existing font units to bound cell
width, at most eight columns and 32 visible rows, and preserve domain generation,
selection, focus and the surviving scroll anchor. The same prepared native
header/state supplies paint, pointer, keyboard and scrollbar geometry. Fixed
controls remain available around the collection. The browser adopts this option
for list/grid views; existing collection transports and identity owners remain.

Use one reused native Linux checkpoint with policy tests, affected builds,
scoped sanitizers, browser race, existing Buttons atomic/keyboard and Form/browser
regressions, and actual UI resize/refresh/input checks in three configurations.
Retain screenshots and plain scoped latency/CPU/RSS/idle observations. File/ELF
sizes are not resident memory and short observations do not establish a speed
improvement. No periodic comparison is due (PR264 reference, 2/5 after PR266,
3/5 after this runtime implementation merges).

Clay evaluation, complete text/shaping/IME, arbitrary recursive layout, broader
theme/accessibility and support/install/release acceptance remain separate finite
criteria. No new dependency, resident, toolkit, module packet format or preference
store is added. Parked Backer/pathname work remains excluded. This advances the
three existing owners without claiming their whole acceptance or closure.
