# EWMH atom dispatch, names and startup publication

STATE-01 #37; 2026-10-19. Implemented and focused Linux checks passed;
one combined six-target checkpoint is pending. Base is merged PR #111,
`d7e6fd3aa56ad63e2e5ab3c7aa2943170122051d`.

Three related improvements share this checkpoint:

1. Numeric dispatch uses a lower bound instead of bsearch followed by a linear
   duplicate rewind. First original entry wins, including `None` and maximum IDs.
2. Scoped/ALL name indexes borrow original static strings/records and sort by
   name/original priority. The name path no longer scans nine table descriptors
   or assumes the window-type table is sorted. Its notification entry is now
   reachable in both scoped/ALL lookup (the old bsearch misses both). Reentrant
   interning uses ordered original records until all indexes are published.
3. Startup passes its operation-owned intern vector directly to `_NET_SUPPORTED`,
   removing one allocation/free and one 63-entry copy pass. Duplicate values,
   `None`, type/format and publication ordering remain exact.

The name index adds 3456 static bytes on the measured LP64 ABI. Over 64 selected
ALL queries, comparisons change from 1344 to 448 (`_NET_WORKAREA`), 1024 to 448
(`_NET_WM_PID`) and 1856 to 448 (missing). The early-table `_NET_CURRENT_DESKTOP`
control changes from 192 to 448; there is no universal speedup claim. Synthetic
all-equal-ID rows use 128, 2048 and 32768 entries and retain separately labelled
algorithmic comparison bounds and measured legacy rewind visits/timings; those
sizes do not describe the core's 63 records. Startup's reference allocation/copy
is executed separately and compared with the actual server property.

The 66000-query numeric differential is retained. Name policy adds 1032 queries
across empty/byte/miss/duplicate vectors of length zero through 128. Actual-core
checks cover every declared name in every scope, invalid/empty/missing/byte keys,
first-table duplicates, reentrant interning, normal/missing-intern generations,
CARDINAL/UTF8 read/write/delete and exact server `_NET_SUPPORTED` contents.
Existing client/stack ordering, destruction, work-area and DRAW barriers remain.

Focused attempts are retained under
`builddir/codex-2026-10-19/ewmh-atom-lookup/`. Early reference counters failed
because compiler purity/hoisting discarded or moved calls outside the measured
window. Explicit counted reference calls fix the fixture; production behavior
was unchanged by those fixture corrections. Server round trips and the final
reference copy pass in `early-ready`. No full checkpoint was run per story.

The known OpenBSD AMD64 `image-svg-isolation-xvfb` startup refusal is retained
under IMAGE-01, with its required omitted entry and fresh-install/staged-geometry
gap. Its original source is `97c728358660675353de35f866c833c926b4f203`, archive
`c5a8dc9783c78e30a16ae790397e304148bf7388318081b77e7ac028e7b0451a`.
No retry for green or VESA cause investigation is included. Native-CPU sanitizer,
physical hardware/toolkit/compositor and release qualification remain separate.
