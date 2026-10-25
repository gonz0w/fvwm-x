# M5 shared wait ownership and LOOP-01 reconciliation

Status: focused Linux checks, small FreeBSD/OpenBSD wrapper checks and independent
source review pass; maintainer integration pending. Owner: M5 / LOOP-01 #36,
CLEAN-01 #34 and FD-01 #7. Base: authorized merged PR238,
`0c9a7d53f30db0170b38ba58944fe8d0ab988ee3`; canonical signature/DCO and exact
reviewed tree `115b614aa9d3d89dbbfd20c8c05bdba7ab6633ce` verified. Completed
branch retired and Project Done. Broad GetOutput owners remain open.

## Bounded repair and retirement

`fvwmRead`, `fvwmWrite` and `fvwmPoll` now require POSIX `sigjmp_buf`,
`sigsetjmp(..., 1)` and `siglongjmp` on supported Linux/FreeBSD/OpenBSD hosts.
The old bare-function feature probe missed sigsetjmp's header/macro form and
selected setjmp/longjmp on the qualified Linux configuration. That fallback
could leave the delivering handler's blocked mask in place after cancellation.
Remove the two misleading probes and fallback branches, with no new option,
dependency, scheduler or configuration migration.

Preserve each volatile result and arm/check-termination/syscall/disarm sequence.
Do not reset a completed I/O result merely because a later signal jumps: consumed
bytes or committed output must not be hidden. Errno initialization, readiness
arrays, partial transport progress and termination behavior otherwise stay intact.
Production termination remains permanent. Repeated reset is a test fixture
operation; this is a mask-contract defect, not a demonstrated live-X recovery bug.
PR238's bounded GetOutput polling remains accepted and unchanged.

Tracked baseline searches find no caller/test of fvwmSelect: only its definition,
declaration, live comments and one historical plan. Retire that adapter and the
exclusive HAVE_SELECT/SELECT_FD_SET_CAST definitions. Retain fd_set_size_t (used
by System and its width test), wider header detection, fio.c and the legacy BSD
signal shims/callers outside this slice. Historical plans/reports retain their
original terminology. Internal adapter retirement does not change FVWM commands.

## Before/after evidence

Private evidence: `builddir/codex-2026-10-25/m5-readiness`. The new actual-signal
test linked to the old Linux helper with its real generated configuration fails
with status 134: caller mask changed for signal 14 at assert_mask. SIGTERM is
actually delivered through its installed handler while an interval timer's
SIGALRM handler is active. This exposes the inherited mask, unlike a direct
fvwmSetTerminate call. The host macOS baseline passes because its old setjmp
behavior saves masks; it is not evidence that every old platform fails.

The regression compares every signal-set member with the pre-call mask, including
a deliberately blocked unrelated signal. It checks blocked read/write/poll twice
with explicit fixture-only reset, then raises SIGTERM after each return to catch
stale jump arming. Preterminated calls skip I/O and preserve buffers/queued data.
Success, partial read, EOF, EBADF, POLLNVAL, timeout and unrelated EINTR checks
remain distinct. Existing packet/write and image-wait controls retain fragmented
I/O, termination and high-descriptor coverage.

## One focused Linux checkpoint

Reuse normal GCC and scoped Clang ASan/UBSan Linux ARM64 caches. Each passes five
selected units: fvwmsignal, module-packet-read, module-command-write,
image-async-wait and event-loop-policy. Scoped units enable leak detection and
halt on sanitizer errors. The normal core build also passes core-readiness,
module-sync and module-startup real-X fixtures. Their existing controlled syscall
and transport adapters verify ordering/deadlines, partial continuation and
retirement; actual handler-mask behavior is established by the new kernel-signal
unit, not inferred solely from those X fixtures.

All selected Linux commands pass on the first combined gate; no Linux tests
repeat for publication or BSD package corrections. Five changed native/build
inputs match host/guest hashes and modes; fourteen tested core/fixture/unit
identities are retained. This is not a new full install, renderer, architecture
or release qualification. No user-visible rendering/UI change needs screenshots
or another theme set.

## Small affected BSD checks

The exact helper/header/test bytes compile as C11 and pass on FreeBSD
15.1-RELEASE-p4 ARM64 (Clang 19.1.7) and OpenBSD 8.0 ARM64 (Clang 22.1.6).
FreeBSD uses its preserved build-default generated configuration; OpenBSD uses
the generated configuration from the qualified 8.0 ARM64 resume cache. These are
standalone selected-API tests, not fresh Meson/full-profile qualification.

FreeBSD preparation first selected a nonexistent build-gcc path, then exposed
missing strlcat/strlcpy headers referenced by that preserved configuration. The
first OpenBSD job likewise fails compilation for the omitted support header.
Those failures earn zero runtime credit. Add the two unchanged project headers
to the small source bundle and use the correct FreeBSD cache path. The final
FreeBSD compile/test passes; one corrected OpenBSD job passes. Production and
regression bytes do not change, and Linux does not repeat. Final collection uses
a tar stream rather than attempting a nonrecursive directory scp.

All owned VM runs shut down normally with QEMU status zero. Managed OpenBSD job keys
are removed. All selected archives are read through their regular payloads and
gzip EOF/CRC. The canonical private archive preserves 95 payloads, including the
original compile failure, run/configuration hashes and corrected results. After
archive verification and no-open-handle/inode checks, retire only the two stopped
disposable OpenBSD job disks (6,143,475,712 logical bytes). Stable VM disks,
approved image/build caches, sources and host evidence stay intact.

[Machine receipt](VERIFICATION-2026-10-25-M5-WAIT-OWNERSHIP.json) records exact
source, binary, command/status, BSD input and retirement identities. No new
native-CPU/hardware/default-BSD-X or release acceptance is claimed.

## Original-scope reconciliation: LOOP-01 #36

The original roadmap scope was shared readiness, lifetime, teardown and deadlines
across ordinary dispatch and interactions. Merged PR96 (`9c91f1c29`) implements
the shared owned/pinned snapshot and common prepare/commit policy. Merged PR97
(`20eda2943`) completes synchronous unlock, blocked-output and startup waits,
including absolute-deadline conversion and interval-clamp continuation.

[Core readiness acceptance](VERIFICATION-2026-10-14-CORE-READINESS.md) and
[synchronous-wait acceptance](VERIFICATION-2026-10-15-MODULE-WAITS.md) retain their
exact original six-target source/installed/default-X evidence, partial-frame,
ordering, retirement and descriptor-limit observations. The latter retains 1944
standard and 425 optional entries plus 50 final affected entries; these are
historical results, not reruns or fresh M5 qualification. Current readiness helpers,
shared snapshots, module continuations and registered fixtures retain those owners.

The imported core-consolidation-queued wording is stale. The original finite
#36 consolidation is complete; this PR also qualifies the newly identified
shared mask repair and closes #36 on merge. Broader admission/process/resource
and runtime audits stay with #7/#15/#34, interaction mutation with #9, redraw/
performance work with #25/#29, and release/provider/hardware work with its
existing owners. Preserve modernization handoffs and #93's current closed/not-planned
supersession because OpenBSD 7.9 is unsupported; its original cause/fix remains
unfulfilled. No broad event-loop rewrite or historical matrix
restart is warranted by this reconciliation.

Independent source review finds no ordering/lifetime/retirement issue. Final
source/evidence review verifies the records and corrects that stale #93 status;
no remaining findings. Signed author-DCO delivery completes this checkpoint.
Requested Astra/high analysis, Sol/high implementation and separate Sol/high
review follow project policy; runtime model metadata is unavailable/unverified.
Periodic reference is merged PR238 at 0/5, 1/5 on this merge; no comparison due.
