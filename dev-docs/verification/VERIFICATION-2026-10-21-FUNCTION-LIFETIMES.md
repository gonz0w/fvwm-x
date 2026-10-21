# Function-wait cancellation and retained lifetimes

2026-10-21 — INPUT-01 / DATA-01 / CLEAN-01, with FD-01 / LOOP-01
ownership dispositions. Base: merged PR142
`83c787c6615ed9cf4bc4dc71183a2f3d8cbefbc4`.
Implemented and focused-qualified; combined qualification is pending.

## Three reproduced failures

- Target selection blocked inside XMaskEvent after SIGTERM and needed further
  input before shutdown. Use the existing finite interaction wait and restore
  pointer/keyboard grabs on cancellation.
- A held complex-function click classifier ignored termination. Its bounded
  existing sleep/event loop now checks the shared cancellation policy and
  returns a distinct internal cancellation result, unwinding through the
  existing function-depth/argument/grab cleanup.
- A destroyed target could still trigger click actions after physical release.
  Detect queued destruction/withdrawal without dispatching lifecycle handlers;
  skip remaining conditional phases. A nested immediate/late Wait may already
  have dispatched destruction, so check the retained record's retirement flag
  before resuming classification and after late immediate actions.

No configuration syntax or dependency changes. Existing click/hold timing,
Escape target selection and shutdown functions remain. These are three concrete
failure behaviors in one finite closeout pickup; no cleanup is manufactured to
fill another batch. Performance tuning remains deferred.

## Focused checks and controls

The preserved preceding Linux ARM64 core, SHA256
`c17298025136189fe59e554efe8fe56d2f01223a2c6073dad27c7d03ffb3f095`,
fails all three new controls: selection and held classification miss a two-second
shutdown deadline, and destruction still produces the click Echo. These are
isolated nested-X failures with owned cleanup, retained under
`builddir/codex-2026-10-21/interaction-lifetime-safety/baselines.tgz`.

Eight focused checks pass on Linux and FreeBSD ARM64 each. After extending the
late-Wait retirement case, only four affected integration checks rerun on each;
unchanged policy/context/module owner units are retained. New cases check normal
click, Escape, both SIGTERM waits, target destruction, late-Wait target retirement,
active-function DestroyFunc refusal, Wait resumption on a named replacement,
ExitFunction execution and child cleanup.

## Selected lifetime dispositions

| Entry point | Disposition |
| --- | --- |
| WaitForButtonsUp | Retains only root/event/button/timing values, checks termination, uses the bounded audited pump and keeps lifecycle dispatch behind physical release/outer unwind; existing release regression retained |
| CMD_Wait | Owns wait text; refreshes X context after dispatch before name/class/binding access; new real-X case destroys the initiating target and resumes on a named replacement |
| execute_complex_function / immediate callees | Three cancellation failures fixed; pending phases skip retired targets, use_depth and outer function flag retain records/items until existing cleanup |
| Execution contexts | Trigger snapshots owned by clones; window/module/live-event references remain borrowed under existing callers; context unit coverage retained |
| Module command context | xinputRefs pins the active input through execute_function and nested Wait; retirement/death-row cleanup waits for zero references and active iterators; runtime/queue unit coverage retained |
| Function destruction / deferred windows | Active use_depth refuses DestroyFunc; complex/menu execution defers managed-window frees and removes client context before reuse; real-X Wait/late-Wait cases exercise retention |

This disposes the selected finite closeout entry points; broad audit owners,
native-CPU sanitizer, hardware/toolkit/compositor and release acceptance stay
open. Handoff to features depends on completion of this checkpoint and the
tracker criteria, not completion of the historical optimization inventory.

## Combined checkpoint

Pending one frozen six-target compiler/install/default-X run, affected optional
profiles and scoped guest ASan/UBSan. Preserve failures and rerun only affected
checks if corrections are needed. PR142 is merged; PR138 performance reference
is now 3/5, so no comparison is due for this unmerged checkpoint.
