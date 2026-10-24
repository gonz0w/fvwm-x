# M1 supported runtime baseline

Status: **combined acceptance passed; ready for PR review** for DEP-GO-01 #179,
DEP-XDG-01 #180 and LEGACY-01 #181. [Source-bound receipt](VERIFICATION-2026-10-23-M1-RUNTIME-BASELINE.json)
and [exact disposition/migration inventory](../decisions/M1-RUNTIME-BASELINE-2026-10-23.md).
Remaining broad owners #23/#35/#40/#42 stay open.

Go is required to build the supported utility family, using the existing patched
stable baseline, local toolchain and vendored offline inputs. Runtime machines
need no compiler. The disabled-Go option/install graph, replaced Python/Perl
producers, original Perl settings frontend and both pre-FVWM3 converter
implementations are removed. The small FvwmPerl settings adapter delegates to
native Go/Form; the framework itself stays with M6. No C, rendering, native
packet or window-state implementation changes in this batch.

## Combined verification

| Target | Go top cases / packages | Default/reduced build, staged CLI and native-X | BSD default-X settings |
| --- | ---: | --- | --- |
| Linux ARM64 | 165 / 12 | Pass | Not BSD |
| Linux AMD64 | 165 / 12 | Pass | Not BSD |
| FreeBSD ARM64 | 164 / 12 | Pass | Pass |
| FreeBSD AMD64 | 164 / 12 | Pass | Pass |
| OpenBSD ARM64 | 164 / 12 | Pass | Pass |
| OpenBSD AMD64 | 164 / 12 | Pass | Pass |

Six targets produce **986 Go top-case executions / 72 package outcomes**
(3686 pass events including subtests), 30 native-X utility runs and four BSD
default-server settings runs. Each target runs nine real compiler-policy tests:
missing SDK, rejected old/prerelease SDK, accepted patched SDK with local
selection/no download, removed disabled option and version-boundary controls.
Eight recipe refusal tests run per target. No matrix is repeated per deletion.

Each profile builds all seven utility executables under the required compiler;
reduced profiles disable PNG/SVG/XPM/BiDi/NLS. Fresh tagged stages exercise Prompt,
Command, directory/xlock menus, Script FIFO and the XDG provider. Checks include
52 directory cases, 27 xlock cases, 11 frozen utility observations, independent
Script ownership/deadline/signal controls and 15 XDG CLI cases per target.
The provider's real native Form saves long literal settings, regenerates/exports
and launches exact literal application arguments. PATH sentinels reject retired
Python/Perl/converter commands; the independent test application's absolute
Python shebang remains test tooling. Retired frontend files are absent from the
fresh stage; the retained adapter matches its 12-line native-owner template.

The core, Script and Form binary/configuration identities remain unchanged.
Older AMD64 MFL builds also compile previously merged PR186 `request.c`; this
is reported separately from unchanged-binary retention. M1 adds no C source
change. Existing utility/Prompt protocol and rendering evidence is retained for
unchanged scope; no full core sanitizer or three-theme rerun is inferred from
creating the PR. Current supported OpenBSD 8.0 kernels/Xenocara are used; 7.9
is historical. AMD64 guests use CPU emulation, not a new native-CPU qualification.

The focused Linux ARM64 commands total about 22 seconds. All story changes land
before that combined run. Broader checks run once because this batch changes the
compiler/profile/install contract and #179/#180 explicitly own six-target
acceptance. Ordinary portable batches keep the faster risk-based cadence.

## Packaging, documentation and local review

The signed implementation is `6c0dc1cb28d631c8ff87325546e6b590d7b62c2d`, tree
`50f89e25bc94684f2909574acb7abf7b7d7731c4`, based on merged planning PR188.
All runtime/native-build/test implementation inputs match the frozen 2367-file
candidate. Subsequent manual/reference corrections and one inactive sanitizer
CI example adding the already qualified explicit SDK preparation are recorded
separately. No hosted workflow is enabled; no product gate repeats for that
example-only alignment.
The deterministic committed-source archive is 5274167 bytes, SHA256
`0eacb876e06ce45ebec106481196400435864d91f845266a63417be10efb387b`.
A fresh extracted-source Meson build produces Command/provider and passes the
provider CLI gate; all 27 manuals render as man and HTML plus the HTML index.
1646 release inputs match the qualified candidate, with seven documentation-only
followups. Retired source/build/install/manual/extraction targets are absent.
All 11 surviving old utility fixture values remain exact; new frozen vectors
come from the original Perl programs, never the candidate.

`make verify` counts 196 portable tests on macOS: 181 pass and 15 have
environment skips.
Native Meson/compiler controls run freshly on all six targets instead of counting
those host skips as passes. Local diff, source/reference, manifest, packaging and
manual review pass. Commits are author-DCO and SSH signed. Actions stays disabled.

## Preserved failures and limits

Newer BSD Meson rejects the retired `golang` option saved in the old lab command
line files. Both original failed FreeBSD receipts are retained. Only that cached
option is removed from the owned builds, with exact before/after hashes;
objects, compiler/dependency caches and other settings stay intact. Resume from
configure; already passed compiler/recipe controls are retained. Fresh reduced
and release-source directories prove the new graph independently. INSTALL.md
advises a new build directory when an old cache retains the removed option.
No product implementation or test correction follows the qualification freeze.

The initial repository check catches a README over the 250-word cap and dated
links to retired sources. Tighten the README to 241 words and pin only those
historical links to the pre-removal commit. The complete portable check then
passes; historical evidence content and translations are preserved.

All captures pass gzip EOF/CRC, payload/hash/mode and source-manifest checks.
Owned QEMU guests shut down normally; ephemeral job keys are removed and not
published. After host verification, retire 60 duplicate Linux capture/input
files (17181297 bytes); source, builds/caches, installed stages and disks remain. Captures
and exact commands remain under the canonical host directory in the receipt;
source, stages, reusable builds/caches and disks are preserved. No new UI needs
screenshots; the source-bound Slate/Dune/classic gallery stays current for this
unchanged rendering scope.

M1 closes its named utility/build/install contract, not the entire accumulated
feature cycle or release. M2 runtime controllers, M6 Perl framework, M7 owned
tooling and full milestone/hardware/toolkit/assistive acceptance remain. PR178
stays the broader integrated reference. Performance tuning stays deferred; no
comparison is due (pre-cleanup PR186 reference: 1/5 now, 2/5 on M1 merge).
