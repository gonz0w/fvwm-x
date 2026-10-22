# Validation and pending request lifecycle — IPC-02

Three client capabilities pass focused merge checks: non-publishing typed
settings validation, connection-local pending-wait snapshots and wait-only
cancellation. [Exact source/command receipt](VERIFICATION-2026-10-22-IPC-LIFECYCLE.json).
Shared platform/release qualification remains pending under the approved
[feature cadence](../decisions/FEATURE-TEST-CADENCE-2026-10-22.md).

Base: merged PR155, `53dcab6b1c0bcd1a343635f9ee91e0530e98568d`.
Eight changed runtime/test hashes match the final candidate. The reused guest
checks all 2165 original PR154 baseline inputs before/after its checkpoint,
with nine explicit overrides (including unchanged PR155 test registration and
the added settings fixture). Documentation is outside these tested runtime
inputs. The receipt binds actual core, helper, MFL, test and sanitizer binaries.
No core dispatch, native packet, OS API, install/discovery or server-dependent
contract changes; no early BSD/install/default-X gate is required here.

Validation shares complete candidate preparation with commit and checks the
observed provider/generation, types, combined schedule and module overrides.
It returns proposed saved/effective metadata without writing, theme submission,
editor retirement or generation change. Persistence is explicitly not attempted;
it cannot reserve a future lock/revision or promise a successful write. Read-only
clients can validate. This is not CFG-01 live preview/shared-core recovery.

Pending queries expose at most eight waits on that connection, their operation,
transport phase and remaining deadline. There is no domain progress/liveness
claim or completed history. Local query/cancel remains available at capacity.
Cancellation queues the original terminal result before its acknowledgement,
releases partial assembly/slot and retains other deadlines. Due expiry wins;
foreign/missing/completed targets are refused. Existing serial isolation drops
late replies after ID/slot reuse. Cancellation does not retract submitted work.
Actual-X tests demonstrate a cancelled write committing after the core resumes;
clients must query before deciding whether to retry.

## Focused validation

One existing Linux ARM64 GCC profile passes six selected entries:
`desktop-policy` (51 cases), `mfl-request`, `mfl-stream`, `mfl-request-xvfb`,
`mfl-settings-xvfb` and `mfl-stream-xvfb`. The three actual-X fixtures take
5.27, 15.60 and 0.80 seconds. Existing absolute five-second negative-path waits
account for most runtime; no install, VM boot or extensive matrix repeats.

Native policy covers strict validation/cancel schemas, literal bounded payloads,
local wait states/deadline arithmetic and assembly cleanup. Helper policy proves
candidate saved/effective values, invalid/stale refusal, no writes/commands/
generation change, and validation despite a known persistence refusal. Actual
core/MFL/helper integration covers read-only validation, capacity, connection
isolation, terminal ordering, slot/ID reuse, partial-assembly cancellation, late
responses and a cancelled write's later commit, alongside prior regressions.

Three scoped Clang ASan/UBSan/LSan exercises pass: native request policy and both
request/settings X fixtures with instrumented MFL and unchanged uninstrumented
core. Leak detection stays enabled without suppressions or diagnostics. Compile
logs contain no warnings. These are emulated guest passes, not native-CPU or
complete release qualification. Existing Form font-exit cleanup remains CLEAN-01
/ Later and is not exercised or requalified.

The initial GCC run passes five entries but its request fixture assumes an
order among simultaneous expiry replies after slot reuse. The fixture now
correlates the complete terminal set by ID; all six final entries pass. The
original failure log/hash is retained. Review also corrects candidate item
metadata to consistently report not-submitted. Both final focused/sanitizer
captures use that corrected source. Local review checks ownership/failure exits,
strict schemas, read-only admission, bounds and honest cancellation semantics.
No unresolved finding remains in the selected scope.

Both changed manuals render and format. FvwmMFL mandoc status remains 4 with
existing mso/LINKSTYLE/section-spacing diagnostics plus two new subsection-
spacing warnings; FvwmDesktop stays status 3 with unchanged diagnostics.
These inherited renderer conventions remain DOCS-01, not a clean-lint claim.
Repository/link/whitespace checks pass. No visible UI changes, so no new
screenshot is claimed; later visible consumers retain actual-X captures.

The 22,172-byte canonical archive contains 40 files. Gzip EOF/CRC, every payload
hash and mode are verified. Only the exact completed guest capture and duplicate
archive are retired after canonical comparison; sources, incremental builds,
install stages, performance references and VM disks remain preserved.

## Cycle and remaining scope

PR155 is merged: this cycle is **1/3 merged**, and this second three-feature PR
becomes **2/3 on merge**. Affected six-target/alternate compiler/relevant optional
profile coverage remains pending at cycle closure. Full install/default-X and
milestone/release acceptance stay at their required boundaries; deferred gates
are not passes. Native-CPU/hardware/toolkit/compositor checks remain separate.

IPC-02 stays open for full topology/binding/other-provider queries, typed actions
beyond preferences, domain progress/decisions/cancellation and service lifecycle
adoption, shared-core generations and full owner acceptance. Wait-only
cancellation does not close that broader protocol. Performance remains a
separate five-merge cadence: PR154 reference, **1/5 merged**, **2/5 after this
merge**. No comparison or tuning is due.
