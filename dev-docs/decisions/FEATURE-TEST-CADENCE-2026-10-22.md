# Feature PR checks and shared qualification cycles

Cadence superseded on 2026-10-23 by [larger cleanup batches and risk-based
platform checks](BATCHED-CLEANUP-CADENCE-2026-10-23.md). The text below records
the earlier decision; its automatic three-PR matrix trigger no longer applies.

Accepted user direction, 2026-10-22: feature development is spending too much
time repeating platform/build/install qualification for small increments.
Separate feature-PR merge checks from accumulated integration and release
acceptance. This supersedes the earlier one-broad-checkpoint-per-PR cadence;
the platform support and release requirements themselves remain unchanged.

## Decision

- A normal feature PR uses one owner-controlled incremental Linux profile:
  affected compilation, policy/regression tests and affected nested-X behavior.
  Add scoped sanitizers for memory/lifetime changes and an early affected-target
  check when OS APIs, architecture/ABI, compiler or build behavior changes.
  Capture actual-X screenshots when visible UI changes. Run repository/diff
  checks. Full matrix, repeated installs and four BSD desktop sessions are
  not default PR merge gates.
- Group roughly three feature PRs into one integration cycle. Close the cycle
  after three merged implementation PRs, or earlier at a natural feature handoff
  or about ten substantive accumulated stories. Tests/docs are not extra stories;
  documentation-only commits do not advance the count. Keep PRs reviewable and
  aim to finish useful issue scope rather than split an envelope into tiny PRs.
- At cycle closure, freeze one accumulated candidate and run the affected
  six-target integration checkpoint once. Select compiler/optional profiles for
  the accumulated changes. Run install and default-server gates when packaging,
  installed discovery or server-dependent behavior changed; do not repeat them
  merely because another ordinary module binary changed. Record every deferred
  release check explicitly. Do not begin a fourth implementation PR until the
  cycle's required checks pass or the user explicitly changes its scope.
- At milestone closure and before a release candidate, run the complete required
  Linux/FreeBSD/OpenBSD × AMD64/ARM64 support matrix, compiler/feature profiles,
  installation, default-X and applicable sanitizer/acceptance gates. Reuse a
  cycle result only where its exact inputs and scope establish the required
  result; run the missing coverage. Do not rerun an identical accepted gate
  simply because the milestone or PR label changed.

The PR gate broadens immediately for a reproduced regression or a changed
contract that focused checks cannot cover. Examples include core dispatch or
global ordering, shared native packet layout, platform-specific process/X
handling, build/dependency detection, installed paths/resources and default-X
pointer/RandR/rendering behavior. Select the necessary tests/targets, rather than
using a general "safety" label to restart the whole matrix.

## Evidence and implementation

The IPC-02 report records the cost already paid, not a promise of future timing.
Its Linux ARM64 GCC selected-test command took 18.758 seconds, with a reused
incremental compile of 0.328 seconds. These exclude setup, other profiles,
collection and recovery. Repeated guest preparation and the documented private
runner failures added substantial elapsed time. See the original commands in
the canonical captures referenced by the
[IPC-02 report](../verification/VERIFICATION-2026-10-22-IPC-REQUESTS.md).
No new benchmark or runtime matrix is needed to change this documentation.

Use existing incremental builds and valid owner-controlled guests. Fresh image,
package/bootstrap and dependency qualification is required when those inputs
change, not for every feature PR. Preserve exact source/build identities and
other owners' work. Consolidate canonical capture/receipt generation at cycle
closure; a PR needs its source identity, selected commands/results, review and
an explicit deferred-check list, not a duplicated full release dossier.

The tracker records the last integrated candidate, merged PRs since it, affected
subsystems, deferred checks and the next cycle boundary. A feature may be
**focused-checks passed / integration pending** and merge without claiming
six-target or release qualification. Broader issues stay open until their
acceptance criteria actually pass. Deferred coverage is not a pass, and a
cycle failure is fixed before dependent work or cycle closure. Preserve original
failures and rerun affected checks rather than repeating valid work.

PR154's existing runtime candidate is already six-target qualified under the
older cadence. It is the next cycle's starting reference after merge; these
documentation changes require only repository/link/diff checks. This change
adds no scheduler, hosted CI, test-selection engine or new qualification runner.
Measure future cycle time from existing logs before adding such infrastructure.
