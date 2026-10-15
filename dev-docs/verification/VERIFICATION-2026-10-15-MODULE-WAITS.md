# Synchronous module readiness — 2026-10-15

Owner: LOOP-01 [#36](https://github.com/gonz0w/fvwm-x/issues/36).
Status: combined checkpoint locally qualified; ready for PR review.
[Machine receipt](VERIFICATION-2026-10-15-MODULE-WAITS-COMBINED.json).
PR #96's ordinary/interactive checkpoint is merged. The separate OpenBSD/VESA
cause investigation #93 stays deferred until much later.

## Problem and resulting behavior

The three remaining synchronous waits in `module_list.c` built descriptor sets
for an unlock response, blocked output and startup. They now use the existing
cancellation-aware `fvwmPoll` wrapper and native descriptor arrays. Shared pure
deadline conversion rounds positive remainders upward and clamps at `INT_MAX`.
Expired/invalid/regressing clocks initialize the timeout result to zero. A
clamped poll interval ending before the absolute deadline continues the wait;
it neither refreshes the deadline nor prematurely retires a live producer.

Lock-on-send keeps its output/unlock barrier, inline command execution, recursive
frame ownership, death-row pins and retire-on-timeout policy. EOF/error readiness
allows buffered complete responses to be consumed before EOF. Invalid input,
invalid output and output hangup use the existing retirement path. Startup keeps
read-before-write order, finite 100 ms X/Escape checks, asynchronous output
budgets, custom Expect, partial-frame continuation and the live module after
startup timeout or terminal readiness failure. Syscall errno is saved before
clock/remaining helpers. Ordinary event dispatch and session callbacks are not
introduced into these synchronous command barriers.

No configuration syntax, packet ABI, installed option or required dependency
changes. Constructor and ICE/session descriptor admission limits remain in
place. Real high-endpoint controls run where inherited process limits permit;
other guests explicitly report that limitation. This is a consolidation, with
no whole-transport high-descriptor or measured desktop-latency claim.

## Tests and exact source

The native `event-loop-policy` unit covers positive and submillisecond rounding,
expiration, maximum clamp and invalid/regressing clocks. Existing actual-core
synchronous/startup fixtures now intercept native poll arrays. Their 25/32 named
cases preserve partial read/write, interruption, activity, recursion, command
ordering, retirement, buffered Escape and asynchronous handoff coverage. New
controls cover real high endpoints where available, invalid endpoints, output
hangup and complete responses followed by real pipe EOF. Two deterministic clock
controls exercise maximum poll-interval expiration without waiting 24 days.
Real child producers still exercise elapsed deadlines and startup continuation.
Drivers accept owned nested servers and borrowed authenticated default displays.
The fixtures use actual core objects/X/pipes with controlled transport and clock
callbacks for deterministic cases. They end with `_exit` and do not qualify full
shutdown leaks or a real external session manager.

Base commit: `9c91f1c2992983f2a8bf3490b607bfd0fc5ddc0d` (merged PR #96).
Full-suite archive: `8cf67ca691c1c55c551de0403ffa15a1cc9059db057aee56b5af9544235cebaf`.
Final archive: `9ed48f0c34d9d1bf9747b5a3589927b8e706b748059e63e676507672ae02864e`.
Both contain 1943 regular files with exact hashes/modes.
All nine final code/build paths match the publication candidate byte for byte.

Review while the full suite ran found that an `INT_MAX`-clamped timeout could
return before the absolute deadline. The final three-file refinement adds
zero-result continuation and its two regressions in `module_list.c`, the
synchronous fixture and its driver. Every other archived source byte/mode is
identical. The full suite is retained against its exact source; all affected
profiles are then incrementally rebuilt, checked, freshly installed and compared
against the final source. This preserves unaffected full-suite evidence without
claiming the final refinement received another whole-suite run.

## Combined local qualification

| Target / profile | Full-suite entries | Final affected entries | Final installed code | Catalogs |
| --- | ---: | ---: | ---: | ---: |
| fvwm-x-linux-arm64-svg/gcc | 249 | 5 | 15 | 11 |
| fvwm-x-linux-arm64-svg/clang | 249 | 5 | 15 | 11 |
| fvwm-x-linux-arm64-svg/software | 228 | 5 | 15 | 11 |
| fvwm-x-linux-arm64-svg/minimal | 197 | 5 | 15 | 0 |
| linux-amd64/gcc | 249 | 5 | 15 | 11 |
| linux-amd64/clang | 249 | 5 | 15 | 11 |
| freebsd-arm64/default | 237 | 5 | 15 | 11 |
| openbsd-arm64/default | 237 | 5 | 15 | 11 |
| freebsd-amd64/default | 237 | 5 | 15 | 11 |
| openbsd-amd64/default | 237 | 5 | 15 | 11 |

The full candidate passes 1944 standard entries across eight
compiler profiles/all six OS/architectures, plus 425 optional
software/minimal entries. The final candidate passes 50
affected entries, covering deadline policy and the synchronous/startup/output/
ordinary-readiness core controls wherever registered. Minimal profiles run their
registered subset. Each final profile has fresh staged installation and installed
version execution; 150 native-code identities and
99 catalogs match tested outputs after normal rpath
transformation. The same comparison also validates the retained full candidate's
150 code identities/99 catalogs.
Compiler/dependency/feature environments and exact argv are retained.

Four private-authority BSD default servers pass
20 full-candidate fixtures: synchronous, startup and
ordinary-readiness probes plus installed WM/MFL interactive geometry at 1×/1.5×.
Four final-candidate default servers add 8 affected
synchronous/startup fixtures. All eight reject an unauthenticated client and
stay alive through their fixtures. Actual cleanup records show
8 clean exits and 0
bounded forced cleanups. No separate #93 cause investigation was undertaken.

Linux ARM64 Clang ASan/UBSan passes three final scoped entries with the supporting
library/core objects instrumented. Pure deadline policy enables leak detection;
controlled actual-core probes disable it because they use `_exit`. All halt on
sanitizer errors. Explicit compiler-rt linkage and effective options are saved.
These local emulated results do not establish native-CPU release qualification.

Portable `make verify` passes 166 entries with
11 explicit native-Meson skips on macOS. That foundation run
preceded the final timeout refinement; its portable tool inputs are unchanged.
Final repository/link/branch/staged checks pass separately. No full portable or
native matrix is repeated merely to create a PR.

## Preserved attempts and limits

The first generated focus command used a stale target name. The next BSD build
caught a duplicated adapter block returning a value from a void test function;
the test edit was corrected before runtime checks passed. Linux's existing
Podman machine needed restarting inside a persistent control session. Final
logic-focused checks passed on Linux/FreeBSD/OpenBSD ARM64 before the combined
run; the maximum-interval refinement also passed an isolated FreeBSD ARM64 check.
All failed setup/test-adapter attempts and their source snapshots remain local.
No deadline was relaxed, and no production workaround was introduced for them.

OpenBSD `/home` capacity checks showed insufficient space for further stages
(ARM64 reported negative ordinary-user availability; AMD64 had 371 MiB free).
After each successful full/default-X job completed, only its current owned tree
was copied to the empty `/usr/obj` partition. Every regular-file hash/mode and
symlink identity matched before removing that original and preserving `/home`
through a symlink. Relocation receipts retain every identity; qualified
predecessors, unrelated trees and VM disks are untouched. The final source
preflight verifies the retained full source before applying its three-file delta.

Native-CPU release sanitizers, physical input/GPU/toolkit/compositor acceptance,
external session-manager evidence and broader descriptor/runtime audits remain
open. No hosted CI, remote success publisher, release or upstream notification
is added. The exact source manifests, executed runners, argv, logs, original
attempts, six complete evidence archives and strict validation are under
`builddir/codex-2026-10-15/module-wait-readiness/` (ignored).
