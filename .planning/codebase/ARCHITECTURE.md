<!-- refreshed: 2026-08-29 -->
# Architecture

**Analysis Date:** 2026-08-29

## System Overview

```text
┌─────────────────────────────────────────────────────────────────────────┐
│                         Input / Adapter Layer                           │
├──────────────────────┬──────────────────────┬───────────────────────────┤
│ X11/XRandR events    │ Config and commands  │ External module pipes     │
│ `fvwm/events.c`      │ `fvwm/read.c`        │ `fvwm/module_list.c`      │
│                      │ `fvwm/functions.c`   │ `fvwm/module_interface.c` │
└──────────┬───────────┴──────────┬───────────┴─────────────┬─────────────┘
           │                      │                         │
           └──────────────────────┼─────────────────────────┘
                                  ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    Core window-manager process                          │
│ Commands, events, policy, frames, placement, focus, menus, EWMH        │
│ `fvwm/`                                                                │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │ uses and mutates
                                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    Shared state and abstractions                        │
│ `ScreenInfo Scr`, `FvwmWindow`, monitor tree, execution contexts       │
│ `fvwm/screen.h`, `fvwm/fvwm.h`, `libs/FScreen.h`, `fvwm/execcontext.h` │
└───────────────┬───────────────────────────────────────┬─────────────────┘
                │                                       │
                ▼                                       ▼
┌──────────────────────────────────┐  ┌───────────────────────────────────┐
│ Static support library           │  │ Out-of-process extension modules  │
│ X wrappers, graphics, parsing,   │  │ `modules/Fvwm*/`                  │
│ module protocol, utilities       │  │ plus Perl API in `perllib/FVWM/`  │
│ `libs/`                          │  │ and UDS clients via `FvwmMFL`      │
└──────────────────────────────────┘  └───────────────────────────────────┘
                │                                       │
                └──────────────────┬────────────────────┘
                                   ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ X server, process pipes, Unix-domain socket, user config and state      │
│ `fvwm/fvwm3.c`, `libs/Module.h`, `modules/FvwmMFL/FvwmMFL.c`           │
└─────────────────────────────────────────────────────────────────────────┘
```

FVWM3 is a single-threaded X11 window-manager core with a process-based extension architecture. The main `fvwm3` executable is assembled from the files in `fvwm/` and statically links `libs/libfvwm3`; each module under `modules/` is a separate executable that also links the support library (`meson.build:501-529`, `libs/meson.build:73-79`).

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| Process bootstrap | Parse CLI options, open the X display, claim root-window redirection, initialize subsystems, load configuration, and start the event loop | `fvwm/fvwm3.c:1687` |
| Event multiplexer | Fairly interleave queued X events, module pipe I/O, session-manager I/O, scheduled commands, and signal work | `fvwm/events.c:4208`, `fvwm/event_loop_policy.c:46` |
| X event dispatcher | Resolve an X window to `FvwmWindow`, create an `EXCT_EVENT` context, and invoke the registered handler table | `fvwm/events.c:4013`, `fvwm/events.c:4116` |
| Command engine | Parse/expand command lines and dispatch built-ins, complex functions, module configuration, or external modules | `fvwm/functions.c:392`, `fvwm/functable.c:55` |
| Command parser implementation | Implement the parser hook contract used by the command engine | `fvwm/cmdparser_hooks.h`, `fvwm/cmdparser_old.c:451` |
| Managed-window lifecycle | Build `FvwmWindow`, merge styles, place and frame clients, register X contexts, and broadcast changes | `fvwm/add_window.c:2122` |
| Window policy and rendering | Own placement, focus, stacking, geometry, frames, borders, decorations, menus, virtual desktops, EWMH, and updates | `fvwm/placement.c`, `fvwm/focus.c`, `fvwm/stack.c`, `fvwm/frame.c`, `fvwm/borders.c`, `fvwm/menus.c`, `fvwm/virtual.c`, `fvwm/ewmh.c`, `fvwm/update.c` |
| Core state model | Hold global screen configuration and the linked list of managed windows | `fvwm/screen.h:300`, `fvwm/fvwm.h:717` |
| Monitor model | Track XRandR outputs, monitor geometry, per-monitor desktops, viewports, EWMH state, and pan frames | `libs/FScreen.h:59`, `libs/FScreen.c` |
| Module host | Spawn module processes, own pipe endpoints, incrementally receive commands, queue outbound packets, and reap modules | `fvwm/module_list.c`, `fvwm/module_runtime.c`, `fvwm/module_interface.c` |
| Module client API | Define packet formats and provide module-side argument, send, receive, mask, and config helpers | `libs/Module.h`, `libs/Module.c` |
| Shared support library | Centralize X wrappers, graphics/images/fonts, locale, parsing, safe allocation, collections, logging, and portability | `libs/`, `libs/meson.build` |
| Core modules | Provide pagers, buttons, event actions, forms, scripting, animation, identification, rearrangement, and automation as isolated processes | `modules/` |
| Module Front Loader | Translate selected binary module packets to JSON and expose command/event access through a protected Unix-domain socket | `modules/FvwmMFL/FvwmMFL.c:830` |
| Prompt client | Connect to the `FvwmMFL` socket and provide interactive or one-shot command input | `bin/FvwmPrompt/FvwmPrompt.go:122` |
| Perl module framework | Expose the pipe protocol, event dispatch, trackers, constants, and module utilities to Perl programs | `perllib/FVWM/Module.pm.in`, `perllib/FVWM/Tracker.pm` |
| Build graph | Detect platform capabilities and dependencies, generate `config.h`, and assemble core, modules, tools, tests, docs, and translations | `meson.build`, `meson.options` |

## Pattern Overview

**Overall:** Event-driven monolith with command dispatch and out-of-process plugin modules.

**Key Characteristics:**
- Keep X server ownership and all authoritative window state in the single `fvwm3` process (`fvwm/fvwm3.c`, `fvwm/screen.h`, `fvwm/fvwm.h`).
- Route user configuration, bindings, menus, schedules, EWMH-generated actions, and module commands through the same command engine (`fvwm/functions.c:392`, `fvwm/read.c:115`, `fvwm/module_interface.c:724`).
- Extend user-visible behavior with separate module executables communicating over two one-way pipes instead of loading code into the core process (`fvwm/module_list.c:123`, `libs/Module.h:240`).
- Reuse platform, graphics, protocol, and parsing code through the non-installed static library declared in `libs/meson.build:73`.
- Represent managed clients with a large mutable `FvwmWindow` record linked both by management order and stacking order (`fvwm/fvwm.h:717`).
- Carry command origin, triggering event, target window, and originating module explicitly in immutable-style heap contexts (`fvwm/execcontext.h:22`, `fvwm/execcontext.c:121`).

## Layers

**Bootstrap and Process Lifecycle:**
- Purpose: Own process startup, X display acquisition, configuration search, initialization order, restart, and shutdown.
- Location: `fvwm/fvwm3.c`, with session support in `fvwm/session.c`.
- Contains: `main()`, signal handlers, X error handlers, default command setup, and `StartupStuff()`.
- Depends on: Every core subsystem and most of `libs/` through explicit headers (`fvwm/fvwm3.c:32-76`).
- Used by: The installed `fvwm3` executable defined in `meson.build:501`.

**Input and Event Coordination:**
- Purpose: Convert X events, module bytes, timers, session messages, and deferred signal requests into bounded units of core work.
- Location: `fvwm/events.c`, `fvwm/event_loop_policy.c`, `fvwm/schedule.c`, `fvwm/module_runtime.c`.
- Contains: `select()`-based descriptor polling, X event budgets, module command budgets, event jump tables, and scheduled-command execution.
- Depends on: Xlib/FEvent, module queues, session management, and global `Scr` flags (`fvwm/events.c:4208-4455`).
- Used by: `main()` through `HandleEvents()` (`fvwm/fvwm3.c:2407`).

**Command Parsing and Dispatch:**
- Purpose: Normalize all FVWM command text into built-in calls, complex-function execution, module config storage, or module process launch.
- Location: `fvwm/functions.c`, `fvwm/cmdparser_old.c`, `fvwm/functable.c`, `fvwm/functable_complex.c`, `fvwm/commands.h`.
- Contains: Parser hook interface, command metadata table, variable expansion, execution deferral, return-code propagation, and complex-function control flow.
- Depends on: `exec_context_t`, parsing helpers in `libs/Parse.c`, global styles/decor state, and individual `CMD_*` implementations throughout `fvwm/` (`fvwm/functions.c:392-740`).
- Used by: Config loading, X event handlers, schedules, menu actions, bindings, EWMH adapters, and modules (`fvwm/read.c:149`, `fvwm/module_interface.c:771`).

**Window Management Domain:**
- Purpose: Maintain the authoritative model and implement window-manager behavior.
- Location: `fvwm/`.
- Contains: Client capture, frames, decorations, placement, movement, focus, stacking, styles, menus, virtual desktops, EWMH/ICCCM, icons, and update batching.
- Depends on: Xlib and support wrappers from `libs/`, command execution contexts, `ScreenInfo Scr`, and `FvwmWindow` (`fvwm/fvwm.h`, `fvwm/screen.h`).
- Used by: X event handlers and `CMD_*` built-ins registered in `fvwm/functable.c`.

**Shared Platform and Protocol Support:**
- Purpose: Keep reusable X11, image, locale, data-structure, memory, logging, and module-protocol machinery outside core policy files.
- Location: `libs/`.
- Contains: `FScreen`, `FEvent`, `FImage`, `Picture*`, `FRender*`, `Flocale*`, `Module`, queues/lists/trees, safe allocation, and portability shims (`libs/meson.build:1-60`).
- Depends on: Required and optional libraries collected as `all_found_deps` in `meson.build:212-416`.
- Used by: The core executable, C modules, `fvwm-root`, and tests (`meson.build:501-508`, `modules/FvwmPager/meson.build`, `bin/meson.build:15-22`).

**Extension Processes:**
- Purpose: Add long-running UI and automation features without putting their loops or state into the core process.
- Location: `modules/FvwmAnimate/` through `modules/FvwmScript/`, plus the Perl support in `perllib/`.
- Contains: One executable-oriented subtree per module, typically a `main()`, message mask setup, config parsing, packet handlers, X event loop, and `meson.build` (`modules/FvwmPager/FvwmPager.c:136`, `modules/FvwmAuto/FvwmAuto.c:101`).
- Depends on: The ABI-like protocol in `libs/Module.h`, `libfvwm3`, and module-specific X facilities.
- Used by: `CMD_Module` and unknown-command fallback in the core command engine (`fvwm/functions.c:716`, `fvwm/module_list.c:123`).

**Tools, Assets, and Documentation:**
- Purpose: Supply command-line helpers, default runtime configuration, generated scripts, man-page sources, and translations.
- Location: `bin/`, `default-config/`, `doc/`, `po/`, `utils/`, `meson-scripts/`.
- Contains: C, Go, Perl, shell, AsciiDoc, image, and gettext assets wired into Meson (`bin/meson.build`, `default-config/meson.build`, `doc/meson.build`, `po/meson.build`).
- Depends on: Configured installation paths from `meson.build:479-492`.
- Used by: Installed users, module processes, and release/documentation builds.

## Data Flow

### Primary Request Path: X Window Map

1. `main()` initializes X, global state, event handlers, styles, and configuration, then calls `HandleEvents()` (`fvwm/fvwm3.c:2012-2407`).
2. `My_XNextEvent()` drains queued module commands, applies the X-event fairness policy, polls X/module/session descriptors, and returns one X event (`fvwm/events.c:4252-4455`).
3. `dispatch_event()` resolves the event window, creates an `EXCT_EVENT` execution context, and invokes `HandleMapRequest` from the registered jump table (`fvwm/events.c:4056-4195`).
4. `HandleMapRequestKeepRaised()` calls `AddWindow()` for an unmanaged client (`fvwm/events.c:2990-3064`).
5. `AddWindow()` merges styles, creates frame/decor windows, places the client, reparents it, selects events, registers it in lists, and initializes EWMH state (`fvwm/add_window.c:2122-2579`).
6. The new state is broadcast to interested modules through `BroadcastConfig()` and related packet functions (`fvwm/add_window.c:2447-2478`, `fvwm/module_interface.c:388`).

### Command Execution Flow

1. A command enters through config file lines, a binding/menu/event, a schedule, startup defaults, or a module pipe (`fvwm/read.c:115-150`, `fvwm/fvwm3.c:1292-1369`, `fvwm/module_interface.c:724-772`).
2. The caller creates or clones an `exec_context_t` identifying the source and target (`fvwm/execcontext.h:22-81`, `fvwm/execcontext.c:121-162`).
3. `_execute_command_line()` creates parser state, handles prefixes, resolves the command token, expands variables, and classifies the executable target (`fvwm/functions.c:392-597`).
4. A built-in invokes the `func_t.action` callback from `func_table`; a complex function recursively executes its items; a `*` line updates module configuration; an unknown token is tried as an external module (`fvwm/functions.c:597-728`, `fvwm/functable.c:55`).
5. Deferred window updates are batched behind `Scr.flags.do_need_window_update` and applied in the event loop by `flush_window_updates()` (`fvwm/events.c:4220-4223`, `fvwm/update.c:719`).

### Module Bidirectional Flow

1. The core resolves a module executable, creates two pipes, forks/execs it, and passes pipe descriptors plus window/decor context in argv (`fvwm/module_list.c:123-220`, `libs/Module.c:300-362`).
2. The module selects message masks and reads typed `FvwmPacket` messages with `ReadFvwmPacket()` (`libs/Module.h:16-28`, `libs/Module.c:53-92`).
3. Core broadcasts are filtered per module and placed in bounded nonblocking output queues (`fvwm/module_list.c:418-473`, `fvwm/module_list.h:70`).
4. The module sends textual FVWM commands with `SendText()`; `module_receive()` incrementally reconstructs the command and pins the module record while input references exist (`libs/Module.c:135-170`, `fvwm/module_runtime.c:167-238`).
5. `My_XNextEvent()` enqueues ready inputs; `ExecuteCommandQueue()` performs a bounded drain; `module_input_execute()` creates `EXCT_MODULE` context and calls the common command engine (`fvwm/events.c:4392-4417`, `fvwm/module_interface.c:724-816`).

### FvwmMFL JSON Socket Flow

1. `FvwmMFL` starts as a normal FVWM module, secures a per-user runtime directory, locks a display-specific instance, and binds a mode-`0600` Unix-domain socket (`modules/FvwmMFL/FvwmMFL.c:737-880`).
2. Libevent reads binary FVWM packets, converts selected packet bodies to JSON, and broadcasts newline-delimited messages to subscribed clients (`modules/FvwmMFL/FvwmMFL.c:719-734`, `modules/FvwmMFL/FvwmMFL.c:225`).
3. Socket clients send command text back through `FvwmMFL`, which writes it to the ordinary module-to-core pipe (`modules/FvwmMFL/FvwmMFL.c`, `libs/Module.c:135`).
4. `FvwmPrompt` discovers the socket through `FVWMMFL_SOCKET`, runs concurrent reader/writer goroutines, and forwards prompt input (`bin/FvwmPrompt/FvwmPrompt.go:19-21`, `bin/FvwmPrompt/FvwmPrompt.go:63-119`).

**State Management:**
- Treat `ScreenInfo Scr` and `Display *dpy` as the process-wide roots of core state; they are defined in `fvwm/fvwm3.c:137-138` and declared to the rest of `fvwm/` through core headers.
- Store managed clients in `Scr.FvwmRoot.next` and a separate circular stacking ring; update both through lifecycle/stack helpers rather than manually (`fvwm/screen.h:300-307`, `fvwm/add_window.c:2258-2275`, `fvwm/stack.c`).
- Store monitor/output state in global RB/TAILQ collections and attach each `FvwmWindow` to `m`/`m_prev` (`libs/FScreen.h:83-155`, `fvwm/fvwm.h:977-978`).
- Use flag-driven deferred updates for style, decoration, destruction, and redraw work at event-loop boundaries (`fvwm/events.c:4216-4239`, `fvwm/update.c:719`).
- Keep module process records and death-row cleanup private to `fvwm/module_runtime.c:16-18`; use the iterator and lifecycle APIs from `fvwm/module_list.h`.

## Key Abstractions

**`FvwmWindow`:**
- Purpose: Authoritative aggregate for one managed X client, including client/frame windows, names, flags, geometry, styles, hints, icons, desktop/layer, EWMH data, and monitor association.
- Examples: `fvwm/fvwm.h:717-979`, construction in `fvwm/add_window.c:2122`.
- Pattern: Large mutable record with access/flag macros; pass `FvwmWindow *` through policy functions and never create partial instances outside the lifecycle code in `fvwm/add_window.c`.

**`ScreenInfo` / `Scr`:**
- Purpose: Process-wide owner for root-window data, managed-window root, graphics contexts, defaults, bindings, functions, focus, styles, decorations, and deferred-work flags.
- Examples: `fvwm/screen.h:300-508`, definition in `fvwm/fvwm3.c:137`.
- Pattern: Global application context; initialize through `InitVariables()` and subsystem initializers in `fvwm/fvwm3.c:1072`.

**`exec_context_t`:**
- Purpose: Preserve command provenance and target information across parsing, expansion, built-ins, complex functions, and modules.
- Examples: `fvwm/execcontext.h:22-81`, `fvwm/execcontext.c`.
- Pattern: Allocate with `exc_create_context()`, derive with `exc_clone_context()`, pass as `const`, and pair every allocation with `exc_destroy_context()`.

**Command Descriptor (`func_t`):**
- Purpose: Map lowercase command keywords to `CMD_*` callbacks, IDs, behavior flags, and target cursors.
- Examples: `fvwm/functable.c:30-55`, declarations in `fvwm/functable.h`, callbacks in `fvwm/commands.h`.
- Pattern: Table-driven command dispatch; register new built-ins in `fvwm/functable.c` and implement them with the shared `F_CMD_ARGS` signature from `fvwm/fvwm.h:61-65`.

**Parser Hooks:**
- Purpose: Separate command-engine orchestration from parser implementation details.
- Examples: `fvwm/cmdparser_hooks.h`, active implementation `fvwm/cmdparser_old.c`, installation in `fvwm/functions.c:1232-1235`.
- Pattern: Vtable/strategy; keep parser-owned allocations inside create/destroy hooks and expose parser phases through `cmdparser_hooks_t`.

**`FvwmPacket` and `ModuleArgs`:**
- Purpose: Define the cross-process module protocol and startup contract.
- Examples: `libs/Module.h:16-28`, `libs/Module.h:240-260`, implementation in `libs/Module.c`.
- Pattern: Binary packet stream from core to modules plus framed textual commands from modules to core; maintain protocol field widths and synchronized consumers when changing it.

**Monitor Tree:**
- Purpose: Represent active XRandR outputs and their per-monitor virtual desktop state.
- Examples: `libs/FScreen.h:59-176`, implementation in `libs/FScreen.c`.
- Pattern: RB tree plus output-order TAILQ; resolve monitors through `monitor_*` helpers and update window ownership through `update_fvwm_monitor()` (`fvwm/update.c:665`).

**Support Wrappers Prefixed `F`:**
- Purpose: Encapsulate third-party or X extension APIs behind FVWM-specific interfaces.
- Examples: `libs/FShape.c`, `libs/FRender.c`, `libs/FEvent.c`, `libs/FBidi.c`; naming rule in `dev-docs/DEVELOPERS.md:152-160`.
- Pattern: Put reusable third-party adaptation in `libs/F<Name>.c`/`.h`; reserve the `F` prefix for wrappers.

## Entry Points

**FVWM3 Window Manager:**
- Location: `fvwm/fvwm3.c:1687`.
- Triggers: Desktop session startup or direct execution of installed `fvwm3`.
- Responsibilities: Initialize the parser/support layers, X connection, globals, EWMH/ICCCM, configuration, session state, and event loop.

**C Module Executables:**
- Location: One `main()` in each module subtree, such as `modules/FvwmPager/FvwmPager.c:136`, `modules/FvwmAuto/FvwmAuto.c:101`, and `modules/FvwmMFL/FvwmMFL.c:830`.
- Triggers: `Module <name>` or unknown-command fallback through `fvwm/module_list.c:123`.
- Responsibilities: Parse FVWM-provided descriptors, select packet masks, load module config, notify startup completion, and run a module-specific loop.

**FvwmScript Parser/Runtime:**
- Location: `modules/FvwmScript/FvwmScript.c`, generated scanner/parser sources in `modules/FvwmScript/scanner.c` and `modules/FvwmScript/script.c`, widget library in `modules/FvwmScript/Widgets/`.
- Triggers: `Module FvwmScript <script>` through the standard module launcher.
- Responsibilities: Parse FvwmScript language files and render/operate its widget set; build composition is in `modules/FvwmScript/meson.build`.

**FvwmPrompt:**
- Location: `bin/FvwmPrompt/FvwmPrompt.go:122`.
- Triggers: Direct user invocation after `FvwmMFL` publishes `FVWMMFL_SOCKET`.
- Responsibilities: Provide readline history/prompting, protect the `Quit` path with confirmation, and relay commands over the local socket.

**Perl Module API:**
- Location: Constructor and event API in `perllib/FVWM/Module.pm.in:91`, module executables such as `modules/FvwmPerl/FvwmPerl.in`.
- Triggers: FVWM module launch or Perl tooling using the installed `FVWM::*` packages.
- Responsibilities: Wrap startup argv, pipe framing, packet/event handling, masks, tracking, and callbacks.

**Utility Scripts and Binary:**
- Location: `bin/fvwm-root.c`, configured scripts in `bin/*.in`, and helper scripts in `utils/`.
- Triggers: Direct command-line use or FVWM configuration commands.
- Responsibilities: Set root images, generate menus, migrate config, expose command clients, and wrap installed Perl libraries (`bin/meson.build`).

## Architectural Constraints

- **Threading:** Keep the C core single-threaded and event-driven; multiplex X, module, timer, and session descriptors through `fvwm/events.c:4252`. `FvwmPrompt` is the exception and uses Go goroutines for socket I/O (`bin/FvwmPrompt/FvwmPrompt.go:101-102`).
- **Global state:** Expect broad mutation of `Scr`, `dpy`, X atoms, monitor collections, and subsystem globals (`fvwm/fvwm3.c:135-199`, `libs/FScreen.h:153-180`). Preserve initialization order in `fvwm/fvwm3.c` when adding state.
- **X ownership:** Only the core process claims `SubstructureRedirectMask` on the root (`fvwm/fvwm3.c:2228-2243`). Modules may open X connections for their own windows but must command core-managed clients through module IPC (`modules/FvwmPager/FvwmPager.c:281-298`).
- **Module ABI:** Packet layouts use native `unsigned long`, X `Window`, and pointer-sized fields (`libs/Module.h:16-35`, `libs/vpacket.h`). Change producers, C consumers, and generated Perl constants together; protocol compatibility is not a serialized network format.
- **Bounded work:** Preserve X-event and module-command budgets so either source cannot starve descriptor polling (`fvwm/event_loop_policy.c`, `fvwm/module_interface.c:812-823`).
- **Module backpressure:** Keep outbound data queued nonblocking and below `MODULE_QUEUE_MAX_BYTES`; slow modules are killed when they exceed the limit (`fvwm/module_list.h:70`, `fvwm/module_list.c:447-473`).
- **Parser depth and ownership:** Respect `MAX_FUNCTION_DEPTH`, parser context creation/destruction, and execution-context create/clone/destroy pairing (`fvwm/cmdparser_old.c:105-166`, `fvwm/execcontext.c`).
- **Generated configuration:** Every C implementation must include generated `config.h` first; capability macros originate from `meson.build:96-452` and the portability tail in `config_defines.h` (`dev-docs/DEVELOPERS.md:141-150`).
- **Circular imports:** C header relationships are highly interconnected rather than runtime imports; include guards prevent textual cycles, while forward declarations such as `struct fmodule` in `fvwm/execcontext.h:49` reduce direct coupling. Do not introduce cross-layer `.c` inclusion.

## Anti-Patterns

### Including Implementation Files

**What happens:** `fvwm/module_list.c:50` includes `libs/System.c` directly while the same implementation also belongs to `libfvwm3` through `libs/meson.build:35`.
**Why it's wrong:** Compilation-unit boundaries become dependent on static-library extraction behavior, and internal/static definitions can leak into an unrelated module-host unit.
**Do this instead:** Include declarations from `libs/System.h` as already done by most callers, and add any missing public declaration there (`libs/System.h`, `libs/System.c`).

### Bypassing the Shared Command Engine

**What happens:** Some internal adapters can be tempted to call movement, focus, or geometry implementations directly even though external actions are represented as FVWM command text; EWMH and module paths already converge on command execution (`dev-docs/PARSING.md:33-39`, `fvwm/module_interface.c:771`).
**Why it's wrong:** Direct calls omit `exec_context_t`, command flags, deferral rules, variable expansion, return codes, and complex-function semantics implemented in `fvwm/functions.c:392-740`.
**Do this instead:** Create a correctly typed execution context and call `execute_function()` or an override wrapper from `fvwm/functions.h`.

### Editing Protocol Structures in Isolation

**What happens:** `FvwmWindow` fields used in module packets carry explicit comments requiring synchronized edits to packet macros and layouts (`fvwm/fvwm.h:782-795`, `fvwm/fvwm.h:843-887`).
**Why it's wrong:** Core and module processes can silently disagree about native binary field positions, producing corrupted window state.
**Do this instead:** Update `fvwm/module_interface.c`, `libs/vpacket.h`, `libs/Module.h`, C module consumers, and Perl constants together; exercise module runtime tests in `tests/test_module_runtime.c`.

### Mutating Core Lists Directly

**What happens:** Managed windows participate in both a null-terminated list and a circular stacking ring (`fvwm/add_window.c:2258-2275`), while modules have active/death-row lists with input reference pinning (`fvwm/module_runtime.c:16-18`).
**Why it's wrong:** Updating only one index leaves stale X contexts, broken stack traversal, or freed module records referenced by queued commands.
**Do this instead:** Use lifecycle helpers in `fvwm/add_window.c`, `fvwm/stack.c`, and `fvwm/module_runtime.c`; use iterator APIs in `fvwm/module_list.h`.

## Error Handling

**Strategy:** Use explicit C return/status values for recoverable failures, centralized logging for diagnostics, selective process exit for bootstrap/protocol invariants, and deferred cleanup for event-loop resources (`libs/log.h`, `fvwm/module_runtime.c`).

**Patterns:**
- Return `NULL`, `False`, or typed status enums from fallible helpers and let the owning layer decide whether to continue (`fvwm/add_window.c:2158-2171`, `fvwm/module_list.h:106-111`).
- Log through `fvwm_debug(__func__, ...)` in C core paths rather than printing directly (`libs/log.h`, `fvwm/fvwm3.c:2014-2018`).
- Filter expected asynchronous X errors such as `BadWindow` in `FvwmErrorHandler`; terminate only when root redirection fails or the X connection is lost (`fvwm/fvwm3.c:1375-1433`).
- Mark failed modules for death-row cleanup rather than freeing records during iteration or while queued inputs retain references (`fvwm/module_runtime.c:141-165`).
- Enforce protocol bounds before allocation or copying (`fvwm/module_runtime.c:199-210`, `libs/Module.c:79-84`).
- In module executables, report startup misuse and terminate when FVWM-provided argv or pipe input is invalid (`modules/FvwmAuto/FvwmAuto.c:133-145`, `modules/FvwmPager/FvwmPager.c:143-146`).

## Cross-Cutting Concerns

**Logging:** Use `libs/log.c`/`libs/log.h` and `fvwm_debug()` in core/shared C code; process-level modules also use `stderr`, while `FvwmPrompt` uses Logrus (`fvwm/fvwm3.c:1766`, `modules/FvwmMFL/FvwmMFL.c`, `bin/FvwmPrompt/FvwmPrompt.go:47`).

**Validation:** Validate X resources at point of use, parser nesting and token lengths in parser helpers, module frame sizes in `fvwm/module_runtime.c`, packet sizes in `libs/Module.c`, and socket path ownership/modes in `modules/FvwmMFL/FvwmMFL.c:737-880`.

**Authentication:** There is no remote identity layer. Access is local: the X server controls display access, anonymous child modules inherit private pipe descriptors, and `FvwmMFL` restricts its runtime directory and socket to the owning user (`modules/FvwmMFL/FvwmMFL.c:773-880`).

**Internationalization:** Initialize locale/gettext before user-visible startup work and wrap translatable strings with the support API (`fvwm/fvwm3.c:1726-1727`, `libs/FGettext.c`, `po/meson.build`).

**Portability:** Put feature detection in `meson.build`, conditional compilation and fallbacks in `config_defines.h`/`libs/`, and platform wrappers behind `F*` interfaces (`meson.build:135-429`, `libs/FShape.c`, `libs/FRender.c`).

---

*Architecture analysis: 2026-08-29*
