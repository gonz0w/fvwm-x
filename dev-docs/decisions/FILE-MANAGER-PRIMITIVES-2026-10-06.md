# Configurable file manager and desktop icon primitives

Status: **direction and phased backlog approved on 2026-10-06**. The user
requested reusable FVWM primitives first, then a separate build thread can
adapt configuration/theme while core work continues. This records requirements;
no runtime implementation, component name, command syntax or dependency has
been selected. Current status and order remain in the
[project tracker](../PROJECT-TRACKER.md#execution-order).

Owners: FILE-VIEW-01, FILE-DATA-01, FILE-ACTION-01, FILE-JOBS-01,
DESKTOP-ICONS-01 and FILE-INTEGRATION-01. FILE-ADVANCED-01 holds deferred
candidates. Coordinate with BUTTON-01/02, CFG-01/02, IPC-02, IMAGE-01,
INPUT-01, PROC-01, MONITOR-01, MODULE-SCALE-01, TEXT-01, A11Y-01,
XDG-01, SELECTION-01 and SESSION-01/02; reuse those contracts and evidence.

The subsequent [runtime Go/Lua decision](RUNTIME-GO-LUA-2026-10-06.md)
supplements these primitives with a qualified isolated scripting consumer.
Earlier exclusion of a new scripting engine is superseded only for LUA-01's
bounded Go host. Native views/actions/jobs retain correctness and ownership;
no GUI toolkit, core rewrite or new unqualified runtime is adopted here.

## Product direction and existing foundation

Enable end users to assemble file managers with FVWM configuration, menus,
bindings, scripts and native modules. Shared services own file-operation
correctness; user configuration controls presentation, navigation and actions.
Provide a documented working example at each qualified checkpoint. A complete
default theme/configuration adoption is a separate consumer workstream.

The desktop helper already provides a menu-based folder browser with Home/Up,
terminal access and file opening. FvwmButtons supplies controls, FvwmScript
supplies widgets/input, FvwmForm supplies dialogs, and module/MFL transport,
colorsets, picture/font and monitor/scale helpers provide shared foundations.
FvwmIconMan models managed windows, not arbitrary file collections. Existing
Script lists are not evidence of a scalable multi-selection file view.

The subsequent [clipboard scope](CLIPBOARD-PRIMITIVES-2026-10-06.md) reuses the
collection and typed-action contracts for text/image content. Keep collection
identity independent of filesystem paths; clipboard content/lifetime/protected
storage remain with their own owners. This adds no file mutation or advanced
thumbnail prerequisite to the clipboard handoff and does not replace file-manager
items with another UI/service implementation.

The principal additions are a reusable collection view, an asynchronous
directory model and reliable file-operation jobs. Folder windows and desktop
icons should share item, action and resource contracts. Audit existing owners
before choosing whether to extend a module or add a bounded native module;
this document does not authorize replacing existing module APIs.

Keep authoritative X/window state and managed-window actions in the core and
shared command engine. Filesystem enumeration, metadata, copying, watching,
search and preview work belong in isolated modules/helpers. One filesystem
call can stall even on a local path; an event-loop time budget is insufficient.
Use cancellable owned work, bounded queues and explicit completion/refusal.
No mandatory GTK/Qt, desktop framework, compositor or new scripting engine.

## Ranked checkpoints and complexity

All initial runtime additions are P2 behind current security/responsiveness
work. The numbering below orders this feature's checkpoints, not the overall
project. Follow tracker stage 6 for portable primitives and stage 7 for the
optional bus adapter. Preserve session/notification/tray dependencies.

| Rank | Owners and checkpoint | Relative complexity |
| --- | --- | --- |
| 1 | FILE-VIEW-01 + FILE-DATA-01 + FILE-ACTION-01: useful configurable local folder browser | Moderate to high; collection view is the largest new UI primitive |
| 2 | FILE-JOBS-01: dependable everyday file operations | High; partial failure, conflicts and cancellation dominate |
| 3 | DESKTOP-ICONS-01: optional pinboard using the qualified collection/action model | Moderate after rank 1; monitor/input/lifetime behavior needs real X |
| 4 | FILE-INTEGRATION-01: default folder handling, application roles and standard file-manager requests | Moderate to high; portable entry point first, optional D-Bus after the common bus contract |
| Later | FILE-ADVANCED-01: separately bounded advanced-feature candidates | Variable; devices, remote locations and undo are substantial service work |

Rank 1 needs no file mutation service. Pinboard shortcuts can be prototyped
after rank 1; file mutation through them waits for rank 2. Design open-folder,
reveal-item and properties requests at rank 1 so interoperability does not
require rewriting the model. Advanced candidates are not required to deliver
the earlier checkpoints; select and qualify each before implementation.

## Rank 1: reusable browsing primitives

- **FILE-VIEW-01:** structured collection entries with stable IDs separate from
  labels and paths; a scrollable list first, then an icon-grid presentation of
  the same model. Support single/multiple selection, keyboard focus/navigation,
  activation, context actions, sorting and bounded incremental updates. Preserve
  selection/focus across refresh where the item remains valid. Render visible
  entries rather than creating a button/window/resource for every file. Define
  large-directory admission/paging and report limits without silent truncation.
- **FILE-DATA-01:** directory enumeration and metadata with owned cancellable
  requests and generations. Provide Home/Up/Back, path entry, hidden-file control,
  refresh and explicit inaccessible/disappeared-entry results. Start with local
  paths and manual refresh; add bounded native watching with a documented
  fallback after platform review. Linux/BSD watchers need their own evidence.
  Preserve native filename bytes independently of display text, including names
  not representable as valid UTF-8; never round-trip identity through a label.
- **FILE-ACTION-01:** expose selected item IDs and literal path/URI arguments to
  configured actions through bounded structured data. Define owned request IDs,
  view/directory generations and accepted/completed/failed/cancelled results.
  Revalidate stale targets; reuse IPC-02 rather than creating another generic
  transport. File names, labels and directory-provided metadata are never FVWM
  commands or shell fragments. Support navigation, opening, terminal-here and
  contextual actions through reviewed argument handling and XDG-01 providers.

Acceptance: pure model/selection/order/bounds tests and native real-X keyboard,
focus, scrolling, refresh, live updates and cancellation. Exercise large
directories, literal quotes/newlines/metacharacters, invalid encoding, stale
IDs, helper death and slow filesystem work while unrelated WM input continues.
Use existing text/image lifetime, scale/cache and module teardown contracts.

## Rank 2: file-operation jobs

FILE-JOBS-01 supplies create-folder, rename, copy, move, link, trash/restore and
explicit permanent deletion through one typed operation service. Define
progress, cancellation, conflict choices and per-item/final outcomes. Distinguish
same-filesystem rename from cross-filesystem copy/remove; never report success
before required steps complete or remove the source after a failed copy.

Define symlink handling, recursive traversal limits, disappearing/replaced files,
permission failures, metadata preservation guarantees and partial destination
cleanup. Trash requires the adopted desktop trash contract and filesystem/mount
handling; do not substitute deletion when trash is unavailable. Permanent
deletion is an explicit action, not a consequence of removing a desktop pin.
Cancellation is best effort with honest partial outcomes, not rollback or undo.

Acceptance: native filesystem fixtures for conflicts, links, recursive trees,
cross-filesystem moves, denied access, full destination, external changes and
cancellation. Real-X progress/decision/cancel controls must remain responsive;
qualify operation ownership across view closure, helper death and WM restart.

## Rank 3: optional desktop pinboard

DESKTOP-ICONS-01 reuses the view/model/actions for persistent pinned references,
movable icons, keyboard access and contextual menus. Removing a pin removes
only the reference. Actual Desktop-directory contents are a subsequent bounded
mode, with file mutation delegated to FILE-JOBS-01.

Define desktop-layer ownership, empty-area/root-menu input, focus, workspaces,
saved placement, work areas, hotplug recovery and per-monitor scaling. Preserve
client pixels and existing root bindings/configurations. Cooperate with the
existing wallpaper owner and respect other desktop managers; no implicit second
wallpaper setter or automatic desktop takeover.

Acceptance: actual-X input/stack/focus tests, saved-position/restart recovery,
mixed-scale monitors, work-area/topology changes, missing targets and concurrent
folder views. Configured placement is data under CFG-01, not executable state.

## Rank 4: application interoperability

FILE-INTEGRATION-01 has two adoption checkpoints:

1. A documented installed CLI/desktop-entry role for opening folders, revealing
   selected items and requesting properties. Reuse XDG-01 for `inode/directory`,
   MIME/application associations, URI handling and safe Desktop Entry execution.
   Do not silently change the user's existing default provider. Detect the
   file-manager role explicitly so opening files does not recurse through the
   folder handler. Unsupported URI schemes return explicit failures.
2. An optional `org.freedesktop.FileManager1` adapter with `ShowFolders`,
   `ShowItems` and `ShowItemProperties`, following SESSION-02's reviewed bus
   lifecycle/dependency contract. Respect another owner, bound requests, preserve
   startup/focus policy and expose only implemented capabilities. No bus is
   required for portable browsing or file operations.

File clipboard and XDND source/target roles are delivered with SELECTION-01,
using literal file URIs, bounded transfers and cancellation/owner-death handling.
They are distinct from a generic WM drag interceptor. A reveal request displays
and selects an item; it never executes the item. Ordinary Open/Save dialogs and
a new FileChooser portal backend are separate scope, not implied here.

Acceptance: independent applications/clients for folder/reveal/properties,
default-provider registration/override, literal URI/path handling and file
transfer interoperability. Qualify absent/failed bus, competing owners,
activation/restart and enabled/disabled installed profiles across supported OSes.

## Deferred advanced candidates

FILE-ADVANCED-01 records thumbnails/previews, tabs or dual panes, search,
bulk rename, archive actions, removable media and optional remote locations.
Promote one bounded feature at a time after its prerequisite checkpoint; do
not turn this list into a single full-feature file-manager rewrite.

Use isolated bounded decoders and cache generations for thumbnails/previews;
incremental cancellable workers for search; preview/conflict handling for bulk
rename; reviewed optional providers for archives/devices/remote locations.
Undo requires its own external-change and recovery contract. Device integration
does not imply autorun, privileged mount handling or new mandatory OS services.

## Configuration/theme handoff and qualification

At the qualified rank-1 checkpoint, the runtime owner publishes installed man
documentation, actual supported configuration/action syntax, versioned data and
result contracts, limits/capabilities and a small working browse example. Record
source/profile evidence, remaining gaps and ownership in the tracker. Prototype
interfaces may be explored earlier but are not a stable consumer handoff.

The user can then use a separate build thread to adapt configuration/theme:
layout, palettes, toolbar/sidebar, bindings and contextual workflows. That
consumer need not wait for jobs, desktop mode or advanced features. Provide
subsequent handoffs for each qualified capability; retain legacy behavior and
feed missing primitives back to their owning items. Neither workstream rewrites
another thread's active source/builds, creates a competing service or claims
theme success as runtime qualification. No chat is created by this record.

Follow focused development checks and one combined batch qualification from
WORKFLOW.md. Required pure native tests, nested-X Linux/relevant default-BSD-X,
six-target builds, enabled/disabled profiles, installs and man pages remain
acceptance gates. Native Linux sanitizer CI and physical/toolkit checks remain
separate evidence. Scope approval is not a passing implementation or a package,
default-theme or release-publishing decision.

## Reference designs and protocols

- [ROX-Filer manual](https://rox.sourceforge.net/Manual/Manual/Manual.html):
  configurable direct manipulation, folder windows and pinned references.
- [PCManFM-Qt](https://github.com/lxqt/pcmanfm-qt): file and desktop icon manager.
- [Thunar documentation](https://docs.xfce.org/xfce/thunar/start): custom actions,
  bulk rename and optional service extensions.
- [File-manager D-Bus interface](https://wiki.freedesktop.org/www/Specifications/file-manager-interface/):
  folder display, item reveal and properties.
- [FileChooser portal](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileChooser.html):
  separate application Open/Save contract.

These are interaction/protocol references, not dependency selections or claims
that FVWM currently implements their feature sets.
