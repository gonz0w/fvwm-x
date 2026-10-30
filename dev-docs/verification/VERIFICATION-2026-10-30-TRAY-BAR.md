# Tray bar popup, paging and preferences checkpoint

The selected #78 batch passes focused Linux build, installed-tray, policy,
settings IPC, actual-X and tray/pure-helper sanitizer checks. It adds an opt-in
bar popup, accessible overflow pages and four saved preferences. **The full
Buttons leak-check run fails** on font-lifetime allocations and remains an
explicit qualification gap. This is a development checkpoint, not complete #78
or R1 acceptance.

[Machine-readable receipts](VERIFICATION-2026-10-30-TRAY-BAR.json) retain commands,
source identities, failures, reuse and limits. The runtime freeze is
`30403c34cc7dec8fd8d75f64213d46a14af37a6287f09621ae7455a507974a51`,
based on merged PR312 `17a1548b6ea866644311d280539794ff45ebb32d`.
Gallery/evidence/tracker edits follow that freeze without changing runtime inputs.
The signed PR identifies the final reviewed tree.

## Delivered behavior

- Strict actual-root-pixel `ToggleAt` anchoring, adjacent placement with edge
  flip/clamp, anchor-monitor scale and standalone `Show` recovery.
- Explicit popup focus/Escape behavior, stable bounded pages with header and
  keyboard navigation, and later fitting clients reachable past oversized ones.
- A Tray launcher in the adaptive bar. Controlled rebuild hides the popup;
  reopening uses fresh geometry. The tray/plug tree survives bar exit because it
  remains outside the bar's X tree. This is not foreign containment or continuous
  tracking of an independently moved bar.
- Saved `tray_enabled`, `tray_gap`, `tray_padding`, `tray_orientation`, a compact
  editor and schema1..6 migration to schema7 through the existing settings owner.
  Enablement is a non-replacing request, not observed readiness. Disable targets
  only the reserved binary/alias. Theme/bar refresh does not restart the owner.
- Optional bounded root-only Buttons `StartupAction`, consumed once after its
  runtime packet reader is ready. This lets the bar request state without a
  core wait or discarded startup updates; it promises neither paint nor final
  geometry. Configurations without it retain their behavior.
- Public settings metadata shared at the envelope to retain all 19 typed fields
  under the existing 8,192-byte limit; native records remain self-contained.

No mandatory library, toolkit, bus service, supervisor or settings store is added.

## Executed checks and corrections

Linux ARM64 uses native CPU virtualization (AppleHV), an Ubuntu 24.04.5 container
on the retained Fedora 43/Linux 6.18.10 lab and a fixture-owned real Xorg dummy
server. Two active outputs expose mixed scale. Independent synthetic clients
observe actual parents, maps, sizes, focus, events and selection ownership.
This is not physical Linux, native AMD64/BSD or real-toolkit evidence.

| Check | Observed result |
| --- | --- |
| Focused builds and policy | Normal tray model and new Buttons startup boundary/one-shot helper pass; affected normal modules and manuals build incrementally. |
| Go/settings | Settings, controller and inspector packages pass. Affected unchanged passes are composed by exact source manifests, not rerun after prose/fixture edits. |
| Installed tray | Tagged install passes. Canonical Meson 1.7.0 metadata plus exact `fix_rpath` scratch equality matches built and installed ELF. Final installed/generated manual bytes match. |
| gate13 actual X | All 14 bar/popup case groups pass in 9.523s, including actual mouse activation, three themes/fonts, settings recovery, rapid off/on, secondary 2× and abrupt bar exit. |
| gate13 settings IPC | Updated 19-field public query/mutation/validation/recovery and independent transport producer checks pass in 15.678s. |
| Final standalone | All 16 older standalone groups pass on the final installed module in 5.005s. |
| sanitizer02 | ASan/UBSan/leak-enabled tray module/model and pure Buttons startup helper pass; all 14 bar cases pass with normal Buttons/core/Desktop/peer and no sanitizer reports. |
| sanitizer01 | Functional cases pass with both native modules instrumented, but the outer report scan fails on six Buttons LeakSanitizer reports. This remains failed. |
| Portable tooling | `make verify` exits 0: 291 tests, including 20 native Meson/compiler skips on macOS; separate native Linux gates cover this batch. |
| Manuals | All three generated manuals render with mandoc at width 78, exit 0; lint exits 3 on generated template macros/spacing/style warnings. |

Earlier gate01/03 have collector target-name errors. Gate02 reproduces a
9,447-byte settings reply; sharing common envelope metadata fixes the strict
original cap test. Gate04 exposes inspector handling of Session null/unavailable
metadata. Gate05 exposes an old fixture leaving invalid configuration stored.
Gate06/07 expose normalized colon projection and invalid static button syntax;
the shipped producer now uses supported spaced syntax and static `Disabled`.
Gate08 exposes the old alias-only KillModule call leaving bars alive; the binary
and alias now match. Gate09 corrects a fixture movement request against the
shipped FixedPosition policy.

Gate10/11 retain failed first theme clicks. The test now observes the asynchronous
startup chain through ordered Buttons geometry, correlated Desktop reply, fresh
Buttons geometry and tray pending-state fences, before the same single real
XTest click. It adds no sleep/retry or substitute activation. Gate12 passes these
cases but catches automatic workarea snapshots reapplying stored VisibleOff
after bar exit. Automatic geometry refresh now preserves runtime visibility;
initial/explicit configuration still applies stored visibility, and queued
explicit intent cannot be downgraded. Gate13 passes the original mapping
assertion and records independent mapped-before/mapped-after observations.

Initial ownership/configuration review also fixes stop/start selection races and
a startup default-frame flash: non-replacing enablement preserves an incumbent;
an isolated candidate has a bounded two-second acquisition grace; first mapping waits for
a complete valid configuration. Xlib calls themselves remain non-preemptible.
No source-reviewed correction is presented as an invented executed red/green.

The final installed debug ELF is 1,458,464 bytes; allocated sections are 361,235
text, 11,728 data and 10,840 BSS. These are feature sizes, not RSS or a paired
performance comparison. The counter remains 2/5 after merged312; a comparison is
not due. Gate13's inherited manual-hash field is stale; final canonical
provenance and the final standalone parity check provide the corrected hash
`8dc2228b6bf48707606539d614ccd6db7cac73fc13701299948f78d6109ec68f`.
Original receipts remain unmodified.

## Qualification limits and continuation

[Three theme/font captures and exact overlays](../../doc/development-preview.md#bar-tray-popup-and-saved-preferences)
show Slate Sans, Dune Serif and enlarged Slate Mono, plus a secondary 2× popup.
PNGs are unmodified 3200×1080 framebuffers. Foreign clients remain 24×20 pixels;
this is not mail/network application or assistive-provider qualification.
The enlarged bar/tray example retains the editor's ordinary shared font.

Sanitizer01 reports 3,144–3,382 leaked bytes per exiting Buttons process. Allocation
stacks enter Fontconfig/Xft from existing startup font loading before the new
callback. Its fixed-storage helper passes leak-enabled checks. A matching
prior-revision runtime comparison was not run, so pre-existing/non-regression
status is unconfirmed. Do not add `FftFini` while font/display users remain.
The finite R2 follow-up is to establish a matching baseline, retire owned font
references in order, then pass repeated theme/rebuild/graceful-exit leak checks.
No suppressions or clean full Buttons/session leak claim accompany this PR.
Individual graceful module exit codes are not all captured; model/outer-fixture
exits and no-report observations are retained. Deliberate abrupt kills test
recovery rather than leak-clean shutdown.

#78 stays open for true containment's two-owner lifetime/geometry/focus contract,
modern StatusNotifier/application menus, real toolkit/visual profiles and
combined affected platform/install/release acceptance. Six-target/compiler,
full installed desktop, physical hardware/compositor and broader sanitizer
qualification were not rerun for each small change. #257/#258 remain parked.

## Review and retention

Separate final review of staged tree
`ea28f4695603ae1b3c71c0909966a0479a0bb0b6` is clear: source identities,
receipt/capture manifests, originals and visible themes, manual provenance and
whitespace agree. The narrower sanitizer pass does not relabel the Buttons
failure. Only review-completion metadata in this report, its JSON and the tracker
follows that review; the signed PR identifies the final tree. This is a read-only
agent review, not independent human approval.
The retained current host evidence includes exact failed and passing archives,
source manifests and one current source archive. Verified guest test/transfer
duplicates (49,943,776 bytes) and superseded host source archives (286,818,277
bytes) were retired. Reusable source, incremental builds, private prefix and the
next performance reference remain. The owned Linux lab stopped normally.
