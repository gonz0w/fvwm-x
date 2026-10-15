# Window update ordering contract — 2026-10-15

Owner: DRAW-01 [#25](https://github.com/gonz0w/fvwm-x/issues/25).
Status: all affected contract/fixture checks pass; ready for PR review.
**The broader full suite retains two unchanged timing failures.** Its complete
platform qualification is not claimed by this test/documentation checkpoint.
[Contract](../decisions/WINDOW-UPDATE-BARRIERS-2026-10-15.md),
[machine receipt](VERIFICATION-2026-10-15-WINDOW-UPDATES-COMBINED.json).
PR #97 is merged; OpenBSD/VESA cause investigation #93 stays deferred.

## Change and executable coverage

STYLE-01/STATE-01 need explicit application/observation boundaries before adding
deferred work. This checkpoint records current dirty-state owners, explicit and
outer-loop commit points, retirement, resource and query/publication constraints.
The manual now explains that global updates run at an ordinary event-loop
boundary even with input queued, and RefreshWindow commits pending styles
globally while choosing one redraw surface. Applying state and finishing paint
work are separate operations.

Production C/headers, installed target definitions, configuration syntax and
dependencies are unchanged. No dirty queue, coalescing behavior or measured
latency/CPU improvement is introduced. Broader DRAW-01 optimization and measured
STYLE-01/STATE-01 work remain open.

The fixture links actual core objects, creates two managed clients and executes
Style, UpdateStyles, Refresh and RefreshWindow through the command engine.
Applied border widths, real server frame/client geometry and `_NET_FRAME_EXTENTS`
are checked before/after barriers. It models a borrowed complex-command record
with the existing execution flag, invokes actual retirement/reclamation, then
enters HandleEvents. A wrapped module-continuation observation point confirms
reclamation and ordered final style application precede that point while a
specific input event remains queued. Destruction, style matching/application,
geometry and X property calls execute normally.

Eight named cases cover initial state, deferred styles, explicit/clean repeated
updates, both refresh commands, borrowed-record lifetime, outer-boundary
retirement/application and pending input. Owned Xvfb and borrowed authenticated
default displays use the same fixture/driver. This does not qualify arbitrary
reentrant callbacks, external observer interleavings or every dirty/resource/DPI
domain. Existing image, expose, stack, interaction and module evidence keeps its
separate scope.

## Exact source and affected results

Base: `20eda2943b1effca11174a2577cded6e58a01808` (merged PR #97).
Full archive: `84d548afc5a31c7ef5ce83a6c620bca6673f46cf2ea452a0e94e9c45ad6d1921`;
1947 regular files, merged main plus exactly three test/build paths. The Meson
change is one additive non-installed fixture/test block; existing Meson content
is byte-identical. All other runtime inputs match PR #97's final frozen source
`9ed48f0c34d9d1bf9747b5a3589927b8e706b748059e63e676507672ae02864e`.

Final review wraps two overlong test lines. Override archive:
`b03c6905863767ed05701cb983c1a5c917fd3941f8b771afca960a660a1faaa7`.
C whitespace tokens and Python AST are unchanged. Full input plus these two
exact overrides derives the final candidate; hashes/modes and guest source/
runtime postflight are recorded. Final test/build paths match publication.
Documentation is checked separately; the actual changed manual was rendered
from the signed implementation commit. No full suite is repeated for wrapping.

The new fixture passes in all ten original compiler/feature profiles, four BSD
default displays and one scoped sanitizer invocation: 120 named observations.
The final wrapped fixture adds ten profile entries plus one scoped sanitizer
entry, each exercising the same eight cases. All affected checks pass.

Linux ARM64 Clang ASan/UBSan instruments the supporting core/library objects;
compiler-rt linkage/effective options and exact runner bytes are recorded.
Controlled cores end with `_exit`; leak detection is disabled and errors halt
execution. This is not full shutdown-leak or native-CPU release qualification.

## Broader full-scope results and remaining gaps

| Target / profile | Entries executed | Current pass / fail after explicit controls |
| --- | ---: | ---: |
| Linux ARM64 GCC / Clang | 250 / 250 | 250/0 / 250/0 |
| Linux AMD64 GCC / Clang | 250 / 250 | 249/1 / 250/0 |
| FreeBSD ARM64 / AMD64 | 238 / 238 | 238/0 / 238/0 |
| OpenBSD ARM64 / AMD64 | 238 / 238 | 238/0 / 237/1 |
| Linux ARM64 software / minimal | 229 / 198 | 229/0 / 198/0 |

Across eight standard profiles, 1952 entries execute and the
explicit composition has 1950 passes and 2
failures. Optional profiles add 427 passes. Nine tests fail
in four original full runs; seven recover unchanged. Original logs and each
subsequent outcome remain intact. Composed entries come directly from their
original or named control records; they are not a claim that the original full
runs passed. No timing bound or production source is changed.

The two remaining gaps are outside this PR's window-update behavior:

- Linux AMD64 GCC `topology-xorg-dummy` exceeds its unchanged 100 ms pressure-noise
  xrandr bound in the full run, a selected rerun and one final control after the
  other owning VM/Clang test jobs finish. VERIFY-01 [#5](https://github.com/gonz0w/fvwm-x/issues/5)
  retains this emulated-platform qualification gap. Clang's full topology test
  passes. No root cause or performance improvement is claimed.
- OpenBSD AMD64 `ci-asset-contract` exceeds the two-second subprocess limit in
  `test_worker_refuses_unlocked_url_without_opening_it`, both in the original
  run and its isolated rerun. CI-01 [#17](https://github.com/gonz0w/fvwm-x/issues/17)
  retains this unchanged verification-tool timing gap. No URL policy, worker or
  deadline is changed, and no further retry is used to obtain a passing label.

Seven other original failures recover on identical source/binaries: FreeBSD
ARM64 menu-image release-grab readiness; OpenBSD ARM64 frame-sync MFL connection;
Linux AMD64 GCC dummy-X display-fd readiness; and OpenBSD AMD64 native-gate
contract (original 180-second timeout), Script/Form selection-window readiness,
desktop MFL echo and scheduled image-effect MFL echo. The original passing
entries and exact controls are checked separately by the validator. These
observations do not establish root causes or resolve broader test reliability.

## Preserved runtime, default X and manual evidence

Actual built/staged hashes for 150 native code artifacts and
99 catalogs match PR #97's retained receipts and archived
installed bytes. They are checked before/after full runs and after final fixture
relinks. No install target changes; no fresh native install is claimed. Accepted
predecessor stages/builds/evidence are preserved.

Four authenticated BSD default servers pass the fixture, reject four
unauthenticated clients and finish with four clean exits/zero forced cleanups.
The final line wraps retain the same policy; their focused checks do not restart
these default servers. No VESA cause investigation is undertaken.

Portable make verify passes 166 entries with 11 explicit native-Meson skips.
Final repository/link/diff/staged checks run separately. All execution is local;
macOS hosts virtual/emulated Linux/BSD environments. Native-CPU, physical
hardware, toolkit/compositor and broader release acceptance stay open.

Baseline/candidate man output renders successfully. The four affected pages
(fvwm3, fvwm3all, fvwm3commands, fvwm3styles) render as 80-column BSD mandoc text;
the changed paragraphs are checked in their relevant pages. Both lint results
retain status 3 and 67/218/101/32 inherited diagnostics. Normalized diagnostic
text is identical after shifted source positions. No new formatter diagnostic
is introduced; this is page review, not a new manual-install profile.

The first focus command requested a target before Meson registration refresh;
the second caught a missing HandleEvents declaration, fixed with its owning
header. Subsequent Linux/FreeBSD/OpenBSD ARM64 focus checks pass. Early collectors
stop at the original failed-run statuses; later scope-aware collection preserves
those raw results. Failed attempts, repeated failures and untouched limits remain
in the local evidence, rather than being replaced with success records.

Evidence: builddir/codex-2026-10-15/window-update-barriers/ retains frozen full/
override inputs, full/final/runner archives, exact executed scripts, manual
baseline/candidate output and validators. Guest trees preserve current and
predecessor logs/artifacts. Unrelated shared desktop and Q35/VESA work is not
imported into this PR. The full-suite gaps above remain open after this bounded
contract/coverage PR is merged.
