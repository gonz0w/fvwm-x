# Settings validation and connection-local waits — IPC-02

Status: implemented subset under the approved feature cadence. Base: merged
PR155, `53dcab6b1c0bcd1a343635f9ee91e0530e98568d`.

Batch three related client capabilities: non-publishing typed preference
validation, bounded pending-wait snapshots and cancellation of a connection's
wait. The existing helper owns settings; MFL owns correlation/assembly/deadlines.
No second core authority, native packet layout, installed path or configuration
contract changes. Existing directed messages carry literal bounded payloads.

Validation shares candidate preparation with commit, checks provider/generation
and complete saved/effective values with module overrides, and returns proposed
metadata. It never persists, submits a theme, rotates generation or retires an
editor. Read-only admission permits it. Persistence is explicitly not attempted;
validation cannot promise a later lock/revision/write succeeds. This is not
CFG-01 live preview or shared-core recovery.

Pending observations are local and bounded to eight records; the existing
five-second absolute deadlines are unchanged. Transport states are not domain
progress or totals. No result history/replay store or service-liveness cache is
introduced. Local control requests remain available at capacity.

Cancellation returns the original terminal `wait_cancelled` then the cancel
acknowledgement, releases assembly/slot and discards late responses by fresh
internal serial. Deadline already reached wins as `deadline_exceeded`; the
cancel returns `request_not_pending`. Missing/completed/foreign targets have
that same refusal. Duplicate outstanding cancel IDs retain ordinary admission
semantics. Read-only clients may cancel only their own waits.

Nothing retracts commands or rolls back changes. A cancelled write can commit
afterwards, and actual-X acceptance demonstrates this. Clients query the owner
before deciding whether to retry. Domain cancellation, progress/decisions,
service adoption and reconnect recovery stay open under IPC-02.

Focused native policy, Linux nested-X and scoped leak-enabled sanitizers cover
these contracts. Shared six-target/alternate-profile integration stays pending
for this cycle; install/default-X/release gates remain at their required boundary.
No OS/ABI/install/server-dependent change requires an early gate in this batch.
