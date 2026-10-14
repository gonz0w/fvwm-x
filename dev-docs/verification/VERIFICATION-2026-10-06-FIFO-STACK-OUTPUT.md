# QUEUE-01 / STACK-02 / OUTPUT-01 qualification — 2026-10-06

Status: locally accepted for the frozen runtime scope below. Native Linux release
CI and physical/toolkit/compositor checks remain separate. Later tree-query,
Go/Lua, desktop/theme and synchronous-deadline work is excluded.

## Completed development stories

- Transient layer changes publish EWMH state for each affected window. The title
  redraw receives that window's highlight state. The actual-core regression first
  fails on stale child EWMH state, then passes parent/child invocation, all three
  focused targets, ABOVE/BELOW and sticky-state preservation, unrelated windows,
  stable stack order and destroyed-parent survivors. The old title baseline was
  inconclusive because transients lacked decorations; the corrected fixture
  explicitly decorates them and verifies X input focus and actual title pixels.
  No claim is made that the former pixel probe demonstrated a highlight defect.
- An explicit unlocked FIFO transfer detaches one head without cleanup scans or
  callbacks, transfers object ownership and refuses active queue locks. Module
  output and dequeue-before-callback scheduler/event drains use it. Generic
  callback traversal and tombstone cleanup retain their existing owner. Empty
  front insertion now maintains the tail, and the static initializer covers all
  fields. Native tests cover mixed insertions, null objects, tail reuse, nested
  removal/addition/destruction, locked refusal and a 10,000-entry drain.
- The user approved asynchronous module output bounds: 64 KiB, 32 complete
  packets or 128 attempts per pass. Partial offsets and packet order remain;
  successful bytes, completed packets and attempts are counted separately.
  Writable readiness resumes remaining output. Synchronous lock-on-send retains
  completion and framed unlock semantics. Pure boundary tests and actual-core
  controlled/real-pipe checks cover partial writes, EINTR/EAGAIN, fast/delayed
  readers, accounting, zero/terminal disconnect and existing death-row pins.

## Evidence and scope

The independent Linux development source is `/tmp/fvwm-button-release-source`.
Ten focused checks pass after the output changes: queue/event policy, module
runtime and producer order, actual module backpressure, scheduler responsiveness,
Move/Resize, viewport/menu servicing and transient layer regression. A relinked
actual-core output fixture passes seven groups; its permanent Meson registration
also passes on the frozen development source.

An instrumented queue probe counts cleanup-record visits, without wall-clock
thresholds. At 1,000/2,000/4,000/8,000 entries, old front removal visits
500,500/2,001,000/8,002,000/32,004,000 records; FIFO transfer visits zero. This proves
removal-work improvement, not end-to-end latency. Inputs/source hashes are in
`builddir/codex-2026-10-06/queue-fifo/`; original output service baseline and
fixture are in `output-service/`.

Final scoped ASan/UBSan checks pass four policies with leak detection and six
actual-core checks without it, including the permanent output fixture. An initial
unit run reported one retained 24-byte queue record in `test_recursive_drain_shares_budget`; the fixture now
explicitly drains its asserted remaining job. Original diagnostic and repaired
checks are retained. This was a test cleanup omission, not an established
production leak. The corrected permanent output fixture also passes its affected
scoped sanitizer rerun; all ten composed scoped checks pass.

No new dependency, thread, scheduler, public command syntax or geometry policy.
Native Linux release CI and physical/toolkit/compositor gates remain unrun.

Frozen combined source: 1271 files, SHA-256
`c2d9ede0f841bd299f13977902e03c06109f36f8ff2812152a72cdd6c3fffc39`.
All seven evidence archives validate exact source hashes/modes and every payload
to EOF. Each standard profile compiles the core/native modules and passes its
full registered suite, composed from the original full run and a fixture-only
affected rerun. Staged binaries match the tested code after Meson's expected
RPATH adjustment. Go and documentation targets are disabled for this runtime
checkpoint; dependencies/features are recorded per profile.

| Local environment | Compiler profiles | Qualified entries | Installed identities |
| --- | --- | --- | --- |
| Linux ARM64 | GCC, Clang | 132 each | 30 |
| Linux AMD64 QEMU | GCC, Clang | 132 each | 30 |
| FreeBSD ARM64/AMD64 QEMU | Native Clang, each | 120 each | 30 |
| OpenBSD ARM64/AMD64 QEMU | Native Clang, each | 120 each | 30 |
| Total | 8 | 1008 | 120 |

Software-rendering/cursor and SVG-disabled Linux ARM64 profiles pass 131/111
entries. Scoped Clang ASan/UBSan passes 10 checks (four pure policies with leaks
enabled, six actual-core checks with leaks disabled). These local results do not
substitute for native Linux release CI. This checkout has no Git remote.

Evidence, source-finalization proof and `qualification-summary.json` are under
`builddir/codex-2026-10-06/fifo-output/`. Development baselines and initial
failures are retained in `development-evidence.tgz` and `sanitizer-history.tgz`,
alongside each guest's original full logs/artifacts. Collecting the final archive
completed before the successful full validator pass; an earlier validator attempt
reported that the still-uncollected Linux AMD64 archive was missing.

The initial compiler source was
`537dddd18f5522bd92a495b5d2bd5011aee5371e040a4861d453921ce18b55a5`.
All four BSD initial runs passed 119 entries and failed the new output fixture: the
existing core SIGCHLD handler sometimes reaped the fixture's reader before its
waitpid assertion. Only `tests/module_output_start.c` changes in the final source,
blocking SIGCHLD while the fixture owns that reader and restoring the prior mask
after waitpid. Every compiler profile receives an affected fixture rebuild/rerun;
original full logs and failing artifacts are retained alongside composed results.
Production code remains byte-identical between initial and final compiler
sources. Initial Linux GCC/Clang runs all pass; initial BSD runs fail only that
fixture. All eight affected fixture reruns pass, and all four default BSD servers
pass six drivers each (24 invocations): Move, Resize, Scroll, Menu and stacking
use the installed core; output uses the build-native actual-core fixture.
Installed module/worker paths come from the staged profile. The repaired
sanitizer output check passes; the other nine scoped checks retain their
unchanged-source evidence. No production behavior was altered for the repair.
