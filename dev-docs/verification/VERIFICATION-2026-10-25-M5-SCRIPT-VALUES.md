# M5 Script argument-value ownership — 2026-10-25

Owners: SCRIPT-01 #4 / CLEAN-01 #34. Base merged PR247:
`af11b649aac8ee8463f33162da6940be8a958820`.
**Focused checks pass; broader owner/release acceptance remains pending.**

The scanner allocates each VAR/STR/GSTR argument token. AddVar/AddConstStr borrow
and duplicate their input, including repeated-variable early returns, but the
argument grammar never released the consumed tokens. The actual old parser/
scanner control retains eight tokens: five VAR, one STR and two GSTR including
an empty literal. The repaired parser releases all eight exactly once while
stored copies and table IDs remain correct. The before-control privately links
the new test/table helper; its parser/scanner objects are unchanged old source.

Three successful grammar reductions now free their token only after constructor
return. Header/widget tokens have different ownership and are untouched.
Pinned Bison 3.8.2 / Flex 2.6.4 generation uses existing generate.py: script.c has
exactly the three action changes; generated script.h and scanner.c are byte-
identical. Constructors, parser tables/token IDs, limits and evaluation order
remain. Version/input/output receipts and the full generated candidate survive.

The typed complete-table owner publishes names, values, frozen count and existing
storage state only after successful parsing. Runtime Set/Loop replace entries
in those arrays; retirement reads current values. Shutdown releases the completed
table only after QuitFunc/message/widget callbacks and at zero execution depth,
clears globals and resets logical MemoryLimit accounting. Active Init/event/
Periodic exits retain tables and accounting for process reclamation. Partial
parser/error stacks, header/widget strings, base X/fonts and blocked pathname
work remain separate. There is no new budget, parser representation or UI.

## One focused checkpoint

Reuse the owned Linux ARM64 normal GCC 13.3.0 and scoped Clang 18.1.3 profiles.
Actual compiler/options/commands are recorded. The initial 542 baseline identities
and two additional unchanged regression-test inputs are verified before candidate
upload; all 548 final identities match host/guest hashes and modes.
All combined gates pass on their first invocation (8.235 command-seconds):

- The affected module and six units build; script-value-owner, script-parser,
  script-storage, script-execution, script-block-owner and script-binding-owner
  pass. Existing parser/value/total-limit boundaries keep their expectations.
- The new unit observes actual consumed lexer tokens, distinct empty fields,
  current-value replacement, frozen extent, repeated clearing, unpublished state
  and active-depth refusal. It passes scoped ASan/UBSan with leak detection enabled.
- Six real-module X cases preserve self-referential Set, Loop counter/value
  replacement and Init/event/Periodic/QuitFunc reads. The observer compares current
  logical name/value byte accounting before retirement and on active exit.
- The existing production execution/budget/recovery X fixture passes.

Each live fixture has fifteen table entries. Idle final/dead-pipe exits release
15 names, 19 watched values (initial plus observed current replacements), and
two arrays; startup refusal releases 15 names, 15 values and two arrays. All
three retire once, clear globals and leave accounting zero. Active Init/event/
Periodic exits make no retirement call, retain all names/two arrays/current
values (32 allocations), and preserve positive accounting. The four released
initial values on those paths are normal replacements. Intermediate temporary
values are not all tracked; these counts establish the selected current-owner
boundary, not whole-module heap size or RSS.

Source/test review and repository checks/lint pass. Before dispatch, a test-only
enum collision with a generated token and an overlong comment were corrected;
no runtime gate failed and no production repair followed the combined run.
Separate final source/raw-evidence review passes with no actionable findings;
signed author-DCO delivery follows.

Canonical private archive: `m5-script-values/native-evidence.tgz` under the
October 25 ignored development tree; 102 payloads plus manifest,
1,788,150 bytes; SHA256
`9f9ca0a6de306a3cab1b126fb64938bef517087fae4da245c5424faa913bf169`. Every payload hash, size and mode is
verified. It retains old-source/control inputs, pinned generated candidates,
raw unit/X results, profile/compile commands, final source and output identities.
The 103 verified guest capture files and duplicate archive are retired after
exact manifest/active-reference checks. Canonical host evidence, reusable
source/build caches and PR243 performance references remain.

No new UI, so no theme/screenshot pass is required. No fresh BSD/AMD64/install/
full default-X, whole-parser/module leak-clean or release claim. Guest scoped
execution is not separate native-CPU/physical/toolkit/compositor qualification.
Broader #4/#34 remain open; #31/#35/#36 remain completed and #93 not planned.
PR243 reference is 4/5 after PR247; this merge makes 5/5 and the next regular
performance observation due. No comparison is due before that merge, and no
speed/RSS improvement is claimed here.
