# LIB-05 dependency and interpreter floor inventory

Date: 2026-10-25. Owning checkpoint: M3.4 / LIB-05 #47. This is a
source/evidence inventory at `856e9e4efb2a8439f714060fb2dc71273ab7a9cf`
(`codex/m3-closure`, after PR206), not a dependency upgrade, package removal,
new minimum-version policy or new platform qualification. The delegated change
contains this report and two identified current-documentation corrections in
INSTALL.md and the maintenance README. The lead owns M3 acceptance, other
source/manual corrections, tracker and GitHub reconciliation.

The frozen [issue #47](https://github.com/gonz0w/fvwm-x/issues/47) body in
`builddir/codex-2026-10-25/m3-closure/open-issues-before.json` asks for unused
mandatory xkbcommon and actual per-target dependency/interpreter floors,
retaining Xlib/libevent/Xft absent an approved evidence-backed change. The
[Xlib checkpoint](VERIFICATION-2026-10-21-XLIB-DEPENDENCY.md) already accepts
removal of the unused direct xkbcommon edge. This report inventories the
remaining floors and owners; it does not repeat that checkpoint or imply
closure of M4 backend retirement, M6 Perl retirement or M7 build/test tooling.

## Meaning of a floor

A **declared floor** is a source check or explicit documented requirement. An
**exercised version** is a version present in a named retained qualification.
An installed package version alone does not prove it was selected, executed or
that all older versions fail. A declared floor without an actual minimum-version
run remains unverified as a tested minimum. Unversioned discovery is recorded
as unversioned, not as support for arbitrarily old packages. Existing accepted
checkpoints remain accepted within their recorded scope.

## Configure, build and developer tools

The authoritative declarations are [meson.build](../../meson.build),
[meson.options](../../meson.options), [INSTALL.md](../../INSTALL.md),
[SUPPORT.md](../SUPPORT.md) and the [maintenance README](../../tools/maintenance/README.md).

| Requirement | Declared contract and owner | Exercised evidence / limit |
| --- | --- | --- |
| C compiler and system development environment | C11 (`c_std=c11`); supported Linux/FreeBSD/OpenBSD headers, libc and X11 SDK. No numeric compiler floor is declared. Capability probes select hardening flags/functions. | M1 retained compiler versions below; M3.1–3.3 preserve source/compile identities. No minimum GCC/Clang release is established here. |
| Meson | `>=1.5.1`, specifically for configuring Perl files without consuming escaped `@`. `SUPPORT.md` recommends 1.7.0 or supported newer for the Linux gate. | Exercised 1.7.0/1.10.2/1.12.1 below. Recommendation 1.7.0 does not replace source floor 1.5.1. No actual 1.5.1 run is claimed. |
| Python for configure/build/docs | Required `python3`, selectable by `-Dpython`; Meson does not explicitly version-check it. Build wrapper/docs paths declare 3.7+ in maintenance README; `build` executes `build.py`. | M1 Python 3.12.3–3.14.7 below. Python 3.7 is a declared compatibility baseline, not an exercised minimum in this inventory. |
| Python for other maintenance/test tools | README developer baseline 3.9+; standard-library tooling, generators, source/translation checks and test drivers are separate from installed desktop execution. | Existing tooling/M1 acceptance is retained. No fresh Python 3.9 minimum run or universal floor for every auxiliary script is claimed. M7 owns retirement. |
| Perl | `find_program('perl', required: true)` for configured installed framework/shebangs; no configure-time numeric floor. Per-file legacy 5.004 guards are listed below. | M1 configures with 5.38.2/5.40.1/5.42.3. No actual minimum-interpreter proof. Perl remains required until accepted M6 retirement. |
| Go SDK | `go_version.py`: stable 1.26.9+ within 1.26, or stable 1.27.2+. Missing, old, malformed and prerelease SDKs fail. `GOTOOLCHAIN=local`; vendored builds disable downloads. | Accepted actual Go 1.26.9 Linux ARM64 proof and six-target Go 1.27.2 qualification; M1 makes the compiler mandatory. Neither the module's `go 1.21` language directive nor a future newer SDK is a separately tested compiler floor. |
| Ninja and pkg-config/provider | INSTALL lists both; no project numeric floor. Ninja is the documented backend; dependency discovery uses package metadata/system probes. | Ninja versions observed in retained logs include Linux ARM64 1.11.1, Linux AMD64 1.12.1 and OpenBSD 1.11.1. No lowest Ninja/pkg-config release is established. |
| Shell, sed and base utilities | `build`, configure helpers and source enumeration use the system shell/base commands; `sed` is explicitly required. No numeric versions. | Existing build/source-package checkpoints exercise those paths; no standalone utility minimum campaign. These are build/developer requirements, not a new GUI dependency. |
| Asciidoctor | Required only for `mandoc=true` or `htmldoc=true` (both default false); no numeric floor. Ruby/dependencies of the selected Asciidoctor installation belong to that build tool. | Existing six-target tooling rendering and M1's 27 man/HTML manuals are retained. No minimum Asciidoctor/Ruby proof or installed-desktop Ruby requirement. |
| gettext tools | NLS catalogs require `msgfmt` and matching gettext headers/library; translation upkeep additionally uses `xgettext`, `msguniq`, `msgmerge`, `msginit`. No numeric floor. | NLS auto warns/disables without msgfmt; enabled requires it; disabled omits catalog build/install. Developer extraction tools are not blanket runtime requirements. |
| Verification utilities | Selected gates use Python drivers, make, Git, native compiler/linker inspection, X servers/clients and platform tools; advisory/race/sanitizer checks have their own tool owners. | See SUPPORT/BSD and owning reports. Test prerequisites are not installed desktop dependencies; stored recipes do not prove all tools ran or establish their minima. |

The [accepted compiler baseline](VERIFICATION-2026-10-14-GO-BASELINE.md) and
[receipt](VERIFICATION-2026-10-14-GO-BASELINE.json) distinguish the real
1.26.9 Linux ARM64 build (ten packages, 519 test/subtest records, vet, Prompt
peer and source/symbol scan) from policy-boundary simulations. Six actual 1.27.2
SDKs were qualified there. Its earlier optional/disabled-Go behavior is
historical: [M1](VERIFICATION-2026-10-23-M1-RUNTIME-BASELINE.md) supersedes that
profile, retaining the patched compiler policy and removing the disabled option.
The approved Go SDK policy is not a promise that any arbitrary toolchain merely
advertising a sufficient version has been independently qualified.

### Retained per-platform exercised tool versions

These are read from M1's actual `default-configure.log` members; they are not
new executions and not universal minimum claims. M1's source-bound
[receipt](VERIFICATION-2026-10-23-M1-RUNTIME-BASELINE.json) binds the six-target
utility/compiler/default/reduced/install checkpoint. Linux AMD64 uses the
retained Debian 13 lab, while the support architecture baseline remains Ubuntu
24.04. FreeBSD is 15.1; OpenBSD is the current 8.0 baseline. QEMU/TCG evidence
does not establish native CPU/hardware or release sanitizer qualification.

| Target | C compiler version | Meson | Python | Perl | Go |
| --- | --- | --- | --- | --- | --- |
| Linux ARM64 | 13.3.0 | 1.7.0 | 3.12.3 | 5.38.2 | 1.27.2 |
| Linux AMD64 | 14.2.0 | 1.7.0 | 3.13.5 | 5.40.1 | 1.27.2 |
| FreeBSD ARM64 | 19.1.7 | 1.10.2 | 3.12.14 | 5.42.3 | 1.27.2 |
| FreeBSD AMD64 | 19.1.7 | 1.10.2 | 3.12.14 | 5.42.3 | 1.27.2 |
| OpenBSD ARM64 | 22.1.6 | 1.12.1 | 3.14.7 | 5.42.3 | 1.27.2 |
| OpenBSD AMD64 | 22.1.6 | 1.12.1 | 3.14.7 | 5.42.3 | 1.27.2 |

Canonical local root: `builddir/codex-2026-10-23/m1-runtime-baseline/`.
Linux archives are `fvwm-x-linux-arm64-svg-evidence.tgz` and
`linux-amd64-evidence.tgz`; FreeBSD archives are
`freebsd-{arm64,amd64}-evidence.tgz`, each with
`evidence/default-configure.log`. OpenBSD uses
`bsd/openbsd-{arm64,amd64}-job/guest-evidence.tgz`, member
`builddir-bsd/m1-checkpoint/default-configure.log`. The retained original
FreeBSD failed-cache receipts remain failures; the final logs above follow only
the accepted removal of the obsolete cached `golang` option.

## Native package floors and feature boundaries

The core, shared archives, native modules and `fvwm-root` use
`all_compile_deps`, a compile-only projection of found dependencies. The image
worker/direct decoder instead use `image_renderer_deps`, and capture uses
`capture_deps`. The broader compile projection intentionally retains Xt
and Xtrans headers for legacy umbrella/native consumers without injecting
irrelevant runtime link edges. Runtime owners receive **full** dependency
objects, including package-declared static/transitive link requirements.
The archive's extracted objects, not just functions named by the caller,
determine those owners. Global optional system `libm` policy remains unchanged.

| Package/API | Configure contract | Runtime ownership / tested-minimum status |
| --- | --- | --- |
| Xlib (`x11`) | Required, no numeric package floor; compile/link probe requires `X11/XKBlib.h` plus `XkbKeycodeToKeysym`. | Core/image/text/worker/capture/Rearrange owners below. Retained, no xkbcommon direct requirement. Xlib package minimum is not established by the symbol probe. |
| XRandR (`xrandr`) | Required `>=1.5`; product targets X11/RandR 1.5+. | Core, graphics/text and Rearrange; worker/decoder use XRandR headers only. No oldest 1.5 package execution claimed. |
| Xext (`xext`) | Required, unversioned. | Core and image group shared drawing objects. No numeric tested floor. |
| Fontconfig and Xft | Required, unversioned. | Core/text group; Xft's actual FreeType transitives remain full package requirements. Neither is removed. No numeric tested floors. |
| libevent | Required `>=2.0`. | Real Form and MFL calls; not a blanket core/other-module runtime edge. No actual 2.0 minimum run claimed. |
| ICE | Required configure package, unversioned. | Enabled session owners require **both SM and ICE**: core, Buttons and Script. Required discovery is distinct from per-target linkage. |
| Xt and Xtrans | Required, unversioned; real header consumers remain. | Xt `Intrinsic.h` in native/shared headers; Xtrans in `libs/fsm.c`. Compile visibility does not imply every consumer loads libXt. Xtrans is header metadata, not a runtime shared-library owner. |
| FreeType (`freetype2`) | Feature `freetype=auto`, unversioned. | Compile feature plus Xft/Fontconfig package transitives; disabling the direct feature does not erase transitives required by enabled packages. |
| FriBidi | `bidi=auto`, `>=0.19.2`. | Enabled core/text owners; no actual 0.19.2 minimum run. |
| iconv | `iconv=auto`, unversioned. | Enabled core/text owners. BSD ports header/library pair is selected and link-probed together; libc implementation can avoid a separate package. No numeric floor. |
| gettext/intl | `nls=auto`, unversioned, usable only with msgfmt. | Enabled core/localized-text owners. libc/ports implementation varies; BSD paired selection is retained. No numeric floor. |
| libpng | `png=auto`; discovery `>=1.2`, then security floor `>=1.6.59` **or audited vendor security-backport assertion**. | Enabled core/image/capture owners. `png-security-backport=true` is an assertion, not a vulnerability check. 1.2 alone is not an accepted unpatched floor. PNG-disabled capture installs a refusal stub. |
| librsvg and selected backend | `svg=auto`, librsvg `>=2.13.92`; first available of cairo, cairo-svg, libsvg-cairo, each unversioned with auto feature controls. Finding librsvg without a backend errors. | Renderer/worker and graphics initialization owners retain enabled dependencies, including old initializer paths and full GLib/GIO/GObject/Cairo/static transitives. No oldest librsvg/backend proof; M4 owns retirement choices. |
| SM | `sm=auto`, unversioned. | Enabled core, Buttons and Script pair with ICE. No separate SM-only acceptance. |
| Xcursor, Xfixes, XPM, XRender | Individual auto features, unversioned. | Core owns enabled four; image group retains Xcursor/XPM/XRender through shared extracted objects. Xfixes is not copied to unrelated module owners. No numeric tested floors. |
| gzip executable | Auto-discovered system gzip, or explicit trusted `image-gzip` executable; no numeric floor. | Compressed XPM decompression only. Missing automatic gzip leaves uncompressed XPM usable; explicit unavailable path fails configure. No new decompressor library requirement. |

### Actual target owner groups

Source: root dependency-group definitions and target declarations in
[bin/meson.build](../../bin/meson.build), [libs/meson.build](../../libs/meson.build)
and `modules/*/meson.build`. These groups apply to all supported targets;
platform/package metadata supplies actual transitives and feature presence.

| Installed target / archive | Full runtime dependencies beyond system policy |
| --- | --- |
| `fvwm3` | X11/Xext/XRandR, Fontconfig/Xft; enabled FriBidi/iconv/intl/PNG/librsvg+backend/SM+ICE/Xcursor/Xfixes/XPM/XRender. No libevent/Xt bag inherited from the archive. |
| `libfvwm3`, Script `libWidgets` | Compile-only dependencies; archive extraction gives final consumers ownership. They are not installed shared-library runtimes. |
| `FvwmAuto`, `FvwmEvent` | Compile-only dependency visibility; no external package runtime group beyond shared system policy. |
| `FvwmMFL` | libevent. |
| `FvwmRearrange` | X11 and XRandR. |
| `FvwmAnimate` | Image group: X11/Xext and enabled PNG/Xcursor/XPM/XRender. |
| `FvwmBacker`, `fvwm-root` | Graphics group: image group + XRandR + enabled librsvg/backend. |
| `FvwmIconMan`, `FvwmIdent` | Text group: graphics group + Fontconfig/Xft + enabled FriBidi/iconv. |
| `FvwmButtons` | Text group + enabled SM and ICE. |
| `FvwmPager` | Localized text group: text group + enabled intl. |
| `FvwmScript` | Localized text group + enabled SM and ICE; extracted widget/archive objects retained. |
| `FvwmForm` | Localized text group + libevent. |
| `fvwm-image-svg-worker` | X11 for real software-effect calls; enabled full librsvg/backend renderer dependencies. XRandR contributes headers, not a direct worker runtime owner. |
| `fvwm-desktop-capture` | X11 plus enabled libpng (disabled-PNG refusal otherwise). |
| Go utility/controller/worker family | Required SDK at build, no runtime Go compiler/Python/Perl dependency for migrated executables. FvwmPrompt retains its six module pins/vendor payload; other executables' imported package closure is distinct. Optional external service commands remain service-specific requirements. |
| `FvwmPerl`, `fvwm-perllib`, installed `.pm` files and `.fpl` adapter | Perl and the retained FVWM/General framework; not retired by the Go-family migration. Adapter delegates settings work to Go/Form but is itself loaded through FvwmPerl. |

The [M3.1](VERIFICATION-2026-10-25-M3-WORKER-LINKAGE.md),
[M3.2](VERIFICATION-2026-10-25-M3-ARCHIVE-LINKAGE.md) and
[M3.3](VERIFICATION-2026-10-25-M3-MODULE-LINKAGE.md) reports bind extracted
objects, compile/link commands, dynamic and recursive runtime closure, package
metadata and focused X tests. Unchanged loaded closure means narrowing link
operands does **not** justify runtime-package or license removal. Their
actual `libfvwm3`/Widgets extraction, full runtime dependency objects and
`pkg-config --static` inventory satisfy the selected static-library-transitive
contract when the combined gates pass. An all-static ELF executable is
unqualified and outside the selected profile; it is not a waived mandatory
gate. Direct decoder tests own renderer dependencies; other existing test
targets intentionally retain broader declarations.

### Exercised package examples, distinct from minima

M3's retained supplements report the following **ambient pkg-config versions**.
They are metadata observations, not every target's selected provider or a
minimum-version campaign. In particular the actual controlled PNG provider is
private; the ambient libpng numbers below do not override its selected paths,
configuration/security policy or maps. FreeType's pkg-config number is recorded
verbatim rather than relabeled as an upstream release number.

| Package | Linux AMD64 supplement | FreeBSD ARM64 supplement |
| --- | --- | --- |
| X11 / XRandR / Xext | 1.8.12 / 1.5.4 / 1.3.4 | 1.8.13 / 1.5.5 / 1.3.7 |
| Fontconfig / Xft / freetype2 | 2.15.0 / 2.3.6 / 26.2.20 | 2.17.1 / 2.3.8 / 26.6.20 |
| libevent / ICE / Xt | 2.1.13-stable / 1.1.1 / 1.2.1 | 2.1.13-stable / 1.1.2 / 1.3.1 |
| FriBidi / librsvg / Cairo | 1.0.16 / 2.60.0 / 1.18.4 | 1.0.16 / 2.62.3 / 1.18.2 |
| SM / Xcursor / Xfixes | 1.2.6 / 1.2.3 / 6.0.0 | 1.2.6 / 1.2.3 / 6.0.1 |
| XPM / XRender | 3.5.17 / 0.9.12 | xpm.pc absent in ambient supplement / 0.9.12 |
| Ambient libpng | 1.6.48, not selected-private-provider proof | 1.6.58, not selected-private-provider proof |

Canonical supplements:
`builddir/codex-2026-10-25/m3-module-linkage/linux-amd64-supplement.json` and
`builddir/codex-2026-10-25/m3-archive-linkage/freebsd-arm64-supplement.json`.
M1 configure logs separately observe Xtrans 1.4.0 on Linux and 1.6.0 on
FreeBSD/OpenBSD. This bounded report does not manufacture missing per-package
minimum runs for the other architectures or promote ambient missing xpm.pc to
proof that an enabled qualified provider was absent. The combined M3 closure
owns exact six-target compiler/profile/selected-provider/install evidence.

## Remaining interpreter declarations and consistency findings

[perllib/meson.build](../../perllib/meson.build) installs FVWM/General modules;
[modules/FvwmPerl/meson.build](../../modules/FvwmPerl/meson.build) configures and
installs the Perl executable. These are current product consumers, not only
build/test scripts. Core module names, FvwmScript and existing configuration
contracts remain usable under the adopted scope. M1/M2 migration evidence does
not authorize a distribution-wide no-interpreter claim.

1. **Legacy Perl guards are not a verified family floor.**
   `modules/FvwmPerl/FvwmPerl.in:21` and
   `perllib/General/FileSystem.pm:17` declare `require 5.004`;
   `FVWM/Module.pm.in:18`, `Tracker.pm:18`, `Module/Toolkit.pm:18` and
   `Module/Terminal.pm:59` declare `use 5.004`. However Module imports
   `FVWM::Constants`, whose current generated implementation uses
   `use warnings` and `our`; `FVWM::Commands` also uses these constructs.
   A 5.004 guard is therefore not credible proof that this imported modern
   family works on 5.004. No 5.004 interpreter run exists in the examined
   evidence and configure does not validate a numeric Perl floor. Record this
   source consistency gap explicitly; do not adopt a guessed replacement
   version or silently waive framework-owner acceptance. Any correction and
   verification belongs to the lead's bounded reconciliation/M6 owner.
2. **Corrected maintenance README optional-Go statement.** Its final compiler
   paragraph previously called the compiler optional and said Go-disabled
   configuration skips it. It now matches current Meson/M1: required patched
   stable SDK, local selection, offline vendored inputs and no disabled-Go
   option. No compiler policy or historical evidence changes.
3. **Corrected INSTALL acceptance sentence.** The installed desktop controller
   paragraph previously said complete controller/session acceptance remained
   open. It now records the owner's 2026-10-25 M2 acceptance and preserves
   separate native/hardware/release qualification. Historical M1/M2 evidence is
   unchanged. This was an acceptance-status mismatch, not a version floor.

The README's Python 3.7 build/docs versus 3.9 developer distinction and Meson
1.5.1 source versus 1.7.0 Linux recommendation are explicit, different contracts;
they are not automatically inconsistent. Python/Meson developer tooling stays
until accepted M7 work. No new mandatory language/toolkit, no package removal
and no general no-interpreter claim follows this inventory.

## Verification and handoff

This delegated work reads source and retained local receipts/archives only:
no network, VM control, compiler/runtime execution, new dependency scan or
new platform matrix. Minimum-version execution gaps above remain gaps; the
accepted checkpoints are not rerun or downgraded because this prose exists.
Requested implementer route is Sol/high; observed client/model metadata is
unavailable, so routing remains unverified. No nested agent or commit is created.

`make check` and working/staged whitespace checks pass. Relative report links
resolve to current checkout files; source/declaration and retained-log review
passes. These are documentation checks, not native dependency qualification.
The two current documentation mismatches above are corrected; the unqualified
legacy Perl declarations remain explicitly recorded for owner disposition.
The lead must complete the combined M3 evidence manifest and separate review,
then apply the original owner criteria before closing #47/#49/#184. This report
does not itself assert those issues are closed.
