# M3.3: remaining production module dependency owners

PR205 merged as `7476fccc71799f26664f0226c91798fa6d054622`, retaining verified
canonical signature, author DCO and exact candidate tree. Its branch is retired.
This batch replaces broad dependency bags for the remaining nine production
archive consumers and makes Script's widget archive compile-only. M3.1/M3.2
core/worker/other module ownership remains intact; M2 owner acceptance is complete.

| Consumer | Full runtime dependency group |
| --- | --- |
| Animate | X11/Xext and enabled PNG/Xcursor/XPM/XRender |
| Backer, fvwm-root | Image group plus XRandR and enabled librsvg/selected backend |
| IconMan, Ident | Graphics group plus Fontconfig/Xft and enabled FriBidi/iconv |
| Buttons | Text group plus enabled SM and ICE |
| Pager | Text group plus enabled gettext |
| Script | Localized text group plus enabled SM and ICE |
| Form | Localized text group plus libevent |

All targets retain `all_compile_deps` for legacy header visibility. The five
ordinary runtime lists avoid repeating optional-library rules in nine files.
`libWidgets` now uses compile-only requirements, retaining existing source,
archive structure and link order. Full dependency objects preserve package
static/transitive requirements; global math policy remains shared. No C/runtime/
configuration/test source changes, new dependencies or M4 backend retirement.

The groups follow archive-object extraction, not only called function names.
`PictureImageLoader.c` contains cursor APIs alongside PNG/XPM loading, and
shared drawing objects require Xext. `flib_init_graphics` retains real older
`Frsvg_init` paths, so its owners retain full librsvg/backend dependencies even
when modern initialization is a no-op. Buttons and Script extract `fsm.c` through
direct `fsm_*` calls; both SM and ICE are required. Form owns real libevent calls.
NLS uses `nls_enabled`, preserving the missing-msgfmt disabled-NLS behavior.
No unused Xt/Xfixes/session edges are copied into unrelated owners.

## Measured results

The [machine report](VERIFICATION-2026-10-25-M3-MODULE-LINKAGE.json) binds the
ten-file source overlay, both archives, object/compile identities, extracted
members, runtime closure, install mappings and staged hashes. Canonical full
commands, maps, undefined symbols, fixture logs/artifacts and target copies are
preserved locally under `builddir/codex-2026-10-25/m3-module-linkage/`.

| Target | Default library operands | Reduced library operands |
| --- | ---: | ---: |
| fvwm-root / Backer, each | 23→14 | 14→6 |
| Animate | 23→7 | 14→5 |
| Buttons / Script, each | 23→20 | 14→11 |
| Form | 23→19 | 14→10 |
| IconMan / Ident / Pager, each | 23→18 | 14→9 |

Operands include global `-lm`; counts are linker inputs, not runtime packages.
All file sizes remain unchanged. Selected direct `DT_NEEDED`, recursive resolved
library sets/paths and extracted libfvwm3/Widgets object sets stay identical.
Every compile command and generated config hash remains exact in both profiles,
as do ninety shared archive objects, twenty-one widget objects and both archive
hashes. Among 138/128 archive consumers, exactly nine link commands change;
129/119 other consumer commands stay exact. Existing test fixtures keep explicit
broad dependencies; no test-target cleanup is implied. Retained consumer coverage
uses unchanged C inputs/commands/archives, without new direct-object/binary hash
inventories or broad re-execution. Equal file size does not imply equal bytes.

This clarifies build ownership. It does not remove runtime packages, shrink
runtime closure or establish a speed improvement. PR204's existing package/
license inventory is retained; fresh ambient system pkg-config versions and
static metadata accompany this group. The system version supplement does not
requalify the private PNG provider; actual selected private paths are bound by
environment/maps/closure. Without LD_LIBRARY_PATH, reduced text modules resolve
transitive PNG to the system library; those supplemental paths are distinct from
the controlled before/after environment and do not qualify reduced X. No version,
third-party payload, legal or vulnerability
assessment changes. A static executable remains unqualified.

## Focused acceptance and corrected fixture paths

Reuse the owner-controlled Debian 13 Linux AMD64 QEMU TCG caches. Capture both
profiles before applying one frozen overlay, then build/link/load all nine
consumers. Eighteen exact target copies follow Meson's install mappings inside
private capture stages; these copies are not a full `meson install`.

Six existing fixture contracts pass on default:

- Font-shadow refusal/recovery through all six actual text modules.
- Script widget pixels/input and its real widget archive.
- Form timer/service behavior and Pager layout.
- Buttons and Script session handling with retained SM/ICE.

The commands come from Meson's registered fixture definitions and are dispatched
directly with isolated artifact directories. An audit found four `--module`
values remained relative build paths; the first font/widget commands already
used staged copies. All six original invocations passed and their bytes matched
the stage, but the four paths did not satisfy intended staged execution. Correct
only those four paths and repeat only those fixtures, without rebuild or source
changes. Preserve original commands/passes and the mismatch in the first archive;
the second archive binds the four corrected stage identities/commands/results.
Accepted staged coverage composes the first two with the four corrected passes:
six contracts, ten total X invocations. No expectation is relaxed.

Reduced coverage is build/link/load and staged identity only; no reduced X pass
is claimed. Animate, Backer and fvwm-root are freshly linked, mapped and loader-
checked, but not directly executed by this selected fixture group. No new UI or
layout behavior is introduced, so theme/screenshot qualification is not repeated.
Existing fixture artifacts are retained for review, not new feature examples.
No sanitizer expansion is needed for a Meson-only declaration change.

No new platform API/ABI, selected ports/header-library pair or package contract
changes. This batch preserves the full dependency objects already used in
Linux/BSD; no BSD boot or full matrix is repeated. Full six-target/compiler/
default/reduced/optional, offline install and remaining static acceptance stays
at combined M3/#184/#49 closure. Native sanitizer, hardware/toolkit/compositor/
release gates remain separate; this partial checkpoint does not close them.
Performance reference remains merged PR204 at 1/5 after PR205, 2/5 on this merge.
No new comparison or benchmark build is due.

## Review, integrity and cleanup

Separate routed analysis, implementation and source/evidence review retain this
bounded ten-file scope. Actual routing metadata is unavailable; requested models
are unverified, with no cost/speed claim. Repository check/lint and working/staged
whitespace checks pass. All production hashes remain frozen after native tests.

Both canonical archives pass full gzip CRC and all 241 manifest-listed payload
hash/size/mode comparisons, plus the two manifest-file hashes: 243 stored
payloads. Retire only 251 completed guest captures/private stage duplicates/
uploads/archives after exact guest verification, totaling 44,657,580 bytes.
Preserve source, caches, existing install stages, disks and canonical host
archives. The owned VM powers off normally with QEMU status zero.

Next: close accumulated M3 platform/install/static acceptance against these
production dependency owners. Keep backend retirement with M4 and unrelated
source/tooling cleanup with its owner.
