# Function-wait cancellation and retained lifetimes

2026-10-21 — INPUT-01 / DATA-01 / CLEAN-01, with FD-01 / LOOP-01
ownership dispositions. Base: merged PR142
`83c787c6615ed9cf4bc4dc71183a2f3d8cbefbc4`.
Locally qualified in PR #143 for maintainer review. The adjacent
[machine receipt](VERIFICATION-2026-10-21-FUNCTION-LIFETIMES.json)
records source, profile and archive identities.

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

One frozen combined checkpoint passes **2276 standard entries**
across eight compiler/default profiles on Linux, FreeBSD Xorg and OpenBSD 8.0
Xenocara, AMD64 and ARM64. Linux ARM64 software/minimal profiles pass
**47 selected affected entries**. Five scoped Linux ARM64 guest
ASan/UBSan entries pass with leak detection: interaction policy, execution
context ownership and the three real-core function-lifetime groups. These are
guest-native observations, not native-CPU sanitizer acceptance.

| Target | Standard profile entries |
| --- | ---: |
| Linux ARM64 GCC / Clang | 291 / 291 |
| Linux AMD64 GCC / Clang | 291 / 291 |
| FreeBSD ARM64 / AMD64 | 278 / 278 |
| OpenBSD 8.0 ARM64 / AMD64 | 278 / 278 |

Ten fresh installs verify 150 ELF identities after the recorded RPATH
transformation, 77 catalogs and 700 data files. Thirty staged function-lifetime
groups, twenty geometry and ten style groups pass. Four authenticated default
BSD displays pass 32 fixture groups (twelve new function groups plus twenty
existing Status/readiness/session/geometry groups), reject unauthenticated
clients and shut down cleanly. OpenBSD verifies actual 8.0 identities, restored
compiler/PNG hashes and separate built/staged loader proofs. Reusable verified
build caches omit completed captures and install stages from the preceding
batch, avoiding another copy of that old test data.

Signed author-DCO implementation `1ab354074c90e270cbb6dcc3f864130165926ace`
freezes 2115 regular inputs,
archive SHA256
`9b31549a25f06b18c48a3610db9229ca66326f223e455091848a067500513175`.
Native runtime inputs remain frozen throughout qualification. A final C
comment-width repair changes only known block-comment text and preserves all
C tokens and line counts. Other C/header/Python/Meson and AsciiDoc inputs stay
exact. Final Markdown/status and verification JSON do not change tested code. Raw logs, per-payload manifests,
full source hashes/modes and install identities validate in the canonical
archives. No full suite is repeated per story or PR.

The first coordinator had a prior-manifest path typo. Four native attempts
stopped during upload before launching tests; corrected dispatch starts their
single full runs. The same typo interrupted ARM64 preparation input upload;
missing inputs were hash-verified and supplied to that same waiting guest,
without a reboot or repeated test. AMD64 continued using exact copied prior
manifests. Error receipts, the upload recovery and temporary manifest-copy
identities are retained; the temporary copies are removed after collection.
These are orchestration errors, not product test failures.

Portable make verify completes once with raw exit zero: 177 passes and eleven
capability skips among 188 registered entries. All 26 manual pages render and
verify exactly; native FreeBSD mandoc renders the affected command/all pages.
Repository, offline links, new-C widths and whitespace checks pass. Exact
completed guest evidence duplicates are retired after canonical capture and
identity verification; source, reusable builds/stages, performance references,
VM disks and other owners' work stay intact. Owned QEMU jobs exit zero and
remove ephemeral keys.

The finite stream, selected-input and interaction-lifetime closeout now has
fixed/verified or already-covered dispositions for every selected entry point.
No confirmed unresolved high-impact defect remains in that set. After
maintainer review/merge, advance the approved stage 5/6 dependencies and
features in tracker order. Broad audits and native-CPU/hardware/toolkit/
compositor/release acceptance remain open; this is not a security certification
or release approval. PR138 remains at 3/5 after merged PR139/141/142; this
unmerged checkpoint does not increment the count and no comparison is due.
