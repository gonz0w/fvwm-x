# M2.3: read-only configuration checking in Go

After merged PR191 and Agent Temper adoption PR193, this bounded #23 slice
removes the Python literal-source walker. The existing isolated Go worker now
owns source reads, include traversal, hashes, diagnostics and budget accounting.
It calls the existing settings package directly. Resident Python validates the
whole result before presenting the existing paged native report. Settings file
commits, generations, editor/preview ownership and module publication stay with
the resident controller; no new service or dependency is added.

The simpler alternative was to leave source walking until the whole controller
port. This independent read-only owner can move now without splitting mutable
state or introducing another persistence authority. Broader resident migration
stays the next M2 owner. No new feature or configuration migration is selected.

Acceptance preserves user/data/cwd Read precedence, optional Quiet for missing
files only, inode-based active cycles, repeated nonrecursive includes, physical
and logical source locations, UTF-8/NUL diagnostics, last-valid schedule values,
explicit unsupported/expanded coverage and the prior finite work limits.
Requests require four bounded path strings; ambiguous or unrelated operation
fields are refused. Sources are opened nonblocking, then type/identity/read use
the same descriptor. Only regular files are read. No configuration is executed.

## Focused checkpoint

[Machine-readable receipt](VERIFICATION-2026-10-24-M2-CONFIGURATION-CHECKER.json)
binds changed inputs, native identities, cross-builds, UI records and raw logs.
Canonical host evidence is
`builddir/codex-2026-10-24/m2-configuration-checker/evidence.tgz` with SHA-256
`6888738657797c7916a8bcc052668e5e3b4f39b9c1d789ed40522c101e3f8624`.
All 57 archived payload hashes/sizes/modes pass integrity verification; the
complete input manifest is retained beside the archive.

One reused Linux ARM64 GCC profile passes the affected worker build, 31 top-level
Go cases plus 26 subcases across four packages in both normal and race modes,
vet, 75 retained controller tests and 12 independent worker boundary tests.
Python's two monkeypatched checker-budget tests move to the Go owner; byte,
line, depth, visit, diagnostic and elapsed limits remain directly tested.
A complete-result validation regression is added to the resident consumer.
The tagged controller stage finds its adjacent helper without an environment
override; offline CLI and changed man-page rendering pass. Eleven combined
commands take 42.022 seconds; no full build/install matrix runs.

The full native-X desktop fixture passes configuration-report locations,
read-only controls and keyboard paging, alongside retained settings/preview/
service controls. It captures 28 images and all three theme configurations:
Slate dark, Dune light and a controlled classic 9x15/font/border/Form override
on the Grove baseline. This is representative theme smoke coverage, not a full
external-theme audit. Both report pages and all three settings profiles are
visually inspected: readable controls and no clipping. The theme captures retain
the fixture's small transient window and screenshot notice behind the editor.
No UI redesign is claimed. Core/MFL/Form/Buttons/config hashes stay exact.
Six offline worker builds pass for Linux/FreeBSD/OpenBSD AMD64/ARM64; these are
compile-only results. No BSD VM/default-X or native-CPU execution is inferred.

## Review, failures and continuation

Lead self-review found Go's lexical path cleaning would change the meaning of
`link/../file` when `link` is a symlink. A deterministic regression first fails,
then passes in the final Linux normal/race gate with filesystem traversal
preserved. The initial test-first package fails for its not-yet-implemented API;
a preceding host invocation lacked its explicit writable Go cache and could not
start compilation. Those are development observations, not failed Linux gates.
The path-regression log remains in the host evidence directory. No runtime or
fixture changes follow the combined checkpoint. Repository/link/whitespace and
gofmt checks complete locally; this review is not independent human approval.

After canonical verification, retire 70 exact-hash guest capture/archive/input
duplicates (3,608,619 bytes). Preserve host evidence, source, reusable builds,
caches, install stage, disks and other owners. No new portable harness/native C
or ownership sanitizer gate is selected for the read-only Go port.

#23, #7, #15 and M2 remain open. The resident state/dispatch/lifetime port, native
capture boundary, Python-free installed desktop, full-session measurements and
owner/platform/release acceptance remain pending. PR178 remains the broader
integrated reference; this portable owner uses focused checks, with the fifth
post-reference risk review retained from PR191. PR186's periodic comparison is
4/5 merged, 5/5 on this merge; compare preserved binaries on the following run.
This migration adds code/transport validation and makes no total size or speed
improvement claim. Performance-only tuning stays deferred.
