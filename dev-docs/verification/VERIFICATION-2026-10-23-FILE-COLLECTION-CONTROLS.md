# File collection controls — 2026-10-23

Status: **focused checks passed; integration pending**. Owners: FILE-VIEW-01 /
FILE-ACTION-01. Base: merged PR165, `2eaaf10092ab0958f57bbd22bf900669c031b6d1`.
[Source/command receipt](VERIFICATION-2026-10-23-FILE-COLLECTION-CONTROLS.json) ·
[Contract](../decisions/FILE-COLLECTION-CONTROLS-2026-10-23.md).

## Delivered checkpoint

- Bounded literal-label filtering, visible/total/page counts and empty-result
  recovery preserve domain IDs and selections excluded by the filter. A changed
  projection advances collection generation; old native controls remain refused.
- Loaded observations sort by name/kind/size/modified, ascending or descending,
  with directories first and deterministic raw-name ties. No rescan runs.
  Refresh retains the first surviving visible page anchor.
- Select page and Clear selection span the existing bounded selection model;
  hidden/off-page selections stay explicit and bulk admission is atomic.
- Properties exposes one selected item's last observed name/kind/mode/size/UTC
  modification time, including symlinks, without I/O or link traversal. The panel
  distinguishes this observation from the worker's pre-dispatch validation.

The optional Go helper, existing Form/MFL connection and file-browser manual
remain the entry points. No C/header, module packet, build/install/discovery,
settings/default binding or server-dependent rendering contract changed. The
full scrollable/grid browser and structured provider/handoff remain open.
Clipboard can reuse metadata only; no secret payload enters this ordinary
search/Form/Echo channel.

## Verification and retained boundaries

The owner-controlled Linux ARM64 guest uses patched Go 1.27.2 and the existing
Go-enabled incremental profile. Two registered entries pass: file-browser-policy
and command-client. The selected five Go packages/26 top tests pass in the
ordinary registered invocation, a JSON evidence invocation and the scoped race
invocation; JSON policy plus race account for 52 test executions. Vet passes.
These counts are executions of the same cases, not distinct test inventories.

The first frozen run passes compile, both registered entries, JSON policy,
race and vet. Its UI run fails because a random temporary path contains an
underscore: XKeysymToKeycode returns the unshifted keycode, so the old fixture
enters a hyphen. The error screenshot shows a missing path rather than blocked
work. The continuation applies Shift and deliberately names the FIFO
`blocked_fifo`, asserting the exact callback bytes before testing cancellation.
Only the native fixture differs between the two 2232-file source freezes; all
product/build/manual/policy inputs remain identical. The continuation's
incremental compile is a no-op and its complete native-X driver passes in
2.553 seconds. Original passing test outcomes are retained without rerunning
all checks. The source-bound receipt includes both archives and the correction.

The actual-X run exercises native filtering, hidden selection feedback, bulk
selection, properties, sorting/direction, no-match recovery, page selection,
folder navigation, literal unusual filename arguments, stale controls, blocked
worker cancellation/reaping/core responsiveness and normal close. Seven
screenshots are hash-verified; four new/overview screens are visually inspected.
The unchanged qualified core/Form/MFL binaries are hash-bound to the prior
checkpoint. This Go-only batch repeats no C sanitizer or blanket platform gate.

Earlier development probes and their failures are preserved: the added selection
row changed an old navigation locator; field/sort locators initially used the
wrong coordinate/row; synthetic presses needed a realistic interval before
release. These are explicit fixture corrections. Passing development screenshots
do not replace the final frozen continuation.

Local `make verify` passes 177 portable cases with 14 explicit unavailable native
tool skips; repository/diff checks pass. The updated generated manual renders
with native FreeBSD mandoc, with expected control/limit/UTC content assertions.
Lint returns 3: Asciidoctor preamble mso/LINKSTYLE errors, source-line style notes
and section-spacing warnings are recorded, not described as clean lint.
Packaging paths/tags are unchanged; the exact prior install rules remain the
reference, with no fresh staged-install qualification claimed.

## Canonical evidence and remaining acceptance

Private owner evidence:
`builddir/codex-2026-10-23/file-collection-controls/` under the workspace root.
The receipt records archive SHA-256, complete member hashes/modes and gzip CRC
checks, nine changed runtime/manual/test inputs and exact binary identities.
The signed publication record binds the final commit/tree after report upkeep;
document-only report/status edits do not alter qualified runtime inputs.

The shared cycle remains **2/3 merged since PR163; 3/3 on this merge**. Complete
one affected accumulated PR164/165/current six-target checkpoint before starting
another implementation PR. Full compiler/feature/install/default-X and milestone/
release acceptance remain at their boundaries. Native-CPU sanitizer, hardware,
compositor/toolkit/accessibility coverage remains separate. This is not a full
release/platform qualification. The independent PR164 performance reference is
1/5 merged, 2/5 on merge; no comparison is due and tuning remains deferred.

Watching, virtual scrolling/grid, full focus/scale/accessibility acceptance,
structured files provider, mutation jobs and protected clipboard transfer,
retention, storage and private content delivery remain open with their owners.
