# Owned desktop operations and accumulated IPC cycle — IPC-02

Five related capabilities are qualified together: typed application/status
refresh admission, operation status/results, generation-checked cancellation,
bounded reconnect retention and coalesced invalidations. One affected checkpoint
integrates merged PR158/159 with this larger batch: **122 selected passes across
six targets/nine profiles**, plus five scoped guest sanitizer exercises.
[Exact source/profile/command receipt](VERIFICATION-2026-10-22-IPC-DESKTOP-OPERATIONS.json).
Base: merged PR159, `0eddcbbc97a82d0b0e6ca8cc53691b43756b610c`;
integrated reference: PR157, `3f4f7052151f110ba6f398a3451e727af4ecf8c4`.

Use the existing two-worker/four-job helper pool. Native commands and legacy job
publication retain their order; the core gains no worker or second X authority.
Typed starts use exact provider/preference generation and known targets. Each
operation gets a separate ID/generation and honest unknown progress. Worker
failure precedes publication; publication failure reports partial-or-unknown
application. Completion means commands submitted, not applied/painted.

Queued cancellation can cancel its future. Running/computed scans retain their
slot until return and discard output after cancellation; worker stop and rollback
are not promised. View loss continues work. At most 16 records survive in memory;
terminals expire after five monotonic minutes or earlier oldest-terminal eviction
under admission pressure. Provider restart/history loss is explicit. Native
wait cancellation still cannot retract work. Scans need no decision UI; domain
file/session/clipboard actions retain their own future acceptance.

Operation watches have one connection-local optional provider filter independent
of the preference watch. Intermediate observations coalesce; terminal invalidation
is sent before eviction. Events expose no labels, paths, values or worker error
text. Query after watching, races, malformed notices or reconnect; the bounded
output queue isolates unqueueable clients. No history/replay/cache is added to
MFL. See [contract](../decisions/IPC-DESKTOP-OPERATIONS-2026-10-22.md) and
[manual](../../doc/FvwmMFL.adoc) for exact syntax and limits.

## Shared checkpoint

Fresh selected entries run once per configured profile: Linux ARM64 GCC/Clang/
minimal and AMD64 GCC/Clang (five × 14), plus FreeBSD and current OpenBSD 8.0 on
both architectures (four × 13). Each profile includes native query/request/
stream/event/JSON/queue policy, 61 desktop policy cases, and actual-X core-state,
request, settings/provider, operation, stream and default desktop consumers.
Linux also covers interaction servicing. These are selected registered entries,
not totals of every unit assertion or a full support/release matrix.

Deterministic policy covers queued/running/late cancellation, stale tokens,
worker/publication failures, unknown totals, repeated poll coalescing, shared job
capacity, 16-record admission/expiry and unavailable IDs. Native request policy
adds allocation-failure sweeps, exact typed routes and bounded notice parsing.
Actual-X uses the shipped owner/main loop and real core/MFL transport. Its first
application scan uses the real backend; later scans are fixture-gated then run
the real backend, proving cancellation/output discard, responsive observation,
reconnect retention, completion and event payload exclusion without production
test hooks. Prior provider/binding/settings/stream/default-desktop regressions
pass in the same checkpoint.

Five leak-enabled Clang ASan/UBSan/LSan exercises pass on the reused guest profile:
query/request policy and operation/request/settings actual-X fixtures. MFL is
instrumented for X fixtures; the current GCC core is uninstrumented. No diagnostic
or suppression is present. This is emulated guest execution, separate from
native-CPU and whole-core qualification. Existing Form exit-cleanup remains
CLEAN-01 / Later and is not requalified. Executor/process-constructor resource-
failure qualification remains PROC-01; these checks cover admitted-future
failure/cancellation rather than every constructor failure.

All six target jobs pass without a frozen runtime/fixture correction or failed
selected gate. One frozen source archive binds all 2192 inputs; eight changed
runtime/test/build hashes match publication. Owned incremental compiler profiles
and fully verified current OpenBSD caches avoid full rebuilds. OpenBSD uses the
official recipe with package/compiler/cache/source proofs, completes normally
and removes generated job keys. No package-trust bypass or retired 7.9 target
is used. Full build/cache data remain canonical; no previous broad suite is
silently counted as a fresh pass.

Portable upkeep passes 177 of 191 cases with 14 explicit unavailable-native-tool
skips. Both manuals render/format. FvwmMFL mandoc remains status 4 for inherited
mso/LINKSTYLE/section-spacing diagnostics plus one added subsection-spacing
warning; FvwmDesktop remains status 3 with unchanged inherited diagnostics.
Dependency rebuilds retain existing compiler warning classes and OpenBSD linker
annotations for bounded string-copy sites; those copies validate lengths before
writing. These are not clean-warning, clean-lint,
whole-core sanitizer or release claims. No visible UI changes need screenshots.

Every canonical archive passes gzip EOF/CRC and full payload/hash/mode checks.
Completed Linux/FreeBSD, sanitizer and performance guest captures/duplicate
archives are retired only after exact canonical comparison. Reusable sources,
builds, install stages, performance references, VM disks and host canonicals stay.
Final report/status/README edits do not change tested runtime inputs; no matrix
or product test is repeated just to publish the PR.

## Remaining scope and cadence

PR158/159 are **2/3 merged**; this third batch qualifies their accumulated cycle.
On merge, use its canonical commit as the next integrated reference and reset
to **0/3**. Full compiler/optional-feature/install/default-X and milestone/release
acceptance remain at their boundary. No installed/discovery/server-input contract
changed here requiring those early gates. Native-CPU/hardware/toolkit/compositor
qualification remains separate; deferred checks are not passes.

IPC-02 stays open for broader adapters/state, domain action/decision/progress
acceptance with SESSION/file/clipboard consumers and shared-core generations/
CFG-01 preview recovery. This desktop-scan adoption does not finish those owners.
The due [PR154→PR159 performance observation](PERFORMANCE-2026-10-22-PR159.md)
uses preserved binaries in 31.438 seconds: mostly overlapping latency ranges,
mixed small changes and explicit core/MFL feature-size growth. No general speedup
or consistent regression is established. PR159 becomes the independent reference
at **0/5**, **1/5 after this merge**. Performance tuning stays deferred.
