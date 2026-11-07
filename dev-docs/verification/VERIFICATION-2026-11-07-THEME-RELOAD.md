# Theme reload and inspector authoring checkpoint

T1 under #214, #217 and #218; base PR367,
`1ad74db32e0abe3f1bf4fd3ea44735538b9b652d`.
**Historical T1 delivery: draft at `22ee9ff`.** The later
[UI/manual closeout](VERIFICATION-2026-11-07-THEME-UI-CLOSEOUT.md) qualifies the
repaired fixture and publishes four captures. Results below retain their original
T1 source and failure status.

## Implemented behavior

`reload-theme` and its settings-menu action prepare the existing palette file in
the bounded nonresident worker. Complete palette/phase/role validation reuses the
duplicate-aware JSON decoder and regular-file reader. One pending transaction
retains worker identity, controller instance and settings generation. Invalid,
stale or closed-owner results cannot replace working data.

A prepared color component waits for resource-preview recovery and checks
freshness again before publication. Blocked/conflicting recovery refuses it;
opacity-only previews must finish first. Existing semantic colorsets are updated
without resetting preferences, menu bindings, Forms, browser identities or drafts.
All wallpaper background entries are retained, including inactive phases.
Identical semantic data avoids another publication. Fixed Echo records report
submission; quoted stderr reports source refusal without opening a notice Form.

The inspector displays ordered, root-relative include ancestry and line spans,
with unknown roots hidden and long paths/row exhaustion explicit. These are
literal candidates, not exact runtime origins. Its reader accepts current,
key-specific application/component metadata and rejects malformed or misplaced
claims. Choices/ranges share metadata rows so complete canonical registry
snapshots fit the unchanged 128-row and 200-rune bounds. A color reload marks the
current inspector stale without rebuilding it; delayed collections compare their
captured color content even when settings identity and palette names survive.

## Observed verification

Native Linux ARM64, retained Go 1.27.2 SDK and owner-controlled incremental cache.
No dependency installation, SDK change or full platform campaign was performed.

| Check | Result |
| --- | --- |
| Repository foundation, whitespace and affected Python syntax | Passed |
| Controller, settings, worker and worker-command Go packages | Passed in the diagnosed correction; retained source-bound results |
| Changed inspector Go package | Passed in T1-C1, 0.072 seconds reported by Go |
| Configured FvwmDesktop and worker builds | Passed; exact retained Meson command arrays/linker values |
| Native UI driver | Failed after 10.41 seconds; no usable profile diagnostics or captures exported |
| New screenshots | Unavailable; none claimed or substituted |
| Affected manual render | Not reached |
| Broader support/race/sanitizer/install/release checks | Queued with existing owners; not fresh passes |

The inspector tests preserve complete registry content, unknown-state rejection,
eight normal/high-contrast, explicit/inherited and retained-preview combinations,
and fixed bounds. Existing controller tests cover unchanged reloads, UI/settings
identity, stale/closed completions, preview recovery, background exclusion and
high contrast. These policy results do not establish actual rendered UI behavior.

Desktop file bytes: 6,756,160 → 6,828,326 (+72,166).
Worker: 6,478,988 → 6,551,935 (+72,947). These are configured Go executable sizes,
not RSS, latency or speed measurements. No new resident service is introduced.
The periodic comparison remains due for the next shared runtime checkpoint.

## Retained failures and stopping point

Predispatch source review corrected omitted configured linker definitions before
any native run. The original native campaign then stopped at its two-attempt
limit. First, named/unnamed palette map types defeated identical-content checks,
and the inspector rejected current producer metadata. The diagnosed correction
fixes those, but its two-row compaction is insufficient for the complete registry.
Four packages pass; the inspector still reaches its row limit. Both five-file
failure exports and archives verify; failures remain failures.

T1-C1 is an explicitly recorded smaller presentation closeout: uniformly fold
choices/ranges, retain all fields/bounds, test only the changed inspector, reuse
the four passes, then build and perform the first UI/manual run. The inspector
and both helpers pass. The UI exits with an X connection error during teardown;
no exported profile diagnostics establish its initial failure cause. Its source
stops the X server before closing the observer display, which can terminate
Python before evidence is written. A source-only diagnostic-preservation fix is
included, with syntax/review only. It is not described as runtime-qualified.

The checkpoint is stopped. No further native retry, guest-log recovery or replan
was performed. Eleven T1-C1 export files and the archive verify; all five command
groups retired. Owned container/machine shutdown is confirmed. One other owner's
VM was preserved under the reviewed resource-bounded shared-host plan. A prior
exclusive-host refusal occurred before VM startup or dispatch and is retained.

## Scope and continuation

The supported reload domain is colorsets 0–8 and 10–14. Wallpaper/image assets,
font preparation, geometry, services and arbitrary legacy configuration replay
remain outside it. Submission does not acknowledge native allocation, completed
painting, external overrides or rollback after a broken command stream.

No issue closure or merge readiness is claimed. Before integration acceptance,
select a bounded diagnostic/UI continuation, establish the initial UI failure,
qualify the fixed fixture, inspect new captures and render the affected manual.
Do not repeat the four unchanged passing package suites or the full OS matrix.
#214/#215 and the frozen #217/#218 affected-profile gates remain open; broader
release qualification stays with its existing owners. Earlier accepted evidence
is preserved under its original source identity.

Artifacts: `builddir/codex-2026-11-07/r1-live-theme-reload`.
Initial plan: `5425e8047506e559c7374fdb2a73ae501776756603043aa69db6d5cf3a7e1f05`.
Shared-host correction: `162b2fd863c98ab880c1e01706513a1c37e700bda6bc58379ca698456c270ff4`.
T1-C1: `5a64e32dc9174a2136c0e68930f792eb2e5f5bf4c7972b80c620b523ebd4e52f`.

Requested Temper workflow, analyst, implementer, product and reviewer roles were
used. Initial reviewer dispatches hit a thread limit; the existing reviewer then
resumed. Actual backend model routing remains unverified.
