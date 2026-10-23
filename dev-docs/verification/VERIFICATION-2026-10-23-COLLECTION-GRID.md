# Collection grid and range selection — 2026-10-23

Status: **focused checks passed; ready for review; shared integration pending**.
Owners: FILE-VIEW-01 / FILE-ACTION-01. Base: merged PR168
`e4ef3f79d3643b910b5014b15c6d1e9ec0f2d15f`.

## Delivered batch

- Reusable ListGrid over the same collection indices: visual-row scroll/focus,
  bounded viewport drawing, coordinate/index hit tests and short-tail handling.
- Native file/directory/link/other marks, scaled from the current ButtonFont,
  with per-cell clipping and no external images, decoders or per-entry pixmaps.
- Additive Shift-key/Shift-click ranges in list/grid. Over-budget ranges refuse
  the complete focus/selection update rather than publishing a partial range.
- Browser --view list|grid and F7 switching retain stable focused/selected IDs,
  immutable ordering/filtering and the existing bounded bitmap transport.

Existing ListItem syntax remains valid. The new type argument and ListGrid are
additive configuration. Grid geometry and owning-line height are checked before
layout. [Contract](../decisions/COLLECTION-GRID-2026-10-23.md),
[receipt](VERIFICATION-2026-10-23-COLLECTION-GRID.json).
No clipboard payload, SQLite dependency, provider API or file mutation is added.

## Verification and evidence composition

The policy candidate has **2251 files**, archive
`c69d63d0107b1569d0d43a78ee21c3b25e8a15ca3bb52f19474764155f1bb3f5`. Linux ARM64 GCC and FreeBSD ARM64 Clang each pass
seven selected registered native entries: collection/input/command policy and
four legacy Form transport/input/button/field drivers. Five Go packages / 36
top-level tests pass on both targets and in Linux's separate race run (108 top
case executions). Both vet/build checks pass. Linux's scoped sanitizer profile
passes three leak-enabled ASan/UBSan policy entries and its native consumer.

The first Linux native-X and sanitized runs pass. FreeBSD's native-X observer
fails while XGetGeometry borrows a retiring Form child. Its original failed
archive is retained. Only the probe changes: exact borrowed-resource/request
tracking permits a retry for confirmed BadWindow/GetGeometry and discards the
incomplete inventory. All other X errors retain their failure path. No product
code or binary changes follow that correction.

The final observer candidate, also 2251 files, is
`51effbbef99e9e2f4eef78e9d2e747b480f21391c3623cd73a89be7d086d3711`. Full source hash/mode guards before/after each gate,
an exact one-fixture difference and unchanged product binary hashes compose the
passing policy/build results with three corrected native-X driver passes.
Only those drivers rerun; passing compilation, policy, race and vet are retained.
The new runs take **13.938 seconds Linux / 6.427 seconds FreeBSD**. Original
checkpoint commands take 32.248 / 12.785 seconds respectively, including the
failed FreeBSD observer; these are gate durations, not total development time.

The final drivers cover empty and 4096-entry collections, grid icons,
keyboard/mouse ranges, list/grid focus and selection identity, refusal of a
4096-item range, offscreen/filtered selections, properties, literal byte names,
stale callbacks, blocked worker cancellation/reap and core responsiveness.
Eleven screenshots per final driver are captured. Grid overview, row 4096 and
range/list screenshots are available in the preserved host set; new grid views
were inspected. This is actual native X output.

Both generated manuals render with native FreeBSD mandoc after exact hash
comparison. Portable verification passes **177 tests / 14 unavailable-tool
skips**. Repository and whitespace checks pass.

All four archives validate compressed EOF/CRC, complete inventories, every
payload hash/mode, selected test sets and explicit success/failure dispositions:

| Target / capture | Files | SHA-256 |
| --- | ---: | --- |
| Linux policy / initial X | 59 | `b3eee99cf39a39d66abe76cfc5f7baf674d7e6c1b3cd3d24f8bfcdd2fa0abeb1` |
| Linux corrected observer | 40 | `06dc6f745efbb57feb11c514c5f37b2c45a2e117bab3a644021b0a4972b4bb25` |
| FreeBSD policy / failed observer | 26 | `ef6a3ef69f1be0aacbb367743c414e1d326f514101a7a1f2ec032b91e5204296` |
| FreeBSD corrected observer | 23 | `0ffb6d2d0a8c371178d22565ce933890c12499bc60a76b97316132e720299261` |

Canonical evidence: `/Users/cam/DEV/fvwm3/builddir/codex-2026-10-23/collection-grid`. Completed guest capture/source-archive duplicates can
retire after exact host verification; source, builds, qualified caches and VM
disks remain preserved.

## Remaining qualification and scope

PR166 remains the integrated reference. PR167/168 are **2/3 merged**; this
checkpoint becomes **3/3 on merge**. Close the accumulated affected six-target
cycle before starting a fourth implementation batch. Full compiler-feature/
install/default-X and owner/milestone/release acceptance stay at their required
boundary. Native-CPU sanitizers, physical hardware, toolkit/compositor and
assistive checks remain separate; the native ASan UI's inherited fontconfig
exit-cleanup exclusion remains explicit.

Performance reference PR164 is **4/5 merged**, **5/5 on this merge**. The next
continuation owes the periodic comparison using existing builds. No tuning or
full matrix is repeated solely because this PR is created.

Incremental providers, watching/mutations, complete file-browser acceptance and
protected clipboard ownership/private delivery/persistence remain open. Type
marks are simple native monochrome geometry; thumbnails and icon-theme loading
are later capabilities. Range selection is additive, and UI counts reflect the
latest submitted state. No broader clipboard protection or release claim follows
from a reusable metadata grid.
