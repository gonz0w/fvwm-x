# STACK-01: measured client identity lookup

Status: narrow indexed lookup locally qualified in the
[startup/identity checkpoint](VERIFICATION-2026-10-07-STARTUP-IDENTITY.md). Subtree traversal remains a separate measured follow-up.
This is outside the frozen OUTPUT-02/DATA-01 checkpoint.

The actual core repeatedly scans its managed list when resolving transient
parents. A bounded synthetic record/context fixture measures forward chains,
wide transients with a late parent, and reverse chains for 128–1024 records. It
uses real Xlib context storage and the production lookup/marking code with
operation counters added only to private measurement copies. It does not create
1024 real desktop clients or measure end-to-end input latency.

At 1024 records, parent lookup visits 523,776 records for a forward chain,
1,047,552 for a wide tree, and 524,799 for a reverse chain. The indexed prototype
uses 1023 context lookups and zero list visits in each indexed case. Reverse-chain
closure still makes 1,048,576 membership tests in 1024 passes with either lookup;
forward/wide trees take two passes. The index does not fix the closure algorithm.
Missing/unpublished contexts still use the compatibility fallback and can retain
linear lookup cost.

## Implementation and identity/lifetime contract

`add_window::find_managed_client` reuses the existing context table. It accepts an
indexed record only when its client XID matches and its managed-list `prev` is
non-NULL. Frame/title/decoration aliases therefore cannot become transient
parents. Source lifecycle checks confirm `destroy_window` clears `prev` before
context teardown, and new/reused records begin with zeroed membership.

The fallback retains the previous list lookup. AddWindow publishes its managed
list entry before creating its client context, and a reused XID can temporarily
resolve to a retired context while the new record is already on the list. Both
cases retain their old lookup result. `get_transientfor_fvwmwindow` uses this owner
while preserving transient/None/root/self guards. No second index or global
membership state is added; existing transient-cycle validation, stack order and
scratch-based closure remain unchanged.

The returned pointer is borrowed under the existing single-threaded lifetime
contract. Future XIO-01 work can reuse this validated client path; other context
consumers have not been migrated indiscriminately.

## Evidence and limits

Permanent actual-core identity fixture: five groups cover client/alias identity,
construction fallback, retirement/reused XID, transient guards and indexed deep/
wide parent selection. Three registered focused Linux checks pass: identity,
actual managed transient layer/focus/destruction integration, and child-list
ownership. Two scoped actual-core Clang ASan/UBSan drivers pass with core leak
checking disabled; they exit without complete normal core teardown.

Sources, exact hashes, compile/link logs, counter outputs, test logs and archives:
`builddir/codex-2026-10-07/stack-identity/`. `focused-summary.json` validates archive
integrity and implementation source scope. The initial measurement fixture set
forward references before initializing all synthetic client IDs and failed an
assertion. That fixture failure is retained; two-pass fixture initialization
passes baseline and prototype checks. Production code did not require that fix. Final fixture-only line wrapping was
followed by an affected registered fixture rerun and two scoped sanitizer
reruns; initial/final archives and the single changed fixture are recorded in
`focused-summary.json`.

BSD/optional/full gates for this lookup pass in the exact 1279-file combined
startup/identity checkpoint.
The prior frozen deadline/tree checkpoint must not be treated as coverage for
this implementation. Native Linux release CI, physical/toolkit/compositor checks
remain separately unrun. An adjacency/worklist closure algorithm needs its own
ordering/group/rank contract and comparison before replacing shared scratch state.
