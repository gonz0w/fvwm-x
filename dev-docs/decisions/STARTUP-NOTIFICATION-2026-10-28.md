# Application startup notification and activity

Selected by the lead under #56 with #67 consumer adoption on 2026-10-28, before
implementation. Owner: Cam. The tracker owns execution and continuation.

Use the existing X11 core and isolated launchers. A new bus, library, cgo binding
or resident watcher costs more than this finite protocol requires. A small native
Xlib exec helper keeps blocking display/announcement work outside the core and
keeps the Go metadata policy pure. The core already receives X events and owns
window identities; it is the appropriate bounded protocol observer.

## Shared launch contract

Carry localized name, icon, Desktop Entry identity, tri-state StartupNotify and
StartupWMClass through current menu/Desktop/browser roles. Explicit false disables
notification including class fallback. True enables it; absent plus a valid class
enables the legacy class case, otherwise launch ordinarily. Optional zero fields
must not invalidate older private menu catalog hashes/references.

A fresh unpredictable correlation ID ends in `_TIME0`: these launch paths do not
retain the original user event timestamp. Never invent a wall-clock X timestamp
or change focus/placement. Clear inherited DESKTOP_STARTUP_ID on ordinary/fallback
launches. Preserve exact literal arguments, arbitrary path bytes, cwd and Terminal.

The private native helper forks one announcement child with a two-second absolute
budget. That child alone opens/synchronizes X and acknowledges successful `new`.
The parent serially terminates/reaps it before exec, sets DESKTOP_STARTUP_ID only
after acknowledgement, and execs the original application in place. Missing helper,
unavailable display or timeout preserves ordinary launch. A known exec failure
attempts bounded `remove`, emits a diagnostic and exits nonzero. No automatic
retry, retained application tracker or late signals after reaping. Post-exec crash
reporting is expiry based. A successful process Start means launch submitted, not
proof that the eventual program exec or mapped window succeeded.

## Bounded core observer

Reassemble format-8, 20-byte ClientMessage fragments by producer XID/root context:
the event window is the producer, not the delivery root. NUL terminates the message;
padding after it is ignored. Producer destruction must not discard a completed
sequence; another producer may send its remove. Parse protocol quoting/UTF-8,
not shell or Desktop Entry escaping. Require valid ID and new NAME/SCREEN; ignore
unknown bounded fields. Protocol data is public and unauthenticated.

Use at most 32 partial streams, 4096 bytes per message, 64 activity records and
512 KiB aggregate fixed/bounded storage. Partial streams expire after five seconds;
pending launch and retained terminal records after sixty. All deadlines are
absolute monotonic and serviced during idle and continuous X activity; updates
do not extend a pending launch. Saturation drops extra tracking without blocking
application launch or evicting unrelated active records; report counters.

Duplicate new updates pending metadata; terminal records suppress late changes
and reopening for their retention period. Resolve the specification's inconsistent
change-before-new wording with bounded invisible orphan change metadata, merged
only by a later new. An unknown remove creates no visible record.

Match explicit `_NET_STARTUP_ID` first. Accept zero or exactly one final NUL in
the property, while rejecting empty, embedded/double-NUL and overlong identities. In its genuine absence, use cached
WM_HINTS.window_group's ID at managed observation/WM_HINTS change; malformed direct
properties do not silently inherit a different identity. Never write an inherited
ID onto the client. Direct property changes are observed. WM_CLIENT_LEADER is a
session identity, not substituted for ICCCM window groups. A WMCLASS fallback
matches only one unambiguous pending record, comparing protocol UTF-8 with ICCCM
Latin-1 class values. An actual unambiguous class completion also broadcasts a
bounded asynchronous standard remove for competing observers, without an XSync
barrier. Avoid property work on the legacy path when no pending work.

Late property changes on otherwise-unmanaged group leaders are deferred: there is
no shared reference-counted X event-mask owner, and temporary mask restoration can
erase another selector's changes. Do not add permanent watches, a second connection
or a broad mask ownership rewrite. This subset must remain visible in #56 rather
than claiming complete startup-notification WM semantics.

## Consumer and evidence

Expose a bounded read-only `startups` snapshot through existing core/MFL query
framing. Show pending, protocol ended, window observed and expired in a native
FvwmDesktop Launch activity form with Refresh/Close. Protocol ended is not success;
launch failed requires actual local owner evidence. No periodic polling service.
The native/Go implementers agree the JSON schema before consumer implementation.

Verify sender and parser independently, interleaved/competing/malformed/oversized
messages, dead producer/different removal producer, deadline/capacity behavior,
explicit/group/class matching, helper/display absence/blocked announcement, literal
argv/cwd, exec failure and application survival. Use one real expiry interval for
the combined idle/busy native fixture, fast injected-clock policy cases for the
exhaustive state transitions, and three themes for new UI. Add early affected BSD
native checks. Preserve the wider installed/release coverage as remaining work.

Sources: [startup-notification 0.2](https://specifications.freedesktop.org/startup-notification/latest/),
[Desktop Entry keys](https://specifications.freedesktop.org/desktop-entry/latest/recognized-keys.html),
[ICCCM window groups](https://xorg.freedesktop.org/releases/current/doc/xorg-docs/icccm/icccm.html#window_groups).
