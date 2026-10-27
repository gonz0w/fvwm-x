# Form reserved-area startup — 2026-10-27

Owners: #51/#69; Cam. Base: merged PR285,
`12b492c91fa1a13e9d6aa22fd94829b470a1c184`. This finite startup slice
passes focused Linux qualification. Both broader owners remain open.
[Exact receipts](VERIFICATION-2026-10-27-FORM-WORKAREA.json) bind sources,
binaries, profiles, commands, archives and observed geometry.

## Result and boundary

Automatic scaled Forms now size their preferred viewport and place their managed
outer frame within the selected monitor's sampled available area. The existing
Monitor14 record supplies per-monitor reserved-edge insets; MonitorUI remains the
topology/scale owner. No new packet, generation, request, polling or process is
introduced. Managed movement still uses the shared core command engine.

A bounded Form-local cache admits checked nonnegative insets for matching names,
discards samples on new MonitorUI publication, and falls back to physical bounds
when no valid current sample exists. Malformed input cannot replace a retained
valid sample in the same topology; a subsequent valid sample can recover during
startup. Preferred size and frame placement use the same checked area. Readable
minima win when an area is too small: oversized axes anchor at its top/left.

Explicit Position/Geometry and post-ready user movement remain authoritative.
Drafts, cursor, selection, anchors, native windows and generations remain intact.
This is **stable initial configuration** and **first-ready admission**. Legacy
reservations have no topology epoch, and work-area-only changes need not publish
new Monitor rows. No atomic freshness through pre-ready panel/hotplug changes,
post-ready automatic relocation or fully scaled first mapped frame is promised.
GetConfigLine end/NULL is not asserted to certify complete transport delivery.

## Reproduction and exact candidate

The retained PR285 Form binary SHA256 was
`de146884f65949d61c2e071d9c076d7fa19289ac11a0f06daec5cb1212fb53a6`.
A fresh actual-X enlarged Classic 2x baseline failed: outer frame
`[260,0,2300,2160]` overlapped real DUMMY0 reserved insets `[0,0,36,0]`;
the matching core-produced usable rectangle was `[0,36,2560,2124]`.
The original screenshot, packets and failure remain preserved separately.

Final compiled-input inventory: 1,546 exact files, aggregate
`0ce73ccb15f5d29080b5bc16ab1e7896bf96e3fa1bf85357a163fbe0ba8ddff0`.
Final normal Form SHA256:
`4febe8484a6058e47c84c41e9e003cc0388e395f4feba6a6ae913f7b2b7fe119`,
1,795,656 bytes. Core/browser binary hashes are unchanged from PR285. The Form
executable grows 11,952 bytes including debug data; no runtime speed/RSS claim.
PR281 performance reference is 2/5 after PR285 and 3/5 on this merge, so no paired
comparison is due.

Primary scenes use driver
`2fbad58b9326aa87786076c23792881164904a91c3057bed9aba4cedf603980b`.
The final nonzero scene uses
`8b1d806f9337f1a1c1aad48c9f457b2f612797d8c2f2228bf066e4f5407ca914`.
Their sole difference chooses an in-area user-move y coordinate for DUMMY1;
DUMMY0 code paths and all compiled inputs remain exact. The old driver bytes and
both manifests are retained. Final guest source/driver/binary rechecks pass.

## Combined verification

Use the retained Ubuntu 24.04.5 ARM64 container on the local Linux 6.18.10 ARM64
Podman VM. Normal debugoptimized and scoped address/undefined debug profiles,
generated-config hashes and selected flags are recorded. Default `cc` reports
GCC 13.3; this does not independently identify every retained profile's compiler.
This local VM/container execution is not physical or native-runner release
qualification.

One incremental checkpoint passes affected Form/policy compilation, form-scale
policy, scoped ASan/UBSan form-scale policy with leak detection, existing actual-X
geometry controls and the changed Form man-page render. No Go code/concurrency,
packet type, core API, dependency, build option or installation rule changes;
unchanged browser/race/all-OS/install gates do not repeat.

Pure cases cover quoted and maximum escaped names, absent/mismatched samples,
all four negative/excessive insets, truncation/trailing junk/decimal overflow,
zero/overflow area, signed/nonzero origins, 64-monitor and overcount bounds,
new-topology invalidation, replay retention, physical fallback and recovery.
No fabricated native producer metadata is used to claim those negative cases.

| Scene | Actual scale | Available rectangle | First-ready outer frame | Captures | Module Wait0 |
| --- | --- | --- | --- | --- | --- |
| Slate |1.25|`[0,36,2560,2124]`|`[973,730,786,932]`|10|29|
| Dune |1|`[0,36,2560,2124]`|`[973,730,622,735]`|9|28|
| Enlarged Classic 18pt |2|`[0,36,2560,2124]`|`[260,36,2300,2124]`|9|28|
| Slate, DUMMY1 at nonzero origin |1.25|`[1291,1200,2536,941]`|`[2253,1200,786,932]`|9|28|

The normal three themes use the actual desktop panel's 36px top reservation.
The extra scene uses real RandR DUMMY0 at 1x and DUMMY1 at 1.25x, physical
`[1280,0,2560,2160]`, with genuine configured insets `[11,13,1200,19]`.
Its large reservation deliberately binds sizing/placement; it is not a physical
1200px panel. Matching actual core Monitor/MonitorUI packets and native frames,
including selected atom/name, establish per-monitor behavior.

One Slate oversized case configures usable `[31,97,295,49]`. The actual client
retains readable minimum 591x98 and outer frame `[31,97,601,143]` anchors on both
axes. Restoring ordinary reservations leaves the already-ready parent geometry,
selection, children and generation unchanged. All four runs retain the complete
Details/continuity suite: four startup rows when they fit, one-row resizing,
visible focus and deliberate offscreen scrolling, cursor/draft, selection and
identity, explicit positive placement, stale lifetime callbacks and clean exits.
Numeric XIDs may be recycled; aliases and actual child waits establish lifetime.

Final total: 37 captures, 113 actual module Wait0 receipts and 16 direct zero exits,
with no unexpected observer X error, failure or cleanup entry. Two fixture failures
remain separate: an inventory typo failed before server startup; a nonzero user
move incorrectly expected y=120 inside a top=1200 reservation although core Move
honors work areas. Correcting only the affected fixture code reruns Slate or the
nonzero scene respectively; successful unchanged passes remain retained. There
was no product-code correction after the combined build.

## Review, coverage debt and cleanup

Lead inspected the [four published unmodified screenshots](../../doc/development-preview.md#reserved-area-startup).
Separate source/evidence/pixel review inspected these plus minimum/oversized,
Classic/nonzero Details and Dune focus views; final document/candidate review is
recorded with integration. Agent review is a second examination, not independent
human or release approval. Named Temper workflow/analyst/implementer/reviewer
requests are now accepted by the tool using the selected mappings; backend model
identity remains unattested. No model was silently substituted.

Coverage-debt review after the recent Form batches retains affected six-target
font/scale/runtime and full consumer dispositions at #51/#69 acceptance. This
batch adds portable consumption of an unchanged record, with no OS/API/native
packet change requiring another BSD boot now. Focused passes do not close those
owners. Negative/native-malformed injection, pre-ready hotplug/reservation races,
physical hardware, assistive/multilingual and full release coverage remain pending.
#257/#258 stay parked.

Canonical archives are under `builddir/codex-2026-10-27/r1-form-workarea/`.
Full archive/member sets, SHA256, size, mode and gzip CRC are verified; exact
identities are in the JSON receipt. The final checkpoint archive has 22 members,
adding only observed profile metadata to the earlier 21-member capture. Only
verified owned guest evidence duplicates retire. Canonical host archives,
source/build/stages/caches/disks and performance references remain. The owned
container and Linux VM stop before the held controller is released.
