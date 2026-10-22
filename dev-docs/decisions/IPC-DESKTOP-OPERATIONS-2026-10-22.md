# Owned desktop operations — IPC-02

Selected after PR159 merge `0eddcbbc97a82d0b0e6ca8cc53691b43756b610c`.
Batch five related operation capabilities together: typed application/status
refresh admission, correlated state/results, generation-checked cancellation,
bounded result retention across client loss and coalesced invalidations. Reuse
the existing two-worker/four-job desktop helper and native module transport.
The tracker owns execution order; SESSION/file/clipboard domain acceptance stays
with those owners. No new daemon, arbitrary job loader, decisions for scans or
file mutation is selected merely to demonstrate a generic operation framework.

Start requires exact provider/preference generation and a known collection target.
Operation records get independent IDs/generations and honest unknown progress.
The main helper loop observes future state and performs existing publication in
order. Failed submission can be partial; completed means commands submitted, not
painted. Workers retain no core X authority. Repeated target work is refused;
legacy jobs keep the same capacity and publication paths.

Cancel requires current operation generation. Cancel queued futures immediately;
for running/computed read-only scans, retain admission until return and discard
output. Do not claim interruption of blocking calls, worker stop or rollback.
Wait cancellation still cannot retract submitted domain work. View loss continues
work. Retain at most 16 memory-only records, expire terminal records after five
minutes and evict oldest admitted terminals under capacity pressure. Restart
invalidates owner identity/history. Missing records are explicitly unavailable.

Reuse bounded per-connection watches, independently filtered for operations and
preferences. Coalesce intermediate observations; announce terminal changes before
eviction. No values, paths or worker exception text enter events. Consumers must
watch then query and resnapshot after races/disconnects/malformed notices. Keep
existing private endpoints/queues/deadlines and all configuration syntax.

Use focused native policy and actual-X during implementation, then one affected
six-target/nine-profile checkpoint for accumulated PR158/159/current changes.
Relevant scoped sanitizers and a single portable harness verification accompany
cycle closure. No install/default-server gate is implied by unchanged installed
paths and server input behavior; full milestone/release acceptance remains due.
No new visual UI; later visible consumers retain screenshots. The due PR154→159
performance observation uses already-qualified binaries; PR159 becomes the next
independent five-merge reference. Keep performance tuning deferred.
