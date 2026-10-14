# Bounded file-image background averages — approved 2026-10-06

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Tracker: IMAGE-01 / INPUT-01. Status: approved by the user on 2026-10-06; implemented with focused checks passed; combined qualification pending.

## Concrete decision

For file-backed color images, compute `bg Average` from the decoded RGB channels
before display-palette reduction. Ignore fully transparent pixels; give every
remaining pixel equal weight, without alpha weighting. Do not include `Tint` in
the calculation; retain the existing later `bgTint` processing. Use white when
no eligible color pixels remain. Explicit background colors are unchanged.

Use checked 64-bit channel sums and a pixel count, accumulated during bounded
image preparation. Convert source 8-bit channels to the 16-bit color range before
division; do not round an 8-bit mean first. Retain the summary with immutable
picture backing so multiple colorsets can reuse it. Allocate the final background
color through the existing FVWM colormap policy. No new runtime library, sampling
algorithm, worker pool or full-image readback is required for these averages.

The decision covers file-backed color images consistently, including PNG, SVG
and XPM. A loader must supply the summary before its path adopts this policy;
activation must not accidentally depend on whether a particular image was
already in the cache. Image variants use their decoded/scaled pixels. Bitmap
foreground/background conversion, generated gradients and externally supplied
root pixmaps remain separate contracts; this proposal does not redefine them.

## Observed compatibility difference

The actual core currently gives opaque images a white `bg Average`: a red native
PNG and a blue deferred image both produce RGB (65535,65535,65535). A deferred
blue image with one transparent pixel produces (0,0,65535). Independent Xlib
queries reproduce all three results on 24-bit and 8-bit Xvfb. The image-owner
fixture activates the existing asynchronous scheduler after normal startup;
the native PNG path remains the existing loader. These controls are recorded in
[the shape/average report](../verification/VERIFICATION-2026-10-06-COLORSET-SHAPE.md).

The legacy colorset code only collects average pixels when both its image and
mask readbacks exist. An opaque image has no mask, so it falls through to white.
The proposed policy makes an opaque red/blue image average to red/blue instead.
Themes can depend on the old white result despite the documented meaning of
Average. This is a visible compatibility change, not just an internal cleanup.

On palette-limited displays, source RGB averaging can also differ from averaging
already quantized/dithered server pixels. Partially transparent pixels below a
backend's binary-mask threshold can now contribute to the average. Their weight
is still one pixel; alpha does not darken their RGB contribution. The final color
continues to be constrained by the display's available colormap.

## Why recommend this

The manual describes Average as the pixmap's average color before Tint. Source
summaries make this behavior consistent, remove image-sized XColor arrays and
full-pixmap XGetImage/XQueryColors loops from file-image completion, and bound CPU
work under the existing scheduler. Keeping sums with their immutable owner also
avoids repeated scans when colorsets share an image. Derived tint resources must
still be acknowledged before publication; this decision alone does not enable
the asynchronous core or finish colorset preparation.

## Alternative

Preserve historical white-on-opaque behavior and exact display-quantized masked
averages. This requires a bounded rendered-pixel/readback continuation, including
server color lookup and palette behavior. It is possible, but adds more state and
round trips and retains the opaque-image anomaly. Existing ordinary loader,
root-pixmap and gradient behavior must remain available until their replacement
contracts are implemented and verified.

Root [AGENTS.md](../../AGENTS.md) requires: “A specific compatibility break
requires an explicit user decision.” The user approved the changed opaque,
partially transparent and palette-limited Average results above on 2026-10-06. Focused controls
are evidence of the current behavior, not approval of the proposed replacement.

Implementation and exact focused evidence: [Average checkpoint](../verification/VERIFICATION-2026-10-06-COLORSET-AVERAGE.md).
