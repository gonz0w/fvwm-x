# Colorset shape ownership and Average controls — 2026-10-06

Tracker: IMAGE-01 / INPUT-01. Status: implemented, focused checks passed;
production activation and combined qualification remain pending.

## Change and ownership

Colorset shapes now borrow a retained immutable picture's depth-one bitmap or
mask. They no longer create a duplicate pixmap and copy its pixels. Deferred
publication uses the loader's existing X acknowledgement, with no new XSync at
that completion boundary. Ordinary command processing keeps its existing final
publication barrier. The `shape_picture` field is core-only ownership state;
the colorset module packet and installed Perl format are unchanged.

Replacement/NoShape moves the complete old backing into the existing delayed
cleanup mechanism, preserving old module packets and shared cache references.
Pending replacements retain the previous shape, and opaque deferred shape
refusal retains it as before. Clearing one colorset cannot free the same mask
still published by another. The existing picture owner, cache and child worker
are reused; no new scheduler or runtime dependency was added.

The cursor fixture now separates the independent Xcursor library's ARGB support
from FVWM's compiled XRender feature. In the mixed optional profile, native
Xcursor still supplies the half-alpha color cursor while the deferred path uses
the user's approved bitmap fallback. Its XRender refusal wrapper is linked only
when that feature is present. This corrects test linkage/expectations; it does
not change the prepared cursor implementation.

## Exact source and results

Frozen source: `builddir/codex-2026-10-06/svg-colorset-shape/source-colorset-shape-final.tgz`.
SHA-256: `6b78e0c5f55be172650725286f96b206584f39959337ad7410f30e075ba35ce9`.

The archive extends the prior cursor checkpoint with four owned files
(`libs/Colorset.h`, `fvwm/colorset.c`, `tests/live_x/image_colorsets_live.py`,
`tests/test_cursor_image_x.c`) and the cursor test's filtered Meson block.
Other thread changes are excluded. Metadata verifies all 1240 file hashes/modes
in the normal, sanitizer and mixed-profile source trees. Built binary hashes
are recorded separately; this is not installed/default-BSD qualification.

| Profile | Focused result |
| --- | --- |
| Ubuntu Linux ARM64, Clang 18.1.3, private PNG 1.6.59 | 4/4 Meson entries (ordinary owners, deferred colorsets, color/core cursors) and one direct 8-bit colorset run pass: 5/5 |
| Same source, local ASan/UBSan | 2/2 direct cursor entries with leaks enabled, 2/2 actual-core entries and one direct 8-bit colorset run with core leaks disabled pass: 5/5 |
| Same source, GCC 13.3.0, Xcursor enabled / FVWM XRender disabled | 2/2 cursor entries pass, each covering 24/8-bit X |

The finite-memory worker fixture is the matching unsanitized build in sanitizer
runs; instrumented parents own the resources being checked. These are local
Linux ARM64 checks, not the required native Linux release sanitizer CI.

Evidence is retained in `evidence-normal.tgz`, `evidence-sanitizer.tgz` and
`evidence-mixed.tgz` beside the frozen source. Final normal logs are
`normal-fvwm-shape-final-tests.log` / `normal-fvwm-shape-final-8-tests.log`;
sanitizer logs use `sanitizer-fvwm-shape-final-asan-*`; mixed-profile acceptance
is `mixed-fvwm-cursor-mixed-tests.log`. The mixed archive retains the preceding
failed build log, distinct from the repaired build and final passing test log.

## Independent Average observation

The observer uses XQueryColor on actual colorset background pixels, rather than
assuming image colors from command text or copying implementation calculations.
Both 24-bit and 8-bit X report:

| Input with bg Average | Actual background RGB |
| --- | --- |
| Native opaque red PNG | (65535,65535,65535), white |
| Deferred opaque blue image | (65535,65535,65535), white |
| Deferred blue image with one fully transparent pixel | (0,0,65535), blue |

Normal and sanitizer evidence JSON stores these values. The legacy code only
collects pixels when both image and mask readbacks are present; no mask means
zero collected pixels and white fallback. The controls retain this behavior;
no new Average policy is implemented in this checkpoint.

The [bounded decoded-RGB Average proposal](../decisions/COLORSET-AVERAGE-PROPOSAL.md)
is a separate compatibility decision. It would correct opaque-image results
and change some palette/partial-alpha outcomes, while eliminating file-image
completion's image-sized XColor arrays and readback/color-query loops.

## Repairs and remaining gates

The first compile used an incorrect delayed-cleanup helper name; it was corrected
to the existing SafeDestroyPicture helper before focused checks. The first mixed
cursor build failed because its test linked an XRender wrapper absent in that
profile; feature-conditional test linkage fixed it. No passing result is assigned
to either failed build.

Derived background/tint resources still need prepare/acknowledge-before-publication
and bounded completion processing. The Average decision is pending. Real SVG
cursor/default BSD X, live mixed-monitor movement, enabled-core responsiveness,
full frozen Linux/BSD/install/optional checks and native Linux release sanitizer
CI remain unclaimed. The combined platform matrix is intentionally deferred to
its owning batch checkpoint.

Subsequent decision and implementation: the user approved decoded RGB averages
on 2026-10-06. The [following checkpoint](VERIFICATION-2026-10-06-COLORSET-AVERAGE.md)
implements that policy and records its separate frozen source. Historical controls
above remain evidence of the preceding behavior.
