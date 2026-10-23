# Shared collection grid and range selection

Status: implemented; focused Linux/FreeBSD and scoped sanitizer checks pass.
Owners: FILE-VIEW-01 / FILE-ACTION-01; shared clipboard metadata prerequisites.

This coherent batch adds a native icon-grid over the existing immutable
collection, bounded additive range selection in list/grid, and browser
presentation switching/initial selection. No existing configuration requires a
rewrite. ListGrid is additive and ListItem retains its old syntax with an
optional literal file/directory/link/other type. The browser adds --view and F7;
its internal version-2 bitmap packet remains unchanged.

One X child draws at most 32 visual rows by 8 columns. Scrolling, keyboard
motion and hit testing use visual rows; bitmap indices always refer to ordered
entries. A short final row has no targets in its unused cells. Grid geometry is
checked before publication and labels are clipped per cell. Native monochrome
type marks scale with ButtonFont; no paths, per-entry pixmaps, external decoder,
thumbnail, toolkit or icon-theme dependency is introduced. Actual pixels remain
the native geometry contract; dynamic per-monitor scale acceptance stays open.

Shift-navigation/Shift-click adds the anchor-to-focus range to the current
selection. It is an additive operation: shrinking a range does not clear earlier
selections. Over-budget ranges refuse the whole focus/selection change. Ordinary
focus movement establishes a new anchor. The consumer's existing clear/select
controls remain authoritative across filtered-out entries and aggregate caps.

List/grid switching submits the current packet, validates it against the exact
view generation, then redraws the same model and stable focused/selected IDs.
Initial --view defaults to list, lasts only for this helper instance and writes
no preference. Directory refresh, literal action revalidation and private worker
ownership remain with the existing adapter. Clipboard contents must never enter
these ordinary labels, Go model, callbacks or Echo channels.

Focused Linux and early FreeBSD checks cover policy/native compiler and actual-X
consumer behavior, with scoped sanitizers for native state/render ownership.
Capture new grid/range/large-collection screenshots from that integration run.
The cycle advances to 3/3 only on merge; accumulated six-target closure is due
before the next implementation batch. Full owner/milestone/release and physical/
assistive/native-CPU qualification remain separate. Incremental providers,
watching/file mutations, protected clipboard content/private delivery and STORE-01
persistence remain open. No SQLite dependency is required by this batch.
