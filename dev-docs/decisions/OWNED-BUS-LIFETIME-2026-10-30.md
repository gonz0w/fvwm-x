# Owned session-bus lifetime

Owner: Cam; accepted #72/#77 lifecycle scope. The finite checkpoint in
[the tracker](../PROJECT-TRACKER.md#r1-owned-session-bus-lifetime--2026-10-30)
precedes implementation. Configuration syntax and defaults stay unchanged.

## Choice and simpler alternatives

Extend the existing optional native helper with one fixed guardian that owns
one foreground dbus-daemon. The previous direct-parent child loses its cleanup
owner when that parent dies. A guardian that stops on parent loss alone would
also interrupt a surviving WM's bus. Share a lifetime pipe between parent and
core instead: only their writers keep the session bus alive.

A separate Go supervisor would add another runtime to this optional resident
path. Reusing the existing native process boundary avoids that extra runtime
and packaging path; its actual cost is measured in the checkpoint evidence.
The enabled executable still loads its existing libdbus closure even though
guardian code makes no D-Bus calls. This is not a claim that historical transient
client measurements measure the guardian. A tiny separate executable is not
selected without evidence that its additional packaging ownership is needed.

No arbitrary supervisor, daemon registry, PID lookup, adoption or automatic
replacement is introduced. Off/reuse/external paths add no guardian. The
disabled helper refuses before launch and remains independent of libdbus.

## Lifetime and private handoff

Guardian FD3 reads the lifetime pipe. Parent and core alone hold writers, with
the core's private FD5 selected by FVWM_SESSION_BUS_LIFETIME_FD=5. The core
validates the writable FIFO (including BSD duplex mode), removes the environment
marker and sets CLOEXEC. Native Restart preserves both FD4's existing flock and
FD5 immediately around its own exec, rolling back on failure. Ordinary executed
applications/modules, the daemon and custom replacement receive neither writer.

Exact RLB acknowledges both retained leases; existing R/RL variants describe
absent leases. The parent requires the combination it supplied before one-shot
autostart, including no-autostart handoff. An old/refusing core can remain usable
but does not acquire a parent-crash lifetime guarantee. Loss of an acknowledgment
does not itself close a writer the core actually retained.

Only the fixed absolute resolved-through-PATH dbus-daemon name and fixed session/
foreground arguments are accepted. Normal final executable symlinks retain the
existing launch contract. A bounded address and actual authentication precede
publishing the owned address to the WM/applications.

## Facts, deadlines and failure

Guardian startup allows five seconds. A SIGCHLD wake pipe is installed before
fork; only its event loop waits for the exact child and retires the handle before
another signal. Healthy idle blocks in poll. Lifetime EOF or failure requests
TERM for two seconds, then KILL with one second of actual Wait observation.
Fatal poll failure enters this cleanup through a distinct bounded fallback;
post-address observer loss alone does not retire a bus held by the live core.

At most two bounded records describe an address and terminal outcome. W with a
normalized child exit status proves actual waitpid; U says termination remains
unconfirmed. Terminal-before-address can describe startup failure. Partial
writes, lost readers, malformed/extra/trailing records and total bytes are
bounded. After U the guardian remains a passive reaper, with no later W/retry.

The parent retains an explicit pipe reader independently of exec.Cmd.Wait.
Daemon receipt and guardian Wait are separate facts. Cleanup releases its writer
after existing acquisition/adapter barriers and observes for at most four
seconds; it never signals the guardian on timeout. A validated surviving core
keeps the other writer even if WM Wait is unconfirmed. Terminal bus loss fences
late probes and notification ownership without altering power/locker ownership.

Independent applications alone do not extend the bus beyond parent/core exit;
they are never signalled. Guardian failure can leave daemon disposition unknown.
Kernel-delayed termination and blocked calls cannot be preempted by reporting
budgets. This is not universal orphan prevention or full service/platform
qualification. Notification server, tray, secure-lock/idle/inhibitor and selected
agent/portal adapters retain their existing owners; #257/#258 remain parked.
