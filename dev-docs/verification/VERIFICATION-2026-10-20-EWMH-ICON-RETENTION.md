# Donated icon retention — 2026-10-20

Owners: DATA-01, IMAGE-01 and CLEAN-01. Base: merged PR #129,
`6515a44734ebd88038890cd51e197ef1c40c3366`.
Implemented; focused and combined qualification pending.

Three related operations validate complete first-exact retained records before
arithmetic/indexing, bound donated image/combined property sizes before X reads
or allocation, and publish an owned reply's retained span synchronously during
partial deletion instead of allocating/copying it again. Existing configuration,
first-match precedence, ordinary pixels/metadata, donation publication and X/error
barriers remain. Refused replacements keep the original list/byte count available
to the other donation owner; no new cache or property coalescing is introduced.

The original-core normal baseline passes. The first probe forgot the existing
property-acquisition allocation; the second probe compared LP64 Xlib format-32
pixels without narrowing to CARD32. Both probe failures are retained, and the
corrected original-core run passes with three allocations per partial deletion.
Neither failure changes production. The candidate should require two, removing
only the intermediate retained-record copy. Runtime pixels/refusals/ownership,
pure record/size boundaries and scoped sanitizers remain to be qualified.

One combined six-target checkpoint follows the coherent batch. Preserve the
three known required OpenBSD AMD64 omissions and its broader full-install/two
staged-geometry gap without retries. VESA #93 stays deferred. Native-CPU,
physical hardware/toolkit/compositor and release acceptance remain separate.
The interactive review VM/VNC remains closed. Raw evidence is under
`builddir/codex-2026-10-20/ewmh-icon-retention`.
