# Native scrolling collection list — 2026-10-23

Status: **focused checks passed; ready for review; shared integration pending**.
Owners: FILE-VIEW-01 / FILE-ACTION-01; metadata prerequisites for the file manager
and clipboard. Base is merged PR167 (`360c04fde7f2c0b0b1b10c0dac77d829395d75cc`).

## Delivered batch

- Reusable native Form List/ListItem/ListFocus, one X child, bounded viewport
  drawing, 4096 labels / 256 selected rows, checked geometry and owned cleanup.
- Keyboard focus/navigation, Space/click selection, wheel/track scrolling and
  mixed input/list Tab traversal. Legacy forms keep the input/history ring.
- Compact whole-view bitmap/focus callbacks (514 raw / 686 encoded bytes), with
  atomic generation/length/padding/focus/selection admission in the model.
- Browser adoption, focused Enter, stable refresh/reveal focus and filtered-out
  selection preservation. Obsolete browser paging state/controls are removed.
  Large configuration uploads now await core acknowledgements every 32 commands.

[Contract](../decisions/COLLECTION-LIST-2026-10-23.md),
[receipt](VERIFICATION-2026-10-23-COLLECTION-LIST.json).
No secret clipboard content, database dependency, provider API or file mutation
is introduced. The broader owner criteria stay open.

## Final verification

The frozen candidate contains **2248 files**; archive SHA-256
`857064d1757ef619b29f3d82c8842792d7bf05c95615343f80fd733255b7de7d`. Both targets verify every candidate hash/mode before
and after execution. The receipt binds changed runtime/manual/fixture inputs;
subsequent changes are status/report documentation only.

Linux ARM64 GCC and FreeBSD ARM64 Clang each pass seven selected registered
entries: list/input/command policy and four legacy native Form transport/input/
button/field drivers. Five Go packages / 35 top-level tests pass on each target;
Linux repeats those five packages / 35 tests with race detection. Both vet/build
checks pass. Linux adds three leak-enabled scoped ASan/UBSan policy entries and
an actual-X run using the sanitized Form; its UI leak detection stays disabled
for the independently recorded existing fontconfig exit allocations.

Three actual-X browser drivers (Linux, FreeBSD and scoped sanitized Form) pass:
empty/4096-row lists, scrolled text focus return, arrows/page/home/end selection,
wheel viewport change with retained focus, offscreen/filtered selections,
properties, literal raw-byte activation, parent navigation, stale callbacks,
blocked FIFO cancellation/reap and core responsiveness. Eight screenshots per
driver are preserved; overview, row 4096, properties and input-return captures
were inspected. This is native X evidence, not a mockup.

Both generated manuals render with native FreeBSD mandoc after exact hash
comparison. Portable verification passes **177 tests / 14
unavailable-tool skips**; repository and diff checks pass. The final frozen
commands take **34.941 seconds Linux / 15.655 seconds FreeBSD**, excluding
transport, environment recovery, development and review time. No benchmark or
full matrix is repeated for PR creation.

Both archives validate compressed EOF/CRC, every file hash/mode, exact complete
capture inventories, selected test sets, Go JSON outcomes and result status:

| Target | Files | Canonical archive SHA-256 |
| --- | ---: | --- |
| Linux ARM64 | 53 | `1a167121003f6dde402e8b54810f080fda55d905b59b461faef9131f10689ea1` |
| FreeBSD ARM64 | 30 | `8136805d052b17ebb30c8ccc846c2795818cc1682a79ed8eeccb0467aaf26e28` |

Canonical host evidence: `/Users/cam/DEV/fvwm3/builddir/codex-2026-10-23/collection-list/review-fix`. Sources/builds/qualified SDK caches remain owned
and preserved. Verified completed guest capture/archive duplicates may retire
under the user's standing space-cleanup authorization.

## Corrections and limits

Initial incremental compilation caught the missing native font type include
and a legacy diagnostic symbol. The 4096-row probe reproduced an MFL queue
closure; pacing fixes the producer rather than enlarging the queue. A partial
FreeBSD source sync retained old Go files; the complete frozen sync corrected
it. The checkpoint script's unintended fresh cache exhausted Linux space; the
cache was preserved and verified on the host before duplicate removal, and the
qualified cache is now reused. Original logs remain in private evidence.

Review moved list-only bitmaps to separate allocations so other Form item
records do not grow, and reset a returning input's viewport with its cursor.
The final focus regression, relevant policy and sanitizer/native-X checks pass.

The shared cycle reference remains PR166: **1/3 merged**, **2/3 on this merge**.
Six-target/full compiler-feature/install/default-X and milestone/release
acceptance stay at their required boundary. Native-CPU sanitizers, physical
hardware, toolkit/compositor and assistive checks remain separate. Performance
reference PR164 is **3/5 merged**, **4/5 on merge**; no comparison is due. Grid,
incremental provider updates, complete browser/file mutations and protected
clipboard content/private delivery remain open. UI counts reflect the latest
submitted state; local row clicks do not round-trip solely to update a label.
