# Command and update type boundaries — 2026-10-19

MAINT-02 #14 / CLEAN-01 #34. Base: merged PR #120,
`b0b0998fa00e273bc24f06bdab5ff2d67c634d12`. This is one coherent three-story
development-speed checkpoint. No runtime algorithm or configuration changes.

## Result

Three command/update dependency improvements pass 100/100 affected native entries across six targets/ten profiles. All 5864 object pairs and ten WM executables retain identical allocated code/data sections; 249 installed-output pairs preserve their code/data or non-ELF bytes. Ten scoped stages verify 249 code/catalog identities. Parser, complex-function and update-plan layout edits now compile six, four and four objects instead of 204–209, 71 and 116–121. Broader PR120 runtime evidence/failures and required OpenBSD CI/SVG/install gaps remain without retries.

| Improvement | Previous dependency fanout | Final layout-change compilations |
| --- | --- | --- |
| Parser context | 204–209 objects via common `fvwm.h` | 6: production/assertion parsing, expansion and command engine |
| Complex-function records | 71 objects via command API | 4: production/assertion command engine and function store |
| Window-update plan | 116–121 objects via screen/style umbrellas | 4: production/assertion style computation and update application |

`fvwm.h` now forward-declares the tagged parser context. Only three C owners
include its complete definition. Command APIs retain the complex-function
pointer type; `functable_complex_types.h` supplies record fields to two owners.
`update_types.h` supplies the existing update-plan fields to its two owners,
while `update.h` retains API declarations and an incomplete plan type. The
unused screen umbrella include is removed. Existing record field types, order
and semantics are preserved; the parser definition only gains a struct tag.
A declaration comparison checks these invariants independently of compilation.

These measurements count actual Ninja dependencies and dry-run compilation
work after touching one layout header, then restoring its timestamp. Linking
shared core consumers still occurs (32–37 links in measured Linux profiles);
this does not eliminate link work or establish a universal timing improvement.
The first incremental ARM GCC compile took 14.734 seconds, an observation only.

## Qualification and reuse

One affected checkpoint covers Linux ARM64 GCC/Clang/software/minimal, Linux
AMD64 GCC/Clang, and default FreeBSD/OpenBSD on both architectures. At most two
heavy guests run together, with Ninja using two jobs each. Existing owner builds
are reused. This source-only declaration/header change uses exact code/data
parity to retain the broader runtime/default-X/sanitizer evidence from PR120;
it does not repeat a full runtime matrix for each item or for PR creation.

- 100/100 native entries pass, ten per profile: execution-context ownership,
  module-command writes, style matching/profile, window updates, style index
  retention/maintenance, style command ownership/update work and EWMH state.
  Actual-core nested-X probes remain assertion-enabled and preserve their
  production core objects, linear/collision checks and ordering observations.
- All 5864 object pairs have exactly equal allocated ELF sections;
  ten WM executables and 249 install-output pairs
  retain allocated sections or exact non-ELF bytes. No comparison-only
  recompilation, line-number exception, section-name normalization or protection
  change is used. Debug information/file hashes may differ after header moves.
- All 2479 registered test entries remain unchanged across the
  ten profiles. Build configuration, default targets, compiler flags and the
  assertion/fault-wrapper compilation contracts remain unchanged.
- Ten fresh scoped stages verify 249 installed code/catalog
  identities after Meson's normal RPATH adjustment. Installed version probes
  and restored Ninja no-op checks pass in every profile. This is scoped install
  evidence, not closure of the earlier broad OpenBSD AMD64 install gap.
- All 2025 frozen regular input hashes/modes match in all six
  guests after verification. Native source synchronization only replaces exact
  predecessor or candidate inputs, and changed files receive fresh mtimes.
- `make verify` runs once: 166 passes and 11 explicit native-Meson capability
  skips on macOS. Final upkeep uses foundation/link/whitespace/source checks.
  Review confirms no runtime statements or record fields changed.

The initial ARM GCC runner selected `command-client-xvfb`, unavailable in that
profile. Meson refused the selection before executing any tests (0.12 seconds).
The completed compile and raw failed invocation are preserved. Replacing that
selection with the registered module-command-write check runs the intended ten
entries once; no successful suite is repeated. Native compiler warnings remain
in the machine receipt/raw logs; no warning names either new layout header.

ARM guest storage was bounded by removing only one completed duplicate PR120
archive after its exact size/SHA256 matched the canonical host evidence. The
host archive and all older source/build evidence remain intact. Other owners'
files, VMs, disks and build trees are untouched.

## Source and evidence identities

Signed author-DCO runtime: `c555a968f30033c42bf98c86dc1c5431b20bce53`;
tree `09c61575dfea44f1d4d37a94327dae73058806f2`. Frozen archive SHA256:
`5c81ca4a8ac621d9f6acbd1c814459c98d70bd329573f1d76b2afbb723e7f0c7` (2025 regular inputs).
Final upkeep changes only Markdown and the machine receipt.

[Machine receipt](VERIFICATION-2026-10-19-COMMAND-TYPE-BOUNDARIES-COMBINED.json) records per-target archive hashes,
source-manifest digest/commit binding, dependency counts, profile tests/parity/stages and
compiler diagnostics. Canonical raw archives, runner scripts, declaration
review, portable logs, initial failed selection and storage receipt are under
`builddir/codex-2026-10-19/command-type-boundaries` in the owning workspace.
Each archive binds baseline/final objects, source manifest, executed script,
commands, test logs, install identities and restored dependency dry runs.

## Retained limits

[PR120 runtime scope](VERIFICATION-2026-10-19-STYLE-INDEX-RETENTION.md) remains
2029/2030 original executed standard plus 447/447 optional entries. Its original
OpenBSD AMD64 legacy-property first-MFL-connection refusal remains failed in raw
scope; its single unchanged control passed all five checks, with cause unproven.
This checkpoint neither retries nor reclassifies that observation. Required
CI-01 worker and IMAGE-01 SVG omissions retain their predecessor identities and
OpenBSD AMD64 broad install/two staged-geometry gap. Prior timing failures and
controls remain with their exact source-bound reports.

Local Linux execution uses the existing macOS-hosted Linux environments, with
BSD native kernels under QEMU. Native-CPU Linux sanitizer, physical hardware,
actual toolkits/compositors and release acceptance remain separate. No new
sanitizer/default-server run is claimed. Actions stays disabled; VESA #93 remains
deferred. The broad MAINT-02, CLEAN-01 and STYLE/STATE owners remain open.
