# Session-owned clipboard capture C1

Owner Cam; selected scope and continuation live in
[the tracker](../PROJECT-TRACKER.md). This composes the accepted content owner
and ordered observer, without changing the owner's chosen history/lock/copy
policy. Installed enablement, numeric defaults and consumer/UI remain pending.

## Ownership and admission

`fvwm-clipboard-capture` is an isolated single-threaded C child with its own X
connection. The session, rather than a settings window, owns one process slot.
Go supervises start, bounded pipe I/O, direct child signals and one Wait. It
never receives content, labels, source titles, hashes or previews. There is no
restart loop or child-slot reuse. Startup has no selected default resource
limits and starts with UNKNOWN lock state. The installed CLI passes no capture
configuration; therefore ordinary startup leaves capture off.

The source-only Config seam requires an absolute helper path, caller-unique
nonzero epoch, 1..4096 items, 1..32768 encoded bytes per item, explicit total
encoded/mapped/locked budgets and positive lifetime. The native owner derives
page/guard charges and prepares protected storage before requesting payload.
A full budget refuses new content without evicting an older or pinned item.
Memory protection/preflight failures refuse capture; failed cleanup retains its
charge and terminates the child. EOF/logout attempts bounded native cleanup;
only confirmed normal exit after requested stop establishes its clean result.
Forced exit, lost X connection, uncertain Wait or protocol loss is not a wipe
claim. The parent retains terminal failure without restarting the slot.

## Formats, sensitivity and source identity

Capture is CLIPBOARD-only, with PRIMARY unchanged. This first configured subset
requires an advertised UTF8_STRING target. It stores original format-8 bytes,
including NULs, without guessing STRING, decoding, rendering or asserting that
these bytes are valid UTF-8. A future text consumer owns validation/projection.

The independent TARGETS conversion accepts at most 256 packed ATOM/32 values,
including INCR, under ordered source fences. Unavailable/refused/malformed
metadata refuses capture. Presence of `x-kde-passwordManagerHint` excludes the
whole item without fetching the hint value or text. This conservative rule also
excludes an advertised malformed/unavailable hint. KeePassXC's
[producer source](https://github.com/keepassxreboot/keepassxc/blob/develop/src/gui/Clipboard.cpp)
sets that format for copied secrets; absence is not proof of confidentiality.
The classifier recognizes this one convention, not arbitrary application intent.

One current source token binds display, selection, epoch, owner and generation.
The observer detects same-window reacquisition and ABA transitions; an owner XID
alone never grants capture. Only one transfer is active. Notifications coalesce
to the latest fresh snapshot. A control can cancel an entered conversion; its
response follows producer retirement. Metadata queries preserve interrupted
pending capture, while clear deliberately prevents recapture of an unchanged
source. One deadline covers format discovery and payload transfer. Existing
Xlib calls cannot be preempted; the native process supplies isolation.

The owner permits one transient native Xlib reply of at most 32 KiB, copied into
prepared storage and accessible reply bytes wiped before XFree. No project
aggregate or Go payload copy is made. This does not guarantee that Xlib's own
reply never reached swap. See the accepted
[copy boundary](CONTENT-SELECTION-SINK-2026-11-01.md).

## Retention and controls

Each retention window begins at its first admitted capture and lasts at most
the caller's lifetime. Later captures share the same absolute deadline and may
therefore expire shortly after publication. This is a maximum window lifetime,
not an installed per-item TTL promise. Clear starts no new window until a later
source change; expiry allows a new window for subsequent captures. This makes
existing strict equal-deadline dedup possible without extending an older item.
Equality compares original encoded bytes through bounded native leases. Cursor
steps and cleanup yield between turns. Pinning grants no capacity or expiry
exemption; automatic dedup never drops a pinned item.

The private child protocol has seven decimal request fields:
`version sequence verb epoch item generation value`. Version is 1; each sequence
is positive and strictly advances. Verbs are status=1, list=2, pin=3, remove=4,
clear=5, state=6 and quit=7. List's value is one bounded storage slot, not a stable
identity. Pin/remove require an exact current item/generation; actions cannot
use a slot index as identity. Other commands require zero item/generation.
Frames have no extra fields, signs, whitespace variants or payload extension.
The general session socket permits only metadata/history verbs, never state or
quit. It retains existing session generation/operation checks and single active
service operation. Large integers use decimal strings in JSON.

A reply has fourteen decimal fields: version, sequence, result, epoch, item,
generation, representation, length, deadline, pinned, mode, charged items,
mapped bytes and locked bytes. Only successful list returns an item descriptor;
it is not a payload lease. Blocked state hides history descriptors and usage.
Result codes are OK=0, BLOCKED=1, REFUSED=2, STALE=3, INVALID=4, FULL=5, FATAL=6
and EMPTY=7. Modes are UNKNOWN=0, UNLOCKED=1 and LOCKED=2. Only one frame/reply is
retained at a time, and output uses bounded nonblocking writes. The parent
checks exact response sequence/epoch and treats malformed replies or response
deadline loss as terminal. C2 keeps canceled read-only STATUS/LIST observations
independent of frontend lifetime: their one correlated numeric reply drains
privately under the existing deadline before another wire request can start.
Canceled submitted mutations/barriers still stop the child rather than accepting
a late lock barrier. Closing an observation does not close session history.

## Lock dependency and qualification

STATE is a trusted parent integration seam, not a public unlock command or a
qualified observation by itself. The child starts UNKNOWN. A future qualified
lock-state source must drive both startup admission and any unlock. Existing
locker readiness is unconfirmed: request, successful Start and exit cannot
prove lock/unlock or external-lock coverage. The session issues LOCKED and waits
for its native barrier before starting an admitted locker, retains history and
never automatically resumes it. Unknown/lost state blocks capture and history
access. Source tests use explicit synthetic state transitions on a private X
server; they do not qualify a real desktop locker.

C1's combined Linux checkpoint covers the actual child and metadata client,
public-pattern direct/INCR producers, hints, source changes, controls, capacity,
dedup, pin/expiry, EOF and an independently compiled cleanup-failure child.
Broader platform/install/default-X/locker acceptance, images, preview/export,
restore, CLIPBOARD_MANAGER/SAVE_TARGETS, persistence and crypto stay queued.
No UI changes imply no new screenshot/theme run in this source checkpoint.
