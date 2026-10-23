# Atomic collection refresh and focus recovery

Status: **focused checks passed; ready for review**.
Owners: FILE-VIEW-01 / FILE-DATA-01 / FILE-ACTION-01; clipboard metadata support.
Base: merged PR169 `ac97a8ab379483c921f7526710f549c6074b5f97`.
[Contract](../decisions/COLLECTION-REFRESH-2026-10-23.md),
[source-bound receipt](VERIFICATION-2026-10-23-COLLECTION-REFRESH.json).

## Delivered batch

- Generation-checked metadata transactions: at most 256 removals/upserts,
  complete resulting ID order, 4096 entries, atomic validation and owned copies.
  Snapshot reconciliation uses a small transaction or one bounded replacement.
  Identical metadata/order consumes no generation; reordering still advances it.
- Stable optional observation revisions and refresh recovery: surviving focused
  IDs and selections remain; disappearing focus prefers the next visible old
  neighbor, then the previous one, otherwise the first new visible entry. Filters
  and filtered-out selection survive, while removed selected IDs are counted.
- Browser manual refresh adopts the shared policy and reports added/updated/
  removed entries and lost selection, or no entry metadata changes. Removed
  properties targets retire. File revisions reflect observations, never content;
  isolated worker revalidation remains authoritative for opening a target.

The native Form still rebuilds its bounded complete configuration. This is an
internal metadata API and working snapshot consumer, not an external streaming
provider protocol, watcher or in-place native control update. Clipboard payloads
remain outside ordinary Go models/labels and command/Echo paths. No SQLite,
file mutation, configuration migration or new package dependency is introduced.

## Verification

Before implementation, the accumulated PR167–169 cycle closes with **49 native
entries on six targets/seven profiles**, six browser drivers, three representative
real theme configurations and exact retained race/scoped sanitizer evidence.
[Predecessor cycle report](VERIFICATION-2026-10-23-COLLECTION-CYCLE.md).
That evidence is not presented as six-target qualification of this new batch.
The due [periodic comparison](PERFORMANCE-2026-10-23-PR169.md) confirms exact core/
MFL identities since PR164 and records measured consumer/native Form sizes.

The final new-feature candidate has **2260 files**, archive
`3ee1741e7d203910e57f8a93f0db5f0a336eb91531ea69bec6cc5153912dee3f`. Five Go packages/**43 top-level cases** pass on the
reused Linux ARM64 profile, plus the separate five-package race run (**86 final
case executions**), vet and helper build. Policy covers stale and exhausted
updates, transaction/result/ID bounds, conflicts, complete ordering, ownership,
no-op and large replacement paths, filtered selections and focus neighbors.
Worker/controller regressions verify metadata revision and stale result refusal.

Three final actual-X drivers load the full shipped **Slate dark / Dune light**
configurations and a **classic larger-font/legacy-color** configuration on
unchanged qualified core, MFL, Form and desktop binaries. Each exercises the
existing empty/4096 list/grid, input/ranges/scroll and worker controls, plus a
focused selected file disappearing during filtered refresh. The replacement
receives focus without selection; the hidden selected directory survives and
unchanged refresh reports no entry metadata change. Each captures **13
screenshots**. New Dune refresh feedback and classic unchanged-refresh views,
plus earlier grid/selection/large-collection theme views, are inspected.
Configuration inputs/hashes, font requests, server geometry and scale are retained.

The initial run passes Go policy/race/vet/build and Slate, then Dune's one-shot
startup assertion receives an initial saved-default reply. The original archive
is preserved. A bounded readiness predicate waits for the requested effective,
module-configuration-sourced, submitted palette/appearance/transparency. Review
also adds removed/ordered ID length checks before hashing and clarifies manual
count semantics. Final affected Go/UI/manual checks run once after those fixes;
passing predecessor native/compiler/platform coverage is retained, not repeated.

The updated manual is generated on Linux and rendered with host mandoc.
Portable verification passes **177 checks / 14 unavailable-tool skips**.
Repository/whitespace checks pass. Both initial/final archives verify complete
compressed EOF/CRC, exact inventories, every hash/mode and explicit outcomes:
initial **46 files**, final **84 files**. Final archive SHA-256:
`66d5535298d7757b45dd7223913ea0ff044bd419f31fba16b2398a23cfbf6f39`.
Private canonical evidence/screenshots: `/Users/cam/DEV/fvwm3/builddir/codex-2026-10-23/collection-refresh`.

## Remaining qualification

PR169 is the integrated and periodic performance reference. This batch is **0/3
and 0/5 merged**, becoming **1/3 and 1/5 only on merge**. Use the next shared
checkpoint for accumulated affected platforms; full install/default-X/compiler/
feature/owner/release acceptance remains at its boundary. No native-C ownership,
OS API, install/discovery or server contract changes require repeating them here.
Native-CPU sanitizers, physical hardware/toolkits/compositors and assistive checks
remain separate. In-place native updates, external providers, watchers, file
mutations and protected clipboard delivery/retention/storage remain open.
Completed guest capture/archive duplicates retire only after exact host proof;
sources, builds, qualified caches/SDKs, binary references and VM disks remain.
