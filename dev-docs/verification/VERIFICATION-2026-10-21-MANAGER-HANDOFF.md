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

Combined six-target/compiler/install/default-X qualification is pending.
[Due periodic performance observation](PERFORMANCE-2026-10-21-PR144.md) reuses
PR138/144 binaries independently of this development build. Broader selection
roles, direct target conversions, multi-screen/race/flood coverage and native-CPU,
physical/provider and final release gates remain open.
