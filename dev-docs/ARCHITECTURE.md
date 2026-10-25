# FVWM-X architecture

This is the maintained source-navigation map, replacing the old generated
planning graph as the project's entry point. Paths identify subsystem owners;
the source, Meson graph and tests determine actual behavior. Update affected
rows with an architectural change, rather than regenerating broad claims.

## Ownership and flow

```text
X events, module commands, timers and configuration
                       |
             event / command dispatch
                       |
       core-owned windows, monitors and resources
                       |
      policy decisions -> ordered X/property updates
                       |
            module packets / status events
```

One core process owns managed-window state. Modules are separate processes;
they own their UI and communicate actions through the command/protocol boundary.
External compositors own compositing. Helpers do not create a second authority
for core windows, monitor state or command order.

The approved [file-manager primitives](decisions/FILE-MANAGER-PRIMITIVES-2026-10-06.md)
now have a native list/grid consumer and bounded metadata refresh (see source
row below); in-place native updates and external provider handoff remain queued. Views/actions reuse
native module/UI/IPC owners; directory enumeration, watching and file-operation
jobs stay in isolated services. Folder windows and the optional desktop pinboard
share the model. A separate config/theme consumer uses qualified documented
contracts; it does not own file-operation correctness or core window state.

The approved [clipboard primitives](decisions/CLIPBOARD-PRIMITIVES-2026-10-06.md)
are also queued. Typed content, retention and protected memory/persistence stay
in an isolated service/module, reusing SELECTION-01 and the collection/action
contracts. Sensitive payloads do not flow through core command expansion or
general event broadcasts; selected UI/decoder/transport copies need protection
review. A qualified text/image example enables a separate configuration consumer
while further runtime work continues. Existing source rows do not imply these
services are implemented or protect against hostile X11 display clients.

The approved [shared desktop scope](decisions/DESKTOP-EXPLORATION-2026-10-06.md)
extends queued CFG/IPC/SESSION/FILE contracts: bounded provider capabilities and
state, typed generation-aware actions, operation progress/cancellation, collection
filtering and owned-setting preview/revert. Existing domain services retain
target/payload/lifetime authority; common presentation does not create a second
window-state owner or move blocking backend work into the core. Adopt with current
session, file and clipboard consumers. Later hardware/application adapters and
richer layouts remain DESKTOP-EXPLORE-01 candidates after the existing program,
with no new source entry points or runtime availability claimed here.

The approved [runtime language scope](decisions/RUNTIME-GO-LUA-2026-10-06.md)
targets Python/Perl runtime helpers for Go migration and explicitly retires
Perl-specific framework interfaces after qualification. Existing source rows
still describe today's implementation. Lua policies live in an isolated Go host
over native primitive/module/IPC contracts; C owns rendering, resources and
authoritative X/window state. Script states/callbacks use serialized ownership
and generations; sensitive content remains in its qualified owner. Native C Lua
embedding, a Go runtime in the WM and arbitrary Perl translation are outside
this scope. Concrete entry points and dependency profiles remain queued.

| Area | Source entry points | Responsibility |
| --- | --- | --- |
| File/collection foundations | [collection model](../bin/FvwmPrompt/internal/collection/model.go), [file adapter](../bin/FvwmPrompt/internal/filemodel/model.go), [local target requests](../bin/FvwmPrompt/internal/filemodel/target.go), [native consumer](../bin/FvwmPrompt/cmd/fvwmfilebrowser/main.go), [worker](../bin/FvwmPrompt/cmd/fvwmfilebrowser/worker.go) | Optional Go/std-lib owner reuses MFL transport and FvwmForm. Stable metadata/selection, byte-preserving directory snapshots and validated literal actions use one bounded isolated worker plus one latest navigation. Portable folder/reveal/properties requests retain literal local path/URI bytes and resolve selection in the worker. Bounded label projections, snapshot sorting, selection and observed properties extend the native list consumer. [Form list policy](../modules/FvwmForm/form_list.c) owns bounded bitmap/focus/scroll state; [widget](../modules/FvwmForm/list_widget.c) draws only the list/grid viewport in one X child, using native type marks without file/decoder resources. Grid rows, hit testing and additive range selection share bounded pure policy. Configuration batches await core acknowledgements; generation-checked indices resolve through collection metadata. [Metadata transactions](../bin/FvwmPrompt/internal/collection/update.go) validate generation, bounded mutations and complete resulting order before atomic publication. Snapshot reconciliation, opaque observation revisions, visible-neighbor focus and selection recovery feed browser refresh feedback. [Selection adapter](../bin/FvwmPrompt/internal/filemodel/selection.go) admits IDs/argument budgets and revalidates the whole set in the worker before configured literal-executable launch. Observed selection summaries do no filesystem/content work; inspected ID sets retire on disappearance. Opt-in native live snapshots stage bounded labels/source ordinals separately, publish selection/focus/viewport through pure policy, and expose generation-tagged callbacks plus a native status handshake. Abort/exit frees staging; admitted transaction identifiers never recycle. The browser adopts live snapshots with native acknowledgements every 32 direct messages, pre/post-publication selection reconciliation, invocation tickets and fixed clipped status/properties labels. Initial configuration remains paced separately. Navigation/layout boundaries or refused-state recovery can rebuild the frame; typed external actions, mutation and protected clipboard payloads retain their owners. |
| Command clients / metadata | [FvwmPrompt.go](../bin/FvwmPrompt/FvwmPrompt.go), [FvwmCommand Go client](../bin/FvwmPrompt/cmd/fvwmcommand/main.go), [session.go](../bin/FvwmPrompt/transport/session.go), [generate.py](../tools/maintenance/generate.py), [Module.pm.in](../perllib/FVWM/Module.pm.in) | Prompt owns its readline UI; FvwmCommand owns its bounded stdin/first-record CLI. Both Go clients share the standard-library transport: one reader, serialized writes and idempotent teardown. Native headers/command table generate checked committed Perl metadata and JSON companions. Extended handler matching excludes the shared category marker. Broader Go ports and Perl retirement retain their own gates. |
| Bootstrap | [fvwm3.c](../fvwm/fvwm3.c), [screen.h](../fvwm/screen.h) | Display ownership, initialization, configuration, restart and global screen state |
| Dispatch / scheduling | [events.c](../fvwm/events.c), [event_loop_policy.c](../fvwm/event_loop_policy.c), [schedule.c](../fvwm/schedule.c) | Event ordering, readiness, bounded work, timers and deferred termination |
| Owned string values | [Strings.c](../libs/Strings.c), [contract tests](../tests/test_string_copy.c) | First-line copies and optional quoted font/configuration copies share a private length-returning owner. Whole-string action/path trimming treats newlines as whitespace. Empty results retain distinct owned storage; NULL remains NULL. Unsigned-byte whitespace and ASCII-space quote conventions remain separate. |
| Configuration streams | [read.c](../fvwm/read.c), [logical-line policy](../libs/config_line.c), [policy tests](../tests/test_config_line.c) | Read/PipeRead preserve completed command order and the descriptor barrier, grow reusable owned line storage to a finite 1 MiB, retain backslash-newline continuation and stop before dispatching a malformed/partial logical line. Existing count limit remains. Current reader changes are part of the XDG integration checkpoint, not a completed wider DATA-01 audit. |
| Standalone xlock menu | [Go producer](../bin/FvwmPrompt/cmd/fvwmmenuxlock/main.go), [utility fixtures](../tests/test_utility_contracts.py), [probe checks](../tests/test_xlock_menu.py) | The installed Go utility uses a direct five-second/1 MiB xlock help probe, byte-based streaming formatting and escaped external label values. Existing CLI/menu actions remain; frozen original CLI contracts require no retired producer. Meson owns a separate xlock-menu install tag. Other utility ports and runtime retirement have separate gates. |
| Standalone directory menu | [Go producer](../bin/FvwmPrompt/cmd/fvwmmenudirectory/main.go), [renderer](../bin/FvwmPrompt/cmd/fvwmmenudirectory/render.go), [navigation state](../bin/FvwmPrompt/cmd/fvwmmenudirectory/state.go), [CLI/filesystem checks](../tests/test_directory_menu.py), [live navigation](../tests/live_x/directory_menu_live.py) | Native Go utility caches entry metadata and format plans, preserves sorting/actions and bounds enumeration. Reuse navigation replaces shell writes with an owned atomic mode-0600 state helper; the existing plain-text state remains readable. All generated managed actions go through FVWM commands. |
| Shared wildcard matching | [wild.c](../libs/wild.c), [pure differential test](../tests/test_wild.c), [real conditions](../tests/live_x/style_wildcard_live.py) | Borrowed NUL-terminated byte strings; latest-star cursors retain constant stack/heap storage, with a leading-star literal suffix path. NULL, escape and legacy empty-tail results are preserved. Core conditions/bindings and shipped modules share this helper; prepared style metadata remains separately owned. |
| Shared token lookup | [Parse.c](../libs/Parse.c), [Parse.h](../libs/Parse.h), [direct units](../tests/test_parse_token.c) | Standard bsearch callback over string pointers or records whose first member is a string pointer. Unsigned-byte ctype preserves valid token matching; Form, Animate and Event are the shipped consumers. The [XDG checkpoint](verification/VERIFICATION-2026-10-12-XDG-PORT.md) qualifies the real scoped callback correction and affected native installs. |
| Full XDG menu service | [Go CLI/worker](../bin/FvwmPrompt/cmd/fvwmmenudesktop), [pure menu policy](../bin/FvwmPrompt/internal/xdgmenu), [acquisition/publication](../bin/FvwmPrompt/internal/xdgsource), [native Form](../modules/FvwmForm/FvwmForm.c), [CLI checks](../tests/test_xdg_desktop.py), [native-X checks](../tests/live_x/xdg_menu_live.py) | Coordinated Go service: bounded file merging, allocation, presentation, icon lookup/conversion and completed native menu output. Application actions reference private literal argv records; record locking/atomic union preserves existing menus. Native Form uses encoded fields, bounded command/input appends, per-invocation result identities and the existing local-command/core Read paths. Source/worker/X state remain separate; no new GUI library. The original Python provider and Perl frontend are retired; the [small settings adapter](../bin/fvwm-menu-desktop-config.fpl.in) delegates to the Go owner until #42 retires the framework. [Combined qualification](verification/VERIFICATION-2026-10-12-XDG-PORT.md) adopts the optional Go profile across six targets; M1 makes the Go utility family mandatory at build time; later framework/controller retirements retain their owners. |
| Script communication example | [Go helper](../bin/FvwmPrompt/cmd/fvwmscriptcom/main.go), [FIFO/CLI checks](../tests/test_script_com_example.py), [native widget interaction](../tests/live_x/script_com_example_live.py), [paired template](../modules/FvwmScript/Scripts/FvwmScript-ComExample.in) | Installed stdlib helper keeps count/Parse/exit framing, pins its user directory and owns private FIFO entries. Connected reads and reply delivery have finite deadlines; peer departure and signals clean up without terminating Script. The replaced Perl producer is retired; the configured native consumer uses the installed Go helper. The script-com-example install tag owns the paired consumer/helper. |
| Commands | [functions.c](../fvwm/functions.c), [functable.c](../fvwm/functable.c), [execcontext.c](../fvwm/execcontext.c), [read.c](../fvwm/read.c) | Shared parsing/expansion/execution and triggering context lifetime |
| Window lifetime / stack | [add_window.c](../fvwm/add_window.c), [fvwm.h](../fvwm/fvwm.h), [initial_window_options.h](../fvwm/initial_window_options.h), [stack.c](../fvwm/stack.c) | Managed records, XID identities, transient relationships and ordering; initial-window options expose a complete layout only to startup/placement field consumers |
| Geometry / input | [geometry.c](../fvwm/geometry.c), [move_resize.c](../fvwm/move_resize.c), [virtual.c](../fvwm/virtual.c), [interaction.c](../fvwm/interaction.c), [interaction_policy.c](../fvwm/interaction_policy.c) | Placement/constraints, shared Move/Resize/viewport readiness, checked button masks and cancellation. Audited service preserves borrowed monitor/window state; common final release waits use the same pump with 20-ms pointer fallback. Lifecycle/topology and image publication remain at outer unwind; viewport locally qualified; active menus reuse the audited owner with bounded motion and pure monotonic submenu timing; their separate checkpoint is locally qualified |
| Monitors / scale | [FScreen.c](../libs/FScreen.c), [private refresh adapters](../libs/fscreen_adapters.h), [monitor_topology.c](../libs/monitor_topology.c), [monitor_scale.c](../libs/monitor_scale.c), [monitor_scale_config.c](../fvwm/monitor_scale_config.c), [geometry_transition.c](../libs/geometry_transition.c) | Committed topology, rational scale, ownership and transitions |
| Style / rendering | [style.c](../fvwm/style.c), [update.c](../fvwm/update.c), [borders.c](../fvwm/borders.c), [Picture.c](../libs/Picture.c), [Flocale.c](../libs/Flocale.c) | Ordered styles, decorations and generation/scale-aware image/font resources; [update/barrier ownership](decisions/WINDOW-UPDATE-BARRIERS-2026-10-15.md), [actual-core style baseline](verification/VERIFICATION-2026-10-15-STYLE-PROFILE.md). [style_id_filter.c](https://github.com/gonz0w/fvwm-x/blob/4a65f590b5fbc1e92d243cf97edf253eab28b07c/fvwm/style_id_filter.c) conservatively rejects unique IDs within one simplification pass; it retains no rule pointers and moves no update barrier. [style_match.c](https://github.com/gonz0w/fvwm-x/blob/5d063475ba6ccfa61987f31079b42bdd719efb12/fvwm/style_match.c) prepares pointer-free selector metadata at rule publication and borrows window identities for one synchronous lookup; necessary-domain queries share lazy field hashes/lengths and compatibility representatives while preserving final matching and ordered precedence |
| Image acquisition / decode | [PictureImageLoader.c](../libs/PictureImageLoader.c), [image_input.c](../libs/image_input.c), [image_decompress.c](../libs/image_decompress.c), [image_xbm.c](../libs/image_xbm.c), [image_svg_client.c](../libs/image_svg_client.c), [fvwm-image-svg-worker.c](../bin/fvwm-image-svg-worker.c), [image_svg_protocol.c](../libs/image_svg_protocol.c) | Opened-file snapshots, checked XBM parsing, finite literal process transport and verified resource-limited SVG rendering; typed results keep X state in the caller, with synchronous module callers and asynchronous core completion; [picture_async.c](../libs/picture_async.c), [picture_argb.c](../libs/picture_argb.c) and [picture_view.c](../libs/picture_view.c) prepare bounded transport, acknowledged X conversion and owner-specific immutable views. Normal-startup core wiring in [image_async.c](../fvwm/image_async.c) and [image_async_wait.c](../fvwm/image_async_wait.c) connects readiness, safe-boundary commits and shutdown; typed per-view owner bindings preserve old images and invalidate cancelled decorations; colorsets and decor scale variants have focused coverage, and normal startup/restart pass focused checks; combined local qualification is accepted |
| X data / properties | [FProperty.c](../libs/FProperty.c), [FSelection.c](../libs/FSelection.c), [icccm2.c](../fvwm/icccm2.c), [ewmh.c](../fvwm/ewmh.c), [XError.c](../libs/XError.c) | Bounded data acquisition, selection lifetime, desktop properties and errors |
| Module transport | [module_list.c](../fvwm/module_list.c), [module_runtime.c](../fvwm/module_runtime.c), [module_queue.c](../fvwm/module_queue.c), [Module.h](../libs/Module.h) | Processes, partial packets, queue ownership, producer ordering and compatibility |
| Status / IPC | [FvwmMFL.c](../modules/FvwmMFL/FvwmMFL.c), [stream.c](../modules/FvwmMFL/stream.c), [request policy](../modules/FvwmMFL/request.c), [core query formatter](../fvwm/query_json.c), [state-X fixture](../tests/live_x/mfl_core_state_live.py), [core-X fixture](../tests/live_x/mfl_request_live.py), [settings-provider fixture](../tests/live_x/mfl_settings_live.py) | Private bounded event/request transport: MFL owns connection/capabilities, the core owns desktop/stream position and directed topology/workspace/binding observations (global, regular/fallback menus, normalized placement masks and modifier/context policy), plus bounded published colorset pages/exact IDs/summary with independent owner generations and preparation state. Appearance configuration hooks share a conservative revision across style/decor/menu/default-font/colorset owners; bounded menu/decor pages expose configured policy without window/face state or conditional application, and FvwmDesktop owns registry/settings generations and persistence. Typed settings requests/results cross existing directed module messages as bounded literal data; partial assemblies expire/cancel with their slot. Provider watches keep one bounded connection-local filter and forward identity/generation invalidations without a second settings cache; conditional queries stay with the preference owner. Its existing worker pool also owns bounded typed catalog/status operations, independent generations/results, cancellation by output discard and finite reconnect retention; MFL forwards coalesced identity/state invalidations without owning job execution. Local pending snapshots expose only waiting state; cancellation never retracts submitted actions. Non-publishing candidate validation shares the desktop owner's commit preparation. Read-only admission, serials and an incarnation cookie isolate stale work. Preference commit and command submission are distinct from core application/asynchronous completion; no second state authority or X11 sandbox claim. |
| Runtime opacity recovery | [appearance_preview.c](../fvwm/appearance_preview.c), [preview_policy.c](../fvwm/preview_policy.c), [menu scalar owner](../fvwm/menustyle.c), [actual-X probe](../tests/live_x/mfl_preview_live.py) | One fixed core lease captures/applies the exact menu-opacity scalar atomically under appearance freshness. Core service deadlines and module retirement recover only on matching generations; no borrowed menu pointer survives a command. MFL client views isolate disconnect/late controls. No saved preferences or general appearance transaction. |
| Script geometry / layout | [script_geometry.c](../modules/FvwmScript/script_geometry.c), [Tools.c](../modules/FvwmScript/Widgets/Tools.c), [font_geometry.h](../libs/font_geometry.h), [Fft.c](../libs/Fft.c), [Flocale.c](../libs/Flocale.c) | Pure X11 range/layout policy, prepared widget font ownership, checked popup placement and wide text metrics; runtime refusal preserves prior state, while broader resource budgets remain separate |
| Shared shadow rendering | [font_shadow.c](../libs/font_shadow.c), [Flocale.c](../libs/Flocale.c), [add_window.c](../fvwm/add_window.c), [Pager](../modules/FvwmPager/init_pager.c), [Form](../modules/FvwmForm/FvwmForm.c) | Checked finite per-draw positions, direct legacy-order Center border iteration and prepare-before-release font ownership; broader font/cache/glyph budgets remain open |
| Installed self-worker paths | [runtimepath](../bin/FvwmPrompt/internal/runtimepath), [resident entry](../bin/FvwmPrompt/cmd/fvwmdesktop), [menu](../bin/FvwmPrompt/cmd/fvwmmenudesktop), [browser](../bin/FvwmPrompt/cmd/fvwmfilebrowser), [directory reuse](../bin/FvwmPrompt/cmd/fvwmmenudirectory) | Prefer the OS image path; when procfs is absent, resolve only an explicitly absolute invocation and its symlinks. Unknown relative locations fail rather than inventing a worker path. No new dependency or configuration syntax. |
| Resident desktop controller | [resident package](../bin/FvwmPrompt/internal/desktopcontroller), [installed entry](../bin/FvwmPrompt/cmd/fvwmdesktop), [native capture helper](../bin/desktop_capture.c), [independent packet peers](../tests/test_desktop_worker.py) | One event loop owns typed settings/editor/core-preview state and native module framing. Private commits use existing vendored Unix syscall wrappers for anchored openat/flock/fsync/renameat on Linux/BSD; no payload or dependency version is added. Acquisition/capture children have bounded duplex I/O and process groups signalled before leader reaping. Queued/running operation cancellation remains distinct from rollback. Power uses a separate job identity from audio; one detached locker has one buffered completion consumed by the event loop. Its lifetime is observed without claiming lock readiness or killing it across controller/WM retirement. |
| Native desktop UI | [modules](../modules), [default-config](../default-config), [Go resident](../bin/FvwmPrompt/internal/desktopcontroller), [wallpaper generator](../default-config/generate_wallpapers.py) | Existing modules, the frozen default configuration and portable desktop-service policy; newer artwork generators and desktop consumers retain separate source ownership and qualification |
| Desktop acquisition/actions | [Go worker policy](../bin/FvwmPrompt/internal/desktopworker), [executable owner](../bin/FvwmPrompt/cmd/fvwmdesktopworker), [resident owner](../bin/FvwmPrompt/internal/desktopcontroller), [independent peers](../tests/test_desktop_worker.py) | M2.1 moves application discovery, Linux/BSD metrics, audio and power queries to a bounded nonresident Go process. It reuses Desktop Entry syntax policy and preserves launcher word quoting/literal argv. Strict typed JSON and pre-publication validation separate worker data from FVWM commands. The Go resident retains two active workers/four jobs and native publication; the executable/bridge own process-group retirement and partial-I/O deadlines. M2.4 moves commits, generations, editor/preview coordination and module dispatch together to Go. M2.5 ports capture to C without new libraries. Native screenshot capture uses a separately supervised Xlib/libpng helper; disabled PNG refuses before job admission. The [shared runtime path](../bin/FvwmPrompt/internal/runtimepath) resolves successful OS invocation aliases and absolute fallbacks before adjacent helper lookup. [Platform handoff](verification/VERIFICATION-2026-10-24-M2-PLATFORM-HANDOFF.md). |
| Configuration checking | [Go source walker](../bin/FvwmPrompt/internal/desktopcheck), [worker](../bin/FvwmPrompt/internal/desktopworker), [resident report consumer](../bin/FvwmPrompt/internal/desktopcontroller) | Read-only literal Read/settings analysis, source hashes and include locations, inode-cycle detection and finite byte/line/file/depth/diagnostic/time limits. Paths preserve filesystem symlink/parent traversal. Go directly reuses the settings registry; resident code validates the entire result before native report publication. No configuration execution, preference commit or generation/preview authority moves. |
| Desktop settings | [Go policy and UI plans](../bin/FvwmPrompt/internal/desktopsettings), [resident owner](../bin/FvwmPrompt/internal/desktopcontroller), [policy tests](../bin/FvwmPrompt/internal/desktopcontroller), [actual-X consumer](../tests/live_x/desktop_live.py) | M2.2 moves the CFG-01 registry, strict types/schema migration, pure candidates/query metadata and settings/appearance/schedule/preview Form plans to Go. The M2.4 Go resident owns direct file commits, saved/effective generations, module publication and editor/preview lifetimes; the Go policy cannot read/write files or run commands. Complete bounded UI plans are validated before publication, and failed editor construction preserves the old draft. Policy calls are direct Go imports; no transitional scalar cache, policy subprocess or second mutable settings authority remains. One bounded generation-bound draft edits saved values; scoped Defaults/Cancel never publish, and accepted changes/reload/restart retire stale consumers. Private schema-versioned storage serializes writers without waiting, refuses stale snapshots, and commits before in-memory/application changes. Module overrides retain precedence; saved-preference preview stays scoped to native launcher opacity. The launcher-opacity preference adds bounded integer metadata and schema-1 to schema-2 data migration. Its native consumer confirms the existing core lease before a revision-checked scoped file save, without theme replay. One captured editor draft transfers/resumes under fresh tokens; an independent native timeout retires orphan preview Forms. Generic IPC saved-preview stays unavailable. |
| Build / verification | [meson.build](../meson.build), [Go compiler policy](../tools/maintenance/go_version.py), [tests](../tests), [maintenance tools](../tools/maintenance/README.md) | Feature detection, targets, installed files and reproducible checks. Required Go checks the actual local compiler against the approved stable patch baseline; it never fetches a toolchain. The explicit BSD verification recipe bootstraps a fixed SDK privately. |
| Configuration checking | [resident report UI](../bin/FvwmPrompt/internal/desktopcontroller), [policy tests](../bin/FvwmPrompt/internal/desktopcontroller), [native report fixture](../tests/live_x/desktop_live.py) | CFG-02 first subset reuses the preference registry in a read-only bounded source walker. Continued physical lines, literal Read search/cycles and include locations remain operation-local. Unsupported/dynamic commands stay unchecked; nothing is evaluated. Offline text/JSON and native paged reports expose observed source state; core runtime diagnostics retain their own scope. |

Status transport now has a dedicated owner in
[status_pipe.c](../fvwm/status_pipe.c): newly created FIFO identity, private
nonblocking descriptor and one bounded pending record. `builtins.c` owns
snapshot dirtiness and environment publication; the existing shared readiness
snapshot resumes output without retaining window pointers. Stop/restart cleanup
retires the matching FIFO. Logging keeps one stream owner in
[log.c](../libs/log.c), commits replacements after construction, preserves
intentional module inheritance and retires failed diagnostic sinks. Runtime
qualification for these four repairs passes the [stream checkpoint](verification/VERIFICATION-2026-10-21-STREAM-OWNERSHIP.md).
The [input/ownership checkpoint](verification/VERIFICATION-2026-10-21-MODULE-INPUT-OWNERSHIP.md) snapshots the fork-time descriptor and
adopts inherited metadata at most once, including disabled/failed adoption.
Core log toggles cannot retarget another process or re-adopt its reused fd.
Module readers terminate malformed framing/truncated EOF instead of scanning
for a new header; valid partial/EAGAIN/EINTR continuations remain. Script
SendString uses the checked parser's borrowed remainder without allocating
discarded skip tokens. Root Send_ConfigInfo checks its module recipient before
module-only senders. Six-target qualification and affected final context checks
pass; the selected already-bounded input helpers retain their exact policies.

Function-owned selection/classification waits now use the existing ID-only
interaction cancellation policy. Target lifecycle events remain queued until
unwind; shutdown releases grabs and skips pending conditional phases. A nested
Wait can retire a retained target before classification resumes, so that record
is checked as well. Six-target compiler/install/default-X qualification passes, with scoped
guest sanitizers and exact entry-point dispositions. [Report](verification/VERIFICATION-2026-10-21-FUNCTION-LIFETIMES.md).

The optional browser's [directory observer](../bin/FvwmPrompt/cmd/fvwmfilebrowser/watch.go)
owns one process slot and one coalesced invalidation, separately from the existing
enumeration/action worker. [Private observer I/O](../bin/FvwmPrompt/cmd/fvwmfilebrowser/watch_worker.go)
contains filesystem calls and finite leases; platform files implement Linux
inotify and BSD kqueue. Observer events never become authoritative entries or
actions: the existing worker/snapshot/native publication path performs refresh.
Manual fallback, supersession and reap ownership follow the
[watch contract](decisions/BROWSER-DIRECTORY-WATCH-2026-10-23.md).

## Boundaries to strengthen within owning fixes

1. Resolve window identity and lifetime through existing ownership mechanisms;
   check client versus frame XIDs and revalidate after destruction boundaries.
2. Keep pure geometry/stack/style policy testable without an X server. Prefer
   typed operation-local state and explicit results over typeless scratch fields.
3. Identify reply-producing X reads and buffered writes. Preserve required
   synchronization/error barriers and avoid hidden blocking work in policy.
4. Share readiness, deadlines and lifecycle handling across normal dispatch and
   interaction waits. Preserve producer continuations and partial packets.

These are incremental CLEAN-01 principles. They do not authorize threading the
core, replacing the command engine, changing the module ABI, migrating all Xlib
calls or performing a mechanical subsystem rewrite.

For the concrete performance findings and dependency order, see the
[optimization roadmap](OPTIMIZATION-ROADMAP-2026-10-05.md). For maintained
constraints and conventions, see [DEVELOPERS.md](DEVELOPERS.md).

`libs/Picture.c::PCacheFvwmPictureAtScale` retains pathname resolution and
per-matching-entry file-stamp validation. Variant attributes, scale, target size
and generation now reject incompatible entries before stamp reads. Reference
counts and immutable backing/view ownership remain with the same cache/IMAGE-01
owners. Controlled lookup and private-X reload/retained-pixel checks are in the
[CACHE-01 qualified report](verification/VERIFICATION-2026-10-14-PICTURE-CACHE.md);
broader cache profiling remains open.

The IMAGE-01 owner continuation separates pending file-icon requests from their
immutable published pixels, and lets XorGC borrow an acknowledged root-visual
picture without an extra pixmap copy. Cursor preparation uses the same scheduler:
bounded premultiplied ARGB32 upload (or the approved bitmap fallback), acknowledged
creation and one handle transfer from an uncached ready lease to its cursor slot.
The [cursor fallback decision](decisions/CURSOR-CORE-FALLBACK-PROPOSAL.md) is
approved. Normal startup enables these owners before configuration. Combined
local qualification is accepted in the image activation report.

Colorset shape masks also borrow retained immutable picture backing. Replacements
enter the existing delayed colorset cleanup as complete backing references, so
shared masks and previous module packets remain valid. Deferred shapes use the
loader acknowledgement directly; ordinary non-effect publication keeps its barrier.
File-backed color pictures now retain bounded source-RGB summaries for Average,
shared across ordinary/deferred loading and immutable views. Derived
background/tint preparation now uses the shared acknowledged effect pipeline;
see the [shape/average report](verification/VERIFICATION-2026-10-06-COLORSET-SHAPE.md).

Colorset image/shape extents now retain 16-bit X11 dimensions across the unchanged
20-field text packet. LoadColorset checks numeric widths and extents before
allocation/mutation; [focused packet evidence](verification/VERIFICATION-2026-10-06-COLORSET-PACKET.md).
The [effect preparation contract](decisions/COLORSET-EFFECTS-PROPOSAL.md) is
implemented in the normal-startup core adapter. [image_effect_x.c](../libs/image_effect_x.c)
queries retained X resources in the existing private worker; checked RGB16
records feed bounded parent upload in [picture_argb.c](../libs/picture_argb.c).
[picture_color.c](../libs/picture_color.c) retains exact allocations and
[picture_async.c](../libs/picture_async.c) owns the one active job and X
acknowledgement. Colorset revisions stage source/shape/effects and scalar state,
retain cancelled binding contexts across commit batches, then move acknowledged
state to the published record. Terminal core turns retain effect reader leases
through actual reap before X shutdown. Hardware arithmetic remains distinct
from software rounding. [Focused effects evidence](verification/VERIFICATION-2026-10-06-COLORSET-EFFECTS.md)
passes Linux normal/software/SVG-disabled/scoped sanitizer and early native BSD
checks; production activation is implemented with eleven focused Linux
startup/owner checks passing, and combined local qualification is accepted.

The FIFO/output correctness batch adds explicit `fqueue_take_first` ownership
transfer for unlocked drains; recursive callback cleanup stays in fqueue.
Sorted scheduler insertion also uses the existing queue tail for O(1) unlocked
ordered/equal appends. Comparators define a stable order without mutating the
queue; locked callbacks retain the original traversal and deferred cleanup.
See the [focused insertion report](verification/VERIFICATION-2026-10-08-QUEUE-INSERTION.md).
The AutoDetect ConfigureRequest path consumes only adjacent size-only requests
through `FMergeQueuedResizeRequests`, bounded by the existing 32-event budget.
Any other event ends the snapshot batch. Raw queue operations retain cached
input state; normal dispatch normalizes merged dimensions before geometry work.
The [performance report](verification/VERIFICATION-2026-10-08-CORE-PERFORMANCE.md)
records focused coverage. The [combined qualification](verification/VERIFICATION-2026-10-11-CORE-PERFORMANCE.md)
accepts the whole frozen checkpoint across all six targets.
`event_loop_policy` owns finite asynchronous output bytes/packets/attempts.
`module_list` retains partial offsets and writable continuation; synchronous
lock-on-send keeps its existing completion barrier. `stack::new_layer` publishes
state and highlight identity for each affected transient. Combined frozen
qualification is locally accepted; native release CI remains separate.

FvwmButtons owns opt-in keyboard focus and held-key state in
`modules/FvwmButtons/keyboard.c`, separately from pointer hover/press records.
Its pure `buttons_navigation` policy ranks actual laid-out rectangles. Its
`buttons_updates` parser owns bounded request tokens; `dynamic.c` prepares
all resources, commits a whole `ChangeButtons` group, then reconciles focus
and repaints the affected controls. Refusal releases prepared resources without
mutating buttons. Legacy `ChangeButton` retains incremental application.
Existing button-tree iteration supplies traversal order. Button records live for the
module lifetime, and dynamic state updates preserve their identity. The shared
`ButtonPressProcess` activation boundary refuses disabled actions; managed
window commands still go through SendText and the core command engine. State
palettes are selected by `buttons_state_colorset` for foreground and background.
Both commands preserve button IDs, actions and layout.

The separate DATA-01 tree-query follow-up initializes `GetEqualSizeChildren`
failure output and validates the parent before acquiring the owned Xlib child
list. FvwmButtons remains its actual consumer. Focused policy and scoped
sanitizer/native BSD checks pass; combined qualification is locally accepted. OUTPUT-02
has an approved monotonic synchronous handshake deadline spanning output and
unlock. The command frame pins its module across input callbacks and death-row
retirement; checked nonblocking reads preserve partial framing while testing the
same deadline before retries. Focused Linux/BSD and scoped sanitizer checks pass;
combined frozen qualification is locally accepted; native release CI remains
separate.

The next STACK-01 batch introduces `add_window::find_managed_client`: existing
client contexts plus XID/membership validation, with a list fallback during
construction and XID reuse. Transient parent lookup reuses it; five identity
groups and actual stack/destruction scoped checks pass. Full combined local coverage passes in the startup/identity checkpoint;
the measured reverse-chain closure algorithm remains pending.

OUTPUT-03's approved startup frame captures ModuleTimeout (30-second default) or
its positive Timeout override. It reuses the monotonic deadline, checked reader
and asynchronous output budgets while retaining the live producer after barrier
timeout. Readiness includes X Escape with a 100-ms maximum; ordinary X events
stay queued and inline command order remains intact. The full frame holds a
module pin. A last read that exhausts the deadline retains the complete transport
frame for later asynchronous handoff rather than discarding its command.
Focused Linux/BSD ARM64 and scoped sanitizer checks pass; combined frozen
identity/startup checkpoint is locally qualified; native release CI remains open.

LIB-01 retains cJSON creation/serialization ownership, with a reviewed 1.7.19
vendor baseline and small downstream numeric/UTF-8 print fixes.
[json_print_policy.h](../libs/json_print_policy.h) owns operation-local UTF-8
sequence validation and checked printer sums. Only JSON presentation converts
invalid bytes; original FVWM/native bytes retain their owners. Focused pure and
real MFL checks pass; [combined qualification](verification/VERIFICATION-2026-10-07-JSON-BOUNDARY.md)
is locally accepted. No production parser or dependency replacement was introduced.

The focused XIO-01 atom work uses [atom_batch.c](../libs/atom_batch.c) for finite
independent core/ICCCM/EWMH tables and missing-entry fallback. Each owner publishes
its results only after interning completes; selection and EWMH property ordering
stay with their existing owners. Three batches replace 100 startup single-name
operations. [Focused evidence](verification/VERIFICATION-2026-10-07-ATOM-BATCH.md)
passes; next combined qualification is pending. DRAW-01 has a separate
[expose-service proposal](decisions/EXPOSE-SERVICE-PROPOSAL.md) awaiting the
presentation timing decision; production expose behavior is unchanged.

## Bounded expose servicing

`libs/FEvent.c::FCollectQueuedExposes` collects a 32-record queued snapshot and
restores unrelated records before callbacks. It does not fetch socket input or
advance deferred timestamps. `fvwm/events.c::handle_all_expose` dispatches current
window identities; `flush_accumulate_expose` merges matching damage with the same
finite bound. Remaining exposures stay with normal dispatch. Iconify republishes
WM_STATE when creating/replacing the icon title changes its XID. See the
[atom/expose checkpoint](verification/VERIFICATION-2026-10-07-ATOM-EXPOSE.md).

Stacking parent traversal uses operation-local `fvwm_parent_chain_state` in
`fvwm/parent_chain_policy.h`. Layer resolves ordinary indexed ancestors and uses
the invoked target on repetition. Raise/Lower/RestackTransients propagate a typed
cycle result through parent redirection, unwind before restacking, discard only
scratch branch bonuses and apply the existing direct-target operation. Validation
reuses the detector while keeping its existing root-parent rejection behavior.
No window hint or registry owner is added. `is_above_unmanaged` reads one root
snapshot, scans from its top and stops at the highest eligible override sibling;
it retains current attribute queries for managed frames.

The next STACK-01 arithmetic source widens the existing scratch rank and its
straight-subtree minimum to signed 64-bit storage. It retains historical branch
weights and sort ownership. Maximum-layer insertion uses comparisons without
adding one. The approved relative Layer sum is checked before mutation; retained
rank sums preserve representable values and stop at signed 64-bit endpoints.
The exact 1306-file combined arithmetic checkpoint is locally qualified. See the [arithmetic report](verification/VERIFICATION-2026-10-07-STACK-ARITHMETIC.md).

The subsequent `fvwm/stack_sort.c` owns a pure iterative stable sort of the existing
temporary transient ring. It borrows nodes only for the operation, allocates no
state, invokes no callbacks/X work and restores circular links on return. The
already-sorted fast path leaves all links unchanged. The subsequent 1314-file combined traversal checkpoint is locally qualified,
separate from the arithmetic source; see the [sort report](verification/VERIFICATION-2026-10-07-STACK-SORT.md).

`fvwm/stack_subtree.c` completes only repeated parent-only closure using one typed
operation-local edge/worklist allocation. Existing scratch parent/rank values and
marked seeds remain the state owner; it never changes stack links or invokes X/
callbacks. The normal two-scan path and order-sensitive group policy stay in
stack.c. Fallible allocation leaves state unchanged and resumes the old scan.
See the [traversal report](verification/VERIFICATION-2026-10-07-STACK-TRAVERSAL.md).

`fvwm/stack_parent.c/h` now owns pure parent preparation and operation-local
continuations for Raise/Lower/RestackTransients. Membership comes from the
stack ring, with first-match keys; shallow ancestry is allocation-free, and
deeper ancestry uses one checked key/frame allocation. No X work/callback
occurs during preparation; keys are unused during the subsequent leaf-first
X execution. Finish clears/frees the scoped owner. The stack wrapper owns
current-style unwind and the approved cycle/storage-failure invoked-target
fallback. New-window placement keeps its existing parent follow-up and full
resynchronization. See [parent-continuation verification](verification/VERIFICATION-2026-10-07-PARENT-CONTINUATION.md).

`fvwm/stack_window_list.h` owns pure frame/icon selection, representable Xlib slot counts and capacity-checked packing. Stack operations retain array/X ownership, submit only populated prefixes and derive full resynchronization membership from the stacking ring. No persistent state or second registry is added; checked element allocation preserves the existing unrecoverable-allocation policy. See [initialized-list verification](verification/VERIFICATION-2026-10-07-STACK-WINDOW-LIST.md).

`fvwm/stack_window_buffer.c/h` owns fallible Layer restack capacity before any
planning or publication. Layer lends the array synchronously to the existing
restack consumer and finishes the owner afterward or on empty selection. The
approved failure path preserves current state and permits retry; the 1328-file
combined checkpoint is locally qualified. Other stacking
allocation owners retain their existing policy. See [Layer storage verification](verification/VERIFICATION-2026-10-07-LAYER-STORAGE.md).

The qualified storage batch adds an already-counted-prefix constructor to the same
window-buffer owner. RaiseOverUnmanaged owns and frees its buffer synchronously and
skips only the extra correction on refusal, retaining completed managed Raise.
Layer ring preparation reuses the checked constructor. See [qualified production evidence](verification/VERIFICATION-2026-10-07-OVERRIDE-STORAGE.md).

Ordinary restacks and full new-window resynchronization now borrow the stable
ring through a 64-XID operation-local batch. Follow-on batches retain the last
XID as their unchanged top anchor; no callbacks/event dispatch or heap restack
array is needed. Layer still lends its preflighted owner. The shared bottom-XID
policy excludes suppressed icons consistently with packing. [Batch verification](verification/VERIFICATION-2026-10-07-STACK-BATCH.md) records the qualified 1332-file checkpoint.

Group-aware Iconify/Deiconify uses the same parent worklist when the selected
range has no usable group hint. One pure eligibility predicate owns style and
None/self/root exclusions; any usable group preserves the ordered scan.
[Group/cleanup evidence](verification/VERIFICATION-2026-10-07-STACK-GROUP-CLEANUP.md) is qualified in the 1335-file publication checkpoint.

`fvwm/stack_restack.c/h` owns numeric packet records before synchronous module
callbacks. Nested stacking marks final publication pending; the outer event loop
queues a current-ring repair through the existing output owner without a sync
wait. Ordinary packet synchronization remains intact. No window pointer or
heap snapshot persists across event-loop turns. [Publication evidence](verification/VERIFICATION-2026-10-07-RESTACK-PUBLICATION.md) is locally qualified; later retirement/allocation work has a separate source.

Stacking callbacks may retire a managed record. Local/public continuation now
resolves its captured client and opaque handle through the existing managed-client
lookup. Prepared parent frames supply captured client IDs so the caller resolves
membership before transient checks or child continuation. Retired frames are
skipped without a second registry. [Retirement evidence](verification/VERIFICATION-2026-10-07-STACK-RETIREMENT.md) is locally qualified in the 1339-file lifecycle checkpoint.

Module output packet/queue and deferred configure admission use fallible storage.
Refusal releases the unowned reservation and retires only the producer through
existing death-row cleanup; callback pins retain its prior owned data. The shared
queue reuses one append linker for ordinary and fallible admission, including
locked visibility. See [allocation report](verification/VERIFICATION-2026-10-07-MODULE-ALLOCATION.md); the 1339-file lifecycle checkpoint is locally qualified.

Producer retirement transfers its existing active-list node to death row without
new allocation. Incoming command admission uses the existing error path on
storage refusal, retaining earlier queued-input pins. See the
[combined lifecycle report](verification/VERIFICATION-2026-10-07-MODULE-LIFECYCLE.md); the 1339-file source is locally qualified.

Module iterators hold death-row cleanup until exhaustion or explicit finish.
Retired stores preserve their iteration successor and use a separate retirement
link; traversal skips retired producers and retains surviving delivery through
nested callbacks. Five early-exit callers release the hold. This adds no copied
list or allocation. See [iteration report](verification/VERIFICATION-2026-10-07-MODULE-ITERATION.md), locally qualified in the 1346-file transport checkpoint.

Word module packets include their header inside the existing capacity; delivery
uses the constructed length. Named packets use fixed operation-local word arrays
and scan/copy only the representable name prefix. Existing copied output queues
still own asynchronous delivery. See [packet report](verification/VERIFICATION-2026-10-07-MODULE-PACKETS.md), locally qualified in the 1346-file transport checkpoint.

Configure construction fills the existing `ConfigWinPacket` consumer structure,
initializes its native padding and copies it into aligned operation-local word
storage. A compile-time assertion bounds the complete record plus header. Allowed
actions use the same local snapshot; picture senders share one metadata constructor.
The interim general field packer is retired in the next 1343-file typed source.
See [typed report](verification/VERIFICATION-2026-10-07-MODULE-TYPED.md), outside the
frozen transport checkpoint. The preceding field algorithm/evidence remains in
its [dated report](verification/VERIFICATION-2026-10-07-MODULE-FIELDS.md).

Configure identity now packs the managed record's pointer value, matching the
shipped `ConfigWinPacket` consumer definition. The exact direct/broadcast
consumer-layout regression includes signed geometry, short/raw fields and
initialized padding. See [configure report](verification/VERIFICATION-2026-10-07-MODULE-CONFIGURE.md).

Configure monitor identity copies `RROutput` directly into the native unsigned
consumer field, including the upper 32-bit identifier range. Focused consumer
controls use bounded synthetic metadata; physical assignment of those identifiers
is not claimed. See [monitor-ID report](verification/VERIFICATION-2026-10-07-MODULE-MONITOR-ID.md),
part of the next typed-schema batch.

Before successful module fork, local pipe arrays remain owned by the constructor.
One helper closes/invalidates those arrays on pre-start failure and fork refusal;
successful parent/child transfer keeps the existing record ownership. Actual
command fixtures compare descriptor bitmaps and retry through the core child
reaper. See [constructor report](verification/VERIFICATION-2026-10-07-MODULE-CONSTRUCTOR.md),
part of the next typed-schema batch; other startup allocation paths remain open.

Module construction reserves optional argv storage geometrically with size_t
counts and checked pointer-element capacity, retaining existing parsing/token
ownership and allocation failure policy. System and rectangle implementations
link through the shared library instead of implementation inclusions in core
owners. See [argument/cleanup report](verification/VERIFICATION-2026-10-07-MODULE-ARGUMENTS.md),
part of the next descriptor batch outside frozen typed-schema qualification.

`GetFdWidth` reads the native descriptor limit once, clamps to FD_SETSIZE and
uses that positive finite bound for unavailable/nonpositive queries. Initialization
shares this result; deterministic/native units cover both APIs. See
[width report](verification/VERIFICATION-2026-10-07-FD-WIDTH.md), part of the next
descriptor batch; broader readiness/descriptor ownership remains separate.

Select-based ICE/session constructors share range-checked close-on-exec preparation
before readiness admission; refusal uses the existing owner cleanup. Watched
borrowed connections outside fd_set range are skipped in readiness/dispatch and
remain owned by their original connection/close callback. Generic flag protection
retains its poll-compatible descriptor range. See
[admission/watch report](verification/VERIFICATION-2026-10-07-FD-SELECT.md),
part of the next descriptor batch; forced high live admission remains separate.

ICE setup retry policy is shared in `libs/ice_error_policy.h`: reinstalling the
own handler retains its previous callback. Core/library setup use that policy
without new global state or changes to authoritative X ownership.

Module-side packet reads use the shared atomic termination wrapper in
`libs/fvwmsignal.c`; nonterminal interruption retains byte progress and declared
word counts are checked before body-byte arithmetic. The core incremental receiver
remains separate. Terminal session-authentication cleanup clears its owned entry
and path pointers. OpenBSD nested verification uses explicit server-locked Xvfb
displays with child-ownership/readiness checks, bounded candidates and independent
teardown; other platforms retain displayfd allocation.

Native module producers use checked stack command frames in `libs/Module.c`;
partial writes retain progress and incomplete output requests termination before
subsequent frames. `libs/fvwmsignal.c` arms termination jumps only across read,
write and readiness syscalls. These wrappers preserve blocking module transport;
they are separate from the core's incremental receiver. FvwmRearrange returns
from cancelled transport to normal resource cleanup rather than freeing from a
signal handler. [Focused scope](verification/VERIFICATION-2026-10-07-MODULE-WRITES.md).

FvwmBacker also uses common termination for SIGPIPE/SIGTERM/SIGINT and returns
from configuration/packet loops in normal flow. EOF requests loop termination.
Its root-pixmap/background ownership is unchanged by this step.
[Focused scope](verification/VERIFICATION-2026-10-07-BACKER-SIGNALS.md).

`GetConfigLine` verifies text and its terminator inside the declared packet before
returning a borrowed static view. `SendFvwmPipe` sends literal-comma byte slices
through the common checked writer, and `InitGetConfigLine` uses bounded stack
storage under the existing command cap. [Focused text scope](verification/VERIFICATION-2026-10-07-MODULE-TEXT.md).

The shared Buttons/IconMan `module_expand_action` bounds literal/substituted
output and the terminator against the existing command cap. Geometry sums and
reverse screen-edge arithmetic use 64-bit values before formatting. Tokens and
ordinary admitted output retain their prior behavior.
[Focused/combined scope](verification/VERIFICATION-2026-10-07-MODULE-ACTIONS.md).

FvwmEvent uses the common packet reader/termination flag instead of a second
framing loop. Dispatch checks required body arguments and terminated tail views;
builtin startup/shutdown carry an explicit empty tail. Ident/Event configuration
collection advances through empty/short lines. Auto/Event's unused quit functions
are removed. [Focused scope](verification/VERIFICATION-2026-10-07-MODULE-EVENTS.md).

FvwmAuto uses one poll descriptor through the existing termination wrapper.
Its millisecond delay is checked before int narrowing; short window packets are
unlocked/discarded before accessing the body. This module-only path retains
blocking packet reads and configured action order; it adds no core event-loop
work. [Focused scope](verification/VERIFICATION-2026-10-08-AUTO-READINESS.md).

FvwmIdent validates native configure records and bounded string/property bodies
before dispatch in both window-list and popup owners. Popup readiness shares the
existing poll/termination wrapper for its module pipe and X connection, retaining
X-event-first processing and blocking packet reads. EOF/errors and cancellation
return through the outer loop. [Focused scope](verification/VERIFICATION-2026-10-08-IDENT-TRANSPORT.md).

## Module popup readiness — 2026-10-08

Auto's shared poll/termination contract is locally qualified in consumer1361.
Ident and Pager now share cancellation-aware two-descriptor readiness while
preserving their X-event/packet order; their combined focused checks pass. Pager
also advances empty configuration lines and drops its disabled quit body.
The subsequent Buttons (borrowed session readiness) and IconMan (tooltip
timing) contracts pass focused checks through shared poll readiness. Core readiness
consolidation and nonblocking ordinary module transport are not complete.
See the [popup report](verification/VERIFICATION-2026-10-08-POPUP-READINESS.md).

IconMan now preserves FTipsCheck deadlines through common two-descriptor poll
and observes termination in X drains; focused normal/scoped/native checks pass.
Buttons adds bounded borrowed-session poll snapshots while retaining fsm admission
and callback ownership plus the 10-ms pending fallback. Final normal/scoped/native
controls pass. Script now adopts the same borrowed adapter and common poll owner
with initialized monotonic periodic timing and idle session servicing. These
changes are outside the frozen popup checkpoint; see the [Buttons report](verification/VERIFICATION-2026-10-08-BUTTONS-READINESS.md).

Script selection-message shutdown waits also use poll, retaining the existing
miss count and final two-second sleep. SwallowExec retains its owned placeholder
separately from a matched external client, preventing empty-widget teardown from
closing Script's own connection. Focused controls require real reparenting/client
closure and authenticated idle XSMP registration. [Script contract](verification/VERIFICATION-2026-10-08-SCRIPT-READINESS.md).


Form readiness: modules/FvwmForm/FvwmForm.c owns a two-descriptor cancellation-aware
poll loop, preserving queued-X priority and pipe-before-new-X order. The already
required libevent timer base/event have explicit cleanup and nonblocking service;
active countdowns cap input readiness waits at 100 ms. Shared draw-table strings
are borrowed from their item owners; timeout drawing borrows its temporary output
only for the draw call and clears the pointer before freeing the buffer.
[Focused evidence](verification/VERIFICATION-2026-10-08-FORM-READINESS.md).


Form input preparation: modules/FvwmForm/form_input.c owns declared body admission,
empty-line trimming, caller-owned quoted/token copies and borrowed timeout argument
views. FvwmForm validates before dispatch and creates a timeout item only after all
required fields are prepared. Refused input returns to ordinary timer/readiness
work. Translation stays at the existing Form wrapper boundary.
[Focused evidence](verification/VERIFICATION-2026-10-08-FORM-INPUT.md).


The live-X HarnessRuntime owns SIGTERM only for its main-thread lifetime, unwinds
existing finite child cleanup and restores the prior handler after its last owned
runtime closes. This testing owner is outside the authoritative core/X state.
[Focused contract](verification/VERIFICATION-2026-10-08-HARNESS-TEARDOWN.md).


Form button preparation and argument cursors share the pure form_input owner. Label/key views refer to source boundaries before translation; text copies allocate for their consumed field and remain caller-owned. Input/Choice default values and existing command expansion are preserved. [Focused scope](verification/VERIFICATION-2026-10-08-FORM-ARGUMENTS.md).


Form UseData prepares required views before replacing owned filename/prefix strings. Read requests use checked formatted allocation and the existing module writer; data variables are released on empty-name return. This remains the existing core configuration read route. [Focused scope](verification/VERIFICATION-2026-10-08-FORM-DATA.md).

Form line/selection/command arrays share checked geometric capacity in form_input.c and one private allocation owner in FvwmForm.c. Legacy int counts and line spare storage remain; element count/size are passed once and capacity publishes after allocation. [Focused scope](verification/VERIFICATION-2026-10-08-FORM-ARRAYS.md).


Form Input borrows parsed name/initial views, prepares its draw table and computes wide X11 extents before publishing a field. A refused width or extent leaves Input ring membership untouched; font tables remain reusable. This is field preparation, not full layout validation. [Evidence](verification/VERIFICATION-2026-10-08-FORM-GEOMETRY.md).

Form command construction matches normalized source name views, uses checked character capacity and returns NULL for incomplete/unsupported lengths or cancellation. Button commands skip refusal; timeout publication withdraws only after command preparation succeeds. Stored substitution bytes remain unchanged. The retained recursive conditional and shell/transport owners are separate. [Evidence](verification/VERIFICATION-2026-10-08-FORM-COMMAND.md).

Form recall/editing now shares per-field value replacement in FvwmForm.c and
bounded history/insertion policy in form_input.c. ReadXServer.c owns the finite
X dispatch/coalescing pass; MainLoop owns queued continuations, libevent and pipe
readiness. The service story remains under focused qualification; it is separate
from the frozen processing acceptance source.

libs/module_packet_reader.c provides explicit caller-owned runtime packet state.
Form and Script initialize it after blocking startup configuration; their runtime
loops preserve partial pipe progress and continue their other readiness owners.
The classic Module.c reader remains the startup/other-consumer API. Packet limits,
native word framing, cancellation and descriptor ownership remain explicit.

Buttons and Pager now initialize their own module_packet_reader after startup
configuration, joining Form/Script runtime ownership. A MORE result returns to
X readiness and Buttons session processing, retaining partial packet state.
Packet dispatch and descriptor teardown remain with the existing module owners.
Focused sourcec249 qualifies four consumers; startup acquisition and remaining
IconMan/Ident runtime readers are separate follow-up work.
[Evidence](verification/VERIFICATION-2026-10-08-RUNTIME-READER.md).

IconMan passes its main-loop-local module_packet_reader to ReadFvwmPipe. Ident
retains a file-private reader from post-configuration window-list acquisition
through popup/reselection, using cancellation-aware readiness before incomplete
input is retried. IconMan drawing owns an empty copy while display metadata is
absent and applies later text normally. This source d21f is focused-qualified
outside frozen c249 acceptance.
[Scope](verification/VERIFICATION-2026-10-08-REMAINING-UI-READER.md).

Module X batch policy is centralized in `libs/module_x_service.c/h` at32 events
or5 ms. Form keeps compatibility wrappers; IconMan/Ident/Buttons/Pager use
the same bounded policy. Coalesced events count toward the same batch; one callback is not
preemptible. See the [foundation scope](verification/VERIFICATION-2026-10-08-MODULE-X-SERVICE.md).

The locally qualified cf44 module batch applies the canonical X pass policy to
IconMan/Ident operation-local batches and Buttons/Pager file-private pass state.
Their adjacent same-window coalescers share the count/time budget and retain
queue order. Ready pipe/session work runs between batches; queued X continuation
uses zero-time readiness. Pager's generic purge no longer scans the full queue
or waits for a server reply. Nested drag/lifecycle waits remain separate.

Pager's `pager_packet.c/h` admission is separate from native framing: it validates
only fields consumed by `messages.c`, preserving unused type handling and older
Deiconify variants. Restack vectors are bounded by the native packet contract.
Unused monitor-focus subscription/callback is removed. `pager_geometry.c/h`
prepares DesktopSize before publication; all five Pager stories are focused-qualified.
Monitor-line and drag/lifetime work are separate. See the
[owning report](verification/VERIFICATION-2026-10-08-PAGER-PACKET.md).

Pager geometry messages now prepare checked native integers and derived desktop
products in modules/FvwmPager/pager_geometry.c before messages.c publishes state.
Monitor names are operation-owned and freed on every exit; fpmonitor.c tolerates
absent highlight vectors and empty monitor queues. Active nested drag waits remain
separate. [Owner evidence](verification/VERIFICATION-2026-10-08-PAGER-MONITOR.md).

Buttons/IconMan runtime consumers use module-owned packet admission helpers before
dispatch. libs/module_packet_reader.c supplies only the reusable bounded-text
predicate; the owning module retains its schema and state. Refused synchronous
IconMan iconify/deiconify packets still acknowledge the producer. [Verification](verification/VERIFICATION-2026-10-08-UI-PACKET.md).

Pager, Script and Ident now use the shared declared-text predicate too. Schemas,
conditionals and runtime lifetimes stay module-owned; the reusable library helper
only bounds the declared body and text offset before searching for a terminator.
[Canonical owner evidence](verification/VERIFICATION-2026-10-08-UI-TEXT.md).

Native Monitor text preparation and checked decimal parsing now belong to
`libs/module_monitor.c/h`. The core formats complete records before SendName;
Pager retains derived desktop policy, and Rearrange prepares checked inset geometry
before publishing its area/desk. Disabled Pager selections use active fallbacks.
[Development evidence](verification/VERIFICATION-2026-10-08-MODULE-MONITOR.md).

Pager monitor hit testing uses wide endpoints and excludes disabled regions.
IconMan numeric/property fields and Pager desk/style selectors use checked
conversion before publication. Pager style setters accept local size_t/offsetof
values rather than null-member pointer expressions.
[Owner evidence](verification/VERIFICATION-2026-10-08-UI-NUMERIC.md).

Pager startup owns its finite displayed-desk allowance in `pager_desk_range.c`; the same helper provides checked relative desk indexes. `fpmonitor_new` supplies positive single-page geometry before startup broadcasts. Initial style lookup uses absolute selected desk IDs. [Current verification](verification/VERIFICATION-2026-10-08-PAGER-STARTUP.md).

Shared numeric configuration conversion remains owned by `libs/Parse.c`. Its
private checked prefix parser serves integer/suffix/base, rectangle and percentage
entry points; published values fit C int, and rectangle outputs publish together.
[Current evidence](verification/VERIFICATION-2026-10-08-PARSE-INTEGER.md).

Pager grid/scale preparation lives in `modules/FvwmPager/pager_geometry.c`;
`pager_shape.c` owns displayed-desk X shape requests. Shape and reconfigure
traversals follow displayed desks, avoiding work proportional to empty grid cells.
See [focused Pager evidence](verification/VERIFICATION-2026-10-08-PAGER-GRID.md).

Pager automatic/window/cell/icon/viewport dimensions use the pure preparation
owner in `pager_geometry.c`. The module publishes bounded derived X request
geometry, preserving ordinary aspect and scale calculations. Window projection
and nested interaction lifetime work remain distinct open owners.

`pager_projection_axis` in Pager's geometry policy prepares forward and inverse
window projection. The X owner publishes each rectangle after both axes prepare;
configuration owns the content minimum separately from bounded render dimensions.
The static-library SuffixToPercentValue API was removed after no shipped callers
were found. See [projection evidence](verification/VERIFICATION-2026-10-08-PAGER-PROJECTION.md).

Modern FvwmScript parser generation is owned by
[generate.py](../tools/maintenance/generate.py). The existing
[fvwmscript-syntax.sh](../modules/FvwmScript/fvwmscript-syntax.sh) entry point is a
portable wrapper that delegates to atomic fresh-output generation. Version and
input/output identities are recorded before publication; generated implementation
files require `config.h` first. Normal builds compile the committed generated C
files. Grammar and scanner token behavior remain owned by
[script.y](../modules/FvwmScript/script.y) and
[scanner.l](../modules/FvwmScript/scanner.l). See the
[adoption checkpoint](verification/VERIFICATION-2026-10-11-PARSER-ADOPTION.md) for
focused checks, production qualification status and retained timing observations.

Translation maintenance uses `tools/maintenance/translations.py`, `po/LINGUAS`
and `po/POTFILES`. Root Meson computes `nls_enabled` from the selected gettext
library and msgfmt, and creates `po/` build/install targets only when enabled.
PO sources are authoritative; committed GMO files and the legacy helper are
retired after the [I18N-01 checkpoint](verification/VERIFICATION-2026-10-11-I18N-ADOPTION.md).

Script modal readiness now borrows MainLoop's operation-owned packet reader,
periodic clock and session continuation. Non-input X events reuse the normal
single-threaded dispatcher; an executing script block or servicing callback
cannot recursively read the same packet storage. A command changing the active
widget cancels cached interaction state before reuse. Font/title replacements
advance a per-widget revision, so pointer/cache address reuse cannot establish
unchanged lifetime. Optional module selection
service preserves the existing requestor/deadline and event queue.
[Font/modal checkpoint](verification/VERIFICATION-2026-10-11-FONT-MODAL.md).

## Native verification and protocol inventory — 2026-10-12

CI-01's `tools/maintenance/ci_assets.py` owns checked materialization of the
fixed release-asset lock into private independent files. `common.fresh_output`
owns complete-directory publication; one acquisition subprocess owns the total
download deadline. Cache bytes are opened relative to a pinned private directory
and hashed from the descriptor copied to output. This helper does not execute downloaded inputs. The approved
`tools/maintenance/qemu_bsd.py` owns four fixed BSD profiles, bounded private
source/script snapshots, fresh disk expansion, loopback SSH, job-key rotation,
scoped evidence and process/cancellation cleanup. Validated archive modes are
preserved under the private workspace; live phase and final failure/cleanup
status snapshots publish atomically. `BOOT-PINS-2026-10-13.json`
adds independently verified published bootstrap keys and ARM firmware. OpenBSD
job storage uses `/home`; only the owned disposable image is changed. Its
inactive candidate replaces the downloaded AnyVM runtime and mutable prepared
cache path. Optional `reboot_prepared_guest` requires a changed, validated BSD
boot identity before accepting updated guest preparation. Default-server/package
qualification remains profile-specific; review runs locally with Actions
disabled. See [CI upkeep](ci/README.md).

CI-01’s `tools/maintenance/ci_png.py` owns the approved fixed PNG tar payload,
private native Linux/BSD build/upstream tests and core/SVG transitive loader receipts.
Built-only and installed checks have independent immutable evidence directories;
explicit repeated observations require a fresh output.
`ci_assets.py` acquires the pinned release; `qemu_bsd.py --png-bootstrap`
transfers its verified payload. CI recipes apply the environment only within
the disposable job. System package installation and production policy remain
separate from the private prefix.

`tests/run_native_gate.sh` owns shared Linux/BSD setup, test and staged-install
behavior; the platform wrappers retain their entry points. Linux sanitizer mode
uses the same feature/dependency assertions and tests instrumented binaries.
`tests/test_native_gate.py` checks orchestration failure/path/profile contracts.
Its optional `--go-clients` profile delegates to `tests/run_go_clients.sh` for
bounded transport/client tests, seven tagged client/utility stages and Linux actual-client
race checks. Meson producers and tests share one cache inside their own build
directory; a non-installed build target also compiles test-only dependencies
before the runtime gate. Vendored inputs and offline toolchain policy remain
unchanged.
`tests/run_bsd_default_x.py` owns an isolated authenticated default
Xorg through the existing bounded harness; `_x11` receives authority access on
OpenBSD. It checks a cookie-less client's refusal and reuses installed
Command/Prompt fixtures. Server-only repetition starts no WM/client fixture;
each attempt owns its result, phase, server/module/patch identities and final
teardown logs. A stale vendor log cannot stand in for a failed new start; any
failed attempt fails the aggregate and cancellation stops further attempts. Its `--xlock-only` and `--directory-only` modes qualify the installed utilities
through real PipeRead/menu map/dismiss checks without executing lock actions.
Prompt fixture readiness requires a successful MFL
connection; the shared Connection closes failed construction. No core or native
module state moves into this harness. See the [client-profile/default-X report](verification/VERIFICATION-2026-10-12-CI-PROFILES.md).

`tests/live_x/script_text_list_live.py` uses a same-X-stream Script key binding
to acknowledge earlier input before querying over MFL. It adds no production
dispatch state. `ewmh_inventory_live.py` is an independent publication observer,
with explicit required-clause findings rather than blanket conformance status.
See the [affected checkpoint](verification/VERIFICATION-2026-10-12-COVERAGE-CI.md)
and [32-row reconciliation](verification/RECONCILIATION-2026-10-12-STAGE3.md).

## Local repository review — 2026-10-13

`Makefile` and `tools/maintenance/check_repo.py` own portable foundation checks;
existing native gate/QEMU recipes still own platform builds, X11 and installs.
GitHub Actions is disabled; inherited workflow metadata is archived. The
[GitHub workflow](GITHUB-WORKFLOW.md) and [stable issue index](GITHUB-ROADMAP.md)
mirror tracker execution manually, with source-bound local evidence and signed
PR review. These entry points do not establish unrun runtime/hardware acceptance.

Core readiness now uses one operation-local snapshot in `events.c` for ordinary
and interactive waits, with existing module death-row pins through transport
service. `image_async_wait.c` selects decoder deadlines over native poll arrays.
Session callbacks stay ordinary-only; queue, scheduler and X-event boundaries
retain their owners. See the [qualified core-readiness report](verification/VERIFICATION-2026-10-14-CORE-READINESS.md).

Synchronous module waits in `module_list.c` use `fvwmPoll` for unlock responses,
blocked output and startup. `event_loop_policy.c` owns absolute-deadline to poll
millisecond conversion; owners retain retirement, inline execution and partial
packet contracts. See the [module-wait checkpoint](verification/VERIFICATION-2026-10-15-MODULE-WAITS.md).

## EWMH state traversal ownership

`ewmh.c` owns immutable scoped/ALL atom-ID and name indexes rebuilt with its existing atom
initialization, paired static/dynamic work-area aggregation and immediate root
client-list publication. [Atom index policy](https://github.com/gonz0w/fvwm-x/blob/773958f73893fb8bd3e2084f4d3d881fce7f870a/fvwm/ewmh_atom_index.c)
retains duplicate priority; [client snapshot ownership](https://github.com/gonz0w/fvwm-x/blob/773958f73893fb8bd3e2084f4d3d881fce7f870a/fvwm/ewmh_client_list.c)
keeps only operation-local IDs, with inline storage and checked growth/refusal.
No window references survive these operations, and current query/property
publication barriers remain with their existing owners.
[Checkpoint evidence](verification/VERIFICATION-2026-10-15-EWMH-STATE.md).
Numeric lower bounds retain the first equal ID without a duplicate rewind.
Name indexes borrow static strings/records and retain original table priority;
reentrant interning uses the original tables while both indexes are withdrawn.
The caller-owned interning vector supplies `_NET_SUPPORTED` before returning,
preserving duplicates, failed-intern `None`, format and publication order.
[Atom checkpoint](verification/VERIFICATION-2026-10-19-EWMH-ATOMS.md).

STYLE-01's optional decoded anchor views borrow pattern strings for one
synchronous overlap proof. Head/tail literal contradictions and fixed-length
zero-slack alignment can prove disjointness without changing the matcher,
retained metadata or update ownership. Raw scans are bounded; uncertain larger
patterns retain their records. [Anchor checkpoint](verification/VERIFICATION-2026-10-19-STYLE-ANCHORS.md).

STYLE-01's [prepared matcher](https://github.com/gonz0w/fvwm-x/blob/ee180b0684f74c10984ddec75a43099987211139/fvwm/style_match.c) derives unescaped minimum
lengths, literal suffix spans and single-star classification from immutable
selector strings. [Operation-local subjects](https://github.com/gonz0w/fvwm-x/blob/ee180b0684f74c10984ddec75a43099987211139/fvwm/style_match.h) lazily cache
byte lengths only when needed; fresh subjects reset them on identity changes.
No managed record or selector string is retained across an operation. Escaped
patterns retain `libs/wild.c`; unescaped families now use the spans/cursors below.
Lookup, destruction matching and existing style/update owners share the same
predicate. See the
[wildcard checkpoint](verification/VERIFICATION-2026-10-16-STYLE-WILDCARDS.md).

The wildcard checkpoint preserves cheap prefix/all-star handling and uses explicit
outer-call counters in policy/core probes. Its standalone real-client observer
runs Xlib calls in a supervised worker; the controller retains bounded child
teardown and automatic placement. Current status/evidence remain in the tracker.

STYLE-01's [repeated-ID simplifier](https://github.com/gonz0w/fvwm-x/blob/122b03bf10f9c77fe17528636c692c938fd1951e/fvwm/style.c) reuses immutable selector fingerprints and
an 8-KiB operation-local filter with conservative first-occurrence bounds.
`window_style.simplify_ordinal` is scratch for one pass; full record moves retain
the destination position. No summary retains a pointer into a retired rule.
The pure `style_match_ids_may_overlap` policy rejects only proven conflicts in
same-field literal/anchored spans, preserving compatibility-selector ORs.
See the [batch scope](verification/VERIFICATION-2026-10-16-STYLE-SIMPLIFICATION.md).

STYLE-01's ordered candidate lookup uses one necessary numeric/literal predicate
per immutable rule and deduplicated operation-local subject buckets. Private
per-list chains merge by ordinal before the existing matcher, preserving category
priority and command order. Append/unlink and all three simplification paths
maintain borrowed links; ordinal exhaustion discards the table for a compact
lazy rebuild. Small/wildcard-heavy lists stay linear, retaining a nonempty table
across threshold crossings. Empty lists release it; no window result or X state
is cached. The seven category records store only `style_id_flags`.

Each live indexed rule contributes to one of six domain counts. The last unlink
clears that domain before the next query, avoiding hashes of retired identity
fields without rescanning survivors. One synchronous borrowed subject shares
untagged hashes and hash-derived lengths across explicit/compatibility domains
and lists. Compatibility representatives are lazy; the existing matcher still
decides every candidate. Identity initialization discards subject caches; no
callback retains borrowed strings. [Retention checkpoint](verification/VERIFICATION-2026-10-19-STYLE-INDEX-RETENTION.md).


## Development build dependency boundaries

The private `fvwm-image-svg-worker` and direct `test-image-svg-render` compile
their decoder/scale sources without linking `libfvwm3`. `image_renderer_deps`
supplies X11/XRandR headers only, plus full enabled librsvg and Cairo
package dependencies. The worker adds actual X11 linkage for software effects;
the direct decoder adds none. Keep package-declared static/transitive closure.
`Fsvg.h` owns the SVG feature flag and active renderer wrappers; callers own
X/Picture types, and the decoder
includes `Xmd.h` explicitly for `CARD32`.

`libfvwm3` uses `all_compile_deps`, preserving umbrella-header visibility without
propagating runtime library requirements. The core owns `core_runtime_deps`,
including enabled SM/ICE; SVG/Cairo belongs to the isolated renderer. FvwmAuto and
FvwmEvent extract libc-only archive helpers; FvwmMFL adds full libevent and
FvwmRearrange adds full X11/XRandR dependency objects. Global math policy stays
shared. The remaining production modules and fvwm-root use image, graphics-init,
text and localized-text runtime groups. Image archive members retain every
enabled decoder/cursor backend; graphics initialization adds XRandR. Text adds Fontconfig/Xft and enabled FriBidi/iconv. Form
owns libevent; Form/Pager/Script own enabled gettext; Buttons/Script own SM/ICE
through fsm.c. Script's libWidgets uses compile-only requirements and preserves
its ordinary archive/link order. Test fixtures retain explicit broad requirements
where not independently mapped. [Remaining production group](verification/VERIFICATION-2026-10-25-M3-MODULE-LINKAGE.md).
Keep full package objects for runtime owners so static/transitive requirements
survive. These narrower
link edges do not imply that Cairo's runtime libraries or full-project packages
can be removed. [Core/archive checkpoint](verification/VERIFICATION-2026-10-25-M3-ARCHIVE-LINKAGE.md),
[combined dependency acceptance](verification/VERIFICATION-2026-10-25-M3-CLOSURE.md).
M4 retains sole Cairo/librsvg runtime ownership in the worker/direct-render
test, after removing obsolete initializer-only edges and API branches.
[Consolidation evidence](verification/VERIFICATION-2026-10-25-M4-SVG.md).

`fvwm/fvwm.h` forward-declares borrowed `window_style` pointers. Complete rule
and special-flag layouts live in `fvwm/style_types.h`, included by field/constant
consumers and `style.h`. `fvwm/style_id.h` supplies selector IDs without shared
window state; the pure matcher/filter APIs consume it directly.
`fvwm/style_match_types.h` contains immutable inline metadata, while
`fvwm/style_match.h` owns the synchronous subject and matching/query API.
Owning `window_style_list` and `style_lookup_index` are private to `style.c`,
which owns their allocation, mutation, invalidation and release. These boundaries
preserve field types/order and avoid unrelated style-layout rebuilds.
[Measured checkpoint](verification/VERIFICATION-2026-10-19-STYLE-TYPE-BOUNDARIES.md).

The Linux `fvwm3-live-x-wrap` fixture extracts the production executable's core
objects and compiles only its two link-wrap probes separately. The XRR mode
macro belongs to its probe; queue and XRR interception remain linker wrappers.
The distinct assertion-enabled core objects owned by `module_output_fixture`
remain shared by the actual-core fixtures, with `-UNDEBUG` retained.

The [affected build-reuse checkpoint](verification/VERIFICATION-2026-10-18-DEVELOPMENT-BUILD-REUSE.md)
records the actual graphs, code/data section parity and retained qualification scope.


STYLE-01's unescaped wildcard families use two immutable classification masks
in `style_match_types.h`. `style_match.c` checks exact-width question patterns,
anchors one-star question spans and matches general unescaped patterns with
operation-local latest-star cursors. Exact remaining-star exhaustion preserves
the legacy empty-tail semantics; escaped patterns still use `libs/wild.c`.
No cursor survives a synchronous match or moves an X/update barrier. See the
[family checkpoint](verification/VERIFICATION-2026-10-18-STYLE-CURSORS.md).

The [combined family receipt](verification/VERIFICATION-2026-10-18-STYLE-CURSORS-COMBINED.json)
binds the matcher, enlarged inline metadata and normal/reference/collision probes
to the qualified source. Subject ownership and every X/update barrier are
unchanged.

STYLE-01's [selector disjointness](verification/VERIFICATION-2026-10-18-STYLE-OVERLAP.md)
uses existing prepared lengths for exact/minimum rejection, fixed byte positions
for question-only patterns, and the existing matcher for literal membership in
an unescaped star pattern. Only shared explicit fields can establish disjointness.
The literal subject borrows its string synchronously and retains its known length;
no metadata, resource, update barrier or lifetime owner changes.

Optional fixed-position and literal-membership proofs use a conservative work
budget. Larger pairs remain unknown and keep their original records; normal
configuration parsing/matching is not capped. The policy bounds extra proof work
without a new state owner, cached result or configuration directive.

The [combined disjointness receipt](verification/VERIFICATION-2026-10-18-STYLE-OVERLAP-COMBINED.json)
keeps original broad and final bounded source scopes separate. The optional
proof budget is per field (4096 fixed positions or matcher size product);
existing merge copy/free allocation and normal long-pattern matching remain.

STYLE-01's [escaped matching batch](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED.md)
prepares decoded widths separately from unescaped overlap proofs. Fixed and
one-star spans avoid recursive matching; general escaped matching uses local
retry cursors. Three masks add sixteen inline bytes on the measured ABI; no
retained pointer/result, shared-glob change or update/lifetime owner is added.

The [escaped-family receipt](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED-COMBINED.json)
binds the decoded metadata and normal/reference/collision streams to one frozen
source. Existing inline style allocations/copies can grow with the sixteen-byte
record increase; no separate heap buffer or retained matching state is added.

STYLE-01's [escaped selector proofs](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED-OVERLAP.md)
extend decoded exact/minimum, fixed-position and literal-membership rejection
to escaped patterns. They reuse existing metadata and synchronous matcher owners.
Fixed proofs bound raw bytes as well as decoded positions; membership bounds raw
width before its retry product. Unknown retains records without capping normal
matching. Compatibility ORs, ordering and update/lifetime barriers stay intact.

The [escaped-proof receipt](verification/VERIFICATION-2026-10-18-STYLE-ESCAPED-OVERLAP-COMBINED.json)
separates original broad and final affected source scopes. Final review retains
the plain-literal fast path without decoding verified bytes again; existing
metadata, shared-glob consumers and X/update/lifetime owners remain unchanged.
Optional raw/retry budgets retain unknown without capping ordinary matching.

Style matching evaluates explicit literals before other explicit predicates,
retaining order within each group and the separate preceding compatibility OR.
Known subject lengths can reject impossible fields before comparing their bytes;
unknown lengths stay lazy. Cursor/escaped-span walks reuse the verified raw
prefix, whose raw/decoded widths agree. These are synchronous borrowed-identity
operations; rule/update/publication order and retained metadata owners remain.


## Command and update layout ownership

`fvwm.h` exposes a borrowed `cmdparser_context_t` pointer; `cmdparser.h` defines
its fields only for parsing, expansion and the command engine. Complex-function
records belong to `functions.c` / `functable_complex.c` through
`functable_complex_types.h`; the existing API retains opaque pointers.
`update.h` retains update APIs and a plan forward declaration. Only `style.c`
and `update.c` import `update_types.h` for the complete operation-owned plan.
No record fields, command behavior, update barriers or configuration change.
The [affected checkpoint](verification/VERIFICATION-2026-10-19-COMMAND-TYPE-BOUNDARIES.md) measures actual layout-header dependencies
and verifies exact native allocated-section parity across ten profiles.


## EWMH publication operation ownership

`ewmh_WMStrut` prepares both records before one existing paired work-area
refresh when both change; independent single-mode and query-refresh owners
remain. `EWMH_SetAllowedActions` shares six typed permission requests within
one publication against the fixed read-only predicate table, preserving the
fullscreen resize override as a distinct key. No result survives the call.
`EWMH_SetDesktopNames` keeps converted buffers and lengths together until one
property assembly/free; representability refusal retains the previous property.
The [publication checkpoint](verification/VERIFICATION-2026-10-19-EWMH-PUBLICATION.md) records independent callback/vector,
work-area/filter and converted-byte/ownership probes. Command configuration,
actual-pixel geometry and existing update/publication barriers are unchanged.


## Font, picture and monitor layout ownership

`Flocale.h`, `PictureBase.h` and `FScreen.h` expose tagged pointer types and
APIs. Field owners explicitly include `flocale_font_types.h`, `picture_types.h`
and `monitor_types.h` respectively. Native IconMan/Pager embedded picture
records import the complete definition. Existing tags, members, order, native
packet layout, lifetime and command/configuration behavior remain unchanged.
The [UI boundary checkpoint](verification/VERIFICATION-2026-10-19-UI-TYPE-BOUNDARIES.md) measures actual Ninja dependencies and
verifies exact native allocated-section parity across ten profiles. API header
changes can still invalidate their consumers; the narrower dependency applies
to record-layout edits in the new headers.


## Existing legacy tray membership ownership

The KDE legacy tray list in `ewmh.c` borrows one matching/end link per mutation.
Insertion reuses the membership search; retirement keeps the existing event-mask
selection/flush before unlink/free/publication. These links never cross event or
command dispatch. Root publication uses the existing `ewmh_client_list` buffer
for ordered window IDs, inline storage and checked spill/count growth. Refused
publication releases only its snapshot, retains the property and leaves the
membership mutation committed for later publication repair. No persistent cache
or new TRAY-01 feature is added. The [tray checkpoint](verification/VERIFICATION-2026-10-19-EWMH-TRAY.md) distinguishes
search reduction established by code review from selected allocation measurements and
records actual property/order/event-mask, spill/refusal and lifetime checks.


## Style preparation and candidate merge ownership

`style_match.c` reuses the verified literal prefix while classifying the
quoted/unquoted remainder; raw offsets and decoded widths remain distinct.
Explicit universal predicates are known true, including NULL subjects, and
are omitted without moving the compatibility OR. Selector/subject layouts do
not grow. `style.c` merges borrowed necessary-key chains through a synchronous
heap of at most nine heads, retaining exact ordinal precedence and existing
index invalidation/retirement owners. The heap ends before `EWMH_GetStyle`;
no work moves across update/server-grab or event/command boundaries. The
[matching checkpoint](verification/VERIFICATION-2026-10-19-STYLE-MATCH-WORK.md) records independent linear visit/result evidence,
source bounds, measured escape scans and limited guest timing observations.

The [flag-predicate checkpoint](verification/VERIFICATION-2026-10-19-STYLE-FLAGS.md) keeps read-only subset/intersection/
masked equality in private fvwm/style_flags_predicate.h. It reads complete
words through memcpy, then byte tails; zero/negative lengths and read-only
overlap retain their contracts. style.c owns the wrappers/callers; conditional
selection still uses blockcmpmask. No mutable flags, records, list/index
lifetimes or update/grab barrier owners change.

The [flag-mutation checkpoint](verification/VERIFICATION-2026-10-20-STYLE-FLAG-MUTATIONS.md) keeps typed union/intersection/clearing
in private fvwm/style_flags_mutate.h. Complete records may alias exactly or be
disjoint; bounded memcpy words and byte tails preserve object-representation
operations. style.c retains simplification, selected-field resource cleanup and
normalization ownership. Default accumulation, record layout and update/grab
barriers retain their existing behavior; generic unused mutation statuses go.

Condition matching owns [condition_names.h](../fvwm/condition_names.h); common
window interfaces retain opaque condition pointers. Startup/placement option
layouts and monitor adapter layouts are likewise private to complete consumers.
See the [measured dependency checkpoint](verification/VERIFICATION-2026-10-20-PRIVATE-RECORD-BOUNDARIES.md).

The [picture cache](../libs/Picture.c) owns immutable published names and explicit
incoming list links in [picture_types.h](../libs/picture_types.h). Cheap variant
keys precede names; [name fingerprints](../libs/picture_cache_name.h) reject only
after the first compatible candidate, with full equality/stamp checks retained.
Final release withdraws membership before resource/view teardown. Ordinary and
acknowledged async pictures share publication; views remain unlinked. See the
[picture-cache checkpoint](verification/VERIFICATION-2026-10-20-PICTURE-CACHE-WORK.md).

Donated icon retention uses the [ImageSafety policy](../libs/ImageSafety.h)
for checked first-exact packed records and combined output sizing.
[ewmh_icons.c](../fvwm/ewmh_icons.c) retains sampled resource ownership, leaves
old hints/counts on refusal and publishes a retained reply span before freeing
the owned reply. The best-icon selector keeps its complete-list validation.
See the [donated-icon checkpoint](verification/VERIFICATION-2026-10-20-EWMH-ICON-RETENTION.md).

Window hierarchy operations use [WinMagic](../libs/WinMagic.h) operation-local
parent-query state: finite query admission, stable-root checks and reply cleanup.
[Pointer lookup](../fvwm/misc.c) preserves context-before-query order;
[IconMan](../modules/FvwmIconMan/x.c) commits only complete representable frame
offsets. No persistent hierarchy cache is introduced. See the
[window-tree checkpoint](verification/VERIFICATION-2026-10-20-WINDOW-TREE-WALKS.md).


Native module visual startup is owned by [PictureBase.c](../libs/PictureBase.c).
Inherited visual/colormap IDs are admitted as a pair, screen-local visual query
arrays are released while their Visual pointers remain Display-owned, and
white/black pixels commit together. Modules save screen defaults on allocation
refusal; core startup checks the shared result explicitly. Existing X error
policy still owns stale/mismatched resource IDs. See the
[ownership checkpoint](verification/VERIFICATION-2026-10-20-VISUAL-STARTUP.md).

Process/session startup ownership: `fvwm/builtins.c::CMD_Exec` checks child
creation and stdin setup, and failed children use `_exit`. Session construction
in `libs/fsm.c` prepares complete authentication data before publishing it,
then admits the network identifier, watch and copied environment value as one
startup operation. Failed publication removes the watch and frees constructor
resources; configured Exec shell interpretation is unchanged.

Swallow-proxy session callbacks: `libs/fsm.c` owns callback-provided previous IDs
and generated IDs. Failed registration publishes no ID; accepted fresh clients
retain the existing no-automatic-save policy. The existing discard-only
property owner answers get requests with an empty list. Pending ICE retirement
is idempotent across the status sweep and close watch, preserving other peers. `tests/session_client_probe.c`
is an independent libSM peer for real authentication, automatic registration retry
and response progression. This is separate from the future desktop session owner.

Core record dependencies: [module_records.h](https://github.com/gonz0w/fvwm-x/blob/cb515d726cba7897e60c36175d684b9209a40d46/fvwm/module_records.h) owns
complete module/input/store/deferred-packet layouts and access macros;
[module_list.h](../fvwm/module_list.h) retains opaque public pointers and iterator
state. [icon_boxes.h](https://github.com/gonz0w/fvwm-x/blob/cb515d726cba7897e60c36175d684b9209a40d46/fvwm/icon_boxes.h) belongs to style/icon/window field
consumers. [event_filter.h](https://github.com/gonz0w/fvwm-x/blob/cb515d726cba7897e60c36175d684b9209a40d46/fvwm/event_filter.h) keeps property-event scratch
arguments beside events.c. Exact code/data parity and measured dependencies
are recorded in the [core-record report](verification/VERIFICATION-2026-10-20-CORE-RECORD-BOUNDARIES.md).

Manager startup in `fvwm/icccm2.c::SetupICCCM2` owns the selection acquisition,
initialized MANAGER event, old-owner watch and finite handoff. It reuses the
monotonic deadline policy and signal-aware poll before normal event dispatch
exists. Only old-owner destruction is consumed; failures close the startup
connection and clear the display pointer before exit hooks. Selection transfer
and EWMH desktop geometry remain separate owners.

Desktop root publication in `fvwm/ewmh.c` prepares one bounded count/viewport/
work-area snapshot. `ewmh_desktop_policy.h` owns pure count/index admission and
server-request arithmetic; `ewmh_events.c` validates pager formats before owner
adoption. Populated/current-desktop shrink requests are refused, not relocated.
The 1024-record publication budget does not restrict configured FVWM desktops.
Legacy root work areas now merge accepted dynamic struts in one window walk,
with common all-desktop reservations and bounded per-desktop edge storage in
the existing publication buffer. `ewmh_workarea_policy.h` owns unsigned range
admission and contained geometry. Typed property results distinguish malformed
replacement from revocation; an all-zero value retains the static auto-hide
reservation. Mutation, deletion, base/style/desk updates and destruction publish
through the existing single-threaded paths. Partial struts, nonsticky-page
panels and multi-monitor root authority remain separately owned.

The browser's [provider transport](../bin/FvwmPrompt/cmd/fvwmfilebrowser/provider.go)
registers the owner, reconciles current native selection, publishes acknowledged
results and chunks replies. [Read-only query policy](../bin/FvwmPrompt/cmd/fvwmfilebrowser/provider_query.go)
serves bounded admitted metadata pages and observed properties with a concrete
map result type. MFL owns connection registration, direct query routing and
bounded response assembly; no path/content cache enters the core.
[One-shot CLI requests](../bin/FvwmPrompt/cmd/fvwmcommand/request.go) await a
correlated terminal reply through the existing transport. See the
[query contract](decisions/FILE-PROVIDER-QUERIES-2026-10-23.md).

[One operation catalog](../bin/FvwmPrompt/cmd/fvwmfilebrowser/provider_operations.go)
defines capability names, frame admission and worker ownership. Public names
are copied so consumer metadata cannot modify later dispatch. Strict bounded
[typed values](../bin/FvwmPrompt/cmd/fvwmfilebrowser/provider_values.go) feed
[operation schemas](../bin/FvwmPrompt/cmd/fvwmfilebrowser/provider_action.go)
before native reconciliation; [state application](../bin/FvwmPrompt/cmd/fvwmfilebrowser/provider_apply.go)
checks current generation before publishing metadata/view changes.
[Selection edit policy](../bin/FvwmPrompt/internal/collection/selection_edit.go)
is shared with native Select visible; it publishes only complete admitted edits.
MFL retains the existing producer wait and response ownership for both queries
and controls. Native acknowledgement distinguishes published state from input
admission; filesystem actions and clipboard content stay with their owners.

[Browser worker requests](../bin/FvwmPrompt/cmd/fvwmfilebrowser/provider_work.go)
admit typed navigation/refresh/open/terminal/configured actions through the
existing isolated worker. One latest accepted serial-bound outcome and explicit
cancel/reap/supersession state live with the browser; MFL retains only its normal
response ownership. Path bytes and configured program identity remain separate
from display text. The [worker contract](decisions/FILE-PROVIDER-WORK-2026-10-23.md)
does not add file mutation or protected clipboard transport.

[Live target publication](../bin/FvwmPrompt/cmd/fvwmfilebrowser/target.go)
uses the existing isolated Locate worker and validates one candidate for exact
selection, filter, first focus, properties and directory/history. Same-folder
handoff commits native selection then sends generation-checked runtime ListFocus;
its [pure policy](../modules/FvwmForm/form_list.c) preserves selection and scrolls
without replacing X resources. Explicit hidden/watch controls reuse snapshot/
observer ownership. State exposes configured labels/availability without programs.
See the [target contract](decisions/FILE-PROVIDER-TARGETS-2026-10-23.md).

[Correlated request exchange](../bin/FvwmPrompt/transport/request.go) owns the
shared command/browser one-shot JSON reader, context deadline and close/join
lifecycle. [Portable reuse](../bin/FvwmPrompt/cmd/fvwmfilebrowser/reuse.go) admits
only idle matching folders/capabilities and returns worker admission receipts;
unknown outcomes cannot trigger duplicate launches. Pure TargetParent shares
Locate's lexical policy. Native publication followed by core-owned presentation
keeps managed-window actions in the command engine. The optional Go profile's
folder desktop entry does not select a default provider. See the
[handoff contract](decisions/BROWSER-CONSUMER-HANDOFF-2026-10-23.md).

Script outgoing messages use the bounded, module-owned `script_message.c` queue:
literal target atom IDs and transferred text have one owner, matching takes
preserve target FIFO and cleanup clears vacated slots. Shutdown retains selection
ownership through one monotonic grace period using `script_delay.c` policy, then
releases pending messages. Xlib calls still require a responsive server.
[Owner evidence](verification/VERIFICATION-2026-10-25-M5-SCRIPT-OWNERSHIP.md).

GetOutput keeps subprocess ownership in `script_process.c`: stdout and a private
CLOEXEC setup-status pipe share one command deadline, while bounded polling
observes the module termination flag. Failed construction/cancellation clears
results and restores owned state; the child stays unreaped through group cleanup.
The caller retains its successful cache on failure.
[Owner evidence](verification/VERIFICATION-2026-10-25-M5-SCRIPT-PROCESS.md).

Shared `fvwmRead`/`fvwmWrite`/`fvwmPoll` waits in `libs/fvwmsignal.c` use POSIX
saved-mask jumps. The existing arm/check/syscall/disarm sequence and volatile
result preserve cancellation and completed-I/O ownership. The unused select
adapter and exclusive feature definitions are retired; generic descriptor types
and the active owned core readiness/continuation policies remain.
[Current evidence and LOOP-01 reconciliation](verification/VERIFICATION-2026-10-25-M5-WAIT-OWNERSHIP.md).

The obsolete signal-mask shim and its nine guarded registrations are retired;
active POSIX handlers/masks and HAVE_SIGACTION ownership remain. GetOutput's
foreground wait now reuses the fixed shared fvwmPoll with its remaining deadline.
Closed-pipe completion keeps 10ms checks; cancellation cleanup keeps raw polling.
[Source parity and focused evidence](verification/VERIFICATION-2026-10-25-M5-POSIX-CONSUMERS.md).
