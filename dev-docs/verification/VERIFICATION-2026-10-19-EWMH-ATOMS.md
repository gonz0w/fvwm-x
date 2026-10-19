# EWMH atom dispatch, names and startup publication

STATE-01 #37; 2026-10-19. Combined local checkpoint passes in its executed scope; the known required
OpenBSD AMD64 SVG omission/install gap remains explicit. Base is merged PR #111,
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

## Frozen checkpoint and results

Three EWMH atom improvements pass one combined checkpoint: 1991/1991 executed standard and 437/437 optional entries across six targets/ten profiles, three scoped sanitizers, nine fresh stages, four authenticated default BSD displays and 18 geometry invocations/126 cases. The known required OpenBSD AMD64 SVG omission/install gap remains under IMAGE-01.

Signed author-DCO runtime: `844c5c7ee0c37e7cfc001beca17171f917d1acf0`.
Tree: `0e4f6e03eb88da8973fa33b8344faeb47002d93b`.
Archive SHA256: `a4ccc108d493e47066e07a573000c3d8df4c33117446fb8c0cb48852fcc80697`; all 1995 regular inputs
match that commit's exact Git bytes/modes. Pre/post hashes and capture manifests
validate every guest, executed script, log, fixture and installed identity.
Final upkeep changes only documentation/evidence; tested non-Markdown inputs
remain exact. [Machine receipt](VERIFICATION-2026-10-19-EWMH-ATOMS-COMBINED.json).

| Scope | Result |
| --- | --- |
| Standard executed / required | 1991/1991 executed; 1991/1992 including the retained required omission |
| Optional software/minimal | 437/437 |
| Scoped Linux ARM64 Clang ASan/UBSan | 3/3; pure policy leak-enabled, process-owning core/update leak-disabled |
| Fresh stage identities | 9 stages, 135 code and 88 catalogs |
| Authenticated default BSD X | 4 displays, 22 fixture invocations; each server closes cleanly and refuses unauthenticated clients |
| Staged geometry | 18 invocations / 126 cases |
| Complete actual-core EWMH drivers | 15 drivers / 30 normal and missing-intern modes |
| Portable repository checks | 166 passes, eleven explicit native-Meson skips |
| Compiler diagnostic review | 11 logs, zero affected-file/changed-line diagnostics |

Each EWMH mode checks 693 declared-name/scope pairs plus empty/missing/byte and
invalid-scope guards, all three withdrawn-index interning generations, cardinal/
UTF8 round trips, four selected name-cost rows, three synthetic duplicate rows
and three exact startup property/reference snapshots. The 3456-byte name-index
cost and early-table comparison increase remain intentional, measured tradeoffs.
Counters cover direct owning wrapped calls; internal libc/Xlib/qsort allocation
is outside this measurement. Existing style/update/core/real-client regression
coverage stays in the combined receipt.

One matrix serves all three stories. Six existing owner-controlled guests run
at most six independent jobs (Ninja two jobs, Meson tests serial), selected after
checking 18 logical CPUs / 128 GiB and current workload. Warm builds are reused;
no guest restart, package refresh or graphics change occurs. Functional test
deadlines are unchanged. No full-suite retry or repeat for report/PR creation.
OpenBSD AMD64's `/home` filesystem filled while saving the final default-X
receipt and collecting evidence. All 242 executed standard entries and all four
original display fixtures had passed. A byte/mode-verified copy preserves the
failed capture and original child receipts; only this run's artifact directory
moves to the spacious `/usr/obj` filesystem through its original path. One
authenticated default-display group is repeated on exact unchanged binaries to
recover complete capture metadata. Builds and the full standard suite are not
repeated. The primary counts above describe the complete accepted control; the
extra original four fixture invocations (including 29 real-client cases) remain
separately retained. The original controller exit is 1 and the capture continuation
completes separately. No VM disk, package, hardware or other owner is changed.

The initial `./build verify` command was rejected as an unsupported wrapper
argument; the documented `make verify` then passes once. Focused counter failures
and final corrections remain recorded in their original archives.

The omitted OpenBSD AMD64 SVG entry is not an executed pass. Its fresh install
and two staged geometry invocations are unperformed; fresh built EWMH, update,
style and real-client checks still pass on the authenticated Xenocara display.
No SVG root-cause or VESA/driver claim follows from this checkpoint. Broader
STATE-01 burst/coalescing requires separately measured invalidators/observers and
DRAW barriers; no persistent managed-window cache or generic write coalescing is
introduced. Configuration commands, arguments, bindings and themes remain usable.

The first archive of the relocated artifact root contained its symlink instead
of its contents (272 bytes). Packaging is corrected by resolving that root;
all captured member hashes and frozen source bytes/modes are checked before
repacking. The invalid archive remains retained, with no further test/build
repeat. The final OpenBSD archive is 8493175 bytes, SHA256
`0f557079c89ab37eefdd0c931d4073e3ee72cf0ee503d1a205ee0b96d045cd36`.
The guest's `/home` remains full; check capacity and relocate only owner-controlled
source/build storage or verified duplicates before the next write batch.
