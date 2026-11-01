# Native file clipboard and drag-and-drop

Selected by Cam's R1 continuation, 2026-10-31; lead owns integration. This is
the missing FILE-INTEGRATION-01 #67 consumer with the necessary SELECTION-01
#22 subset. Whole #22, protected text/image history and release acceptance remain
separate. No product implementation or passing qualification is inferred here.

## Outcome and ownership

An explicit `fvwm-file-browser --file-transfer` opt-in enables file Copy, Cut,
Paste and XDND source/target interaction. Copy/Move only; the destination is the
displayed directory, not an inferred folder under the pointer. Every received
operation shows explicit confirmation and uses existing typed file jobs. No
dropped subject is executed. Support up to eight source parent groups with
sequential admission, per-item/partial results and cancellation.

Use one browser-lived native X11 helper, `fvwm-file-transfer`, plus an additive
Form list input adapter. Form is replaced during navigation; keeping clipboard
ownership inside Form would break Cut → navigate → Paste. A persistent helper
avoids an unrelated Form lifetime rewrite. No mandatory toolkit, bus, new Xlib
binding, history daemon or default session resident service is introduced.

The helper owns CLIPBOARD, XdndSelection and asynchronous selection/protocol
state. Form owns actual window/list XIDs, genuine input timestamps, drag
threshold and its pointer grab throughout the gesture. The browser maps native
selection bits/generations to existing file identities and prepares/executes
jobs in isolated workers. URI bytes use the private helper/browser pipe only;
they never enter Form, Echo, generic core commands, labels or diagnostics.
Only ordinary file references enter Go. Protected clipboard payloads remain
outside this API and with CONTENT/RETENTION/STORE.

## Native Form/helper link

The browser creates a short private runtime directory and passes its socket,
128-bit nonce and Form epoch as literal `FileTransfer` directive metadata.
The native helper owns the AF_UNIX SOCK_STREAM listener; at most two peers
allow old/new Form retirement. Refuse pre-existing sockets, unsafe directories,
wrong users where peer credentials are available, mismatched nonce and stale
epoch. This isolates lifetimes; it does not defeat hostile same-user/X11 clients.

`libs/file_transfer_protocol.h` freezes the 608-byte metadata record: 24
big-endian uint32 words plus at most 512 selected-bit bytes. The header owns
word layout, kinds and limits. Encode/decode validates kind-specific fields,
zero unused fields/padding, epoch, generation and row/selection bounds. Preserve
partial records and ordering; coalesce motion only. Queues hold at most eight
records. Overflow cancels the gesture explicitly; never discard release/cancel.

Attach reports real `CF.frame` and list child XIDs, current native generation
and epoch. Generation changes refresh metadata. Form emits list-focused Copy/Cut/Paste actions and begin/motion/release/
cancel and forwards only recognized XDND ClientMessage records, with format32
and five exact protocol words. This removes any need for a foreign-window
XdndProxy: Form receives its own ClientMessages and forwards them cooperatively.
Form advertises XdndAware only while a current helper attachment is ready.

Opt-in Button1 drag starts after four scaled logical units; ordinary clicks
retain existing semantics. Defer an opted-in selection toggle until non-drag
release so dragging an already selected row preserves the group. Do not take
root/WM bindings or text-input paste. Escape, pointer loss, Form destruction,
peer EOF and stale native generations cancel only the relevant gesture. Native
feedback supplies accepted Copy/Move/refusal cursor and list-border feedback.

## Private browser/helper control

Use bounded newline-delimited JSON on inherited stdin/stdout, version1. Each
record has `v:1`; commands use `command` as discriminator and `id`, Form `epoch`, and transfer/lease IDs are canonical
decimal uint64 strings, never floating-point numbers. Native `generation` is a
uint32 number. URI text is ASCII percent-encoded file URI data, not path labels.
Maximum line512KiB, at most eight queued control records, aggregate output1MiB.
Unknown/duplicate fields/types and malformed framing refuse explicitly.

Commands: `expect-form` (epoch), `context` (epoch/generation/enabled), `offer`
(action/uris), `read`, `drag-data` (transfer/action/uris), `complete`
(transfer/success/action), conditional `clear` (lease), `cancel` (transfer),
`shutdown`. All commands carry `id`; replies carry that same `id` and `status`
`ok` or `error` with a fixed diagnostic code. Offer returns `lease`; a successful
read reply returns `transfer` so a later failed conversion can be correlated even
when no proposal is emitted. Parent EOF
retires the helper and its operations; browser retains ownership until Wait.

Events: `ready`; `action` (transfer/kind copy-cut-paste/epoch/generation/time/
rows/selection base64 bits); `attached` (epoch/generation/frame/list); `drag-begin`
(transfer/epoch/generation/time/rows/selection base64 bits); `proposal`
(id for read, transfer/kind clipboard-or-dnd/action/uris/epoch/generation and
own lease when applicable); `result` (transfer/status success-failed-cancelled-timeout/action); `lost` (lease);
`unavailable` (fixed code). Motion stays native. Operational diagnostics exclude URI-bearing records. Isolated verification
fixtures retain only their known synthetic references as bounded test evidence.
Commands `offer`/`read` may supply native `time`; when absent the helper obtains
a fresh server timestamp asynchronously before selection ownership/conversion.
`admit` (transfer) acknowledges confirmed job admission and switches its deadline
from confirmation to the bounded job phase before Go starts mutation.
The first implementation must keep these schemas shared and documented in its
tests rather than grow a generic command registry.

## File and X11 transfer profile

Use ICCCM2 selections and XDND versions3–5. Offer `text/uri-list` and clipboard
`x-special/gnome-copied-files` copy/cut. Support TARGETS, TIMESTAMP, bounded
MULTIPLE and direct/INCR transfers. INCR is not an advertised representation.
XDND source also supplies `text/x-xdnd-username` metadata where required.
Reject unsupported targets including DELETE, unsupported actions, remote hosts,
malformed URI encoding and stale/invalid timestamps. No PRIMARY synchronization,
CLIPBOARD_MANAGER or SAVE_TARGETS adoption in this file-only checkpoint.

Encode a real local hostname in outgoing XDND file URIs. Accept empty authority,
localhost or the captured local hostname only; preserve encoded path bytes and
decode exactly once with existing `filemodel.ParseTarget`. No userinfo, ports,
query/fragment, NUL, relative/root-only targets or dot components. Deduplicate
lexical paths without following symlinks; filesystem observation stays in workers.

Bounds: 256 references, 4096 decoded bytes per path, 256KiB encoded URI data,
eight parent groups, one source drag and one incoming proposal per browser,
four concurrent outgoing conversions, eight MULTIPLE pairs and16KiB INCR chunks.
Immutable outgoing buffers survive replacement until their in-flight requests
retire. Bound aggregate retained URI bytes to2MiB. Transfers have five-second
inactivity and30-second absolute limits; source status waits are bounded.

Clipboard survives folder navigation and Form replacement, and ends when its
browser/helper exits or ownership changes. A new Copy/Cut replaces its lease.
Clear an internal cut lease only after successful Move and only if still current;
never clear a newer/foreign selection. Clipboard conversion means data delivery,
not filesystem completion or an intentional paste from arbitrary X11 clients.

## Confirmation and file operations

Observe each grouped source through existing cancellable directory workers,
capture source identities and destination, then show one Copy/Move confirmation.
Transfer requests carry an optional captured destination identity; the existing
job worker compares the actually opened directory device/inode/type/mode before
mutation. Mutable directory timestamps/size are not identity checks. Existing
callers without the additive field keep their accepted behavior.
No mutation precedes confirmation. Use existing file-job workers sequentially;
stop later groups after failure, unknown outcome or cancel, retain admitted work
until actual Wait, and expose partial per-item results. No aggregate rollback,
automatic replay or source deletion triggered by XdndFinished.

Confirm within30seconds. After confirmation, allow at most300seconds for this
transfer job sequence while retaining normal file-job cancellation semantics.
Send successful XdndFinished only after all requested work completes successfully.
Cancellation, partial failure, timeout or uncertainty is unsuccessful/unknown.
A timeout cannot prove a blocked filesystem mutation stopped; retain ownership
and report its eventual Wait/outcome honestly. Do not reuse a destroyed target
or changed Form generation to admit a later mutation.

## Qualification and source boundaries

Implement a coherent set before the combined gate: native owner/requestor/INCR,
Form attachment/input, browser file policy/job coordination, independent X11
clients and installed portability. Run affected incremental Linux builds and
policy checks during development, then one combined normal/race/scoped-sanitizer/
real-X/checkpoint with Slate, Dune and enlarged Classic screenshots. Check native
BSD APIs early; complete affected six-target installed acceptance together,
reusing unchanged PR315 and settings/platform evidence. No suite per helper.

Review implementation and exact evidence separately before a signed author-DCO
PR. Standing R1 merge authorization applies only after applicable checks/review.
No issue closes merely because a platform check or component merges. #67 closes
only when the file-transfer and installed interoperability criteria qualify;
#22 retains other clipboard/core/Script/Form transfer rows and manager adoption.

Primary protocol references: [XDND](https://wiki.freedesktop.org/www/Specifications/XDND/),
[ICCCM2 selections](https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html),
[clipboard conventions](https://specifications.freedesktop.org/clipboard/latest/).

## Confirmation input completion

Real integration exposed a dependency in existing Form buttons: release events
were selected only when the Form reached its explicit pointer grab. A server
can process a fast release first. Adding the initial release subscription also
requires matching press ownership, so an orphan or unrelated release cannot
activate a command. The bounded correction retains configured ActivateOnPress,
keyboard matching, explicit grab/cursor and modal restoration, with cancellation
before commands and withdrawal. It adds no configuration migration.

Qualification uses a fixture-owned paused Form and genuine XTest input for a
deterministic pre-fix failure, then focused ordinary, orphan, button-chord,
outside, modal, keyboard, unmap/remap and scaled interactions. Existing browser
pointer confirmations remain required alongside Alt-Y confirmation and Alt-Z
cancellation. The [Xlib event contract](https://www.x.org/releases/X11R7.6/doc/libX11/specs/libX11/libX11.html)
explains automatic-grab masks. The original gate12 event schedule remains
unproven; the deterministic regression owns this correction's evidence.
