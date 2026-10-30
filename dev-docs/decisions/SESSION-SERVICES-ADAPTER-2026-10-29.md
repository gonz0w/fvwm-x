# Optional session bus client — 2026-10-29

Owner: Cam; checkpoint: R1 session control and optional services, #72/#77.
Decision: use an isolated, transient libdbus client for the finite inventory and
outbound notification operations. `session-dbus` defaults to `disabled`; enabling
it requires `dbus-1 >= 1.14`. The disabled worker has no bus dependency. This does
not adopt a resident service framework or notification server. Full six-target
release qualification remains pending under #77.

## Alternatives and evidence

The finite comparison used the same private session bus, real dunst 1.9.2 server,
GetId/GetNameOwner/GetCapabilities and Notify/CloseNotification workload. Five
fresh probes per candidate ran on the owned ARM64 Linux container under the
macOS lab. These are local client costs, not native hardware or desktop-wide
performance results. GNU time measured peak RSS; startup includes its invocation.

| Candidate | Median probe time | Median peak RSS | Prototype executable |
|---|---:|---:|---:|
| Disabled stub | 0.56 ms | 996 KiB | 70,336 B |
| libdbus 1.14.10 client | 1.28 ms | 2,912 KiB | 78,024 B |
| godbus/v5 v5.2.4 client | 1.80 ms | 5,204 KiB | 5,163,363 B |

Executables are unstripped prototype builds (C `-O2`, normal Go build); sizes
exclude shared libraries and are not installed release-size comparisons. CPU
samples were below GNU time's reporting precision. Libdbus's observed closure
includes libsystemd and its distribution dependencies; this cost exists only in
the enabled isolated worker, not the core or default-disabled product. A stub
provides explicit local fallback, not equivalent bus functionality.

The Go candidate was pinned to commit
`4e46d5eb6d1926b98ce405d4909cf3588698ace5` (v5.2.4; source archive SHA256
`49b2a94fa0e22cccb2b16d91e297b71bd13c406f33c3ed74115f8d08accee306`). It adds
`golang.org/x/sys v0.27.0`; no project Go module/vendor change was made.
Its default exported-method dispatch starts a goroutine per incoming call
(`conn.go`), and its sequential signal handler queues without a fixed cap. Those
paths are unsuitable evidence for a bounded resident server; a later server
proposal must evaluate admission before dispatch separately. This client choice
avoids that unneeded resident framework and keeps the current Go dependency set.

## Bounds and failure behavior

The worker accepts only probe, Notify and CloseNotification, with fixed bus
methods and no exported methods, activation, shell commands or system bus. It
requires one explicit local Unix session address and validates bounded strings,
owner/bus identities, replies and notification IDs. Mutations address the observed
unique server owner and require the expected owner/bus tuple. Notifications are
plain text, without actions, image payloads or callback commands. The parent
tracks at most 32 IDs for that server generation and owns its timeout and reaping.

The library message limit is 16 KiB. Its 32 KiB aggregate receive setting is a
backpressure threshold, not a strict process-memory ceiling: a read/message can
overshoot it. Per-message FD limit is zero; aggregate FD threshold is one so
zero-FD messages can still be read. The first prototype set the aggregate limit
to zero and timed out during registration because libdbus disabled reads at
`live_fds >= limit`. The one-line corrected variant completed probe/Notify/Close.
The parent bounds the worker to two seconds and retains its slot until actual
process exit; one kernel-blocked call cannot be preempted by a time budget.

The first timing fallback inherited Python's pre-exec high-water mark, so those
RSS samples were discarded. Corrected GNU time samples and the initial timeout
are retained in the checkpoint receipts. No same-UID confidentiality, automatic
service activation, forced logout, secure locking, or full platform acceptance is
claimed. Default no-bus behavior and independent applications remain available.

Primary references: [libdbus connection API](https://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html),
[read-watch implementation](https://dbus.freedesktop.org/doc/api/html/dbus-transport-socket_8c_source.html),
[pinned godbus source](https://github.com/godbus/dbus/tree/4e46d5eb6d1926b98ce405d4909cf3588698ace5),
[notification protocol](https://specifications.freedesktop.org/notification/latest/protocol.html).
