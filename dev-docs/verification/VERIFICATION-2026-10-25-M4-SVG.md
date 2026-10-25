# M4: consolidate the existing librsvg/Cairo renderer

Status: **combined local acceptance passed; review/integration pending**.
Seven profiles validate 62 unit entries, 20 real-X fixture dispatches, 144 exact
M3/M4 frame comparisons, 24 paired negative controls and the SVG-off refusal.
Seven full installs bind 112 native executable identities and 678 regular files
/ 419,547,205 bytes across the captured profiles. These totals describe the
matrix, not a distribution-size improvement.

[Machine manifest](VERIFICATION-2026-10-25-M4-SVG.json) ·
[Current dependency/license/cost inventory](DEPENDENCY-INVENTORY-2026-10-25-M4.json).
Archive SHA256/CRC, every payload hash/size/mode, source/native/stage guards,
actual commands and the original failures are independently reconciled.

Owning scope: SVG-CONSOLIDATE-01 #183 under LIB-04 #46, selected after the
owner-authorized PR207 merge at `14604024459b1504869d456d009433027f7f3b2a`.
Its canonical signature, author DCO and exact reviewed tree pass. #184/#49/#47
closed completed; the readback confirms 64 open issues. M3's accepted results
remain preserved. Raw local M4 evidence, abbreviated **W**, is under
`builddir/codex-2026-10-25/m4-svg/`.

## Finite removal and API contract

The existing modern renderer requires librsvg >=2.52 and Cairo. The intrinsic
pixel-size function was [introduced in 2.52](https://gnome.pages.gitlab.gnome.org/librsvg/Rsvg-2.0/method.Handle.get_intrinsic_size_in_pixels.html);
[document rendering predates it](https://gnome.pages.gitlab.gnome.org/librsvg/Rsvg-2.0/method.Handle.render_document.html).
This is an API minimum, not an assertion that upstream still maintains 2.52.
Use distributor-supported, patched packages. Current upstream
[maintenance policy](https://gnome.pages.gitlab.gnome.org/librsvg/devel-docs/supported_versions.html)
and separate vendor/advisory qualification remain distinct from this cleanup.
No package upgrade or new renderer is adopted.

| Target | Observed librsvg API metadata | Observed Cairo metadata |
| --- | --- | --- |
| fvwm-x-linux-arm64-svg | 2.58.0 | 1.18.0 |
| linux-amd64 | 2.60.0 | 1.18.4 |
| freebsd-arm64 | 2.62.3 | 1.18.2 |
| freebsd-amd64 | 2.62.3 | 1.18.2 |
| openbsd-arm64 | 2.63.2 | 1.18.6 |
| openbsd-amd64 | 2.63.2 | 1.18.6 |

These are selected native pkg-config observations, not minimum-version or
vendor-security verdicts.

The coherent batch removes:

- The `cairo`, `cairo-svg` and `libsvg-cairo` backend-selection options and search
  loop. `svg=enabled/disabled/auto` is the sole feature selector; enabled SVG
  requires full librsvg/Cairo dependency objects and matching headers/libraries.
- Pre-2.32 incremental parser/base-URI fallback and pre-2.52 dimension/render
  alternatives. The retained stream's `GFile` base preserves relative resources.
- Deprecated dimension/helper aliases and unused SVG-disabled renderer stubs.
  A function-local dimension record retains the same integer width/height and
  double em/ex fields. `USE_SVG` and used renderer wrappers remain.
- Old GLib/librsvg initialization definitions, all three calls, and their
  initializer-only includes, plus an unused root-setter include.
- Initializer-only SVG/Cairo runtime edges from the core and graphics/text
  consumer groups. The worker and direct-render test retain full package/static
  transitives. General compile-only visibility remains intact.

The existing viewport, geometry, mirror/rotation/scale, alpha conversion,
limits, refusal and ownership algorithms are unchanged. No FVWM configuration
migration, toolkit, compositor, event-loop rewrite or new dependency download.
Installed AsciiDoc, INSTALL, BSD package instructions/preparation, feature
summary and native gate invocations follow the single backend. Old saved build
commands must drop retired options; a fresh build directory avoids stale cache
options. Only this chat's reused verification caches receive explicit recorded
three-key migration, preserving original bytes and every other option.

## Requirement-to-evidence map

| #183 requirement | Evidence / current disposition |
| --- | --- |
| Header/symbol/package trace and API floor | Exhaustive caller trace; M3 canonical binaries and current selected provider metadata; native link-only API/header check. 2.52 metadata boundary controls use current libraries, not an actual minimum-release execution. |
| Remove obsolete machinery; keep disabled SVG and limits | Source diff plus enabled worker comparisons and Linux ARM64 SVG-off worker refusal. Worker process/resource/deadline policy stays unchanged. |
| Options, recipes, manuals and inventories | Updated Meson/package/native-gate inputs and installed AsciiDoc; affected manual render passes. Six-target current dependency/license/size inventory is published. |
| Artwork/resources/fallback/alpha/scale/refusal/ownership | Selected direct renderer units, real-X image-input, SVG-isolation and image-effect controls; per-target canonical M3/M4 pixel comparisons. All six enabled targets pass. |
| Installed/transitive costs without renderer adoption | Canonical M3 installed/linkage records versus current native16 maps, loader identities and actual full installs. No LunaSVG or Cairo replacement; Final aggregate passes. |

## One combined affected checkpoint

The matrix selects six SVG-enabled default targets and one Linux ARM64
`svg-off` profile, rather than repeating all fifteen M3 profiles. The off profile
keeps PNG/XPM and other automatic features available; it is not the old all-off
reduced profile. It reuses this chat's completed reduced cache to avoid another
build tree in the space-constrained container. Exact options are captured.

Each default runs nine relevant units and three staged real-X fixture dispatches:
`image-input-xvfb`, `image-svg-isolation-xvfb`, `image-effect-xvfb`. They cover
opened-file/relative-resource PNG/SVG handoffs, available PNG/XPM/bitmap paths,
alpha/geometry/effects, malformed and special-input refusal, bounded secondary
waits, core service, timeout/cancellation and child cleanup. Feature absence is
recorded: FreeBSD's profile does not claim an unavailable XPM decoder.

The SVG-off profile runs eight units, two X fixtures and an explicit installed
worker request that must exit 2 with zero protocol output. No SVG-render pass is
credited to it. Six enabled profiles additionally compare 24 representative
frames byte-for-byte against each architecture's canonical M3 installed worker,
plus four paired negative controls. Twenty-three frames require nonempty output;
one known M3 masked/filter/transform result must remain completely transparent.

Tracked source and complete install inventories contain no shipped SVG assets.
The generated SVGs are verification fixtures, not new product artwork. Solid,
alpha, gradients, text, clipping/filter and relative-PNG inputs exercise four
scale/transform variants. Captured PNGs are actual worker output, not new UI or
a theme qualification. Existing UI/theme results retain their original scope.
No general SVG conformance claim follows from equality with an existing renderer.

Full `meson install --no-rebuild --destdir` stages include unchanged Go, retained
Perl, configuration and data. Native16 ELF identity checks permit only Meson's
verified install RPATH removal/mode normalization; other bytes must match.
Installed files remain exact throughout tests. Pure tests retain their declared
worker environment; staged X and explicit worker comparisons cover installed
paths. No unrelated Go/race, session, default-BSD-server or sanitizer matrix is
repeated for branch/alias removal with unchanged lifetimes and process policy.

Nine native configuration controls pass: three retired-option refusals; old
metadata refusal; accepted floor metadata with current libraries; mismatched
header and library refusals; missing Cairo refusal; SVG-off success without
renderer metadata discovery. These fixtures do not establish execution on actual
librsvg 2.52. Eight existing native-gate driver unit tests and shell syntax pass.
The affected manual renders with Asciidoctor 2.0.20; its new API/option text is
present in the 82,390-byte result. Canonical control/manual archive SHA256:
`99eb00f406273bdc4102fffcbafe2ab423a40776ea39f01550c96eec6b28578b`.

## Measured costs and license provenance

The same enabled profiles retain their direct NEEDED and resolved runtime
library sets; only unnecessary source/link ownership disappears. Installed
native16 file sizes change as follows (bytes, including debug/metadata):

| Target | M3 native16 bytes | M4 native16 bytes | Change |
| --- | ---: | ---: | ---: |
| fvwm-x-linux-arm64-svg | 20,247,864 | 20,246,912 | -952 |
| linux-amd64 | 19,540,200 | 19,539,352 | -848 |
| freebsd-arm64 | 19,531,792 | 19,531,712 | -80 |
| freebsd-amd64 | 19,501,088 | 19,501,008 | -80 |
| openbsd-arm64 | 18,892,976 | 18,892,856 | -120 |
| openbsd-amd64 | 18,213,032 | 18,212,896 | -136 |

These small file reductions do not establish faster rendering or less runtime
memory. SVG-off costs include a feature-selection change from the canonical M3
default and are not reported as cleanup-only savings. Current complete install
and loaded-library identities remain in the machine records.

Linux and FreeBSD current library payloads exactly match retained M3 hashes and
can retain that package/notice provenance. OpenBSD ARM64 records current hashes
with retained component/source locators where older hashes were unavailable;
its private PNG receipt hashes match. OpenBSD AMD64 matches 47 of 49 current
library paths; libc and ld.so bytes differ and are recorded without attributing
a cause or inheriting exact-payload notice verification. No security setting is
changed to make bytes match. Own executable images printed by OpenBSD ldd are
separated from third-party library inventory. Unknown ownership/notice fields
stay unknown; no new legal or advisory certification follows. Header-only and
build/interpreter floors retain the prior LIB-05 inventory, with the SVG API
minimum updated here.

## Failures, reuse and remaining boundary

The first two ARM comparison helpers incorrectly required every transformed
frame to be visible. Their canonical M3 workers already produced fully clipped
output for the masked transform, before M4 was compared. Preserve both initial
archives and diagnostics. The corrected fixed-case oracle requires zero alpha
on both versions and exact bytes; other 23 visible frames keep their expectation.
Only that comparison repeats, after guards prove original source, built/staged
native16 and complete installed inventories unchanged. Passing build, map,
install, nine units and three X fixtures are retained. No production parsing or
rendering expectation is relaxed to hide a new regression. The existing clipping
behavior remains a limitation for the separately bounded renderer work.

The first OpenBSD AMD64 job exceeds its preparation reboot deadline before any
M4 run. Preserve its status, preparation/reconnect logs and key cleanup, with zero
native-test credit. One fresh job uses identical source, SDK/cache, recipe,
deadline and QEMU hardware inputs; only job paths/SSH port differ. Its actual
native acceptance passes, with normal QEMU exit and removed job keys. The six
completed profiles are not rerun; the original attempt earns no native credit.

#183 meets its selected criteria and is linked for closure on PR merge after
final review. It stays open during review. #46's later LunaSVG comparison remains open and separate; #8 image,
#11/#13 advisory, native CPU/hardware/toolkit and stage/release gates remain
unclaimed. No speed or package-size gain is inferred from fewer link operands.
Performance reference PR204 stays 2/5 before this implementation merge, 3/5 on
merge; no benchmark build/comparison is due.

Separate source review finds no defect; final archive/inventory/candidate review
is required before signed delivery. Requested routed model settings are unverified where the client
provides no observed metadata. Agent review is not independent human approval.
