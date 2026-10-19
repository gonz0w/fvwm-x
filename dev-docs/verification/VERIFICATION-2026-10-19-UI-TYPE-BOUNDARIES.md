# Font, picture and monitor type boundaries — 2026-10-19

MAINT-02 #14 / CLEAN-01 #34. Base: merged PR #122,
`52a67b1bc7a35b489eaf4f19a20e6910bea4bf57` (same tree as tested
predecessor `9f589584fbd2a955dfbc8ebea898fc4e39cb714b`). One coherent
three-story development-speed checkpoint; no runtime/configuration changes.

## Result

Three font/picture/monitor dependency improvements pass 170/170 affected native
entries across six targets/ten profiles (17 focused entries retained by exact
source/binary identity, 153 fresh). All 5874 object pairs, ten WM executables
and 249 install-output pairs preserve allocated code/data or non-ELF bytes. Ten
scoped stages verify 249 code/catalog identities. ARM GCC layout fanout falls
from 252/289/226 to 49/66/92 objects. Broader runtime evidence and required
OpenBSD gaps remain source-scoped to PR122.

| Record boundary | Previous object dependencies | Final dependencies / dry compilation tasks |
| --- | --- | --- |
| `FlocaleFont` | 246–252 via `Flocale.h` | 48–49 via `flocale_font_types.h` |
| `FvwmPicture` | 278–289 via `PictureBase.h` | 60–66 via `picture_types.h` |
| `struct monitor` | 221–226 via `FScreen.h` | 92 via `monitor_types.h` |

The API headers retain the same tagged pointer types and declarations. Complete
record definitions move unchanged to explicit field-owner headers. Native
FvwmIconMan/Pager embedded records include the complete picture layout. All
104 C consumers change only include directives and nearby blank separators;
physical C line counts remain unchanged, preserving assertion-line constants.
Field names/types/order, layouts, allocation/lifetime, native packet metadata,
compiler options and runtime statements remain unchanged. No configuration or
user-option migration is introduced. API header edits can still rebuild their
API consumers; the saving applies to record-layout edits in the new headers.

Ninja dependency lists are compared over the same full object universe, including
non-default objects. Touching each new header, dry-running Ninja and restoring
its exact timestamp confirms those compilation counts. Shared consumers still
link: 116–127 link tasks per dry run. The change reduces compilation fanout,
not all build work, and does not establish a universal wall-clock speedup.

## Affected qualification and reuse

The frozen checkpoint covers Linux ARM64 GCC/Clang/software/minimal, Linux
AMD64 GCC/Clang and default FreeBSD/OpenBSD on both architectures. Existing
owner-controlled incremental builds are reused, with at most two heavy guests
and two Ninja jobs each. Reconfiguration retains the native gate's dependency
refresh. Every baseline object target is explicitly built, including objects
excluded from the default build, before comparisons.

- 170/170 native entries pass: EWMH state, window updates, style profiles, module
  packets, font shadows/text and shadow consumers, picture cache/average/color,
  monitor scale/topology/ownership/configuration, FScreen topology adapter,
  module monitor and pager monitor. Each profile has the same 17 named entries.
- Linux ARM GCC's successful focused 17 are retained after reconfiguration only
  when all 18 actual binary hashes and the frozen source match. The other 153
  entries execute fresh. Meson's prerequisite build remains for fresh tests.
- All 5874 object pairs and ten production WM executables have exactly
  equal allocated ELF sections. All 249 install-output
  pairs preserve allocated sections or exact non-ELF bytes. No assertion-line
  exception, comparison-only recompilation or section-content normalization is
  used. The ELF build-ID note is excluded; debug information/full hashes may
  differ after header moves. The comparison preserves section types/flags/sizes.
- All 2479 registered test entries and all compiler command
  databases are unchanged. Every profile retains 75 assertion-enabled actual
  core compilations; the six Linux profiles retain two fault-wrapper compiles.
- Ten fresh scoped install stages verify 249 code/catalog
  identities after Meson's normal RPATH adjustment. Installed version and final
  restored Ninja no-op probes pass. The OpenBSD AMD64 scoped stage does not
  close its predecessor's broader full-install/staged-geometry gap.
- All 2033 frozen regular input hashes/modes pass six postchecks.
  Baseline source proofs bind all 2028 predecessor inputs; baseline WM hashes
  equal the prior qualified profile identities. Actual commands, native runner,
  source manifests, object comparisons and install identities are archived.
- `make verify` runs once on macOS: 166 passes and 11 explicit native-Meson
  capability skips. Local declaration/source review passes with no findings.
  Final Markdown/JSON upkeep uses foundation/link/whitespace/source checks.

This declaration/include-only change retains PR122's broader runtime,
default-X and sanitizer evidence by exact code/data parity across all ten
profiles. It is affected qualification, not a fresh execution of those suites.
No full runtime matrix repeats per story or merely for PR creation.

The initial development build intentionally exposes incomplete field consumers;
its compile errors and subsequent include migration are retained. The first
line-preservation helper refused one unmatched include separator after partial
edits; the completed builds and correction remain recorded. Final source has
exact C line counts and strict object parity; no native test failed or was
retried for a green result. A capacity guard initially refused before combined
source sync. Only completed task-owned duplicate guest archives were evacuated
after exact size/SHA256 matched canonical host copies. Source/build trees,
VM disks and other owners' files remain; 1 GiB owner / 256 MiB temporary-space
floors are unchanged.

## Source and evidence

Signed author-DCO tested input commit: `1ec8cebab2d001e942c712ec61fe5034604bda0a`;
tree `5a44496de3080c501837f9c26388e29de2f4e5fa`. Rebased signed input:
`654e83831b45abda9545bf00a3f7325f3d90c134` has that identical
tree and all 2033 exact input hashes/modes. Frozen archive SHA256:
`b9a2bfe8fb7ca8c06c7fc76e19c51d0b8c9890ac764e5e4c2a8c04ce8ca48141` (2033 regular inputs).
Final upkeep changes Markdown and the machine receipt only; rebasing onto the
merged predecessor retains those exact production input hashes/modes.

[Machine receipt](VERIFICATION-2026-10-19-UI-TYPE-BOUNDARIES-COMBINED.json) records profile counts, dependencies,
source/commit binding, executed-script and canonical archive hashes, installed
identities and compiler diagnostics. Raw evidence remains under
`builddir/codex-2026-10-19/ui-type-boundaries` in the owning workspace, including
all original development/capacity attempts. No compiler warning names the three
new layout headers; existing diagnostics remain visible in the receipt/logs.

## Review closure and retained limits

The user reviewed the isolated PR122 QEMU/VNC desktop and reported it was very
fast and appeared to work perfectly. At the user's request, its VM and VNC
bridge were gracefully stopped; its disk/logs and the original qualification
VM remain. This manual profile feedback is not physical-device or release
acceptance. The shutdown receipt remains in the predecessor's `interactive/`.

[PR122's original scope](VERIFICATION-2026-10-19-EWMH-PUBLICATION.md) remains
2029 executed standard and 447 optional passes, four scoped sanitizers, nine
fresh full stages and four authenticated default BSD displays. Three required
OpenBSD AMD64 omissions stay open without retries: CI-01's worker timeout,
IMAGE-01's SVG/MFL startup refusal and PR120's legacy-property first-connection
refusal. The latter's one already-completed unchanged control passes its five
checks with unproved cause; it is not rerun or rebound here. Broad OpenBSD AMD64
full install and two staged-geometry probes remain unperformed. Earlier timing
failures/controls retain their original reports and source identities.

Linux environments are macOS-hosted; BSD native kernels run under QEMU.
Native-CPU Linux sanitizer, physical hardware, actual toolkits/compositors and
release acceptance remain separate. No fresh sanitizer/default-server run is
claimed. Actions stays disabled; VESA #93 remains deferred. Broader MAINT-02,
CLEAN-01, STATE-01 and STYLE-01 owners stay open.
