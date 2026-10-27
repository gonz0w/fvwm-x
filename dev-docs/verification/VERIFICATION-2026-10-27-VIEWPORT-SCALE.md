# Viewport Form/browser scaling checkpoint

Owner: Cam. Base merged PR275 `db0175ce848b2b22c8965cbd5365ebc255e024d4`;
candidate `codex/r1-viewport-scaling`. **Focused Linux consumer checks pass.**
#51 and #69 retain broader frozen acceptance.
[Contract](../decisions/VIEWPORT-SCALE-2026-10-27.md) and
[source/command/archive receipt](VERIFICATION-2026-10-27-VIEWPORT-SCALE.json).

The optional browser and single-viewport Forms adopt the core monitor scale.
`UIScale monitor` is additive; ordinary Forms default to `off`. Supported
controls retain native window identities, logical metrics and text/collection
state while owned fonts and geometry publish together. Unsupported compositions
retain a complete 1x view with a diagnostic. No dependency, resident, polling
source or collection-generation relaxation is introduced. Core-owned parent
geometry is not multiplied a second time.

## Focused qualification

One retained Linux ARM64 GCC 13.3.0 incremental profile builds the affected Form,
browser, policies and non-installed font-failure wrapper. `form-list`,
`form-input`, `ui-layout` and `form-scale` pass. Browser package tests and scoped
vet pass. New form-scale policy passes ASan/UBSan with leak detection. Four
existing actual-X Form checks pass: input, button, fields and geometry. Both
changed manuals render without diagnostics; only the Form reference rerenders
after its final status-field wording correction.

Two private Go launcher attempts fail before test execution because their copied
temporary paths reference a previous checkpoint. Correcting that launcher lets
the unfinished Go phases pass; successful C phases are retained. Later source
corrections rebuild only the affected normal/sanitized Form. The final runtime
manifest binds compiled/policy inputs; native Python corrections have their own
hash. No Go concurrency change is selected, so no race repeat is claimed. No
all-OS/compiler/install matrix runs merely because this feature PR is created.

## Native contract and sanitizer scope

The Xorg-dummy checkpoint changes physical millimetre facts on two real RandR
outputs, observing core-owned quarter factors 4, 5, 6 and 8. It compares monitor
atoms, topology epochs, actual root/client/child rectangles, native collection
packets, real input callbacks and actual module wait receipts. Profiles are
Slate dark, Dune light and enlarged 18pt Classic; configured font size alone is
not substituted for automatic scaling evidence. All three profiles pass. Their
39 actual module child Wait receipts and every direct child exit zero; no final
unexpected observer X error or cleanup failure remains. The 36 saved captures
include four unmodified published examples in the
[gallery](../../doc/development-preview.md#native-monitor-scaling).

Repeated monitor movement preserves collection packets and child identities;
actual input heights grow monotonically (30/39/48/60 pixels at 1/1.25/1.5/2x
in Slate/Dune, 43/55/67/86 in Classic). Pending collection work retains the old
view and adopts only the latest target after abort. Real ResizeRequest holds
verify old geometry, one request per minimum/target tuple and adoption only
after a real suitable Configure. The browser's original idle-close clock is
unchanged. Actual callbacks prove literal drafts and cursor insertion, range
and first-visible identity across moves. Grid refusal retains the old view;
real enlargement permits exact-generation admission. Refresh, output removal
with a newer committed epoch and a fresh provider lifetime pass.

A separately linked wrapper returns NULL on the second armed non-1x font
acquisition and holds later attempts until explicitly disarmed. This tests
partial-stage rollback/recovery with retained geometry, collection state and an
actual unsent draft. It is not installed and creates no production environment
hook. Native wrapper cases use ASan/UBSan with leak detection disabled; they
establish bounds/UB behavior, not complete legacy Form leak freedom. The pure
policy's separate sanitizer test enables leak detection.

## Corrections and original failures

The initial timeout fixture incorrectly expected children after normal expiry.
It now samples identities before expiry and checks the original timer deadline,
terminal zero and actual Wait zero. The product clock is unchanged. A native
Selection-first Form then exposes a legacy background assumption: a first
non-drawable item has no DrawTable. Normal and sanitized reproductions are kept.
Background GC selection now finds a drawable item with guards, preserving item
and configuration order. Normal and sanitized unsupported cases verify 1x
fallback after this repair.

Other fixture repairs establish explicit monitor placement, identify controls
from native declarations/creation order instead of guessing by height, and
settle WM focus/raise and actual pointer hits before input. The original grid
timeout is the documented refusal at a minimum one-row list height. The fixture
must observe the retained old view, enlarge the actual window and retry the
exact native generation. A grid hit check also explicitly establishes the top
row after keyboard scrolling; removal waits for a new committed epoch rather
than assuming RandR and module observations are simultaneous. Restart permits
legal numeric XID reuse after proving the old owner is gone and waited, with
distinct new provider/browser/Form identities. Each original failure and the
diagnostic-only focus run is retained separately from qualification.

## Limits and disposition

This is focused emulated Linux consumer qualification. Six-target consumer,
physical display, compositor/toolkit and assistive checks remain with #51 and
release owners. #69 retains unproven frozen rows. Parked #257/#258 source is
not inspected or changed. No speed gain is inferred from test command times.
PR269's periodic counter is 3/5 after PR275, becoming 4/5 on this merge; a
performance comparison is not due. Successful initial focused commands total
13.925 seconds; the final combined native run takes 55.754 seconds. Earlier
native failures take 155.768 seconds and failed launcher attempts 0.612 seconds;
these remain visible rather than included as passes. Transport, review, later
affected rebuilds and final manual rendering are separate. The matched
unstripped Form grows 105,296 bytes; core bytes are unchanged. No memory or
speed improvement is claimed for this feature.

Host archives verify 14 bundles, including all original failures, final policy/
native receipts and capture bytes. Only verified owned guest duplicates and
preserved private diagnostic helpers are retired. Source/build caches, stages,
VM disk and PR269 performance references remain intact. The owned container and
VM are stopped; the controlling session is released. Final source equality
checks all 1,541 compiled/policy inputs and the separately frozen native driver.

Current Temper roles use explicit model/instruction routing because native
named-role loading is unavailable. Terra returns capacity failure; bounded
browser/docs work uses the recorded Sol fallback. Separate Sol source review
finds no remaining scoped source, native or pixel blocker and verifies the
frozen inputs, real lifecycle oracles and unmodified published capture bytes.
This separate agent review is not human approval or release acceptance.
Backend identity is not independently attested.
