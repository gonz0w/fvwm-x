# Manager startup handoff — 2026-10-21

SELECTION-01 S01–S03 / DATA-01 / INPUT-01. Base: merged PR144,
`350b39092e02992fad9164cc7ddb1591a7c31893`.

## Selected batch

Three related startup repairs in `SetupICCCM2`, preserving FVWM configuration:

1. Initialize the complete XEvent and publish timestamp, selection and actual
   owner in MANAGER; reserved data remains zero.
2. Replace the unbounded old-owner window wait with the existing monotonic
   deadline policy, a ten-second limit and finite signal-aware poll slices.
   Watch/query races tolerate the old window disappearing. Startup consumes
   only its destruction event, with no command dispatch or reentrant main loop.
3. Recheck selection ownership while waiting and before proceeding; refusal,
   cancellation and supersession close only the new startup connection. The
   global display pointer is cleared before exit hooks can use it. A foreign
   successor's selection and the old peer's window remain intact.

The [ICCCM 2.0 manager-selection contract](https://xorg.freedesktop.org/archive/current/doc/xorg-docs/icccm/icccm.html#Manager_Selections)
defines announcement contents and the old manager's release/destruction ordering.
The new finite refusal never forces takeover by killing a peer. Xlib requests
still depend on a responsive X server: a timer cannot preempt one blocking Xlib
call. Main-loop, configuration ordering and later SelectionClear behavior remain
with existing owners. No new atoms, clipboard daemon, sandbox or EWMH desktop
geometry policy is introduced.

## Development evidence

Focused Linux and FreeBSD ARM64 controls pass three groups/six cases each:
announcement identity and non-replacement refusal; cooperative release and
superseding owner; stalled-owner timeout and SIGTERM cancellation. They use
independent actual X clients, including the old peer's root redirect lease.
Scoped Linux guest ASan/UBSan passes the existing deadline policy plus all three
manager groups, with sanitizer exit codes distinct from expected startup refusal.

The preserved PR144 installed core fails the announcement check. Its separate
superseded/stalled controls exceed their three/thirteen-second observation
limits when a baseline-only observer accepts the malformed announcement, so
that finding cannot mask the wait defects. No exploit claim is made.
The first focused fixture used a nonexistent harness cleanup method; original
failures are preserved, and only affected controls rerun after its correction.

Three manager announcement, bounded/cancellable handoff and ownership/refusal
repairs pass 2308 standard / 14 selected optional entries on six targets, four
scoped sanitizer entries, ten fresh installs and four authenticated BSD
displays. Original fixture failures and isolated baseline controls remain
explicit.

| Gate | Passed |
| --- | --- |
| Linux ARM64/AMD64, GCC/Clang | 295 standard entries per profile |
| FreeBSD/OpenBSD 8.0, ARM64/AMD64 | 282 standard entries per target |
| Linux software/minimal | 14 selected entries combined |
| Fresh staged identities | 150 ELF, 77 catalogs, 700 data files |
| Staged real-X | 30 manager groups (60 peer cases), 20 geometry and ten style groups |
| Default BSD Xorg/Xenocara | 32 groups on four authenticated displays, including 24 manager peer cases |
| Scoped Linux guest ASan/UBSan | Four entries, including all six manager peer cases |
| Portable `make verify` | 177 passes / 14 capability skips, one run |

Generated configuration hashes remain unchanged in all ten profiles. All 2,123
frozen regular source inputs pass native hash/mode postchecks. Canonical archives
pass EOF/CRC, payload, result and source-identity validation. Each staged native
consumer matches its tested bytes after Meson's platform RPATH adjustment.
Authenticated default displays refuse unauthorized clients and stop with status
zero. No full matrix is repeated. Scoped sanitizer binaries and relevant inputs
are postchecked by identity; the separate native-CPU release gate remains open.

All 26 generated manual pages are verified on FreeBSD; the affected fvwm3 and
combined manual render through native mandoc. New C lines respect 80 columns;
source/exit-hook/error-handler and CLI/manual review pass. The final updates
change Markdown and this receipt only; tested implementation inputs stay exact.

Signed author-DCO implementation: `c4c1a3f1edc0b7fb0de021e1dae3405b8c785b2b`.
Frozen source SHA256:
`b43f98d99c82d336186661eacb743adc9d77a1e57b63721b531b73bf57e7b76d`.
[Machine receipt](VERIFICATION-2026-10-21-MANAGER-HANDOFF.json) binds the actual
source, commands/results, focused failures/repairs, old-core controls, sanitizer
flags/binary postchecks, installs, default displays and manuals. Canonical raw
evidence: `builddir/codex-2026-10-21/manager-handoff-safety`. Completed capture
and archive duplicates on persistent Linux/FreeBSD guests and temporary host
build-cache copies are retired after identity verification. Canonical evidence,
sources, reusable builds, stages/performance references and VM disks
remain intact. Both owned OpenBSD 8.0 jobs stop cleanly and remove ephemeral keys.

[Due PR138/144 performance observation](PERFORMANCE-2026-10-21-PR144.md) uses
preserved binaries independently of this implementation; PR144 is the new
reference at zero subsequent merged checkpoints. This unmerged PR does not count.
No performance-only tuning is selected. Broader selection roles, direct target
conversions, multi-screen/adversarial race/flood coverage and native-CPU,
physical GPU/input/provider/toolkit/compositor and release gates remain open.
