# M5 Script instruction-tree ownership — 2026-10-25

Owner: SCRIPT-01 #4 / CLEAN-01 #34. Base merged PR245:
`9b8780be18f926bacd61734ba2fb977761b1d978`.
**Focused checks pass; broader owner/release acceptance remains pending.**

Init previously released only its root and instruction vector after returning.
Its argument arrays and nested blocks remained allocated in the resident module.
A real-parser control with the exact old two-free cleanup observes 21 owned
allocations, two freed and nineteen retained. This control uses old parser
objects plus private test/helper inputs; it does not execute old BuildGUI.

`script_block.c` now releases complete instruction trees: Type 14 owns its final
two argument slots, types 15/16 the final slot; other encoded longs remain values.
Heap roots and embedded widget-case blocks have distinct release operations.
The typed owner freezes the parsed object count and publishes only after a
successful parse. Empty cases do not read uninitialized pointer slots. Parser
scratch aliases are cleared after publication. Init retires fully after normal
execution returns; Quit/Periodic/case roots retire after callbacks, message drain
and widget teardown. The dead-pipe path retains its early X-cleanup disposition.

Retirement requires zero interpreter depth. Quit from Init/event/Periodic keeps
active trees and aliases for process reclamation. It does not free underneath
live instruction frames or add interpreter unwinding. Complete-parse startup
refusal can retire idle roots without activating QuitFunc early. Normal shutdown
clears borrowed ScriptProp/x11base/case aliases; the freed header pointer is NULL.
Parser-error stacks, variable/string tables, base X/fonts, bindings and storage
budgets remain separate. Parser/generated source and configuration syntax stay
unchanged. The blocked pathname proposal is not retried.

## Combined focused evidence

Reuse the owned Linux ARM64 guest and incremental normal GCC 13.3.0 / scoped
Clang 18.1.3 profiles under `/tmp/fvwm-module-waits-20261015`. Actual profile
options/compile commands confirm `address,undefined` in the scoped build.
536 baseline and 540 final C/header/build/test inputs match hashes and modes.

- The affected module and four unit targets build. script-parser,
  script-execution and script-widget-owner pass initially; the new ownership
  unit passes after its test-only case-label correction. Their unchanged binaries
  are retained, with no broad build or three-unit repeat.
- The new unit checks seven actual parsed tree shapes, independent topology/free
  counts, embedded multi-case arrays, poisoned later empty Main slots, frozen
  object count, repeated clearing, active-depth refusal and non-owned value slots.
  The same ownership unit passes scoped ASan/UBSan with leak detection enabled.
  Parsed child subcases use `_exit`; exact watched counts establish tree release,
  not whole-parser/module leak qualification.
- Six actual-module X cases use the real production objects and private entry/
  parse/free/startup-notification observers. Idle final exit, complete-parse
  startup refusal and dead pipe each free all 34 watched allocations. Init Quit
  retains all 34. Event/Periodic Quit free 22 Init allocations and retain the
  twelve callback-root allocations. Callback output, borrowed aliases and startup
  publication checks pass. No production debug hook is added.
- The existing production-module execution/budget/recovery X fixture passes.
  Repository checks/lint, source review and working/staged whitespace pass.

Original failures remain: an overlay sibling-include omission; optimized direct
free observers with stale cached reads (the first count assertion and later
contradictory output are invalid instrumentation); and two fixture grammar
mistakes (`1:` versus `1 :`, brace versus string WriteToFile filename). Volatile
observer storage and test-only no-builtin-free preserve the original 21/2/19
expectation. Test syntax fixes change no grammar or production behavior. Only
failed/unfinished checks continue; successful builds/units/probe links are reused.
A metadata-collection quoting error is corrected by saving its script; no runtime
gate is repeated for metadata. No production correction follows these failures.

Canonical private evidence: `m5-script-blocks/native-evidence.tgz` under the
October 25 ignored development tree, 138 payloads plus manifest,
2,988,647 bytes; SHA256
`25b78ba29fe7039b0c70b07f54f4b4e56528d85e38630512898d3cd32f9c57bf`.
All payload hashes, sizes and modes are verified. It retains baseline/control
inputs and binaries, original failures, frozen source, scripts, raw unit/X logs,
profile options and final output identities. Separate final source/evidence review
passes with no actionable findings; signed author-DCO delivery follows. The 139 verified guest capture files and
duplicate archive are retired after exact manifest and active-reference checks;
source/build caches, canonical evidence and PR243 references remain.

No new UI, so no theme/screenshot pass is required. No fresh BSD/AMD64, install,
full feature/default-X or whole-module sanitizer/release claim. Guest results do
not establish separate native-CPU/physical/toolkit/compositor qualification.
PR243 performance reference is 2/5 after PR245; this candidate becomes 3/5 on
merge. No comparison is due or speed/RSS improvement claimed. Broader #4/#34
remain open; #31/#35/#36 remain completed and #93 not planned.
