# Image-cursor owner preparation — 2026-10-06

Tracker: IMAGE-01 / INPUT-01. Status: implementation mechanics have focused
coverage; the bitmap fallback compatibility decision is pending. Production
activation and full combined qualification remain off/open respectively.

## Prepared behavior

Deferred SVG, compressed XPM and unusual image cursors use the same single-worker
scheduler as other core images. ARGB32 conversion premultiplies RGB in bounded
upload steps, avoiding palette conversion and XGetImage copying. The cursor is
created inside the existing owned X request range and acknowledged before a live
slot receives it. Refusal retains the previous cursor. Native/default cursor
commands and superseding requests cancel the pending slot interest.

One ready uncached lease transfers its cursor handle. Until that transfer, its
immutable backing owns the cursor; cancellation/failed creation/shutdown release
only owned resources. Detached leases cannot transfer a handle. Optional colors
are resolved by the command and retained in its typed slot context. No borrowed
window is retained by the callback; publication uses current core state.

XPM hotspot metadata comes from the existing parse of the one expanded snapshot;
there is no second expansion. Explicit and source hotspots use the existing scale
and clamp policy. Known native cursor files retain their existing animated loader.
Recognized ordinary PNG/XPM/bitmap cursor inputs retain their current path.

## Exact source and evidence

Frozen source: `builddir/codex-2026-10-06/svg-cursor-owner/source-cursor-owner-final.tgz`.
SHA-256: `116c1b65f8c936d94138b83532366967316e686e8007bcc4498bb1044138576c`.
All 1240 source contents/modes match the normal, sanitizer and optional-profile
source trees. The archive extends the root-owner checkpoint with 13 owned cursor
preparation/fixture files and scoped Meson registration. Concurrent desktop and
tool adoption edits remain excluded.

`evidence-normal.tgz`, `evidence-sanitizer.tgz` and `evidence-monochrome.tgz` in
that directory contain exact compiler/options/source and binary identities,
failed/final logs and artifacts. Normal/sanitizer metadata each records 12 built
identities; the optional profile records three built identities. These are not
installed-artifact qualification. Use `tests-accepted.log` and
`tests-accepted-core.log` for the final normal/sanitizer outcomes, and
`tests-monochrome.log` for the optional outcome. Historical run directories can
contain failed probe assertions; the final logs identify the passing executions.

| Profile | Entries | Result |
| --- | --- | --- |
| Ubuntu Linux ARM64, Clang 18.1.3, private PNG 1.6.59 | owner/policy regression including color and forced-core cursors | 16/16 pass |
| Same source, scoped ASan/UBSan | direct owner/policy checks with leak detection | 7/7 pass |
| Same source, scoped ASan/UBSan | actual core/MFL owner checks; leak detection disabled | 9/9 pass |
| Same source, GCC 13.3.0, Xcursor and XRender disabled | cursor-image-owner-xvfb, 24/8-bit | 1/1 pass |

Cursor probes use real server cursors, independent XFixes images when available,
transparent/partial-alpha pixels, 1×/2× dimensions/hotspots, misnamed and compressed
XPM hotspots, old cursor retention, owned creation refusal and native-command
cancellation. The finite-memory decoder fixture is matching unsanitized code;
parent/probe/core/MFL lifetime code is instrumented in the sanitizer profile.
The actual-core regression includes real SVG decoration rendering, but this does
not claim a real SVG cursor/default BSD server or installed cursor qualification.

## Concrete compatibility finding

With Xcursor present but XCURSOR_CORE=1, the existing library path turns the
control's alpha-128 pixel into opaque white. The prepared FVWM bitmap fallback
makes that pixel transparent. Both report a four-by-four image and hotspot (2,2).
The probe prints both observed behaviors. The proposal also covers Xcursor's
non-default dither choices and XRender-disabled builds with Xcursor present;
that mixed optional build is not claimed as tested here.

[CURSOR-CORE-FALLBACK-PROPOSAL.md](../decisions/CURSOR-CORE-FALLBACK-PROPOSAL.md)
records the decision and alternative. Passing mechanical tests do not approve
changed cursor appearance. The earlier asynchronous timing approval does not
cover this output difference.

## Failed checks and remaining gates

The scoped archive initially duplicated the root probe registration; its freeze
filter was corrected. The first direct cursor fixture lacked FScreenInit and
failed before a cursor was loaded; required monitor initialization fixed it.
The independent legacy pixel expectation initially guessed black. The actual
XFixes observation showed white; the diagnostic and expectation were corrected
and all final affected checks rerun. No result from failed fixtures is counted.

Before activation: resolve the cursor compatibility decision, finish derived
colorset/other-owner X acknowledgement and bounded completion work, and exercise
actual enabled-WM slow loads. At the combined checkpoint: freeze all adopted
owners once and run Linux/BSD compiler, install, default-X, interaction/mixed-DPI,
real cursor/optional-feature and relevant sanitizer gates. Native Linux release
sanitizer CI remains separately unrun. No new BSD result is claimed here.

Subsequent decision: on 2026-10-06 the user approved the simpler FVWM bitmap
fallback. This resolves the compatibility decision above; the recorded test
scope and remaining production/combined qualification gates are unchanged.
