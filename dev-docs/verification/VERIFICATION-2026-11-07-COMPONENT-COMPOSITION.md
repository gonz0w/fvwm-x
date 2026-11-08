# Component themes and resilient named icons — #214/#215

Y2 expands PR373 into one related feature batch: four component-setting stories
plus shared asset-local fallback for application menus, directory roles and tray
items. Five new affected Go package checks pass; all six supported OS/architecture
Go helper builds pass (Linux ARM64 native; five cross-build profiles). Three
changed manuals render and are inspected with mandoc at 60/80 columns. The
corrected Classic component scene passes all four cases, composing earlier
unchanged Slate/Dune passes. The final mixed-icon scene passes all 13 cases.
Separate final review and integration determine issue closure; R1 is still in
development. The [single receipt index](VERIFICATION-2026-11-07-COMPONENT-COMPOSITION.json)
retains both delivery stages and exact source/archive identities.

Malformed PNG/XPM headers and vanished assets can select explicit fallbacks,
then text. Healthy sibling images/actions survive. Malformed selected theme
metadata chooses text without changing inheritance precedence. Strict resolution,
invalid requests, cancellation, aggregate budgets and publication failures retain
failure status. Same-size PNGs now use the existing decoder to validate pixels,
then retain original bytes. XPM body and SVG decoding remain native.

## Frozen acceptance map

Earlier evidence is reused only for unchanged contracts. [Composition](VERIFICATION-2026-10-26-THEME-COMPOSITION.md),
[high contrast](VERIFICATION-2026-10-26-HIGH-CONTRAST.md),
[appearance](VERIFICATION-2026-11-02-APPEARANCE-CHECKPOINT.md),
[named menus](VERIFICATION-2026-10-26-NAMED-MENU-ICONS.md),
[glyph/lifecycle](VERIFICATION-2026-11-03-DRAWING-GLYPHS.md) and
[theme reload](VERIFICATION-2026-11-07-THEME-UI-CLOSEOUT.md) retain their recorded
source/profile scopes, including numeric native state/contrast observations.

| Issue criterion | Result supplied by accepted and current evidence |
| --- | --- |
| #214.1 versioned roles | Existing color/state/motion/contrast/icon roles plus current v2 font/density/style/border/corner projection. Native corner radius is explicitly zero. |
| #214.2 independent choices and scoped precedence | Accepted palette/wallpaper/icon choices plus launcher/Form fonts/style/density, inherited defaults, module overrides and scoped reset; three configurations compose. |
| #214.3 native mapping, metadata and one store | Existing Colorset/native consumers and provenance; current schema12 migration, effective fallback metadata, complete 38-setting reply and compact editors. |
| #214.4 assets/invalidation/refusal/reduced images | Existing recovery/conflict and refresh-generation tests; new invalid-asset, hard-failure, replacement, retained-export and format-disabled text cases. |
| #214.5 source-bound native evidence/docs/profiles | Accepted numeric contrast/state results; three-theme/enlarged captures, manuals and current affected Linux/native plus six-target Go compilation. #75 retains whole shipped-theme acceptance. |
| #215.1 request/resolver contract | Accepted theme/name/size/quarter-scale/state/override/fallback, inheritance/hicolor/legacy paths; new optional resolution preserves strict admission and order. |
| #215.2 theme/scale/cycle/missing/invalidation/formats | Accepted installed/user theme and cycle/scale lookups; current actual quarter-scale pixels, mixed bad/good images, replacement and disabled-format producers. |
| #215.3 shared isolation/generations/bounds | Existing isolated owners; current fatal-boundary/cancellation tests and retained exports under the explicit durable-artifact contract. No new decoder/cache/collector. |
| #215.4 accepted artwork | Unchanged approved 16 Lucide glyphs, complete notices/provenance, default-off install: 20 files / 19,201 content bytes. Papirus remains user-installed. |
| #215.5 two consumers/theme/native evidence | Application/directory menus pass actual image and text checks in three configurations; tray mixed-sibling protocol regression also passes. Published captures are inspected. |

## Execution and limits

Implementation preceded this combined checkpoint. Its first run passed all 23
preceding commands; only mixed-icon UI failed. The first fixture correction
recreated self-destroying directory menus, but repeated publication could cancel
pending SVG work or change inode identity. The last correction reuses the
existing Lucide exact-path persistent readiness probe, then checks the original
unmodified directory menu with strict pixels and forced dismissal/recreation.
The final scene passes; none of the preceding 23 commands was repeated.
Both earlier failures remain failures in the receipt. An initial host admission
also refused a changed foreign-VM PID before starting anything; the corrected
functional-only count/RSS guard preserved that VM. All executed owned labs stop,
all command groups retire without forced kill, and no observer X error occurs.

Ready-image presentation and fallback are qualified here; cold SVG activation
for self-destroying menus is not asserted. The compiled all-formats-disabled Go
producers run on the full-feature native core; this is not a fresh minimal C
build. Existing IMAGE/CACHE acceptance stays scoped to its original results.
Native BSD, physical/assistive, full install/default-X and whole-release gates
remain separately required at the R1 release checkpoint under the current
risk-based cadence. Cross-compilation is not native BSD execution. No new
sanitizer/race campaign, benchmark or RSS improvement is claimed.

The [durable-export contract](../decisions/NAMED-MENU-ICONS-2026-10-26.md#y2-exported-artifact-lifetime-contract--2026-11-07)
resolves A5's lifetime choice: keep immutable images while live/saved/copied
exports reference them; manual retirement follows removal of all references.
Per-job bounds do not imply a cumulative disk quota. No shared-cache GC or
worker-exit deletion is introduced.

Nine fresh [published captures](../../doc/development-preview.md#independent-component-settings-in-progress)
are inspected: three Classic component views, three application palettes,
Classic directory, disabled-format application and invalid-theme directory.
The green square is synthetic healthy-image evidence, not shipped artwork or a
contrast claim. All 12 icon captures remain in the final raw archive
`e8086db1823383bb8be7a172f0c6f22e63a834ecec1b8655a11ed2f63afe4c58`
(31 files). Initial combined archive has 154 files; exact digests are in the
index. Source/build/SDK/cache remain reusable; cross-build outputs are hashed
then retired. No package install or disk expansion occurred.

## Historical Y1 draft checkpoint

The following records the earlier partial delivery and its original failures.
Y2 above supersedes its pending Classic/current-draft status; it does not
reclassify those historical attempts or broaden their evidence.


This draft batches four feature stories: launcher flat/relief facets, separate
launcher item/title fonts, three next-start Form font roles, and independent
launcher/Form density. Eight new keys use the existing atomic store and module
override precedence. Schema12 loads exact schemas1–11 without rewriting them.
Compact Defaults, Launcher and Forms editors retain page-local reset/apply.
Effective role version2 exposes resolved fallback without claiming observed
per-widget ownership. No new dependency, resident helper or preferences store.

Base: merged PR372, `0333fe370db7d1924470228dc439864ac16c11c1`.
PR372 and #218 are complete. Broader #214 remains open.
The [receipt](VERIFICATION-2026-11-07-COMPONENT-COMPOSITION.json) identifies
source overlays, actual commands, failures, reuse and archive identities.

## Results and remaining blocker

All five affected Go packages pass on the owned Linux ARM64 VM. The combined
incremental MFL/request-unit/three-Go build passes. The corrected request unit
passes, including exact16/8/32KiB fragmentation/JSON boundaries and rejection of
asynchronous fragments for synchronous local operations. Both native manuals
render; host mandoc renders at60/80columns and the new text is inspected.

The combined native-X scene passes all five Slate and four Dune cases:
launcher/Form geometry independence, next-start Form roles, scoped reset
isolation, keyboard page navigation/Cancel, plus Slate's actual maximum-string
saved/module-override roundtrip. That real38-setting JSON reply is10801bytes.
Seven unmodified captures are preserved and visually inspected in the
[gallery](../../doc/development-preview.md#independent-component-settings-in-progress).
Labels/controls fit in the captured normal-size editor pages; screenshots do
not establish accessibility acceptance. The enlarged Classic menu capture shows
its legacy navy/wheat Form; its editors were not reached.

Classic stops at the strict assertion that the changed launcher must also grow
wider, after the height-growth assertion succeeds. The run does not retain the
two widths before this assertion. Static review identifies an unsupported
expectation: changing from monospace18 to proportional Sans20 need not widen
the items; fixed menu gaps and the title also determine width. Submitted commands
and the capture support role application, without proving full acceptance.
A source-only repair now compares Classic monospace18 with monospace24/28 and
records both geometries before asserting. This final fixture correction is
unexecuted; the failed run is not reclassified. Do not call this a passing
three-theme checkpoint or weaken the oracle merely to obtain a pass. Remaining
Classic Form/reset/editor coverage is unrun. The PR stays draft.

## Failure history and bounded stop

1. Initial combined Go run: four packages pass; the controller exposes a real
   maximum-reply regression (up to11272bytes versus8192). No native build/UI ran.
2. Diagnosed correction: desktop settings results alone become16KiB; controller
   and incremental builds pass. The new request-unit fixture aborts because it
   incorrectly includes synchronous file list/register operations in its async
   fragment table. Production correctly rejects those fragments.
3. After explicit STOP/checkpoint and separately reviewed remaining-only plan,
   the test classification is corrected. Only its incremental rebuild/unit,
   two previously unrun manuals and one previously unrun UI command execute.
   Unit/manuals pass; Classic's width assertion fails. The one remaining
   execution is exhausted. No further native attempt is authorized by this record.

All original receipts remain intact. Failed dispatch receipts retain their
build-controls assertion; separate holder cleanup receipts confirm the owned
container and VM stopped after every attempt. All command groups are absent
without forced kill; the UI observer reports no unexpected X or cleanup errors.
The final254-file raw archive is
`d93cf2617e0a859aa56a734abaa041029b0f8d40d91bed69264adbcab10fef30`.
Actual native command work totals about24seconds across these three executions;
preparation, review, VM admission/export and host time are separate.

## Scope and cost

The readable desktop-settings JSON ceiling is16KiB; other provider replies stay
8KiB and core queries32KiB. Producer fallback and both native assembly/final
validation use aligned limits. Existing request/client/wait/queue/deadline caps
remain. The maximum extra allocation is8KiB per active desktop reply (up to2MiB
at all256possible wait slots); no zero-memory or measured-RSS claim is made.
State-file limits remain unchanged. The change adds no codec or dependency.

All2534 admitted sources verify and production binaries remain unchanged in the
remaining-only run; only its request-unit binary rebuilds. Source/build/SDK/cache
are retained for continuation. Earlier unchanged X1 core/ownership checks and
five package/build passes are reused, not re-executed or broadened. Repository
foundation/whitespace checks pass. No full platform/install/default-X matrix,
new sanitizer/race campaign or performance comparison ran. Those release gates
remain separately owned; marginal tuning and unrelated findings stay backlog.
