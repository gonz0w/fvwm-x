# Deferred colorset, decoration, mini-icon and menu owners — 2026-10-06

Owner: IMAGE-01 / INPUT-01. Status: implemented with focused Linux ARM64
preparation coverage. Production startup still does not enable the optional
image loader. File icons, cursor/root-tile owners, derived-resource refusal
handling and combined activation/qualification remain open.

The user approved asynchronous compressed-XPM and unusual-format completion on
2026-10-06. See the [approved contract](../decisions/IMAGE-ASYNC-FORMAT-PROPOSAL.md).
The timing decision is cleared; owner and qualification gates remain required.

## Implemented ownership

- Each pending picture can bind directly to its typed owner. Completion avoids
  global owner searches. A binding survives temporary commit-batch references;
  its finalizer releases context only with the last picture reference. Success
  and refusal settle once, deferral does not settle, and cancellation removes
  interest. A callback may release its owner's picture reference.
- Colorsets retain separate pending background/shape leases. Old published
  resources remain usable until successful preparation. Completion reuses typed
  tint, average-color and background processing with final command attributes,
  without dispatching a reconstructed command. Explicit gradient/plain/shape
  overrides cancel pending interest. Delayed module cleanup retains an immutable
  whole picture, including mask, alpha and allocated colors, rather than
  mutating a shared cache entry.
- Decoration parsing prepares temporary faces and transfers leases to their
  final addresses. Destruction invalidates borrowed face/decor addresses before
  freeing them. MultiPixmap button expansion acquires independent leases. Live
  faces retain variants across the existing thirteen supported scale factors;
  each window resolves its current factor when drawing. Repeated paints retain
  pending variants, and replacements inherit immutable old scaled pixels.
- Mini-icon callbacks retain a client XID, then resolve the live context and
  validate window, lease and scale identity at commit. Ready pixels trigger the
  native module packet, EWMH donation and decoration repaint. Pending/absent
  pictures cannot enter donation. The sanitizer exposed an existing signed
  alpha shift in donation; conversion to CARD32 before shifting repairs it.
- Menu pictures bind to their containing static root or style. Mapped roots
  defer mutation; completion marks layout for the next popup. Item continuation
  transfers and temporary MenuStyle transfers relocate their bindings. Teardown
  invalidates slots before release. Menu/tear-off cloning retains immutable
  backing snapshots. SidePic replacement preserves old backing and distinguishes
  a style-derived variant from an explicit root picture. Same-key popup cache
  lookup releases its extra reference. Absent pictures stay out of size/draw
  operations.

The test-only fixture links unchanged production core objects and wraps
CaptureAllWindows to enable the dormant adapter. Unbound owners discard results.
This fixture does not enable the loader in the installed WM.

## Focused evidence

Frozen candidate: `builddir/codex-2026-10-06/svg-owners/source-owners-final.tgz`.
SHA-256: `985c964a00f821241cbd9a08da9c6cf98451c97426a6a1a6d8beabac2db0e135`.
Both normal and sanitizer trees match all **1238 source hashes and modes**.
The archive overlays owned files onto the accepted core-adapter candidate and
filters Meson additions; concurrent desktop/tooling work is excluded.

Ubuntu Linux ARM64, Clang 18.1.3, patched private libpng 1.6.59:

| Scope | Result |
| --- | --- |
| Normal focused transport, conversion, adapter, owner and actual-core fixtures | 11/11 pass |
| Scoped local ASan/UBSan policy/direct owner fixtures, leak checking enabled | 5/5 pass |
| Scoped local ASan/UBSan actual-core fixtures, leak checking disabled | 6/6 pass |

The direct picture/adapter/decor-menu probes cover both 24-bit and 8-bit Xvfb.
Actual-core pixel/packet checks cover decoration, mini-icons and menus at 1×
and 2×. Colorset packets cover first-load absence, retained old XIDs/types,
replacement, supersession, shape validity, final tint/dither, overrides and
cleanup. Decoration checks use both a finite synthetic worker and the real SVG
decoder. Mini-icon checks observe native packets and donated ARGB data, including
failed replacement and client destruction. Menus retain mapped pixels/geometry,
receive completion on a later popup, retain failed SidePic backing, clear it
explicitly and cancel destroyed roots/items. A persistent tear-off keeps its
snapshot while an independent menu image completes.

Worker fixtures are unsanitized: the real worker's finite address-space policy
cannot accommodate ASan's reserved address space. The synthetic producer and
real worker binaries come from the matching normal build. Actual core and MFL
remain instrumented. This is scoped local evidence; native release sanitizer CI
remains unrun.

`results.json`, final profile metadata, native binary hashes, Meson logs, X logs,
packet/pixel evidence and failed checks are retained in the same batch directory.
Final normal and sanitizer evidence archives record ten relevant binary hashes
per profile. These are build identities, not staged-install qualification.

## Failures retained and repaired

The filtered graph initially referenced a fixture before its definition; placing
its addition after the baseline definitions corrected the graph. Tests then
exposed observer mistakes: waiting for policy before X acknowledgement,
querying an explicitly cleared mask, treating immediate tint repaint as decode
completion, and expecting the parser's released intermediate Buttons slot.
The first fatal X observer exit skipped cleanup; its verified task-owned
processes were stopped and its artifacts retained. Later observers record X
errors and clean up in finally blocks.

The popup observer initially required an override-redirect window and too much
height, while FVWM uses save-under and can show a single row. It also compared
text highlighting before initial paint. The corrected check waits for paint and
compares image pixels plus dimensions. An explicit tear-off binding and pointer
placement avoid inadvertently entering another modal menu interaction.

The sanitizer reproduced the signed EWMH alpha shift and initially could not
execute a missing real-worker fixture. The arithmetic repair and matching normal
worker fixture pass the final 5+6 scoped checks. Normal checks were rerun after
the source repair and pass 11/11 against the final archive.

## Remaining gates

File-icon leases, cursor/root-tile completion and all-owner activation remain
pending. Derived colorset shape/background X allocation refusal still needs its
own prepare/acknowledgement review; successful decoder conversion alone does not
qualify every downstream allocation. Existing colorset tint/average work is not
newly bounded by this checkpoint.

Run enabled-WM slow-image input/timer/interaction checks and then the combined
frozen-source Linux/BSD compiler, full-suite, install, default-server,
optional-feature and sanitizer gates once the owner batch is complete. This
report does not close IMAGE-01, claim installed asynchronous parity or record
new BSD coverage. Earlier platform evidence remains valid within its original
scope.
