# Directory metadata backend evaluation

Owner Cam; FILE-DATA-01 #66; base merged PR339
`7edc6342603eb25778b53e3324f070d715c19e68`. Status: selected non-installed
source checkpoint. The tracker owns execution; the frozen release contract and
[library rubric](LIBRARY-EVALUATIONS-2026-10-25.md) own acceptance.

## Outcome and simpler baseline

Live browsing already uses bounded Go snapshots, filemodel ordering and
collection projections. Reconcile its accepted provider/watch evidence rather
than reimplementing or requalifying unchanged consumers. Compare that actual
baseline and an evaluation-only JSON file snapshot with optional SQLite/FTS5.
SQLite is useful only if its query/update/recovery benefits justify the binding,
index/schema, dependency and maintenance costs. No adoption is presumed.

Author one manually built command in the existing Go module, excluded from
normal builds by `fvwm_metadata_eval`. Reuse real filemodel/collection methods.
The optional `fvwm_sqlite_eval` plus cgo variant calls the SQLite C API directly;
no Go driver/module dependency, production target or installed binary is added.
A standalone C benchmark would omit the eventual Go binding/copy cost. The
CGO-disabled baseline remains independently buildable. Apple SQLite3.54.0 is a
host discovery identity, not a portable candidate pin; freeze each target's
exact source/version/features before its comparison executes.

## Three related implementation stories

1. Freeze deterministic public metadata and independent ordered-ID/generation
   oracles. Sizes 0/12/256/4096 and explicit 4097 refusal; raw names remain bytes,
   labels are separate, domain IDs are never backend row identities. Include
   duplicate labels, invalid UTF-8, punctuation/controls, Unicode lowercase,
   directories/files/symlinks/special entries and large signed numeric metadata.
2. Add a bounded manual baseline probe reusing actual ordering, literal filter,
   ordered full projections and reconciliation, with an evaluation-only bounded JSON snapshot
   roundtrip. This is not a newly shipped cache or persistence format. Check
   no-op, metadata update, replacement, stale generations and restart.
3. Add optional SQLite load/query/update/reopen and finite failure controls.
   Bind values; retain original names as BLOBs, Go-normalized labels separately
   and domain IDs/generation explicitly. Query semantics must match the Go
   baseline. FTS5 token/prefix queries use a separately named workload and oracle;
   they do not replace existing substring filtering.

Existing 4 MiB wire, 4096-entry/path and 128-byte filter limits remain. Reject before
publication; no filesystem discovery or file contents in the corpus. The private
probe owns a fresh 0700 work directory and 0600 data files. No caller file is
replaced. Fixed operations and bounded repetitions avoid a new backend framework.

## Failures and comparison boundary

Author two-connection busy refusal, explicit read-only open, bounded page-limit
full refusal, malformed/truncated/wrong-schema snapshot/database, committed
reopen and uncommitted-update rollback. Read back limits; never fill a shared
filesystem. Failure must preserve the published generation and permit a clean
reopen/rebuild. Missing build/SQLite/FTS5 capability is unavailable, not success.
A synthetic error classification is not an observed absent engine/module.

Report startup separately from repeated load/query/update/reopen time, process
CPU/raw OS RSS, Go allocations, separately available SQLite counters, private
file bytes and the full build/link/install closure. Neither allocator counter
is total memory; process restart is not power-loss recovery. Use the same public
workload and modes within each target, never cross-OS speedup claims.

This source PR uses separate review/static and affected compilation only.
Runtime/failure oracles and repeated six-target costs run together at the named
substantial evaluation checkpoint, after exact profile/source pins. Preserve
failed/unavailable results. The due periodic WM observation joins a reused
shared session; a private query benchmark is not a WM speed improvement.

## Scope and decisions

No production database/schema, persistence, search daemon, browsing default,
mandatory cgo/SQLite dependency or sensitive clipboard index. No UI: theme and
screenshot checks are not applicable. Adoption and accepted integration receive
a later explicit evidence-based disposition. Existing browser and clipboard
results remain source-scoped; installed clipboard-copy and lock-time decisions
do not block this public-metadata evaluation.

## Accepted directory evidence to reconcile

- [Foundations](../verification/VERIFICATION-2026-10-23-FILE-BROWSER-FOUNDATIONS.md):
  bounded byte-preserving scans, IDs/generations, navigation, ordering/filtering,
  isolated cancellation and manual refresh; original profile limits explicit.
- [Directory watch](../verification/VERIFICATION-2026-10-23-BROWSER-DIRECTORY-WATCH.md):
  inotify/kqueue, bounded coalescing/reconciliation, loss/manual fallback/reap.
- [Accumulated provider cycle](../verification/VERIFICATION-2026-10-23-BROWSER-PROVIDER-CYCLE.md):
  six targets/seven profiles and source-bound retained/fresh browser/watch cases.

These establish accepted component scope for #66's first two criteria; final
current release integration remains separate. Backend criteria 3–5 stay open.


## Source implementation and measurement disposition

[Source receipt](../verification/VERIFICATION-2026-11-02-DIRECTORY-BACKEND-SOURCE.md)
records affected compilation without executing the authored oracles. Separate
verify, measure and control invocations prevent reference/fault work contaminating
backend cost samples; a measured result needs its matching verified digest.
FTS index construction/query scans and incremental storage stay separate from
literal browsing. Comparison runtime, target pins and adoption remain queued.


The probe returns complete bounded ordered ID projections, not pages. It does
not measure pagination, rendering or product query round trips; those retain
the existing file-view/consumer and release scope. No additional paging workload
is selected by this source checkpoint.
