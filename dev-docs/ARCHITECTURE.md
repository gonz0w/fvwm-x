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
are queued, not new source entry points. Collection views and typed actions reuse
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
| Command clients / metadata | [FvwmPrompt.go](../bin/FvwmPrompt/FvwmPrompt.go), [FvwmCommand Go client](../bin/FvwmPrompt/cmd/fvwmcommand/main.go), [session.go](../bin/FvwmPrompt/transport/session.go), [generate.py](../tools/maintenance/generate.py), [Module.pm.in](../perllib/FVWM/Module.pm.in) | Prompt owns its readline UI; Go-enabled FvwmCommand owns its bounded stdin/first-record CLI, and disabled-Go retains the legacy Python client. Both Go clients share the standard-library transport: one reader, serialized writes and idempotent teardown. Native headers/command table generate checked committed Perl metadata and JSON companions. Extended handler matching excludes the shared category marker. Broader Go ports and Perl retirement retain their own gates. |
| Bootstrap | [fvwm3.c](../fvwm/fvwm3.c), [screen.h](../fvwm/screen.h) | Display ownership, initialization, configuration, restart and global screen state |
| Dispatch / scheduling | [events.c](../fvwm/events.c), [event_loop_policy.c](../fvwm/event_loop_policy.c), [schedule.c](../fvwm/schedule.c) | Event ordering, readiness, bounded work, timers and deferred termination |
| Configuration streams | [read.c](../fvwm/read.c), [logical-line policy](../libs/config_line.c), [policy tests](../tests/test_config_line.c) | Read/PipeRead preserve completed command order and the descriptor barrier, grow reusable owned line storage to a finite 1 MiB, retain backslash-newline continuation and stop before dispatching a malformed/partial logical line. Existing count limit remains. Current reader changes are part of the XDG integration checkpoint, not a completed wider DATA-01 audit. |
| Standalone xlock menu | [Go producer](../bin/FvwmPrompt/cmd/fvwmmenuxlock/main.go), [Perl fallback](../bin/fvwm-menu-xlock.in), [utility fixtures](../tests/test_utility_contracts.py), [probe checks](../tests/test_xlock_menu.py) | Go-enabled installs use a direct five-second/1 MiB xlock help probe, byte-based streaming formatting and escaped external label values. Existing CLI/menu actions remain; Go-disabled builds retain Perl. Meson owns a separate xlock-menu install tag. Other utility ports and runtime retirement have separate gates. |
| Standalone directory menu | [Go producer](../bin/FvwmPrompt/cmd/fvwmmenudirectory/main.go), [renderer](../bin/FvwmPrompt/cmd/fvwmmenudirectory/render.go), [navigation state](../bin/FvwmPrompt/cmd/fvwmmenudirectory/state.go), [CLI/filesystem checks](../tests/test_directory_menu.py), [live navigation](../tests/live_x/directory_menu_live.py) | Optional Go utility caches entry metadata and format plans, preserves sorting/actions and bounds enumeration. Reuse navigation replaces shell writes with an owned atomic mode-0600 state helper; the existing plain-text state remains readable. All generated managed actions go through FVWM commands. Go-disabled retains Perl under the directory-menu install tag. |
| Configuration converter | [Go CLI](../bin/FvwmPrompt/cmd/fvwmconvert26/main.go), [conversion policy](../bin/FvwmPrompt/cmd/fvwmconvert26/convert.go), [file ownership](../bin/FvwmPrompt/cmd/fvwmconvert26/files.go), [byte/filesystem fixtures](../tests/test_config_converter.py), [Perl fallback](../bin/fvwm-convert-2.6.in) | Optional standard-library utility preserves the historical conversion heuristics and literal Read search. It bounds source/logical-line/reference data, keeps function context within a conversion job and publishes complete mode-0600 outputs without replacing an existing path. It never executes configuration commands. Go-disabled retains Perl under the config-converter install tag. |
| Full XDG menu service | [Go CLI/worker](../bin/FvwmPrompt/cmd/fvwmmenudesktop), [pure menu policy](../bin/FvwmPrompt/internal/xdgmenu), [acquisition/publication](../bin/FvwmPrompt/internal/xdgsource), [native Form](../modules/FvwmForm/FvwmForm.c), [CLI checks](../tests/test_xdg_desktop.py), [native-X checks](../tests/live_x/xdg_menu_live.py) | Coordinated optional Go candidate: bounded file merging, allocation, presentation, icon lookup/conversion and completed native menu output. Application actions reference private literal argv records; record locking/atomic union preserves existing menus. Native Form uses encoded fields, bounded command/input appends, per-invocation result identities and the existing local-command/core Read paths. Source/worker/X state remain separate; no new GUI library. Disabled-Go retains the [Python provider](../bin/fvwm-menu-desktop.in) and [Perl frontend](../bin/fvwm-menu-desktop-config.fpl). [Development](verification/DEVELOPMENT-2026-10-12-XDG-PROVIDER.md) passes focused checks; six-target/install/native-X qualification and later Perl retirement remain open, separate from the launcher/controller. |
| Script communication example | [Go helper](../bin/FvwmPrompt/cmd/fvwmscriptcom/main.go), [FIFO/CLI checks](../tests/test_script_com_example.py), [native widget interaction](../tests/live_x/script_com_example_live.py), [paired template](../modules/FvwmScript/Scripts/FvwmScript-ComExample.in) | Optional stdlib helper keeps count/Parse/exit framing, pins its user directory and owns private FIFO entries. Connected reads and reply delivery have finite deadlines; peer departure and signals clean up without terminating Script. The legacy Perl file stays installed for explicit interpreter invocations and Go-disabled templates. The script-com-example install tag owns the paired consumer/helper. |
| Commands | [functions.c](../fvwm/functions.c), [functable.c](../fvwm/functable.c), [execcontext.c](../fvwm/execcontext.c), [read.c](../fvwm/read.c) | Shared parsing/expansion/execution and triggering context lifetime |
| Window lifetime / stack | [add_window.c](../fvwm/add_window.c), [fvwm.h](../fvwm/fvwm.h), [stack.c](../fvwm/stack.c) | Managed records, XID identities, transient relationships and ordering |
| Geometry / input | [geometry.c](../fvwm/geometry.c), [move_resize.c](../fvwm/move_resize.c), [virtual.c](../fvwm/virtual.c), [interaction.c](../fvwm/interaction.c), [interaction_policy.c](../fvwm/interaction_policy.c) | Placement/constraints, shared Move/Resize/viewport readiness, checked button masks and cancellation. Audited service preserves borrowed monitor/window state; common final release waits use the same pump with 20-ms pointer fallback. Lifecycle/topology and image publication remain at outer unwind; viewport locally qualified; active menus reuse the audited owner with bounded motion and pure monotonic submenu timing; their separate checkpoint is locally qualified |
| Monitors / scale | [FScreen.c](../libs/FScreen.c), [monitor_topology.c](../libs/monitor_topology.c), [monitor_scale.c](../libs/monitor_scale.c), [monitor_scale_config.c](../fvwm/monitor_scale_config.c), [geometry_transition.c](../libs/geometry_transition.c) | Committed topology, rational scale, ownership and transitions |
| Style / rendering | [style.c](../fvwm/style.c), [update.c](../fvwm/update.c), [borders.c](../fvwm/borders.c), [Picture.c](../libs/Picture.c), [Flocale.c](../libs/Flocale.c) | Ordered styles, decorations and generation/scale-aware image/font resources |
| Image acquisition / decode | [PictureImageLoader.c](../libs/PictureImageLoader.c), [image_input.c](../libs/image_input.c), [image_decompress.c](../libs/image_decompress.c), [image_xbm.c](../libs/image_xbm.c), [image_svg_client.c](../libs/image_svg_client.c), [fvwm-image-svg-worker.c](../bin/fvwm-image-svg-worker.c), [image_svg_protocol.c](../libs/image_svg_protocol.c) | Opened-file snapshots, checked XBM parsing, finite literal process transport and verified resource-limited SVG rendering; typed results keep X state in the caller, with synchronous module callers and asynchronous core completion; [picture_async.c](../libs/picture_async.c), [picture_argb.c](../libs/picture_argb.c) and [picture_view.c](../libs/picture_view.c) prepare bounded transport, acknowledged X conversion and owner-specific immutable views. Normal-startup core wiring in [image_async.c](../fvwm/image_async.c) and [image_async_wait.c](../fvwm/image_async_wait.c) connects readiness, safe-boundary commits and shutdown; typed per-view owner bindings preserve old images and invalidate cancelled decorations; colorsets and decor scale variants have focused coverage, and normal startup/restart pass focused checks; combined local qualification is accepted |
| X data / properties | [FProperty.c](../libs/FProperty.c), [FSelection.c](../libs/FSelection.c), [ewmh.c](../fvwm/ewmh.c), [XError.c](../libs/XError.c) | Bounded data acquisition, selection lifetime, desktop properties and errors |
| Module transport | [module_list.c](../fvwm/module_list.c), [module_runtime.c](../fvwm/module_runtime.c), [module_queue.c](../fvwm/module_queue.c), [Module.h](../libs/Module.h) | Processes, partial packets, queue ownership, producer ordering and compatibility |
| Status / IPC | [FvwmMFL.c](../modules/FvwmMFL/FvwmMFL.c), [stream.c](../modules/FvwmMFL/stream.c) | Protected local socket, bounded framing and JSON event fan-out |
| Script geometry / layout | [script_geometry.c](../modules/FvwmScript/script_geometry.c), [Tools.c](../modules/FvwmScript/Widgets/Tools.c), [font_geometry.h](../libs/font_geometry.h), [Fft.c](../libs/Fft.c), [Flocale.c](../libs/Flocale.c) | Pure X11 range/layout policy, prepared widget font ownership, checked popup placement and wide text metrics; runtime refusal preserves prior state, while broader resource budgets remain separate |
| Shared shadow rendering | [font_shadow.c](../libs/font_shadow.c), [Flocale.c](../libs/Flocale.c), [add_window.c](../fvwm/add_window.c), [Pager](../modules/FvwmPager/init_pager.c), [Form](../modules/FvwmForm/FvwmForm.c) | Checked finite per-draw positions, direct legacy-order Center border iteration and prepare-before-release font ownership; broader font/cache/glyph budgets remain open |
| Native desktop UI | [modules](../modules), [default-config](../default-config), [desktop.py](../default-config/desktop.py), [icon generator](../default-config/generate_icons.py), [wallpaper generator](../default-config/generate_wallpapers.py) | Existing modules, default configuration and portable desktop-service policy; committed self-contained SVG artwork uses the isolated decoder, labeled fallback remains available, and launcher updates retain unchanged native icon resources |
| Build / verification | [meson.build](../meson.build), [tests](../tests), [maintenance tools](../tools/maintenance/README.md) | Feature detection, targets, installed files and reproducible checks |

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

`tests/run_native_gate.sh` owns shared Linux/BSD setup, test and staged-install
behavior; the platform wrappers retain their entry points. Linux sanitizer mode
uses the same feature/dependency assertions and tests instrumented binaries.
`tests/test_native_gate.py` checks orchestration failure/path/profile contracts.
Its optional `--go-clients` profile delegates to `tests/run_go_clients.sh` for
bounded transport/client tests, four tagged client/utility stages and Linux actual-client
race checks. `tests/run_bsd_default_x.py` owns an isolated authenticated default
Xorg through the existing bounded harness; `_x11` receives authority access on
OpenBSD. It checks a cookie-less client's refusal and reuses installed
Command/Prompt fixtures. Its `--xlock-only` and `--directory-only` modes qualify the installed utilities
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
