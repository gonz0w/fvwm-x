# Codebase Structure

**Analysis Date:** 2026-08-29

## Directory Layout

```text
fvwm3/
├── fvwm/                   # Core window-manager executable and policy
├── libs/                   # Static support library and module protocol API
├── modules/                # Out-of-process FVWM module executables
│   ├── FvwmAnimate/        # Window animation module
│   ├── FvwmAuto/           # Delayed focus/enter action module
│   ├── FvwmBacker/         # Desktop background/action module
│   ├── FvwmButtons/        # Configurable button-bar module
│   ├── FvwmEvent/          # Event-to-command binding module
│   ├── FvwmForm/           # Form/dialog module and templates
│   ├── FvwmIconMan/        # Icon/window manager module
│   ├── FvwmIdent/          # Window identification module
│   ├── FvwmMFL/            # Module Front Loader and JSON UDS bridge
│   ├── FvwmPager/          # Pager and monitor/desktop UI module
│   ├── FvwmPerl/           # Perl module launcher
│   ├── FvwmRearrange/      # Tiling/cascading module
│   └── FvwmScript/         # Script parser, runtime, widgets, and scripts
├── bin/                    # Installed utilities and configured scripts
│   └── FvwmPrompt/         # Go Unix-socket prompt client plus vendored deps
├── perllib/                # Reusable Perl FVWM module/event/tracker packages
├── default-config/         # Shipped config, images, and FvwmScript dialogs
├── doc/                    # AsciiDoc man-page and HTML source
├── po/                     # gettext source catalogs and generated catalogs
├── tests/                  # Meson-registered focused C tests
├── dev-docs/               # Contributor and internal design notes
├── contrib/                # Optional examples, migration rules, and tools
├── meson-scripts/          # Build/release/document generation helpers
├── utils/                  # Runtime/build shell helper scripts
├── .github/                # CI, release, issue, and PR configuration
├── meson.build             # Top-level build and dependency graph
├── meson.options           # Configurable Meson feature switches
├── config_defines.h        # Portability tail merged into generated config.h
├── build                   # Convenience wrapper around Meson/Ninja
├── Dockerfile              # Containerized build/runtime definition
├── shell.nix               # Nix development environment
├── README.md               # Project overview
├── INSTALL.md              # Build and installation instructions
└── CHANGELOG.md            # Release-facing change history
```

## Directory Purposes

**`fvwm/`:**
- Purpose: Contains all policy and orchestration compiled into the authoritative `fvwm3` process.
- Contains: Lowercase `.c`/`.h` pairs for events, command dispatch, window lifecycle, frames, decorations, placement, focus, stacking, styles, menus, virtual desktops, EWMH/ICCCM, sessions, and module hosting (`fvwm/meson.build`).
- Key files: `fvwm/fvwm3.c`, `fvwm/events.c`, `fvwm/functions.c`, `fvwm/fvwm.h`, `fvwm/screen.h`, `fvwm/functable.c`, `fvwm/add_window.c`, `fvwm/module_list.c`, `fvwm/module_runtime.c`, `fvwm/module_interface.c`.
- Boundary: Put window-manager decisions here; put generally reusable X/platform/protocol machinery in `libs/` (`libs/meson.build`).

**`libs/`:**
- Purpose: Provides the non-installed static `libfvwm3` used by the core, modules, tools, and tests.
- Contains: X extension wrappers (`FScreen`, `FEvent`, `FShape`, `FRender`), image/font/locale helpers, module protocol, parsers, logging, allocation, strings, queues/lists/trees, geometry, and portability shims (`libs/meson.build:1-79`).
- Key files: `libs/fvwmlib.h`, `libs/Module.h`, `libs/Module.c`, `libs/FScreen.h`, `libs/FScreen.c`, `libs/FEvent.h`, `libs/Picture.h`, `libs/Parse.h`, `libs/log.h`, `libs/safemalloc.h`.
- Boundary: Use capital `F` only for wrappers around third-party libraries/extensions, as required by `dev-docs/DEVELOPERS.md:152-160`.

**`modules/`:**
- Purpose: Houses separately launched extension processes connected to the core through the protocol in `libs/Module.h`.
- Contains: Thirteen feature subdirectories, each with its own sources and `meson.build`; complex modules co-locate private headers, parsers, widgets, scripts, samples, or config (`meson.build:510-529`).
- Key files: `modules/FvwmPager/FvwmPager.c`, `modules/FvwmButtons/FvwmButtons.c`, `modules/FvwmMFL/FvwmMFL.c`, `modules/FvwmScript/FvwmScript.c`, and each `modules/*/meson.build`.
- Boundary: Give every independent long-running extension its own `modules/Fvwm<Name>/` process instead of adding another loop to `fvwm/`.

**`modules/FvwmScript/`:**
- Purpose: Implements a script language and widget toolkit as one FVWM module.
- Contains: Runtime sources in the directory root, grammar sources `scanner.l` and `script.y`, checked-in generated C files `scanner.c` and `script.c`, reusable widgets in `modules/FvwmScript/Widgets/`, and installed examples in `modules/FvwmScript/Scripts/`.
- Key files: `modules/FvwmScript/FvwmScript.c`, `modules/FvwmScript/Instructions.c`, `modules/FvwmScript/meson.build`.

**`bin/`:**
- Purpose: Contains user-facing utilities installed in the normal executable directory rather than FVWM's versioned module directory.
- Contains: C utility `fvwm-root.c`; Perl/shell templates ending in `.in`; data file `fvwm-menu-desktop-config.fpl`; and Go project `bin/FvwmPrompt/` (`bin/meson.build`).
- Key files: `bin/meson.build`, `bin/fvwm-root.c`, `bin/FvwmCommand.in`, `bin/FvwmPrompt/FvwmPrompt.go`.

**`bin/FvwmPrompt/`:**
- Purpose: Builds the optional Go command prompt that communicates through `FvwmMFL`.
- Contains: One application source, `go.mod`, `go.sum`, source-discovery script, Meson custom target, and committed `vendor/` dependencies (`bin/FvwmPrompt/meson.build`).
- Key files: `bin/FvwmPrompt/FvwmPrompt.go`, `bin/FvwmPrompt/go.mod`, `bin/FvwmPrompt/vendor/`.

**`perllib/`:**
- Purpose: Provides a higher-level Perl implementation of the FVWM module protocol and state trackers.
- Contains: `FVWM::Module`, events/constants/commands, tracker classes, module toolkit/terminal helpers, and general parse/filesystem utilities (`perllib/meson.build`).
- Key files: `perllib/FVWM/Module.pm.in`, `perllib/FVWM/Event.pm`, `perllib/FVWM/Tracker.pm`, `perllib/FVWM/Tracker/WindowList.pm`, `perllib/FVWM/create-constants`, `perllib/FVWM/create-commands`.

**`default-config/`:**
- Purpose: Supplies the fallback user experience installed below FVWM's data directory.
- Contains: Main `config`, background and icon PNGs, tray config, and confirmation/date FvwmScript files (`default-config/meson.build`).
- Key files: `default-config/config`, `default-config/images/`, `default-config/FvwmScript-ConfirmQuit`.

**`doc/`:**
- Purpose: Stores source for installed man pages and optional HTML documentation.
- Contains: One `.adoc` per executable/module plus split core command/menu/style pages and a common source (`doc/meson.build`).
- Key files: `doc/fvwm3.adoc`, `doc/fvwm3_manpage_source.adoc`, `doc/fvwm3commands.adoc`, `doc/FvwmPager.adoc`, `doc/FvwmMFL.adoc`, `doc/meson.build`.
- Boundary: Document every user-visible feature in the corresponding AsciiDoc source, per `dev-docs/DEVELOPERS.md:168-173`.

**`po/`:**
- Purpose: Holds gettext translations for user-visible strings.
- Contains: `.po` source catalogs, `fvwm3.pot`, `LINGUAS`, `POTFILES`, helper script, and `.gmo` catalogs (`po/meson.build`).
- Key files: `po/POTFILES`, `po/LINGUAS`, `po/fvwm3.pot`, `po/meson.build`.

**`tests/`:**
- Purpose: Contains focused C tests wired to Meson's native test runner.
- Contains: Direct tests of image path safety, module runtime framing/lifecycle, and event-loop fairness policy (`tests/meson.build`).
- Key files: `tests/test_image_safety.c`, `tests/test_module_runtime.c`, `tests/test_event_loop_policy.c`.
- Boundary: Add core/shared unit tests here and explicitly declare each executable and `test()` in `tests/meson.build`.

**`dev-docs/`:**
- Purpose: Preserves contributor rules and internal architecture/design documentation.
- Contains: Parsing internals, developer conventions, proposed command design, desktop notes, and TODO material.
- Key files: `dev-docs/DEVELOPERS.md`, `dev-docs/PARSING.md`, `dev-docs/NEW-COMMANDS.md`, `dev-docs/TODO.md`.

**`contrib/`:**
- Purpose: Keeps useful but non-core examples and development aids outside installed core behavior.
- Contains: Coccinelle transformations, RandR diagnostic source, status-panel examples, and xsession desktop template (`contrib/`).
- Key files: `contrib/coccinelle/*.cocci`, `contrib/randr-tool/randr.c`, `contrib/xsession/fvwm3.desktop.in`.

**`meson-scripts/` and `utils/`:**
- Purpose: Separate build-time generation/release tasks from small FVWM-oriented shell helpers.
- Contains: Config-header merge, AsciiDoc extraction, distribution cleanup, website generation, version calculation, and menu scripts.
- Key files: `meson-scripts/conf-cmd.sh`, `meson-scripts/generate_ad_file.sh`, `meson-scripts/dist-tarball.sh`, `utils/fvwm-version-str.sh`.

**`.github/`:**
- Purpose: Defines hosted CI, release automation, dependency updates, and contribution templates.
- Contains: Workflows in `.github/workflows/`, issue templates, PR template, and Dependabot config.
- Key files: `.github/workflows/ccpp.yml`, `.github/workflows/release.yml`, `.github/PULL_REQUEST_TEMPLATE.md`.

## Key File Locations

**Entry Points:**
- `fvwm/fvwm3.c:1687`: Main window-manager process.
- `modules/FvwmPager/FvwmPager.c:136`: Representative X-aware C module entry point.
- `modules/FvwmAuto/FvwmAuto.c:101`: Representative pipe/timer-only C module entry point using `ParseModuleArgs()`.
- `modules/FvwmMFL/FvwmMFL.c:830`: Libevent Unix-domain socket bridge entry point.
- `bin/FvwmPrompt/FvwmPrompt.go:122`: Go prompt application entry point.
- `modules/FvwmPerl/FvwmPerl.in`: Perl module launcher template.
- `bin/fvwm-root.c`: Standalone C utility entry point.

**Configuration:**
- `meson.build`: Project definition, dependency detection, generated `config.h`, and top-level target graph.
- `meson.options`: Feature switches for graphics, NLS, sessions, docs, Go, and Python selection.
- `config_defines.h`: Static portability macros appended to generated configuration.
- `.clang-format`: C/C++ formatting profile.
- `.editorconfig`: Repository-wide newline/whitespace and C/Meson indentation rules.
- `default-config/config`: Installed fallback FVWM runtime configuration.
- `fvwm/ConfigFvwmDefaults`: Core defaults loaded from the data directory.

**Core Logic:**
- `fvwm/events.c`: X/module/session/timer event loop and X event handlers.
- `fvwm/functions.c`: Shared textual command execution engine.
- `fvwm/functable.c`: Built-in command registry.
- `fvwm/add_window.c`: Managed-client construction and initial placement.
- `fvwm/fvwm.h`: `FvwmWindow`, styles, flags, and command argument macros.
- `fvwm/screen.h`: `ScreenInfo` global state model.
- `fvwm/execcontext.h`: Command origin/target context.
- `fvwm/style.c`: Style parsing, merging, and lifecycle.
- `fvwm/frame.c`, `fvwm/borders.c`, `fvwm/decorations.c`: Frame geometry and rendering.
- `fvwm/placement.c`, `fvwm/move_resize.c`, `fvwm/stack.c`, `fvwm/focus.c`: Window policy.
- `fvwm/ewmh.c`, `fvwm/ewmh_events.c`, `fvwm/icccm2.c`: Desktop protocol adapters.
- `fvwm/module_list.c`, `fvwm/module_runtime.c`, `fvwm/module_interface.c`: Core side of module execution and IPC.

**Shared APIs:**
- `libs/Module.h`: Module packet constants, framing structures, startup args, and client API.
- `libs/vpacket.h`: Typed packet body layout shared with module consumers.
- `libs/FScreen.h`: Monitor/output/desktop model.
- `libs/FEvent.h`: X event wrappers and last-event support.
- `libs/Picture.h`, `libs/FImage.h`, `libs/FRenderInterface.h`: Image/rendering abstractions.
- `libs/Parse.h`: Token parsing API used by core and modules.
- `libs/log.h`, `libs/safemalloc.h`: Diagnostics and allocation contracts.

**Module Implementations:**
- `modules/FvwmButtons/`: Multi-file UI module organized by parsing, buttons, drawing, icons, and output.
- `modules/FvwmIconMan/`: Multi-file module organized by config, FVWM packet handling, window lists, X manager, and debug support.
- `modules/FvwmPager/`: Multi-file pager organized by initialization, monitor model, message handling, pager X behavior, and updates.
- `modules/FvwmScript/`: Script grammar/runtime with a private static widget library.
- `modules/FvwmMFL/FvwmMFL.c`: Single-file protocol-to-JSON bridge.

**Testing:**
- `tests/meson.build`: All registered test executables and test names.
- `tests/test_event_loop_policy.c`: Fairness/budget policy tests.
- `tests/test_module_runtime.c`: Module input framing, lifecycle, and queue-limit tests.
- `tests/test_image_safety.c`: Image path/input safety tests.
- `modules/FvwmMFL/test_fmd.pl`: Ad hoc FvwmMFL socket client/test script; it is not registered in `tests/meson.build`.

**Documentation:**
- `README.md`: Project summary and contributor links.
- `INSTALL.md`: Build requirements and commands.
- `dev-docs/DEVELOPERS.md`: Required style and file-placement conventions.
- `dev-docs/PARSING.md`: Detailed command and module parsing model.
- `doc/fvwm3_manpage_source.adoc`: Main source from which command/menu/style sections are extracted.
- `doc/Fvwm<Name>.adoc`: Module-specific user documentation.

## Naming Conventions

**Files:**
- Use lowercase underscore-separated names for core implementation/header pairs in `fvwm/`, such as `fvwm/event_loop_policy.c` and `fvwm/event_loop_policy.h`; the lowercase rule is explicit in `dev-docs/DEVELOPERS.md:152-156`.
- Use `F<Name>.c`/`.h` in `libs/` only for wrappers around third-party libraries or X extensions, such as `libs/FShape.c` and `libs/FBidi.c` (`dev-docs/DEVELOPERS.md:156-160`).
- Match module executable capitalization for module root files and docs: `modules/FvwmPager/FvwmPager.c`, `modules/FvwmPager/FvwmPager.h`, and `doc/FvwmPager.adoc`.
- Use lowercase concern names for private module files: `modules/FvwmButtons/parse.c`, `modules/FvwmButtons/draw.c`, `modules/FvwmPager/messages.c`.
- Use `.in` for files configured with installation paths or shebangs: `bin/FvwmCommand.in`, `modules/FvwmPerl/FvwmPerl.in`, `perllib/FVWM/Module.pm.in`.
- Use `test_<unit>.c` for C tests in `tests/`, such as `tests/test_module_runtime.c`.
- Name build declarations `meson.build` in every participating subtree and keep project options in top-level `meson.options`.

**Directories:**
- Use lowercase functional directories at repository level: `fvwm/`, `libs/`, `modules/`, `perllib/`, `default-config/`, `dev-docs/`.
- Use `Fvwm<Name>/` PascalCase for module subdirectories, matching their executables: `modules/FvwmIconMan/`.
- Mirror Perl namespaces as directories: `perllib/FVWM/Tracker/WindowList.pm` represents `FVWM::Tracker::WindowList`.
- Keep private module subcomponents in descriptive capitalized directories only where the module's existing convention requires it: `modules/FvwmScript/Widgets/` and `modules/FvwmScript/Scripts/`.

**Symbols:**
- Implement user commands as `CMD_<Command>` with `F_CMD_ARGS`, declare them in `fvwm/commands.h`, and register lowercase keywords in `fvwm/functable.c`.
- Name X event handlers `Handle<Event>` and register them in `InitEventHandlerJumpTable()` (`fvwm/events.c:4056`).
- Prefix private file-scope helpers with descriptive lowercase names or `_` where the surrounding file uses that convention, such as `_execute_command_line()` in `fvwm/functions.c`.
- Use `Fvwm*` for public core domain types (`FvwmWindow`, `FvwmPacket`, `FvwmPicture`) and `fvwm_*` for newer procedural helpers (`fvwm_bounded_queue_drain`) in `fvwm/event_loop_policy.c`.
- Use uppercase `M_*` and `MX_*` for module protocol message constants (`libs/Module.h`).

## Where to Add New Code

**New Core Window-Management Feature:**
- Primary code: Add a lowercase `.c`/`.h` pair under `fvwm/` when the feature owns window-manager policy or mutates `FvwmWindow`/`ScreenInfo`, following `fvwm/placement.c` or `fvwm/focus.c`.
- Build registration: Add the implementation to `fvwm_sources` in `fvwm/meson.build`.
- Command surface: Declare `CMD_<Name>` in `fvwm/commands.h` and add its lowercase descriptor to `fvwm/functable.c`.
- Documentation: Update `doc/fvwm3_manpage_source.adoc` or the matching `doc/fvwm3*.adoc` page.
- Tests: Add `tests/test_<feature>.c` and register its executable/test in `tests/meson.build`.

**New Built-In Command:**
- Primary code: Place the implementation in the existing owning file under `fvwm/` rather than creating a generic command dump; examples are geometry commands in `fvwm/move_resize.c` and focus commands in `fvwm/focus.c`.
- Interface: Use `F_CMD_ARGS` from `fvwm/fvwm.h:61`, declaration in `fvwm/commands.h`, and `CMD_ENT` registration in sorted `fvwm/functable.c`.
- Parsing: Reuse `libs/Parse.h` and preserve `exec_context_t`; do not parse at the event/module adapter before `fvwm/functions.c`.
- Documentation: Add syntax and semantics to `doc/fvwm3_manpage_source.adoc`.

**New Independent Module:**
- Implementation: Create `modules/Fvwm<Name>/`, with `Fvwm<Name>.c` as entry point, private lowercase files by concern, and local `meson.build` following `modules/FvwmPager/` or `modules/FvwmButtons/`.
- Protocol: Use `ParseModuleArgs()`, `SetMessageMask()`, `ReadFvwmPacket()`, `SendText()`, and `SendFinishedStartupNotification()` from `libs/Module.h`.
- Build registration: Add the module name/subdirectory to the `modules` dictionary in top-level `meson.build:510-525`.
- Configuration: Co-locate module-specific shipped config in its module directory and install it from that module's `meson.build`, as in `modules/FvwmButtons/meson.build`.
- Documentation: Add `doc/Fvwm<Name>.adoc` and list it in `doc/meson.build`.

**New Shared X/Platform Abstraction:**
- Implementation: Add `.c`/`.h` under `libs/`; use `F<Name>` only if wrapping an external/X API, otherwise use a descriptive lowercase name (`libs/fqueue.c`).
- Build registration: Add the `.c` source to `libfvwm3_sources` in `libs/meson.build`.
- Capability detection: Add dependency/header/function checks to top-level `meson.build` and feature controls to `meson.options` when optional.
- Tests: Add a focused direct-source or `libfvwm3`-linked test in `tests/meson.build`.

**New FvwmScript Widget or Script:**
- Widget implementation: Add `modules/FvwmScript/Widgets/<Widget>.c` and any header, then register it in `libwidget_sources` in `modules/FvwmScript/meson.build`.
- Installed script: Add `modules/FvwmScript/Scripts/FvwmScript-<Name>` and include it in `install_scripts` in `modules/FvwmScript/meson.build`.
- Runtime grammar: Edit `modules/FvwmScript/scanner.l` or `modules/FvwmScript/script.y` and keep the corresponding checked-in generated C source synchronized.

**New User Utility:**
- C utility: Add source under `bin/`, link it with `libfvwm3` in `bin/meson.build`, and add `doc/<utility>.adoc`.
- Configured Perl/shell utility: Add `<name>.in` under `bin/` and configure/install it through `bin/meson.build` using `file_config`.
- Go utility: Use a self-contained project below `bin/<Name>/` and a Meson custom target like `bin/FvwmPrompt/meson.build`; keep dependencies vendored when the build uses `-mod=vendor`.

**New Default Asset or Configuration Fragment:**
- Runtime file: Place it below `default-config/` in the matching functional subdirectory.
- Build registration: Add its repository-relative path to `default_conf_files` in `default-config/meson.build` with preserved paths.
- Consumer: Reference it through `FVWM_DATADIR` in `default-config/config` or core defaults in `fvwm/fvwm3.c:1292`.

**Utilities:**
- Shared C helpers: Use `libs/` and expose a narrow header; do not add generic reusable functions to `fvwm/misc.c` when they do not depend on core state.
- Build-only shell helpers: Use `meson-scripts/`, referenced from `meson.build` or `doc/meson.build`.
- Installed/runtime shell helpers: Use `utils/` or configured scripts in `bin/`, depending on whether they are installed (`bin/meson.build`, `utils/`).

## Special Directories

**`bin/FvwmPrompt/vendor/`:**
- Purpose: Contains complete third-party Go source needed by the `-mod=vendor` build in `bin/FvwmPrompt/meson.build:23`.
- Generated: Yes, from Go module metadata.
- Committed: Yes; treat it as external code and make application changes in `bin/FvwmPrompt/FvwmPrompt.go`.

**`po/`:**
- Purpose: Contains translation sources and compiled gettext catalogs used by `i18n.gettext()` in `po/meson.build`.
- Generated: Mixed; `.po`, `LINGUAS`, and `POTFILES` are source/config, while `fvwm3.pot` and `*.gmo` are generation outputs.
- Committed: Yes for the files present under `po/`; update them with the translation workflow rather than hand-editing `.gmo` binaries.

**`modules/FvwmScript/Widgets/`:**
- Purpose: Private widget implementation compiled into the `Widgets` static library used only by `FvwmScript` (`modules/FvwmScript/meson.build:22-30`).
- Generated: No.
- Committed: Yes.

**`modules/FvwmScript/Scripts/`:**
- Purpose: Shipped examples and utilities for the FvwmScript runtime.
- Generated: Mixed; most are sources, while `FvwmScript-ComExample` is configured from `FvwmScript-ComExample.in` during the build (`modules/FvwmScript/meson.build:76-91`).
- Committed: Source files are committed; configured output is ignored by `.gitignore`.

**`default-config/images/`:**
- Purpose: Shipped backgrounds, title/window action icons, and application icons used by the fallback config.
- Generated: No.
- Committed: Yes; installation paths are enumerated in `default-config/meson.build`.

**`doc/`:**
- Purpose: Source and generated intermediates for man/HTML documentation.
- Generated: Mixed; `.adoc` files are sources, while `fvwm3_commands.ad`, `fvwm3_menus.ad`, `fvwm3_styles.ad`, `.1`, `.html`, and `doc/index.adoc` are build outputs defined in `doc/meson.build`.
- Committed: Source `.adoc` files are committed; generated pages/intermediates are ignored by `.gitignore`.

**Out-of-tree build directory (default `compile/`):**
- Purpose: Holds Meson/Ninja object files, generated `config.h`, configured scripts, binaries, and documentation.
- Generated: Yes; the default name comes from `build:9`.
- Committed: No; keep build artifacts outside source directories and do not add generated `config.h` to the repository (`.gitignore`).

**`.planning/codebase/`:**
- Purpose: Stores GSD codebase maps used by planning and execution commands.
- Generated: Yes, from repository analysis.
- Committed: Managed by the GSD orchestrator; mapper agents write files but do not commit them.

---

*Structure analysis: 2026-08-29*
