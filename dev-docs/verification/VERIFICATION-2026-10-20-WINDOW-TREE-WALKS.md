# Window-tree walks — DATA-01 / CLEAN-01

Implementation and six focused checks pass after merged PR #130; full combined
qualification is pending. Three active operations share a 4096-query per-operation
ancestor budget and stable-root validation. Top-ancestor lookup preserves None
for a top-level input. Core pointer lookup preserves context checks before parent
queries. IconMan frame translation publishes offsets only after a complete
representable walk; failure retains the original window and offsets. Query-owned
child replies are released on every exit. No query barrier, configuration syntax
or server-error policy changes. A query budget cannot preempt one blocking Xlib
request. The recursive uncalled client lookup stays outside this batch.

Focused Linux ARM64 checks: window-tree, window-tree-xvfb, window-children,
window-update-xvfb, iconman-transport-xvfb and buttons-transport-xvfb all pass.
Native-kernel guest execution on this macOS host is separate from native CPU,
physical hardware and release qualification. Full required OpenBSD AMD64 gaps
from the predecessor remain owned and are not retried for a green matrix.
