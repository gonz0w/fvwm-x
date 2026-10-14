# Synchronous frame notifications — 2026-10-04

Synchronous M_CONFIGURE_WINDOW subscribers no longer hold Move/Resize open
while waiting for each intermediate geometry reply. The user approved deferred
final geometry subject to secure processing. Intermediate packets for these
subscribers are now copied and coalesced by window ID; final delivery waits
until the enclosing command stack and grabs have unwound. Asynchronous frame
subscribers retain their intermediate updates.

## Ownership, bounds and reply handling

A snapshot owns its packet bytes, with no retained pointer to a FvwmWindow or
drag resource. The existing wire packet includes its legacy numeric window
token; the core never dereferences that token to deliver the snapshot. Header,
size, alignment, packet type and declared length are validated before key
access, allocation or copying. At most 64 distinct windows can await delivery
per module. Snapshot allocations share the existing eight-megabyte output cap;
repeated same-sized updates overwrite owned bytes without another allocation.
Over-limit or invalid deferred frames retire that producer through the normal
module lifecycle.

Snapshots are detached before delivery callbacks. A producer lifetime lease
keeps its record alive if a callback retires it, and cleanup occurs after the
send returns. Destruction notifications remove unsent snapshots for that
window even when the module did not subscribe to destruction events. Module
retirement/shutdown frees all remaining snapshots and their accounting.

Only the outer event loop releases delivery, one snapshot per pass, interleaved
with normal readiness/event work. The enclosing command's final geometry
updates also coalesce before that release. Explicit refresh from authoritative
window state handles rollback: an aborted move can restore its original frame
without producing a normal frame-update packet. A consumed outer-loop polling
hint prevents nested Wait calls from spinning on snapshots they cannot deliver.

The module protocol has no request IDs. Sending intermediate packets without
waiting would leave unlock replies that could satisfy a later synchronous
request incorrectly. This implementation omits those intermediate handshakes
and retains the existing synchronous reply/ModuleTimeout path for the final
packet. Reply commands, including real Style/UpdateStyles mutation, run only
after the drag's resources and parent continuation unwind. Ordinary sync
packets, no-grab masks and input admission keep their existing semantics.

## Verification matrix

The completed matrix and exact evidence counts are recorded in
`builddir/codex-2026-10-04/frame-sync/matrix-summary.json`.

| Environment | Full gate | Final affected follow-up | Server coverage |
| --- | --- | --- | --- |
| Linux ARM64 GCC, Podman VM | 52/52 | 8/8 after the outer-poll hint fix; final fixture 1/1 | Xvfb and Xorg dummy |
| Linux ARM64 Clang/lld, Podman VM | 52/52 | 8/8 after the outer-poll hint fix; final fixture 1/1 | Xvfb and Xorg dummy |
| Linux AMD64 GCC, Podman emulation | 52/52 | Corrected fixture included | Xvfb and Xorg dummy |
| FreeBSD 15.1 ARM64, QEMU/HVF | 42/42 | Four default-server scripts; final fixture 1/1 and default-server rerun passed | Xorg scfb |
| FreeBSD 15.1 AMD64, QEMU/TCG | 42/42 | Four default-server scripts; final fixture 1/1 and default-server rerun passed | Xorg scfb |
| OpenBSD 7.9 ARM64, QEMU/HVF | 42/42 | Four default-server scripts; final fixture 1/1 and default-server rerun passed | Xenocara wsfb, root-sized fallback |
| OpenBSD 7.9 AMD64, QEMU/TCG | 42/42 | Final fixture 1/1 and four default-server scripts passed | Default Xenocara |

Native module-runtime checks cover 4,096 same-window replacements, independent
windows and stable removal order, copied-byte ownership, nesting/unwind gates,
final overwrite after interaction end, destruction discard, the 64-window and
shared byte limits, invalid frames, and lifetime-safe retirement/cleanup.
Existing real-pipe input bounds and queue-order regressions remain passing.

The new real-X fixture checks fourteen cases: Move and Resize, each with
release, Escape rollback, receiver disconnect, target destruction, target
withdrawal, SIGTERM and an asynchronous subscriber. It establishes the actual
pointer grab, generates continuous motion, and observes independent input and
a due timer while the receiver could withhold replies. No synchronous
intermediate packet reaches that receiver. Release/cancellation completes
without its reply, and the parent continuation runs before final delivery.

Before unlocking final delivery, the test compares the copied geometry with
the actual frame, including original geometry after Escape. A real mutating
reply and a fresh synchronous request recover normally; a disconnect also
recovers. Termination exits without delivering its unsent snapshot. Async
cases observe multiple intermediate packets and the authoritative final frame.
The four BSD default-server runs also repeat held-drag servicing, twelve
cancellation cases (including wireframe grabs), and representative FVWM
config/theme compatibility.

A saved pre-deferral ARM64 WM fails the new fixture: it delivers an intermediate
synchronous packet and times out waiting for the due timer while its receiver
withholds unlock. The control binary, script, packet trace and failed output
are retained. This isolates the frame-wait behavior in the previously passing
servicing build; it is not a complete upstream historical build.

## Harness corrections and provenance

Initial evidence remains saved. The first setup used later desktop-test
variables absent from the frozen build graph; those unrelated dependencies
were excluded from the verification snapshot and preserved in the checkout.
The initial producer incorrectly labelled unrelated startup packets as destroy
messages; it now records only configure/destroy packets. A subsequent live run
caught and fixed the production rollback snapshot defect. The SIGTERM case was
then corrected to call the existing ManagedProcess wrapper's process API. The first
Linux AMD64 full gate passed 51/52: the fixture observed its file after creation
but before the buffered packet line was written. Its readiness barrier now
requires packet contents. The corrected full gate passes 52/52, and the final
fixture passes on both ARM64 compilers and all four BSD nested/default servers.
The failed AMD64 stdout and Meson JSON/text are retained.

QEMU runs use separate ports from another chat's guests, disposable snapshots
and a private FreeBSD EFI variable file. The first OpenBSD AMD64 launch used
virtio networking while its image expects em0; the corrected launch uses
its e1000e device. These are harness corrections, not WM portability changes.

Ignored `builddir/codex-2026-10-04/frame-sync/` contains the frozen source,
platform archives, stdout, default-server logs, controls and independent source
fingerprints. Thirty-four files cover this batch and the preceding servicing
batch; GCC and Clang ARM64 share one source workspace. The snapshot extends the
previous servicing source and excludes later appearance/Picom runtime edits
and their desktop-test dependency addition. Documentation formatting was
refreshed after BSD gates; the final manual was copied before fingerprints.
All four disposable verification guests shut down after evidence collection,
leaving the other chat's guests untouched.
The AMD64 archive records one inaccessible empty X-server log from an October
3 run; current evidence is retained. Staged installs do not alter the host
desktop. Native Linux sanitizer CI
remains configured but unrun; these local gates use no sanitizer instrumentation.

Reproduce the focused Linux gate with live-X enabled:

```sh
meson test -C builddir module-runtime module-queue-order \
    interaction-frame-sync-xvfb interaction-servicing-xvfb \
    interaction-cancellation-xvfb --print-errorlogs
```

The frame fixture accepts --display for the default BSD server. Full harnesses
remain tests/run_on_linux.sh and tests/run_on_bsd.sh.

## Remaining scope

Final delivery still uses the legacy synchronous wait and timeout after the
grab is gone; this does not make every synchronous module callback nonblocking.
Other synchronous event types also retain their existing behavior. INPUT-01
remains partial: arbitrary style/resource mutation, target lifetime pinning,
hints/topology and nested interactions require further work. The test matrix
establishes bounds, ownership, servicing and ordering under controlled loads,
not physical desktop latency or arbitrary third-party-theme compatibility.
