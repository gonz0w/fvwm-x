# M2.7: installed controller and platform handoff

Cam explicitly authorized merging PR199 and continuing M2. Its canonical commit
`c1605f7f2e9713466be2a2d68a51cbd72398737e` has a verified GitHub signature and
retained author DCO. All 1,597 PR199 runtime inputs match on integration; its
completed remote/local branch is removed. This checkpoint qualifies the
accumulated resident/worker/native-capture family, with two reproduced fixes.

A PNG-disabled resident now reports capture unavailable before admitting work.
The independent capture stub keeps its own explicit refusal. On OpenBSD, a
successful `os.Executable()` can still return the invocation symlink. Returning
that alias made the resident search beside its wrapper for a nonexistent worker:
direct installed checking succeeded, while alias checking failed with ENOENT.
The shared resolver now normalizes successful OS paths as well as the existing
absolute-invocation fallback. Desktop helpers, menu/browser self-workers and
directory reuse retain one shared policy; unresolved paths fail explicitly.
No configuration migration, new dependency or scheduler/deadline change follows.

The BSD default-X driver now runs the installed controller and staged desktop
assets against build-tree core/modules. It retains private Xauthority,
unauthenticated-client refusal, bounded waits and owned cleanup. Its
`--desktop-controller-only` mode runs settings/provider/preview/checker coverage;
it does not run the fixture's later launcher, screenshot-action, theme-profile
or final parser/log checks. A report-window failure preserves window names and
an image. Review makes these diagnostics best-effort and propagates cancellation
immediately, preserving the original failure and cleanup path.

## Composed qualification

[Machine receipts](VERIFICATION-2026-10-24-M2-PLATFORM-HANDOFF.json) bind the
source scopes, archives, profiles, outcomes and retained/fresh checks. Canonical
host artifacts are under `builddir/codex-2026-10-24/m2-platform-handoff/`.
All frozen source bytes/modes, gzip CRCs and captured payload hashes/sizes/modes
are checked. The final runtime/build input parity covers 1,185 files. The last
observer-only review fix affects failure/cancellation diagnostics; the successful
native test path and production/build inputs remain exact.

- Linux, FreeBSD 15.1 and OpenBSD 8.0 execute their affected native Go binaries on
  AMD64 and ARM64. Initial ten-package policy/vet checks cover 121 top cases/73
  subcases on Linux, 120/73 on BSD. The discovered shared lookup repair repeats
  only its five-package caller family: 66 Linux or 65 BSD top cases, each with
  37 subcases. Their union is
  122/73 on Linux and 121/73 on BSD, not a fresh full WM suite.
- Default and reduced helper profiles build and install on all six targets.
  Reduced PNG/SVG/XPM/BiDi/NLS options are disabled. Seven independent installed
  module/CLI peers pass per final profile, including an isolated executable
  symlink with no neighboring worker. Linked reduced residents and native stubs
  refuse capture explicitly. Reduced helper qualification is not a full reduced
  WM/compiler matrix.
- Native capture policy and independent Xvfb RGB checks at 16/24/30 bits pass on
  all six targets, with private unique files, write-failure cleanup and invalid
  input/display/indexed-visual refusals. This closes current OpenBSD execution
  debt for the C capture helper. Capture code and core/config/MFL/Form/Buttons
  hashes remain unchanged across the late Go path repair.
- Linux installed settings/provider/preview/checker integration and four BSD
  authenticated default-X settings sessions pass. OpenBSD's previously missing
  native report now maps with the original deadline. Fresh editor/report images
  are inspected, including OpenBSD's 800x600 server. PR199's Slate dark, Dune light
  and classic theme images remain the source-bound rendering reference; there is
  no new layout or fresh three-theme/full-desktop claim.
- Linux ARM64 runs the new disabled-capture admission regression under the race
  detector, then both runtime-path policies under race after repair. Earlier
  concurrency coverage retains its original scope. The affected manual renders,
  portable driver contracts, formatting, repository checks and separate review
  complete the checkpoint. No C sanitizer repeat follows this Go/fixture change.

## Failures, review and resource limits

Keep each original failed phase failed. Two early ARM gates used a private
harness assertion expecting the stub word “disabled”; actual “unavailable” was
correct. Only the corrected stub/linked-resident checks continue. Restoring the
pre-M1 OpenBSD cache exposes its retired `golang` option and two missing existing
utility outputs. A suffix cache also lacks Meson's excluded log directory.
These are bounded cache/harness corrections, not product passes. A verified
current compiled cache retains zero inferred test passes; completed policies
are retained separately by their exact input/archive identities.

The OpenBSD native report failure reproduces the real alias defect; both pure
policy and installed regressions accompany its fix. The first native comparison
probe mistakenly expected exit 2; its observed direct 0/alias 1 ENOENT remains
preserved, rather than being relabeled a passing probe. A host fixture initially
assumed `/var` was already canonical; canonicalizing its temporary root handles
macOS `/var` → `/private` without weakening the alias assertion.

Low Linux disk space uses owner-controlled memory-backed compiler scratch.
Go 1.27's `testing.TempDir` also honors GOTMPDIR, exposing one fixture-generated
shell to that mount's noexec rule. The test launcher now clears GOTMPDIR only
for the child. Only that failed literal-action case repeats; other passed cases
and the completed build are composed with the subsequently completed
vet/race/install suffix.
No test deadline, product assertion, security setting or mount policy is relaxed.
Only verified guest duplicates retire; source/build/cache/stages/disks and other
owners remain. The receipts record 96 verified guest files and 13,980,493 bytes removed,
including 59 files/12,719,298 bytes before the repair follow-up.

Separate routed implementation, analysis and final review remain bounded under
the selected Agent Temper profile. Requested gpt-6.1-sol/high and
gpt-6-astra/high routes return no observed model/client metadata, so model
selection remains unverified and no cost/speed claim follows. Review resolves
retained-source/command-sequence binding, exception-path owned-job cleanup and
cancellation-safe diagnostics. Agent review is not independent human approval.
Linux/FreeBSD lab guests power off after collection; disposable OpenBSD jobs use
the existing runner's process teardown and remove ephemeral job keys.

## Remaining acceptance

This is the six-target affected-family/install handoff, not full release
qualification or closure of broad #23/#7/#15. PR199's Python-free root and paired
session observations remain historical evidence with their original identities;
the late shared-resolver repair is not a fresh whole-session measurement.
Native-CPU qualification, true scheduler wakeups, transient/startup-peak memory,
GC observations and the remaining full-session owner acceptance remain open.
No general performance improvement is inferred. Physical hardware, toolkits,
compositors and assistive checks retain separate owners. Finish the remaining M2
acceptance before M3 pickup; M6/M7, new features and performance tuning stay out
of this checkpoint.
