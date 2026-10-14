# File-image colorset effect preparation — approved 2026-10-06

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Owner: IMAGE-01 / INPUT-01. Status: approved by the user on 2026-10-06;
implemented with focused qualification passing. Combined activation is pending.

## Concrete decision

Permit file-backed color colorset backgrounds to prepare effects asynchronously,
including ordinary PNG/XPM inputs and changes to Tint, ImageAlpha or background
colors that require rebuilding the image. Keep the previous usable published
image until a matching replacement succeeds; use existing absent-image behavior
on a first load. Failure keeps the previous usable resource and emits the existing
diagnostic. Configuration requests remain ordered; commands can return before
new effect pixels and their matching module update are ready.

Keep the existing XRender path's arithmetic and the existing software path's
arithmetic, mask/alpha rules and palette policy. Do not adopt source-RGB tinting,
change dithering or reread an unchanged cached file when an effect changes.
Decoded-RGB Average is a separate approved policy and remains unchanged.

Move software readback/blending into the existing image process/scheduler owner,
with one active worker in the core. Use a five-second total worker deadline and
1024 MiB worker address/data allocation limit. Approved finite build overrides
are 1–60 seconds and 128–4096 MiB; preserve stricter inherited process limits.
The deadline starts when the worker is launched and includes its X connection,
readback, color queries, blending and output transport. Queue waiting and bounded
parent palette conversion/upload/acknowledgement are separate continuations,
not a promise that an entire queued request finishes within five seconds.

## Observed problem and compatibility

The existing colorset completion creates a derived pixmap, then calls
PGraphicsRenderPixmaps and performs a final XSync before broadcasting it. On
servers/builds without usable XRender, PCreateRenderPixmap reads source, mask,
alpha and destination images and allocates image-sized color/alpha arrays. It
queries colors and blends every pixel in the core. A fixed callback count cannot
preempt this work or its X replies; simply deleting XSync would publish resources
without the required acknowledgement and would not solve software blending.

The [dimension/effects checkpoint](../verification/VERIFICATION-2026-10-06-COLORSET-GEOMETRY.md)
records independent current 24/8-bit XRender and software pixels. For a red PNG
with green Tint 50, the 24-bit results differ by one channel step; 8-bit results
also differ by palette allocation. Preserve each backend's current rendering
rather than silently replacing both with a scalar source-RGB preview.

Earlier SVG/unusual-format approvals cover loading those inputs, not standalone
effect changes on ordinary file pictures. This proposal extends completion timing
and adds finite limits to previously unlimited software work. Very large/slow
legacy effects can fail under the defaults. Themes that assume immediate derived
pixels or module packets can observe later completion. Root pixmaps, generated
gradients, bitmap foreground/background conversion, unrelated icon/menu effects
and standalone module adoption retain their separate contracts.

Root [AGENTS.md](../../AGENTS.md) requires: “A specific compatibility break
requires an explicit user decision.” The user approved these timing and refusal changes on 2026-10-06. The existing SVG worker limits do not by
themselves approve these new software-effect limits.

## Implementation and ownership contract

- Reuse the existing literal process transport, readiness/deadline service,
  child reaper and single active scheduler. Add a private worker operation,
  without a second pool, thread or command interpreter. Keep it available when
  optional SVG support is disabled, using existing required Xlib dependencies.
- The worker may query only the internally supplied, acknowledged source
  pixmap/mask/alpha and colormap. It neither allocates colors/pixmaps nor changes
  authoritative window state. No file reopen or shell evaluation is needed.
- Pin immutable source backing and relevant palette/background references before
  launch. Hold them through actual child exit/reaping even after cancellation;
  retiring a request must not make a reused XID visible to its still-live reader.
  Extend the existing process retirement owner where necessary; do not rely on
  unowned SIGCHLD callbacks or detach a child while releasing borrowed resources.
- Use bounded readback tiles and checked typed RGB16 output, preserving queried
  server colors and legacy integer tint/alpha blending. Preserve exact background
  pixel reuse for fully masked pixels. Parent allocation follows the existing
  software PictureAllocColor policy without image dithering.
- Preserve the existing 64 Mi-pixel image ceiling. An eight-byte typed record per
  pixel needs at most 512 MiB plus a fixed header; validate dimensions, count,
  record kinds and exact length before use. This is an internal output bound,
  not a new source-file/decompression limit. Acquire output only for the active
  request, consume incrementally and avoid image-sized worker color arrays.
- Stage derived pixmaps/palette leases privately in bounded parent conversion.
  Hardware XRender preparation also targets private resources. Acknowledge all
  creation/render requests using the existing owned X scope before publication.
  Preserve previous pixels and associated module state until the matching revision
  is acknowledged. Requested configuration is stored in order; superseded, failed
  or destroyed owners cannot publish stale derived resources.
- Commit only at the core's ordinary safe boundary. Nested/modal waits service
  transport, not arbitrary commands or effect commits. Cleanup and child draining
  must remain asynchronous; reuse existing delayed module-resource retirement.

A separate X connection is worker isolation, not a claim of an OS filesystem or
X-server permission sandbox. Xlib exchanges themselves remain server-dependent;
worker deadlines bound the parent's wait for this operation, not the server's
independent responsiveness. Palette queries/allocations in the parent must stay
bounded per turn and retain required reply/error handling.

## Focused implementation stories and combined gate

First implement pure legacy RGB16 blend policy and checked output protocol tests.
Then extend process ownership for source/palette pins through exit, and verify
finite-memory/timeout/cancellation on affected BSD targets early. Add worker
software readback and bounded parent upload, followed by colorset revision staging
and hardware/soft-path owned acknowledgement. Each story uses focused checks.

Required controls include exact 24/8-bit pixels against the existing independent
results, opaque/masked/partial-alpha images, Tint/ImageAlpha/background-only
updates, cached source retention, shared source owners, slow worker input/timers,
malformed/oversized/partial output, refusal/timeout/previous-image retention,
supersession/destruction/XID reuse, shutdown and delayed module resource use.
Qualify SVG-disabled and XRender-disabled builds as well as normal hardware
rendering. Production core activation and the extensive frozen-source
Linux/BSD/install/default-X/sanitizer pass remain one combined checkpoint.
Native Linux release sanitizer CI and physical hardware remain separate gates.

Implementation and exact focused evidence: [colorset effects checkpoint](../verification/VERIFICATION-2026-10-06-COLORSET-EFFECTS.md).
