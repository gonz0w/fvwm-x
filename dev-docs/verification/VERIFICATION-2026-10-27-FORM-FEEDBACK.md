# Continuous native collection feedback

Owner: Cam. Base merged PR280 `c5c9dfff12fb79aa4d0f0cf03b2bee9646ab7f2f`;
candidate `codex/r1-form-feedback`. #51/#69 retain broader acceptance.
[Source/command/archive receipt](VERIFICATION-2026-10-27-FORM-FEEDBACK.json),
[periodic comparison](PERFORMANCE-2026-10-27-FORM-FEEDBACK.md).

Native focus-index, selection and scroll changes now invalidate the browser's
selection observation. `ListWatch` is additive and off by default. One marker
stays outstanding until exact `ListWatchStatus` reports the current versioned
selection and rearms in the same native handler. Ordinary ListStatus does not
rearm; ListNotify remains startup-only. Programmatic updates, redraw and no-op
input stay silent. No polling, new resident, toolkit or packet type is added.

The browser uses a distinct bounded highest-Form mailbox, fresh correlation and
current acknowledged row mapping. Feedback updates only changed labels, retaining
native/model/render counters, hidden IDs, drafts, cursor and window identities.
The existing provider-state fingerprint includes selection/focus and changes
accordingly; it is not a new publication counter. Combined hidden/visible overflow
retains the admitted model, reports unavailable and recovers on valid input.
Actions still validate their own fresh native packet. Current-form feedback can
renew existing leases; automatic publication and retired markers cannot.

## Focused evidence

One retained Linux ARM64 profile passes affected compilation, four C policies,
browser package tests/race/vet, scoped ASan/UBSan list/scale policies with leak
detection, four legacy actual-X Form regressions and both man-page renders.
Host checks are bounded pure/net.Pipe cases and static/source checks; no full
host socket package or OS/install matrix is repeated. Exact commands and times
are in the receipt. Emulated execution does not establish native-CPU release
qualification.

Separate review catches a recovery defect before acceptance: continuing after a
watch timeout could admit its late reply into the ordinary single-slot ACK queue
and terminate on the following valid reply. A mutex-protected current-request
watch gate now checks, admits, retires and drains atomically. Reserved watch
replies never consume the ordinary ACK slot. A production JSON Session.Read
regression supplies late A, valid B and an ordinary causal barrier before B is
consumed; retirement-race and isolation tests retain exact packet validation.
Only the five changed Go inputs rebuild/retest after this repair. C/sanitizer/
legacy/manual phases retain their original identical input scope.

The first native run reaches the hidden-selection refusal/recovery checks, then
sends Clear selection and Clear filter consecutively. Raw traffic proves both
callbacks carry generation5; clearing publishes generation6, so the second
callback is correctly refused. The fixture now waits for cleared state, exact
new native generation and completed render acknowledgement before Clear filter.
The final 4,096-visible/zero-selected/same-window assertions remain unchanged.
Original failure evidence is preserved.

The three-profile functional checkpoint passes Slate, Dune and enlarged Classic:
63 captures, 51 actual module child Wait0 receipts, every tracked child zero,
and no final unexpected X/cleanup failure. It proves burst coalescing, wrong-token
and ordinary-status non-rearming, latest-state ACK/post-ACK wakeup, legacy silence
and programmatic/no-op silence. Browser evidence observes actual counts/readiness
labels before any provider query. A 157-hidden +100-visible selection is refused;
deselecting one admits256 and restores readiness. Existing scale, resize, bitmap,
focus/caret, draft/cursor, range/top, font-refusal, grid, removal and lifetime
oracles remain in the same run.

## Visual and remaining acceptance

Four unmodified captures are published in the development preview. An original
Classic refusal image has missing spans on static Watching/Progress lines.
It remains preserved in the original native-final archive. A bounded Classic-only
follow-up uses driver `92b2d94655f5b5515d097d4eb9c463da17b24e71f30dc52f6307dfd40121d60f`,
separate from the original three-theme driver `939b16ad…`. At refusal and recovery,
the full status-header raster is identical before a monitor-metadata/producer
round trip, after it and after another quiet observation. Scale/resources,
selection and child identities stay unchanged. All six captures look clean on
manual inspection; the anomaly is not reproduced in these quiet captures. This
does not prove universal paint completion or assistive-technology behavior. The
follow-up passes 25 captures, 17 actual module Wait0 receipts and all 102 tracked
children zero, with no unexpected X/cleanup error. Its two unmodified Classic
images are published alongside the original Slate/Dune captures; the manifest
records each image's exact archive/driver identity.

Full-text disclosure, configured multilingual glyph availability, physical/
assistive/toolkit and affected six-target/release gates remain open. Parked
#257/#258 source stays excluded. Explicit configured Temper role/model/effort
routes are used; native named-role loading and backend identity remain unattested.
Separate agent review is not human approval. Source, reply-gate regression and
supplemental pixel/provenance review have no remaining actionable finding. Only
verified owned guest evidence duplicates are retired after canonical retrieval;
source/build/stages/disks and both performance references remain. The owned
container and VM are stopped and the held controller is released.
