# Independent native component settings — #214

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
