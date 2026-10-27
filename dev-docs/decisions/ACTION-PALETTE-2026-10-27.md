# ACTIONS-01: one native palette with four providers

Owner: Cam. The finite #219 release contract covers windows, applications,
advertised desktops and registered actions. Optional file/notes adapters are
outside this checkpoint. Reuse the accepted native collection/layout and #213
revision/lifetime policy; neither broad theme/text owners nor a search daemon
are prerequisites.

## Architecture and ownership

The existing FvwmDesktop resident owns one optional palette. Core X/window state
stays in the core. It prepares copied bounded window/desktop metadata and
revalidates typed effects through the shared command engine. Existing committed
application discovery owns literal argv and localized labels. A finite typed
registry owns eight invocable Desktop actions; command completion metadata is
not an execution catalog.

No installed executable, mandatory dependency, database, additional resident,
new packet type or public MFL resource is added. The palette uses the existing
module Send_Query/MX_REPLY chunk transport directly. Shipped function/command
metadata is regenerated from the native table, not a new grammar.

## Core metadata and action bridge

`Send_Query COOKIE SERIAL palette OFFSET LIMIT GENERATION` prepares a snapshot
with offset zero and generation `-`; continuations require the exact returned
generation. Pages contain at most 16 rows in the existing 32 KiB reply envelope.
Rows carry opaque stable identity, snapshot index, kind, literal label/detail,
native icon mark and availability. The result discloses scope, total, continuation,
coverage and truncation. A prepared observation is not a live authoritative cache.

Scan at most 4,096 managed records, retain at most 512 eligible windows and 128
advertised desktops. Respect SkipWindowList and scheduled destruction. Desktop
coverage derives from each active monitor's advertised desktop count, not four
shipped buttons. Names/details are bounded, UTF-8 sanitized and visibly shortened;
truncation remains explicit. No filesystem access or X round trip occurs during
preparation. At most four module-owned snapshots fit the statically checked
1 MiB global storage bound; row storage is allocated only on use.

A window identity binds the core incarnation and monotonic managed-record
allocation serial; neither XID, pointer nor title alone is an identity. Wrap
permanently refuses new IDs. Desktop targets bind monitor output/role and desk,
with committed topology and advertised-range revalidation. No window/monitor or
source-string pointers remain in a prepared snapshot.

`Send_Palette COOKIE SERIAL GENERATION ROW_INDEX` accepts only the requesting
module/cookie's exact snapshot and revalidates the current target immediately
before each fixed command-engine operation. Window presentation uses existing
Iconify/Raise/Focus semantics; desktop switching preserves existing monitor mode
semantics. A success reports submission, not painted completion. Stale/destroyed/
reused targets are refused; partial command failure does not imply rollback.

`Send_Palette COOKIE SERIAL - -` releases only that module/cookie's snapshot.
Each view has a new cookie, so a late close cannot release a successor. Module
kill and final destruction also free snapshots; duplicate cleanup is harmless.
No expiry timer or default event subscription is needed for this storage.

## Native model and view

Four provider records disclose version, state, reason and coverage. Windows and
desktops share the core snapshot; applications reuse up to 500 committed entries;
eight typed actions reuse existing settings, appearance, inspector, checker,
discovery, terminal, files and Run behaviors. Missing one provider disables its
rows while other providers remain usable. No completed command text or label
becomes a shell command.

The existing collection caps the combined view at 1,156 rows and 128 UTF-8 query
bytes. Matching retains literal case-insensitive semantics, with no fuzzy,
normalization, IME or shaping promise. Printable Unicode labels remain printable;
configuration expansion/control characters are escaped for native presentation.
Original application identity/argv remains unchanged and is checked again before
launch. Registered capabilities are checked again at activation.

One binding owns one active core refresh and one pending latest intent. Partial
reply buffers, page counts, deadlines and callback generations are bounded.
Validate the complete temporary model before completing a ticket or publishing;
malformed, superseded and closed-lifetime results cannot commit. Query changes,
provider refresh, native ACK and selection generations retain separate ownership.

The native Form has query/filter, a virtualized versioned list, Activate, Refresh
and Close. Asynchronous bounded native batches wait for real Form ACKs without
blocking the controller event loop. Surviving identity retains query, selection,
focus and scroll. Window event subscriptions exist only while the palette is
open; bursts coalesce, and own-Form events are excluded. Automatic events do not
renew the user inactivity lease. There is no per-control polling loop.

Super+A opens the palette, preserving the existing Super+P Run binding. F3
filters, F5 refreshes, Tab/arrow keys navigate, Enter activates and Escape closes.
Ordinary native focus-grab/release restores focus on Cancel. Before activation,
the palette disables its own focus-return so later Form destruction cannot undo
an explicit target focus. Refused activation restores Cancel recovery.

## Qualification boundary

Use one combined affected Linux policy/race/native checkpoint with Slate dark,
Dune light and enlarged text. Exercise real four-provider actions, literal data,
limits, supersession, producer loss, stale identities, release and keyboard focus.
Retain original failures. Scoped leak-enabled C tests cover production snapshot
allocation/release and pure limits. Affected support gates compile and execute
only changed C/Go contracts; do not repeat all installs or multiply every OS by
themes. Physical/assistive, full release and kernel wakeup claims require their
own observations. The [verification report](../verification/VERIFICATION-2026-10-27-ACTION-PALETTE.md)
records actual results and any remaining limits.
