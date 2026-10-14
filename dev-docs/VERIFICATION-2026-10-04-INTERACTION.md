# Interactive cancellation and motion budgets — 2026-10-04

Move and Resize now stop when the client is destroyed or withdraws, without
waiting for a physically held button to be released. Termination signals unwind
both opaque and wireframe operations. This is the cancellation foundation for
INPUT-01; timer/module responsiveness during a held drag is still unfinished.

## Implementation

The shared interaction state stores window IDs and a sticky cancellation result.
It peeks for the target's destruction/ICCCM withdrawal and leaves those events in
the X queue. The normal dispatcher handles lifecycle changes after the command
stack returns; cancellation does not free the drag's borrowed `FvwmWindow` or
reenter arbitrary command handlers. Parent copies, frame/icon unmaps and
unrelated clients do not count as withdrawals. Unmapped placement windows are
not cancelled by withdrawal notifications.

A poll-based X readiness wait replaces the two blocking masked-event waits.
Its finite 100 ms timeout closes the signal-before-wait race. Each loop also
checks cancellation while paging and processing motion. Cancellation erases the
wireframe, follows the existing geometry/colormap cleanup, releases grabs and
skips button-up waits. Managed titlebar/focus cleanup checks the captured target
IDs too, while preserving swallowed-click behavior. General button-up waits
honor termination.

Both motion drains use one helper with a maximum of 32 events per call. Button
press/release and key events stop coalescing. The Move drain's mistaken
`ButtonRelease` event number is replaced with `ButtonReleaseMask`; Resize now
includes key barriers in its drain. Final release coordinates remain intact.
Move initializes its output position even when cancelled before a release, with
a safe fallback if the geometry query fails.

## Verification

| Platform/compiler | Local full suite | Follow-up/default server |
| --- | --- | --- |
| Linux ARM64 GCC | 43/43 | Final sources, cancellation and compatibility; held-pointer ordering diagnostic |
| Linux ARM64 Clang/lld | 43/43 | Final portable cancellation probe and native policy |
| Linux AMD64 GCC | 43/43 | Final portable cancellation probe and native policy; pre-fix negative control |
| FreeBSD 15.1 ARM64 | 34/34 | Default scfb Xorg on a private framebuffer-enabled QEMU overlay |
| FreeBSD 15.1 AMD64 | 34/34 | Default scfb Xorg with UEFI framebuffer |
| OpenBSD 7.9 ARM64 | 34/34 | Default wsfb Xenocara, including the zero-monitor root fallback |
| OpenBSD 7.9 AMD64 | 34/34 | Default Xenocara |

Native policy covers client destruction, real and synthetic withdrawal, parent
copies, unrelated windows, `None`, unmapped placement and irrelevant events.
The X queue probe checks the motion budget over several turns, release/key
barriers, sticky cancellation and preservation of lifecycle-event order.

The registered real-X gate exercises twelve held-button cases: Move/Resize
client destruction and withdrawal from client-area and titlebar bindings, with
click-to-focus enabled, plus opaque and wireframe SIGTERM. Unrelated client
destruction must leave the interaction active. Cancellation and cleanup must
complete before the harness releases the button. An authenticated second X
client proves a wireframe blocks its round trip and unblocks it after shutdown.

The pre-fix Linux AMD64 binary fails the new test waiting for continuation after
client destruction. The final binary passes. A FreeBSD ARM64 pre-fix control was
also run. The existing real-pointer profile still observes entered, continuation,
timer, module ordering: the continuation stays suspended through the hold;
the timer and module command resume after release. Existing interactive geometry,
FVWM binding/function/theme fixtures and other regression gates remain passing.

The first wireframe probe opened a new X connection during the server grab;
OpenBSD AMD64 exposed an intermittent connection-setup failure. The final probe
connects before the grab and tests a round trip instead. One subsequent OpenBSD
ARM64 suite had an unrelated configure-test display-opening failure; its isolated
follow-up and subsequent full 34/34 rerun pass. These harness failures and final follow-ups are retained in
the evidence. They are not counted as successful failing runs.

## Scope and remaining work

All gates use the prior hardening snapshot plus these interaction changes.
Concurrent desktop/theme files, the new desktop test registrations and the
separate default ImagePath change are preserved in the shared checkout and
excluded from this snapshot. These counts do not verify that other work.

The six changed production/build files are independently fingerprinted across
Linux and the BSD workspaces. Evidence and the exact interaction snapshot are
under `builddir/codex-2026-10-04/interaction/`. Shared BSD guests are left running and their concurrent source directories are
untouched. Separate source directories and private ARM64 overlays provide
isolation; the private overlays are shut down after saving evidence.

This batch does not make Move/Resize asynchronous. It does not pin window
resources against arbitrary reentrant Recapture/style commands or change the
legacy wireframe server-grab policy. Root-selection/menu/nested interactions,
normal-hint/topology changes, restart signals and command/continuation routing
need further coverage. Xlib queries and queue searches remain synchronous;
32 coalesced events is a count bound, not a wall-clock bound on every X call.
Measured QEMU/harness completion times are not desktop latency guarantees.
Native Linux sanitizer CI remains configured but unrun; local ASan was not used.
