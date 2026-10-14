# Decoded RGB image averages — 2026-10-06

Tracker: IMAGE-01 / INPUT-01. Status: approved, implemented, focused checks
passed; combined qualification and production asynchronous activation pending.

The user approved source-RGB `bg Average` on 2026-10-06. File-backed color
pictures now retain the same summary through ordinary PNG/XPM/SVG conversion and
the deferred ARGB builder. Sums are accumulated during existing conversion, before
palette reduction; there is no second whole-image scan. Fully transparent pixels
are ignored; each other pixel contributes equally, without alpha weighting.
16-bit means preserve fractional source-channel precision. Checked 64-bit sums
and a count need constant memory; overflow refusal preserves accumulator state.

Immutable backing owns the result. Views copy summary values with their backing
and snapshots retain them through replacement/destruction. Cache hits and pending
completion use the same contract. Empty summaries are distinguishable from absent
metadata and preserve the white fallback. Bitmap conversion, generated gradients
and externally supplied root pixmaps retain their separate legacy paths.

Colorsets use these summaries instead of allocating an image-sized XColor array
or reading the file-image pixmap/mask back and querying all its colors. Explicit
background colors, Tint exclusion and later bgTint remain. The manual documents
the approved opaque/palette/partial-alpha differences. No packet format, runtime
library or worker count changed. Derived background/tint publication is still
separate work; this checkpoint does not enable production asynchronous loading.

## Source and focused verification

Frozen source: `builddir/codex-2026-10-06/svg-colorset-average/source-colorset-average-final.tgz`.
SHA-256: `7ced858a7de255e67f39bfd3d7ac048520360b5b0ab69cdba4e6f929fb499c98`.

The archive extends the shape checkpoint with 11 owned files, filtered library
and test Meson changes, and the Average manual paragraph. Three new files are the
pure policy helper/header and its native test. Other thread work is excluded.
Metadata verifies all 1243 source hashes/modes in both source trees, plus 12 built
binary identities per profile. These are build identities, not staged installs.

| Profile | Result |
| --- | --- |
| Ubuntu Linux ARM64, Clang 18.1.3, private PNG 1.6.59 | 15/15 affected policy/image-owner Meson entries and one direct 8-bit colorset run pass: 16/16 |
| Same source, scoped local ASan/UBSan | 8/8 direct/policy entries with leaks enabled, 7/7 actual-core entries plus one direct 8-bit colorset run with core leaks disabled pass: 16/16 |

Normal evidence is `evidence-normal.tgz`; sanitizer evidence is
`evidence-sanitizer.tgz`, beside the frozen source. Final normal logs are
`normal-fvwm-average-final-tests.log` / `normal-fvwm-average-final-8-tests.log`;
sanitizer acceptance logs are `sanitizer-fvwm-average-asan-direct-tests.log`,
`sanitizer-fvwm-average-asan-core-tests.log` and
`sanitizer-fvwm-average-asan-8-tests.log`. Matching unsanitized finite-memory
workers are used with instrumented parents.

Coverage includes accumulator limits/invalid states, empty input, fractional
means and partial alpha; bounded multi-tile conversion at 24/8-bit and strict
palette profiles; ordinary XPM source means and snapshot/view lifetime; actual
opaque native PNG/deferred colorset colors, partial-alpha straight RGB, empty
white fallback, explicit background, Tint exclusion and shared-cache consistency.
Affected decor/menu/file-icon/mini-icon/root/cursor owner regressions also pass.

Independent XQueryColor controls report native opaque red PNG as red, deferred
opaque blue as blue, masked blue as blue, and half-alpha source RGB (128,192,64)
as (32896,49344,16448) on both 24-bit and 8-bit X. The historical white-on-opaque
controls remain preserved in the preceding frozen shape report.

The first compile invocation preceded Meson regeneration and could not find the
new target; explicit incremental reconfiguration registered it. Final acceptance
uses the reconfigured build. Added C/header lines satisfy width/whitespace rules.

## Remaining work

Derived background/tint resources need bounded preparation and acknowledgement
before publication. Full Linux/BSD/install/default-server/optional qualification,
real SVG cursor checks, enabled-core responsiveness, live mixed-monitor movement
and native Linux release sanitizer CI remain unclaimed. This focused checkpoint
keeps the user's combined-matrix cadence.
