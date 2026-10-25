# M5 Script binding and message ownership — 2026-10-25

Owners: SCRIPT-01 #4 / CLEAN-01 #34. Base merged PR246:
`15feef97d6438c412a6e5381726b28aef195298d`.
**Focused checks pass; broader owner and release acceptance remain pending.**

Key allocated a constructor action and passed it to AddBinding, which copies
stored strings. The caller's original survived both successful and zero-match
results. Old production Key objects retain three observed constructor allocations
across controlled 0/1/3-copy consumer results; the candidate releases all three.
The invalid-key/modifier paths still bypass AddBinding and complete their existing
cleanup. The unit consumer controls match counts; the X probe uses real AddBinding.

Release the constructor action after AddBinding returns. Correct its stale shared
ownership comment without changing the shared implementation. Extend the existing
static runtime cleanup to retire BindingsList with FreeBindingList and free/clear
LastString. QuitFunc, message draining and widget callbacks retain their order.
Zero execution depth gates every newly retired owner; active Init/event/Periodic
exits retain borrowed bindings/message storage for process reclamation. This also
preserves the dead-pipe early X-cleanup disposition. Cleared pointers make idle
retirement idempotent. No precedence/modifier/duplicate/message contract changes.
Parser/variable tables, base X/fonts, selection storage and blocked pathname work
remain separate; no new production API, module protocol, dependency or UI.

## Focused evidence

One combined gate reuses the owned Linux ARM64 normal GCC 13.3.0 and scoped
Clang 18.1.3 ASan/UBSan profiles. Actual options/compiler commands are archived.
538 baseline and 540 final C/header/build/test identities match hashes and modes.
All gates pass on their first combined invocation (17.634 recorded command-seconds).

- The affected production module, script-binding-owner, script-block-owner and
  script-execution build; all three units pass. The same new ownership unit passes
  address/undefined sanitizers with leak detection enabled. Volatile observer
  storage and test-only no-builtin-free preserve exact allocation observations.
- The producer unit calls actual Key objects with an independent copying consumer
  for zero/one/three matches, checks copied action bytes after constructor release,
  and uses the real shared destructor for complete records/copies. It also covers
  rejected key/modifier inputs.
- Six actual-module X cases use real entry/AddBinding/CopyString/free observation.
  Duplicate F12 retains newest precedence, LockMask is ignored as before, Shift
  mismatch/match and message spaces remain. QuitFunc reads the latest message and
  creates a binding before retirement. Active exits assert zero binding frees,
  with exactly one latest message retained for event/Periodic cases.

| Exit | Constructors freed/owned | Binding allocations freed/owned | Message allocations freed/owned | Retained |
| --- | --- | --- | --- | --- |
| Idle final | 4/4 | 12/12 | 3/3 | 0 |
| Startup refusal | 0/0 | 0/0 | 0/0 | 0 |
| Idle dead pipe | 4/4 | 12/12 | 3/3 | 0 |
| Init Quit | 4/4 | 0/12 | 0/0 | 12 |
| Event Quit | 4/4 | 0/12 | 0/1 | 13 |
| Periodic Quit | 4/4 | 0/12 | 0/1 | 13 |

The existing production text/list X fixture also passes. Native unit/live inputs
are frozen together; no full matrix, build or successful test is repeated for PR
creation. Before dispatch, review strengthens active-owner count assertions and a
fixture spelling is corrected to the existing LastString grammar. No runtime gate
failure or production correction follows the initial candidate.

Repository checks/lint and working/staged whitespace pass. Separate final
source/test/raw-evidence review passes with no actionable findings; signed
author-DCO delivery follows.
Canonical private archive: `m5-script-bindings/native-evidence.tgz` under the
October 25 ignored development tree; 103 declared payloads plus manifest,
2,204,159 bytes; SHA256
`8ed6d71cec9f4f42e79fb4dbee08b8a889200418ad02dda50f7753cf2e4692e7`. Every payload hash, size and mode is
verified. It retains baseline source/object control inputs, compiler/link recipes,
raw results, configuration/profile receipts, source snapshots and output identities.
The 104 guest capture files and duplicate archive are retired after another exact
manifest and zero-active-reference check. Canonical host evidence, reusable
sources/builds and PR243 performance references remain.

No new UI means theme/screenshot checks are not applicable. There is no fresh
BSD/AMD64/install/full default-X or whole-module leak/release qualification claim.
Guest scoped execution is not separate native-CPU/physical/toolkit/compositor
acceptance. Broad #4/#34 remain open; #31/#35/#36 remain completed, #93 not planned.
PR243 performance reference is 3/5 after PR246; this merge would make 4/5.
No comparison is due and no speed/RSS improvement is claimed.
