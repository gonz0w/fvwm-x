# Fixed session providers and power requests — 2026-10-29

Owner: Cam. R1 checkpoint under SESSION-01 #72, SESSION-02 #77 and the bounded
manual-lock consumer of LOCK-01 #225. This extends the accepted PR305 client
contract; it does not select a resident inhibitor, authentication or lock server.

## Selected boundary

Keep the five existing service inventory records and add explicit bounded power
and locker observations. One parent serializes mutations and retains actual
child/Wait ownership. The existing controller owns presentation and user
confirmation; core-managed window actions retain their existing command engine.
One shared `session_power=portable|login1` preference defaults to portable. It
applies to subsequent requests after the owner handshake and explicit probe.
The existing store advances to schema 6, reads historical schemas, and exposes
fifteen preferences; the four-field Session editor is separate from the eleven-
field appearance editor. Private owner transport is version 2; installed
consumers and the parent must use a coherent version. An older running parent
requires a fresh login after upgrading; a Desktop/core restart alone does not
replace that parent. Version mismatch reports unavailable without changing login.

The simpler alternative—adding labels around the old worker—would retain
controller-local locker tracking, ambiguous power outcomes and competing policy.
The selected shared pure native plan and stable parent supply one authority for
those operations without an arbitrary service registry or new dependency.

## Native and optional system provider

Native planning selects fixed absolute executables through conservative PATH
roots: Linux systemctl when present, otherwise shutdown; FreeBSD/OpenBSD retain
the existing shutdown flags. Discovery never performs a power action or starts a
daemon. Installed native tools are request-capable with authorization unknown.
The OS continues to authorize them. Zero exit means a request was accepted;
nonzero exit alone does not prove a particular authorization failure. Never
replay or choose another provider after uncertain submission, denial or timeout.

The optional existing system-bus client uses the same disabled-default
`session-dbus` build feature and isolated libdbus helper. It accepts only a local
Unix address, taking DBUS_SYSTEM_BUS_ADDRESS when explicitly supplied, otherwise
using the conventional `/var/run/dbus/system_bus_socket` path. Custom package
locations can use that standard environment variable. It neither creates a
system bus nor activates a missing service.

Only the fixed org.freedesktop.login1 Manager and its capability/poweroff/reboot
methods are selected. A probe records bus ID, unique owner and separate capability
results. Requests recheck that identity and the selected permission, then submit
only an explicit yes with interactive=false. The current specification also
returns no, na, challenge, inhibited, inhibitor-blocked and
challenge-inhibitor-blocked; each remains distinct and nonauthorizing. Unknown or
failed capability queries remain unknown without discarding successful other
actions. No flags bypass an inhibitor. This endpoint contract can be supplied by
compatible installed services; live systemd-logind/elogind availability is
separate qualification. See the [primary interface specification](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.login1.html).

The helper keeps its existing message/output limits and receive backpressure
threshold. Power supervision allows four seconds, notification supervision two;
private client requests have five-second deadlines. The parent retains a helper
lease until actual Wait even after cancellation. A kernel-blocked call cannot
be preempted by a context budget. A possibly submitted request whose completion
cannot be established is outcome-unconfirmed, not permission denied or undone.

## Confirmation, manual lock and remaining work

The public settings generation advances when the observed provider/locker
changes. Native confirmations bind that generation, requested configuration,
parent CAS and provider generation. Pending probes expose no ready capability;
completion invalidates intermediate confirmation tokens. No fallback follows
an intended owner's failed handshake or request. Direct/external-session
configuration retains its existing documented native fallback.

Manual Lock observes one directly launched xlock/slock and its actual Wait result.
Once admitted, requester disconnect, UI/core restart, deadline and WM exit do not
terminate that locker. Its process state never establishes secure protection,
including when a provider daemonizes. No process-name/PID-file scanning discovers
or adopts unrelated lockers. Coordinated Suspend therefore refuses before any
power method; a process start, fixed delay or successful LockSession call is not
a qualified secure-readiness witness.

Persistent inhibition, automatic idle/lid/display-power coordination, physical
secure-lock/authentication and full supported-target service qualification remain
with their existing owners. Native tests use owned harmless command fixtures and
a private fixture bus/provider; they do not power off, reboot, suspend or change
authentication on the lab or host. #72/#77/#225 do not close for this slice alone.
