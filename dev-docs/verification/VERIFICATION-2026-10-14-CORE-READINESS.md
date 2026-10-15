# Core descriptor readiness — 2026-10-14

Owner: LOOP-01 [#36](https://github.com/gonz0w/fvwm-x/issues/36).
Status: combined checkpoint locally qualified; ready for PR review.
[Machine receipt](VERIFICATION-2026-10-14-CORE-READINESS-COMBINED.json).
PR #95's cache checkpoint remains accepted and merged. The separate
OpenBSD/VESA cause investigation #93 remains deferred until much later.

## Problem and resulting behavior

Ordinary dispatch constructed fd sets, then the image adapter scanned those
sets to construct poll descriptors when a decoder was active. Interactive waits
constructed a separate module/descriptor snapshot. Both paths now use one typed,
operation-local snapshot and module transport service, with native poll even
when no decoder is active. This is a consolidation, not a desktop latency claim.

The three related stories are native poll/deadline handling, shared snapshot
ownership, and actual-core continuation/order/retirement coverage. Each snapshot
retains watched module records through existing `xinputRefs` death-row pins.
An earlier transport operation may retire a later watched producer without
freeing its record beneath this pass; retired producers are skipped. Queue-held
inputs retain their own references after snapshot release. Invalid module
endpoints retire that producer rather than causing the ordinary path's old
whole-wait EBADF retry. Ordinary X/session invalid-descriptor retry is retained.

Command drains, per-producer read-before-write service, scheduler replenishment,
X-event fairness and image transport boundaries stay at their existing sites.
Session callbacks remain ordinary-only. Strict image/startup deadline ownership,
upward millisecond rounding, finite zero waits, infinite waits, overflow clamps,
interruption and termination are covered. An infinite interactive wait also
honors a finite pending scheduler deadline. No configuration syntax, packet ABI,
installed option, mandatory dependency, extra thread or backend changes.

The internal `image_async_wait` API now accepts a poll array. Both core paths
and the existing decoder/native unit consumers are updated together. Existing
module-construction and synchronous/session fd-set limits elsewhere remain;
this checkpoint does not claim every transport supports high descriptors.

## Tests and frozen source

The [native unit](../../tests/test_image_async_wait.c) checks real pipe
read/write/EOF/invalid-fd readiness, high descriptors where guest limits permit,
a synthetic high-fd control, decoder deadline precedence/ties, submillisecond
rounding, interruption, termination and absence of select fallback.
The [actual-core fixture](../../tests/core_readiness_start.c) checks ordinary
and interactive partial-frame continuation, read/write/session order, EINTR,
pins during controlled retirement, cleanup afterward and real high input fds
where permitted. It uses real pipes and X, with controlled wrappers at transport
handoff/output and the session callback; it is not external-session-manager
qualification. The [driver](../../tests/live_x/core_readiness_live.py) can
borrow an authenticated default display. Existing image-worker controls use the
same native descriptor API.

Base: `abd7c4f8e570c167dc62ed689bba882b935c9f56` (merged PR #95).
Source archive: `c49119e477ee7434945145484eab32246986d3ffe6b25d2bece6bcc47d3f4a10`.
All 1941 regular-file hashes/modes are verified before
and after native qualification. Eight code/build paths define this checkpoint;
all other non-documentation bytes match the publication candidate. One C test
comment is shortened on the same line to meet the 80-column convention; the
receipt verifies the exact replacement and both file hashes, with code tokens
and line numbers unchanged. No runtime rerun is required for that documentation
change. Other shared-root
edits, accepted sources/builds, VM disks and evidence remain preserved.

## Combined local qualification

Linux AMD64 GCC combines 248 original passes with one registered topology
fixture pass after the verified storage relocation described below. The same
binary, source, 100 ms xrandr deadline and assertions are retained. The initial
full run and isolated retry failed different pressure operations. A bounded
current/prior/current/prior comparison returned pass/pass/fail/pass, and an
independent prior baseline passed. All attempts remain archived. The timing
difference is unexplained; the later registered pass is not a cause/repair
claim. Other profiles retain their full-run records.

Each profile has complete registered-suite coverage, a staged install, installed
version execution and exact installed-byte comparisons after normal rpath
transformation. Compiler/feature configuration and dependency environments are
recorded in the receipt. Optional Go and manual generation are disabled here;
their unchanged accepted owners remain separate.

| Target / profile | Passing entries | Installed code | Catalogs |
| --- | ---: | ---: | ---: |
| fvwm-x-linux-arm64-svg/gcc | 249 | 15 | 11 |
| fvwm-x-linux-arm64-svg/clang | 249 | 15 | 11 |
| fvwm-x-linux-arm64-svg/software | 228 | 15 | 11 |
| fvwm-x-linux-arm64-svg/minimal | 197 | 15 | 0 |
| linux-amd64/gcc | 249 | 15 | 11 |
| linux-amd64/clang | 249 | 15 | 11 |
| freebsd-arm64/default | 237 | 15 | 11 |
| openbsd-arm64/default | 237 | 15 | 11 |
| freebsd-amd64/default | 237 | 15 | 11 |
| openbsd-amd64/default | 237 | 15 | 11 |

The eight standard compiler profiles cover all six target OS/architectures:
1944 entries. Optional software/minimal profiles add
425; 150 installed native-code identities
and 99 catalogs match their tested outputs. Full suites
use one test process per guest while distinct target environments run in
parallel. Owning focused build directories are reused and dependency detection
is refreshed at the combined checkpoint.

Four private-authority BSD default servers pass 16 fixture
runs: the new core probe, existing cache probe, and installed WM/MFL interactive
geometry smoke at 1× and 1.5×. All four refuse an unauthenticated client and
remain alive through fixture execution. Cleanup records
4 clean exits and 0
bounded forced cleanups; the machine receipt retains actual statuses. The
orchestrator allows ten seconds for owned process teardown. This is routine
platform acceptance, with no further #93 cause investigation.

Linux ARM64 Clang ASan/UBSan passes two scoped entries with the supporting
library and actual-core fixture instrumented. The pure wait unit enables leak
detection. The controlled core fixture ends its startup probe with `_exit`;
leak detection is disabled and no full-shutdown/leak claim follows. Both tests
halt on sanitizer errors. The installed compiler runtime is selected explicitly
and effective sanitizer/link options are retained.

The isolated PR checkout passes `make verify`: 166 portable
passes and 11 explicit native-Meson skips on macOS. Final
documentation links and branch/staged whitespace checks pass. These foundation
checks do not establish native runtime support by themselves.

Actual native sequence: recorded `meson setup` (or dependency-refresh
reconfigure), `meson compile -C BUILD -j 2`,
`meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs`,
`meson install -C BUILD --no-rebuild --destdir STAGE`, then installed
`fvwm3 --version`. Default-server fixture argv and sanitizer settings are in
the receipt; full commands, logs, registration/config metadata, installed bytes
and executed orchestration scripts remain in six archived local evidence trees.

## Limits and retained evidence

Linux AMD64 exhausted its 987 MiB tmpfs while copying GCC verification outputs
following successful staged installation. This checkpoint's 297 MiB tree was
copied to the guest's disk (23 GiB available); all 4302 file/symlink identities
and modes matched before removing only that original copy. A symlink preserves
the conventional path. Earlier checkpoints remain untouched. Partial copies,
failed commands and the relocation receipt are retained; only unfinished
artifact/compiler steps resume. No source or runtime deadline changes.


Initial focused unit/core checks passed on Linux AMD64/ARM64 and FreeBSD/OpenBSD
ARM64. They are development evidence; the combined receipt owns the final full
profile/install/default-server scope. Target binaries execute in local VMs or
the Linux container. Emulated timings do not qualify physical desktop latency.
Native-CPU release sanitizers, physical input/GPU/toolkit/compositor and external
session-manager evidence remain separate. Broader module/synchronous readiness
and remaining descriptor-limit audits are not declared complete by this PR.

Ignored local evidence: `builddir/codex-2026-10-14/core-readiness/`, including
source manifests, focused/full commands, original attempts, six completed
artifact archives, portable verification and strict combined validation.
No hosted CI, remote success publisher, release or upstream notification is added.
