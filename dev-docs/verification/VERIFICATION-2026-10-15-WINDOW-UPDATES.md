# Window update ordering contract — 2026-10-15

Owner: DRAW-01 [#25](https://github.com/gonz0w/fvwm-x/issues/25).
Status: implemented; focused checks pass; combined local qualification running.
[Contract](../decisions/WINDOW-UPDATE-BARRIERS-2026-10-15.md).

This checkpoint establishes the existing ownership/application/painting contract
before measured STYLE-01/STATE-01 optimization. It adds an actual-core regression
and corrects the manual's idle-queue wording and RefreshWindow scope. It changes
no production C/header, installed target, configuration syntax or dependency.
PR #97 is merged; OpenBSD/VESA cause investigation #93 remains deferred.

## Scope and checks

The fixture links the real core, creates two managed clients and executes style,
UpdateStyles, Refresh and RefreshWindow commands through the shared engine.
Applied border widths, actual server frame/client geometry and `_NET_FRAME_EXTENTS`
are checked before/after barriers. It models a borrowed complex-command record
using the existing execution flag, invokes actual retirement/reclamation, then
enters `HandleEvents`. A wrapped module-continuation observation point confirms
retirement and style application precede that point while an input event remains
queued. Destruction, style matching/application, geometry and X property calls
are not mocked. This is an ordering regression, not a latency/CPU benchmark or
coverage of arbitrary reentrant callbacks, DPI transitions or external clients.

Eight named cases cover initial state, deferred style application, explicit and
clean repeated updates, both refresh commands, borrowed-record lifetime,
outer-boundary retirement/application and pending input. The driver accepts
owned Xvfb and borrowed authenticated default displays. Its controlled core ends
with `_exit`; scoped sanitizer checks therefore disable leak detection.

Focused Xvfb checks pass on Linux, FreeBSD and OpenBSD ARM64. Portable
`make verify` passes, with native-Meson-only skips explicitly retained. The
combined six-target compiler/feature suites, BSD default-X checks and scoped
Linux sanitizer fixture are running. Predecessor install evidence is retained;
actual tested and staged native hashes are checked against PR #97's receipts.
Final counts, exact source/runner identities and evidence will be recorded before
this PR is promoted for review.

## Preserved attempts and limitations

The first focus invocation requested a new Meson target before refreshing target
registration. The second caught the fixture's missing HandleEvents declaration;
including its owning eventhandler header fixed it. Subsequent focus runs pass.
Both attempts and compiler output are retained. No production fix was required.

All execution is local; macOS hosts emulated Linux/BSD environments. This is not
native-CPU release sanitizer evidence or physical hardware qualification. Broader
DRAW-01 coalescing, STYLE-01/STATE-01 profiling and release gates stay open.
