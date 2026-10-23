# Collection and local folder-browser foundations

Status: implemented; focused checks passed, shared integration pending. Owners: FILE-VIEW-01,
FILE-DATA-01 and FILE-ACTION-01. The user selected supporting file/clipboard work
after merged PR164; further opacity/general appearance work is deferred.

## Bounded checkpoint

Use the existing optional Go runtime and native FvwmForm/MFL controls. Add the
stdlib-only fvwm-file-browser example, a domain-neutral collection metadata/
selection model, and a file adapter owning raw filename bytes and observations.
The native consumer uses twelve-entry pages rather than a new widget/window for
every filesystem entry. This is a working foundation consumer, not completion
of the full virtual scrolling list/grid, external files provider or browser
handoff. Existing folder menus/configurations and disabled-Go profiles remain.

Collection IDs are distinct from labels and list positions. Validate replacement
before publication, bound entries/selection and preserve only surviving IDs.
File IDs include directory and entry device/inode/mode plus original filename
bytes; hard links with distinct names remain separate entries. Refresh preserves
identity across metadata edits, while activation revalidates full observed
metadata. Non-UTF-8 filenames remain bytes; private JSON uses base64 byte fields.
The first consumer uses bounded ASCII display escapes and never reconstructs a
path from a displayed label.

## Worker and action ownership

No filesystem calls execute in the core or browser control loop. One fresh
process owns enumeration, metadata and application dispatch. Keep one active
child/result slot and one latest navigation replacement. Cancellation requests
termination, invalidates output and retains the slot until Wait completes. The
ten-second deadline cannot guarantee that a kernel call exits immediately.
Blocked/open FIFO, supersession and cancelled-output controls qualify the actual
process boundary; do not describe future cancellation alone as preemption.

Refuse folders over 4096 entries (including hidden names), paths over 4096 bytes,
private envelopes over 4 MiB and selections over 256 items. Failed/over-limit
loads retain the previous snapshot. Sorting operates on the admitted bounded
snapshot; no partial unsorted prefix is silently published. Local manual refresh
comes first; watching, recursive search and mutation are separate owners.

Typed callbacks bind version, random consumer identity and rendered generation.
Each Form has a distinct owned resource alias and busy/ready lifetime. Old
callbacks cannot act on a new snapshot. MFL framing, serialized writes, endpoint
discovery and native Form Base64 substitutions are reused. No packet type or
core dispatch changes. The shared endpoint helper is moved from FvwmCommand to
its existing transport package; its original policy tests remain consumers.

Known actions are navigation, literal executable/path open and terminal-here.
Recheck root/entry identity before dispatch; symlinks and special files are listed
but refused. No shell evaluates names. Process started is an explicit handoff,
not proof of completed application work. Path revalidation cannot lease a path
across a later external application's open; destructive jobs need a separate
safe descriptor/traversal contract and are unavailable here.

## Consumer and handoff limits

The Form example has keyboard/multiple selection, page navigation, Home/Up/Back,
path entry, hidden toggle, refresh, cancel and close. History is bounded to 32
successful locations; selection/focus policy is reusable independently of the
Form's redraw. Idle closure after five minutes bounds an orphan helper/Form;
window-manager closure can leave the helper until that deadline. Per-render
aliases prevent old teardown/configuration from owning a replacement Form.

No mandatory toolkit, daemon, new dependency or default desktop switch is added.
The optional executable and man page share the file-browser install tag. Manual
selection uses the existing Go-enabled docs switch and manifest. Actual command
syntax and current capabilities live in the installed manual.

Clipboard can reuse metadata/selection, not this ordinary file-path Echo/Form
channel for sensitive payloads. Content identity, asynchronous X11 transfers,
retention, memory protection, private payload delivery and optional encryption
remain unimplemented with their existing owners. No protected-content or secret
storage claim follows from this checkpoint.

[Focused verification](../verification/VERIFICATION-2026-10-23-FILE-BROWSER-FOUNDATIONS.md).
