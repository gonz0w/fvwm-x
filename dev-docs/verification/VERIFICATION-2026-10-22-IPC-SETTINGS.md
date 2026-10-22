# Desktop settings requests and typed actions — IPC-02

Three related features pass the focused PR gate: registry/value snapshots,
generation-checked atomic updates, and correlated scoped reset/reload. Shared
integration and full release qualification remain pending under the
[faster cadence](../decisions/FEATURE-TEST-CADENCE-2026-10-22.md).
[Source/command receipt](VERIFICATION-2026-10-22-IPC-SETTINGS.json).

The base is merged PR154, `1de89441e651b22003d44d9fba1d4a68eed9b3a4`.
The receipt binds the eight affected runtime/test inputs and actual GCC/ASan
binary identities. Publication commit/tree are recorded in the PR; later
documentation does not alter tested runtime inputs. The existing guest's 2165
baseline inputs are checked before/after the run, with only the identified
runtime/test overrides and new fixture allowed. Core commands, native packet
layouts, other modules, dependency profiles and installed paths do not change.

FvwmDesktop remains authoritative for its seven registered preferences,
saved/effective values, module overrides and opaque settings generation.
Actions require the observed provider identity and generation, validate combined
changes, and reuse
the private atomic file replacement, nonblocking writer lock and file revision
check. Restart/update/reload invalidates old generations. MFL forwards bounded
literal data through existing directed module messages, assembles a pending
response and releases it; it does not own a settings cache.

Results distinguish observed, committed and reloaded preferences from command
submission and unknown asynchronous completion. A submission failure after
commit retains its committed outcome and generation. A timed-out/disconnected
action may already have committed; clients query rather than blindly replay.
Read-only connections can query but cannot submit typed settings actions.
Provider absence is observed by the absolute deadline, not inferred from a core
barrier. The [contract](../decisions/IPC-SETTINGS-PROVIDER-2026-10-22.md) and
FvwmMFL/FvwmDesktop manuals define exact envelopes, bounds and errors.

## Actual focused checks

One reused Linux ARM64 GCC development profile passes six selected registered
entries: `desktop-policy`, `mfl-request`, `mfl-stream`, `mfl-request-xvfb`,
`mfl-settings-xvfb` and `mfl-stream-xvfb`. Desktop policy has 49 tests, including
seven new settings-provider cases. Native request policy covers strict schema,
payload bounds/literal framing, cookie/serial isolation, chunk order/NUL/size,
result validation and idempotent partial-buffer cleanup.

The actual-X settings fixture starts the real core, native MFL and generated
FvwmDesktop helper. It checks all seven records/override sources, scoped queries,
atomic day/night/appearance changes, private file bytes, stale/read-only writers,
invalid value preservation, scoped reset, external-file refusal, reload and
helper restart identity/generation, and multiple real owners with targeted
requests. An independent native-packet producer then proves
absent/partial timeouts, malformed part refusal, healthy-client progress and
late/disconnected partial-response cleanup. Existing request/stream fixtures
retain legacy compatibility coverage. The final settings X fixture passes in
15.48 seconds; existing request/stream fixtures take 5.27/0.77 seconds.

A reused Linux ARM64 Clang ASan/UBSan/LSan profile passes native request policy
and the actual-X settings lifecycle with an instrumented MFL and unchanged
uninstrumented core. Leak detection remains enabled, with no suppressions or
diagnostics. This is scoped guest evidence, not native-CPU/full-release coverage.
The existing Form font-exit cleanup gap remains CLEAN-01 / Later; this fixture
does not run Form or requalify that gap.

Review finds ambiguous replies with multiple helper instances: a non-owner's
stale rejection could race a commit by the intended owner. Explicit provider
identity makes only the selected instance reply. Unit and actual-X multiple-
owner/restart controls verify the correction; the affected Linux and scoped
sanitizer checks rerun. The independent PR149/154 performance observation
remains valid without repetition. Earlier passes remain source-scoped and do
not substitute for the final targeted contract.

The initial Clang build reports one intentional adjacent-string test-vector
warning. Parentheses make the grouping explicit; only GCC/sanitizer native
policy reruns for that test-only cleanup. The subsequent provider-targeting
correction separately reruns all six selected entries and both scoped
sanitizer exercises against its final product/fixture inputs, with no compiler
warnings or sanitizer diagnostics. Earlier captures remain historical; they
do not count as extra final passes. No install, BSD boot/default-X or full
matrix is repeated.

Canonical captures validate gzip EOF/CRC, all payload hashes and modes. Completed
guest captures and duplicate archives are removed only after exact canonical
comparison. Host evidence, sources, incremental builds, install stages,
performance references and VM disks remain preserved. Repository/link/diff
checks pass. Both manuals render and format: mandoc remains status 4 for
FvwmMFL (one extra subsection-spacing warning) and status 3 for FvwmDesktop
(unchanged diagnostics). Existing mso/LINKSTYLE/spacing issues remain DOCS-01.
No new visual UI is selected, so no new screenshot is claimed.

## Deferred qualification and cycle

This is the first feature PR after PR154's integrated reference: 0/3 subsequent
PRs merged, one three-story batch in review. At merge, count it as 1/3. Shared
six-target integration, alternate compilers and relevant optional profiles are
pending for the accumulated cycle; complete install/default-X and release
acceptance remain pending at their required boundaries. No OS API, native packet
layout, install/discovery or server-specific contract changes warrant an early
BSD/install/default-X gate here. Native-CPU/hardware/toolkit/compositor evidence
remains separate. Deferred checks are not reported as passes.

IPC-02 stays open for complete topology/binding/other-provider queries, typed
domain actions beyond settings, progress/cancellation and service adoption.
CFG-01 preview/recovery still needs observable shared-core generations; a helper
settings generation does not supply them. The due five-merge
[PR149→PR154 performance observation](PERFORMANCE-2026-10-22-PR154.md) uses
already-qualified binaries, adds no benchmark build/install and resets that
independent counter to PR154 at 0/5. This pending feature PR advances it on merge.
