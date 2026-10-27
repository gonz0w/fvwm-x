# Complete browser values through native read-only inputs

Owner: Cam. Base: merged PR281 `798acfb8ed0ffc9c0f44976a68b4a7f173dced6f`;
candidate `codex/r1-browser-disclosure`. [Machine-readable receipt](VERIFICATION-2026-10-27-BROWSER-DISCLOSURE.json),
[manual](../../doc/fvwm-file-browser.adoc#complete-value-details) and
[development screenshots](../../doc/development-preview.md#complete-value-details).
This is a finite #51/#69 component checkpoint; both broader owners remain open.

F1 opens an isolated snapshot of the complete admitted browser values, including
original focused/single-inspected filename bytes. Compact parent labels retain
their existing bounds. Fields use reversible ASCII escapes and literal InputHex;
400-byte parts never split an escape. Admission precedes expansion: at most
13 fields, 128 KiB source, 512 KiB escaped text and 1024 parts. Excess produces
an explicit refusal, not a partial snapshot. No filesystem/content query runs.

`InputReadonly name` is additive and startup-only. Human edits, deletion,
history and paste cannot replace its value; ordinary editable Inputs, setup,
InputAppend and Restart keep their existing behavior. Cursor/focus navigation
uses the existing native input owner. The details Form has one section viewport,
read-only value, keyboard Read/Previous/Next/Close and monitor scaling. One owned
snapshot/page is active; parent, snapshot, page and invocation identities retire
stale callbacks. A separate correlated command ACK gate keeps detail traffic out
of parent replies. Opening/paging does not publish parent projection state or
change parent windows, drafts, selection or model/native/render/binding counters.
The snapshot stays frozen across background updates until reopened.

## Qualification and cadence

One reused Linux ARM64 guest passes affected compilation, four C policies,
whole browser package/race/vet, four existing actual-X Form modes, three new
read-only scenes, scoped sanitizer policy and both man-page renders. After a
C-only redraw correction, only the two affected native builds, three read-only
scenes, editable fields and three read-only sanitizer scenes repeat. The four
pure policy inputs, Go and manuals remain exact. Full module ASan/UBSan checks
use leak detection off; the separate pure input policy uses leak detection on.
Neither establishes whole-module leak freedom or native-CPU release acceptance.
No OS/install matrix or paired performance comparison repeats for publication.

| Native configuration | Actual RandR scale | Captures | Actual proxied module Wait0 |
| --- | ---: | ---: | ---: |
| Slate dark | 1.25 | 3 | 22 |
| Dune light | 1 | 3 | 22 |
| Classic, DejaVu Sans Mono 18 | 2 | 3 | 22 |

All 12 directly tracked processes exit zero; the final profiles have no
unexpected observer X error or cleanup failure. Actual interaction proves
complete >1000-byte paths and escaped filenames, section/part navigation,
exact Home/End restoration, blocked editing, retained parent state/drafts/cursor,
frozen refresh/reopen, stale callback refusal, WM close and parent replacement/
exit teardown. A new Form can reuse its predecessor's numeric XID; alias/form
lifetimes and actual old/new child Wait receipts establish replacement.

Two long-value scenes compare complete 1355-byte UTF-8 values through an
existing fixed private printf/Base64 command and small completion receipt;
a third scene checks the three-byte readonly destination after long-field traversal.
They prove editing/history/direct paste refusal, paste traversal into readonly,
editable controls, Restart and short-field traversal. Source/command evidence is
composed explicitly: initial legacy input/button/fields/geometry use the initial
driver; final readonly/fields and sanitizer use the corrected driver. Source,
mode, command, archive and per-profile driver identities remain in the receipt.

## Findings and preserved failures

Early review corrects zero-based native chooser ordinals, transport-safe part
size and a readonly traversal cursor clamp. Host Unix-socket restrictions and
initial shortcut expectations remain in the author receipt; the complete Linux
browser package passes. Native observer corrections preserve original artifacts:
long values exceeded the existing 1000-byte outbound command cap; geometry
heuristics misidentified controls; a Space fallback raced a pending click;
standalone colorset zero hid glyphs; histogram/core-Nop samples were insufficient
for exact painting; and numeric XIDs were incorrectly assumed unique forever.
Corrected observers retain complete byte and strict spatial assertions.

The strict Xft regression then exposes a real readonly redraw defect: 54 changed
pixels remain at the right edge after Home/End/Home. Clear ends before the caret/
remaining glyph draw. Readonly keyboard painting now uses the common input paint
path; clear and clip cover the same complete interior, preserving the bevel and
restoring draw-table region state. The unchanged regression has zero differing
pixels in every final theme. Pre-fix binaries, raw rasters, commands and failures
remain in verified canonical archives.

Initial functional profiles pass but enlarged Classic captures extend beyond
the screenshot. The capture follow-up verifies actual client/frame containment;
Classic alone needs a fixture-owned core Move with unchanged dimensions, IDs,
font and scale. Slate/Dune retain their passing contained captures. Per-image
provenance distinguishes these driver revisions. This does not qualify general
default placement, hardware, assistive providers or multilingual glyph coverage.

Form debug-file growth against PR281 is 2152 bytes; browser file growth is
11614 bytes. These are executable observations, not RSS or speed measurements.
No new toolkit, mandatory package, default service or polling source is added.
A temporary native Details module and bounded snapshot exist only when opened.
Periodic comparison remains the PR281 reference at 0/5, 1/5 on this merge.

Separate source/region/evidence/pixel/final-document review has no remaining
actionable finding. Every retired guest duplicate has a verified
canonical host archive. The owned container and VM are stopped, controller
released; source/build/stages/disks and performance references remain.
Configured Temper routes are requested; native named-role loading is unavailable
and backend identity is unattested. Agent review is not human approval.
#51/#69 retain other consumer, affected six-target, physical/assistive and release
acceptance. Parked #257/#258 remain excluded.
