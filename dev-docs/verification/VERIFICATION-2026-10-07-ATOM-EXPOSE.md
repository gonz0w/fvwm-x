# XIO-01 / DRAW-01 / INPUT-01 / STANDARDS-01: atoms, redraws and icon identity

Status: locally qualified from the exact frozen source. Focused checks and the
combined Linux/BSD compiler/install/default-server/optional/sanitizer gates pass.
Native Linux release CI and physical hardware checks remain separate.

## Source and behavior

The checkpoint is `builddir/codex-2026-10-07/atom-expose/source-atom-expose.tgz`:
1291 source files, SHA-256
`02eea2e1bf8c26acf2e539c60eca7d5da3a3af7b9d66a837d49b0fc8a0a4024b`.
It extends the accepted 1282-file JSON checkpoint through 19 owned paths. Core
changes are applied as owning function blocks, retaining unrelated shared edits
outside the snapshot. The source manifest and file modes identify its scope.

Startup atom batching is described in the [focused atom report](VERIFICATION-2026-10-07-ATOM-BATCH.md).
Three independent tables (28/8/64 names) preserve names, destinations, EWMH
ordering, manager selection ownership and partial-failure behavior. The unchanged
baseline makes 100 single-name initialization calls; the new core makes three
batches, retaining the separate colorset call. This measures API calls and
correctness, not desktop latency.

The user approved the [bounded redraw contract](../decisions/EXPOSE-SERVICE-PROPOSAL.md)
on 2026-10-07. The actual unchanged-core baseline invalidates 1000 queued
exposures without calling its redraw handler. Its complete evidence is retained
under `builddir/codex-2026-10-07/expose-drain/`.

`FCollectQueuedExposes` inspects at most 32 records from a QueuedAlready snapshot.
It removes selected exposures and restores unrelated records in FIFO order,
without changing cached events or input timestamps. It runs no callbacks or
socket-input fetch. Core servicing dispatches the selected records through
normal current-context lookup after queue restoration, outside Xlib traversal.
Matching-window rectangle merging uses the same finite snapshot. Nested merges
have independent bounds; this is not an aggregate 32-event or elapsed-time
preemption guarantee. Remaining damage continues through normal dispatch.

The pixel fixture also exposed first-icon WM_STATE publication with a zero icon
XID, although a title window had been created. Iconify now republishes the
current title XID when creation/replacement changes it, retaining existing
transient publication order. [ICCCM 2.0's WM_STATE clause](https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html)
expects the icon field to identify the WM's icon window, or None if none exists.
This is a scoped identity correction, not a claim of complete ICCCM conformance.

## Focused evidence

- Queue policy: a 1000-record backlog stops after 32 inspected records; matching
  windows, foreign FIFO, Configure/Destroy prefixes, invalid records, appended
  records beyond the original snapshot, current/previous event caches and input
  timestamps are checked with bounded controlled queue operations.
- Actual core: rectangle union and foreign-window order pass. A 2000-record queue
  dispatches 32 outer redraws and leaves 944 exposures; subsequent normal
  dispatch covers every original damage point. Tear-off routing, nested callback
  cache restoration and loss of X context during a redraw pass. The fixture's
  intercepted drawing is a routing/damage check, not a pixel claim.
- Actual pixels: real icon text returns to the same pixel values after 80 clear
  requests followed by Read, Raise or Refresh. A real tear-off menu restores its
  pixels after clear/Refresh and closes normally. First and reused icon XIDs
  match WM_STATE. Command completion and diagnostics are checked.
- Existing atom unit and actual startup normal/partial-failure cases pass in the
  same development source. Five combined focused tests pass.
- Scoped local ASan/UBSan: eight policy tests retain leak checking; thirteen
  actual-core tests disable it for fixture/process lifetime scopes. All 21 pass.
  These local emulated runs do not satisfy native Linux release sanitizer CI.

Initial fixture/setup failures are retained: ambiguous Meson target selection,
an unchanged archive timestamp preventing test registration, a first-icon lookup
that assumed already-correct WM_STATE, a popup lookup that assumed override-
redirect instead of FVWM's save-under attribute, and a tear-off title/name
mismatch. The early icon-style diagnosis was not the root cause; both style
variants exposed the same zero-XID publication. The production redraw correction
passed its routing/queue regressions throughout these fixture repairs.

## Combined qualification

All eight full compiler profiles pass: Linux GCC/Clang on AMD64/ARM64 has 143
registered entries per profile; FreeBSD/OpenBSD AMD64/ARM64 has 131 per profile.
This is 1096 standard test entries. All 120 installed executable/module identities
match tested code after verified install RPATH transformations.

All four BSD default servers pass 13 driver invocations each (52 total): existing
Move/Resize/Scroll/menu servicing, stacking, module output/sync/startup, client
identity and JSON boundaries, plus atom mapping/selection, bounded expose routing
and real redraw pixels. Zero-monitor wsfb and the AMD64 framebuffer drivers use
their existing default-server profiles.

Optional XRender-disabled and SVG-disabled profiles pass 142 and 122 registered
entries. The 21 scoped ASan/UBSan checks pass with eight pure leak-enabled and
thirteen actual-core leak-disabled scopes. No frozen production-source change
was required during these combined gates.

All seven collected evidence archives were read through EOF. Every target's
1291 source hashes and file modes match the frozen archive; recorded test names
match Meson's registered tests, and installed bytes match verified RPATH changes.
`qualification-summary.json` records these checks and archive hashes. The
focused archive's 17 executable-source hashes also match the frozen source.
OpenBSD's preceding accepted JSON evidence was copied to the existing owned
home-partition evidence directory, verified by hash/mode, then linked at its old
path to preserve evidence while making room. Relocation records are collected.

Native Linux release CI, physical mixed-DPI hardware, independent toolkit and
compositor interoperability remain separate unrun gates. Prior checkpoints are
historical; their results are not substitutes for this source.

## Subsequent stacking/query work

The [Layer cycle decision](../decisions/TRANSIENT-PARENT-CYCLE-PROPOSAL.md) and
[other redirection decision](../decisions/STACK-REDIRECTION-CYCLE-PROPOSAL.md)
were approved on 2026-10-07. Those guards, hint validation and highest-override
query work are activated in the subsequent stacking/query source. Production
stacking in this frozen atom/expose snapshot remains unchanged; its qualification
does not cover later work.
