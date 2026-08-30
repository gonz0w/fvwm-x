<!-- GSD:project-start source:PROJECT.md -->

## Project

**FVWM-X**

FVWM-X is an internal, modernized X11 window manager derived from FVWM3. It
keeps FVWM's efficient event-driven core and process-isolated module model while
making multi-monitor, mixed-DPI, input, configuration, rendering, and
accessibility behavior reliable for current desktop hardware.

The first milestone establishes monitor and HiDPI foundations for users with
mixed-density XRandR layouts.

**Core Value:** Users can depend on a responsive X11 desktop whose windows remain usable and
visually consistent across changing monitor layouts and scale factors.

### Constraints

- **Platform**: X11/XRandR 1.5+ only — native Wayland is explicitly excluded
- **Architecture**: Keep authoritative X state single-threaded — avoid races
  and preserve existing event-loop ordering

- **Responsiveness**: No monitor or scale operation may introduce blocking work
  into the main event loop

- **Rendering**: Scale FVWM-owned UI and geometry, not arbitrary client pixels
- **Compositing**: Integrate cleanly with external compositors; do not add one
- **Compatibility**: Prefer a coherent FVWM-X contract over legacy config or
  module compatibility when they conflict

- **Verification**: Pure policy requires native unit coverage; X-visible
  behavior requires nested-X integration coverage on native Linux CI

- **Environment**: Local macOS verification uses Podman x86_64 emulation;
  AddressSanitizer remains a native Linux CI responsibility
<!-- GSD:project-end -->

<!-- GSD:stack-start source:codebase/STACK.md -->

## Technology Stack

## Languages

- C11 - The `fvwm3` window-manager process, shared static library, native utilities, and 13 core modules are compiled from `fvwm/`, `libs/`, `modules/`, and `bin/fvwm-root.c`; the standard is fixed by `c_std=c11` in `meson.build`.
- Project version 1.1.6 - The release version comes from `.current-version` and is injected into the Meson project by `meson.build`.
- Python 3 - Required at configure time for generated shebangs and used at runtime by `bin/FvwmCommand.in` and `bin/fvwm-menu-desktop.in`; no minimum Python 3 minor version is declared in `meson.build`.
- Perl 5 - Required at configure time and used by `modules/FvwmPerl/FvwmPerl.in`, `perllib/FVWM/`, `perllib/General/`, and menu/helper scripts in `bin/`; the reusable Perl modules retain a `use 5.004` floor in files such as `perllib/FVWM/Tracker.pm`.
- Go 1.21+ - Optional compiled `FvwmPrompt` command shell in `bin/FvwmPrompt/FvwmPrompt.go`; both `meson.build` and `bin/FvwmPrompt/go.mod` require Go 1.21.
- Bash - The developer build wrapper `build` and release/build helpers including `meson-scripts/dist-tarball.sh` use Bash.
- POSIX shell - Portable utility and documentation scripts such as `meson-scripts/generate-manpages-website.sh`, `utils/fvwm-version-str.sh`, and `utils/fvwm_make_directory_menu.sh` use `/bin/sh`.
- Meson DSL - Build definitions are rooted at `meson.build`, with component definitions in paths such as `fvwm/meson.build`, `libs/meson.build`, and `modules/FvwmMFL/meson.build`.
- AsciiDoc - User and module documentation sources live in `doc/*.adoc` and are transformed by the optional documentation targets in `doc/meson.build`.
- Gettext PO - Translation source and compiled catalogs live in `po/*.po`, `po/fvwm3.pot`, and `po/*.gmo`; Meson's gettext integration is configured in `po/meson.build`.
- Nix - An optional, unpinned development shell is described by `shell.nix`.

## Runtime

- Native Unix process on an X11 display - `fvwm/fvwm3.c` opens the display with `XOpenDisplay`, takes the root-window manager role, and drives a file-descriptor/event loop; this is an X11 window manager, not a Wayland compositor.
- Unix-like operating systems - Linux is the primary CI platform in `.github/workflows/ccpp.yml`; portability branches for Darwin and BSD exist in `meson.build`, `config_defines.h`, and `libs/meson.build`.
- X server connection - `DISPLAY` or the `-d`/`--display` option is required for an operational session, as enforced in `fvwm/fvwm3.c` and `bin/FvwmCommand.in`.
- Multi-process module model - `fvwm/module_list.c` starts modules with `fork`/`execvp` and communicates through two nonblocking pipes using the packet contract in `libs/Module.h`.
- Optional Go runtime component is statically built as a native executable by `bin/FvwmPrompt/meson.build`; it does not require a Go runtime after compilation.
- Native dependencies are discovered through Meson/pkg-config in `meson.build` and supplied by the operating-system package manager; there is no repository-level native dependency lockfile.
- Go modules manage `bin/FvwmPrompt/` dependencies using `bin/FvwmPrompt/go.mod` and `bin/FvwmPrompt/go.sum`.
- Go lock/checksum data: present at `bin/FvwmPrompt/go.sum`; builds use the committed vendor tree under `bin/FvwmPrompt/vendor/` via `-mod=vendor` in `bin/FvwmPrompt/meson.build`.
- Nix development dependencies are listed in `shell.nix`, but there is no `flake.lock` or other Nix pin.

## Frameworks

- Xlib/Xt/Xft - Required native X11 UI and window-management foundation, used directly throughout `fvwm/`, `libs/fvwm_x11.h`, and native modules under `modules/`; there is no higher-level GUI application framework.
- libevent 2.0+ - Event dispatch, Unix-domain socket listeners, buffered clients, timers, and signal events in `modules/FvwmMFL/FvwmMFL.c` and `modules/FvwmForm/FvwmForm.c`.
- Internal FVWM module framework - Binary event packets and text commands are defined by `libs/Module.h` and implemented by `libs/Module.c`, while process lifecycle and pipe transport are managed in `fvwm/module_list.c` and `fvwm/module_interface.c`.
- Internal widget toolkit - `modules/FvwmScript/Widgets/` is compiled into a static library by `modules/FvwmScript/meson.build`; widgets draw directly through X11 rather than using GTK or Qt.
- Meson test runner - Native assertion-based C executables are registered in `tests/meson.build` for image-size safety, module framing/runtime, and event-loop policy.
- GCC/Clang sanitizers - AddressSanitizer and UndefinedBehaviorSanitizer runs are configured in `.github/workflows/ccpp.yml`.
- Meson 1.5.1+ - Configures native feature detection, generated files, install layout, tests, documentation, and distribution archives from `meson.build` and `meson.options`.
- Ninja - Executes builds through `build` and is the backend expected by `INSTALL.md`; no minimum version is declared.
- pkg-config - Resolves native X11, rendering, font, internationalization, and event dependencies declared by `dependency()` calls in `meson.build`.
- GCC and Clang - Both compilers are exercised on Ubuntu and Alpine in `.github/workflows/ccpp.yml`.
- Asciidoctor - Optional HTML and man-page generation in `doc/meson.build`; enabled by `-Dhtmldoc=true` and/or `-Dmandoc=true` from `meson.options`.
- Docker - `Dockerfile` builds inside `fvwmorg/fvwm3-build:latest`; CI chooses that image or `fvwmorg/fvwm3-build-alpine:latest` in `.github/workflows/ccpp.yml`.
- Nix shell - `shell.nix` offers a local dependency environment equivalent to the documented native build prerequisites.
- clang-format/EditorConfig - C formatting and whitespace defaults are recorded in `.clang-format` and `.editorconfig`.

## Key Dependencies

- `x11` - Core X server connection, windows, events, properties, input, and drawing throughout `fvwm/` and `libs/fvwm_x11.h`; required by `meson.build`.
- `xrandr` >=1.5 - Required multi-monitor discovery and change events in `libs/FScreen.c` and `fvwm/events.c`.
- `xext` - Required X Shape extension support exposed through `libs/FShape.h` and enabled unconditionally in `meson.build`.
- `xft` and `fontconfig` - Required client-side font matching/rendering and fallback logic in `libs/Fft.c` and `libs/fvwm_x11.h`.
- `xt` - Required X Toolkit Intrinsics interfaces included through `libs/fvwm_x11.h` and native modules such as `modules/FvwmButtons/button.c`.
- `ice` and `xtrans` - Required X Inter-Client Exchange/transport libraries; session-management implementation is in `fvwm/session.c` and `libs/fsm.c`.
- `xkbcommon` - Required build dependency declared in `meson.build`; X keyboard integration itself is exposed through Xlib/XKB wrappers in `libs/fvwm_x11.h` and modifier handling in `fvwm/bindings.c`.
- `libevent` >=2.0 - Required by `meson.build`, with direct consumers in `modules/FvwmMFL/FvwmMFL.c` and `modules/FvwmForm/FvwmForm.c`.
- Python 3, Perl, and sed - Required build-time programs discovered in `meson.build`; Python and Perl are also interpreters for installed utilities configured by `bin/meson.build`, `perllib/meson.build`, and module build files.
- cJSON 1.7.17 - Vendored C JSON implementation in `libs/cJSON.c` and `libs/cJSON.h`, linked through `libs/meson.build` and used for FvwmMFL/status payloads in `modules/FvwmMFL/FvwmMFL.c` and `fvwm/builtins.c`.
- FreeType 2 - Optional font functionality enabled through `-Dfreetype` in `meson.options` and detected in `meson.build`.
- FriBidi >=0.19.2 - Optional bidirectional text conversion and shaping in `libs/FBidi.c` and `libs/BidiJoin.c`.
- iconv and libintl/gettext - Optional character-set conversion and NLS in `libs/Ficonv.c`, `libs/Flocale.c`, and `libs/FGettext.c`.
- libpng >=1.2 - Optional PNG icon/image decoder in `libs/PictureImageLoader.c`; default configuration images under `default-config/images/` expect PNG support.
- librsvg >=2.13.92 plus one of Cairo, Cairo SVG, or libsvg-cairo - Optional SVG decoding/rendering in `libs/PictureImageLoader.c` and wrappers in `libs/Fsvg.h`.
- X Session Management (`sm`) - Optional XSMP client/server support in `fvwm/session.c` and `libs/fsm.c`.
- Xcursor - Optional themed/animated cursor loading in `libs/PictureImageLoader.c` and `libs/Fcursor.h`.
- XFixes - Optional pointer barriers for monitor edges in `fvwm/cursor.c`.
- XPM - Optional X PixMap image support through `libs/Fxpm.h` and `libs/PictureImageLoader.c`.
- XRender - Optional alpha/compositing operations initialized in `libs/FRenderInit.c` and implemented in `libs/FRender.c`.
- Asciidoctor - Optional documentation compiler used only when documentation options are enabled in `meson.options`.
- `github.com/ergochat/readline` 0.1.3 - Interactive line editing and history in `bin/FvwmPrompt/FvwmPrompt.go`.
- `github.com/fatih/color` 1.13.0 - Colored interactive prompt output in `bin/FvwmPrompt/FvwmPrompt.go`.
- `github.com/sirupsen/logrus` 1.9.4 - FvwmPrompt error logging in `bin/FvwmPrompt/FvwmPrompt.go`.
- `github.com/mattn/go-colorable` 0.1.9, `github.com/mattn/go-isatty` 0.0.14, `golang.org/x/sys` 0.15.0, and `golang.org/x/text` 0.9.0 - Indirect, vendored dependencies pinned by `bin/FvwmPrompt/go.mod` and `bin/FvwmPrompt/go.sum`.
- `python-xdg` - Parses Freedesktop menu, desktop-entry, icon-theme, locale, and base-directory data in `bin/fvwm-menu-desktop.in`.
- Pillow or ImageMagick (`identify`/`convert`) - Optional icon scaling fallback selected by `bin/fvwm-menu-desktop.in`.
- Core/CPAN Perl modules - Standard modules such as `Getopt::Long`, `IO::Select`, and `Config` are used in `bin/` and `perllib/`; the example status consumer additionally needs `JSON::XS` in `contrib/status/read_status.pl`.

## Configuration

- Runtime display selection uses `DISPLAY`, which `fvwm/fvwm3.c` normalizes and republishes together with `HOSTDISPLAY` for child processes.
- User data defaults to the FVWM user directory and is exposed as `FVWM_USERDIR`; installed shared data/module locations are compiled from Meson install paths in `meson.build` and made available to scripts through configured templates such as `perllib/FVWM/Module.pm.in`.
- Default startup behavior and assets are installed from `default-config/config` and `default-config/images/` according to `default-config/meson.build`.
- FvwmMFL endpoint discovery uses `FVWMMFL_SOCKET`, `FVWMMFL_SOCKET_PATH`, `XDG_RUNTIME_DIR`, and `TMPDIR` in `modules/FvwmMFL/FvwmMFL.c`, `bin/FvwmCommand.in`, and `bin/FvwmPrompt/FvwmPrompt.go`.
- Optional session management uses `SESSION_MANAGER`, `SESSION_MANAGER_NAME`, and `SM_SAVE_DIR` in `fvwm/session.c` and `libs/fsm.c`.
- Logging is opt-in through `-v`, `-o`, or `FVWM3_LOGFILE`, implemented in `fvwm/fvwm3.c` and `libs/log.c`.
- No `.env` files are present; this native desktop application uses process environment variables and FVWM configuration files rather than dotenv loading.
- `meson.build` is the authoritative build graph and dependency detector; `meson.options` controls optional text, image, session, Go, and documentation features.
- `build` is the preferred wrapper for Meson setup, Ninja compilation, documentation flags, install prefix, and installation, as documented in `INSTALL.md`.
- `config_defines.h` is combined with Meson feature probes to generate build-directory `config.h` through `meson-scripts/conf-cmd.sh`.
- `.current-version` and `utils/fvwm-version-str.sh` supply release and VCS version strings to `meson.build`.
- `Dockerfile` provides the container build path; `shell.nix` provides an alternative local dependency shell.
- `.editorconfig` and `.clang-format` define repository formatting; no separate lint framework configuration is present.
- `modules/FvwmScript/script.c` and `modules/FvwmScript/scanner.c` are committed generated Bison 2.3/Flex outputs, so Bison and Flex are not normal build prerequisites.

## Platform Requirements

- Install Meson >=1.5.1, Ninja, pkg-config, a C11 compiler, Python 3, Perl, sed, and the required X11/libevent development packages listed in `INSTALL.md` and detected by `meson.build`.
- Use `./build` for routine builds, or `meson setup <dir>` followed by `meson compile -C <dir>`; test execution is `meson test -C <dir>` as used by `.github/workflows/ccpp.yml`.
- Go >=1.21 is required only when `-Dgolang=enabled` or automatic detection builds `bin/FvwmPrompt/`; disable it with `-Dgolang=disabled` through `meson.options`.
- Asciidoctor is required only for `-Dmandoc=true` or `-Dhtmldoc=true`, as implemented in `doc/meson.build`.
- Linux and BSD are intended developer platforms per `dev-docs/DEVELOPERS.md`; Darwin-specific build handling exists in `meson.build` and `libs/meson.build`.
- Deploy as installed native executables, modules, scripts, catalogs, assets, and an X session entry; install paths are summarized and generated by `meson.build`, including `bin/`, versioned `libexec/fvwm3/<version>/`, `share/fvwm3/`, locale data, and `share/xsessions/fvwm3.desktop`.
- Requires a functioning X11/Xorg-compatible display and the required shared libraries resolved at build/package time; no application server, database, container orchestrator, or cloud runtime is involved.
- Wayland is not a native target; any use under a Wayland desktop requires an external X11 compatibility server because core operation is implemented through Xlib in `fvwm/fvwm3.c` and `libs/fvwm_x11.h`.
- Release artifacts are source tarballs produced by `meson dist` in `.github/workflows/release.yml`; distribution packaging is expected to supply native libraries and optional features.

<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->

## Conventions

## Naming Patterns

- Put core C implementation and header files in lowercase snake case, as in `fvwm/event_loop_policy.c`, `fvwm/event_loop_policy.h`, `fvwm/module_runtime.c`, and `fvwm/module_list.h`.
- Reserve an initial `F` in `libs/` filenames for wrappers around third-party libraries or FVWM-facing abstractions, as documented in `dev-docs/DEVELOPERS.md`; examples include `libs/FShape.c`, `libs/FBidi.c`, and `libs/FEvent.c`. Do not apply that prefix to ordinary helpers such as `libs/fqueue.c`.
- Name loadable module directories and primary entry files in `Fvwm<Name>` form, as in `modules/FvwmPager/FvwmPager.c` and `modules/FvwmButtons/FvwmButtons.c`; keep subsidiary files descriptive and lowercase, such as `modules/FvwmPager/fpmonitor.c`.
- Name standalone C tests `test_<unit>.c`, as in `tests/test_image_safety.c`, `tests/test_module_runtime.c`, and `tests/test_event_loop_policy.c`.
- Use `.pm` files and namespace-shaped directories for Perl packages, as in `perllib/FVWM/Module/Toolkit.pm`; generated/configured executable scripts use `.in`, as in `bin/fvwm-menu-desktop.in`.
- Treat `bin/FvwmPrompt/vendor/` as third-party code. Do not restyle it to match project-owned C, Go, or Perl files; its source list is consumed through `bin/FvwmPrompt/meson.build`.
- Use lower snake case for new C helpers and APIs, as in `fvwm/event_loop_policy.c` (`fvwm_bounded_queue_drain`) and `fvwm/module_runtime.c` (`module_output_queue_reserve`). Mark file-private helpers `static`, as in `module_list_insert()` in `fvwm/module_runtime.c`.
- Preserve established subsystem prefixes and legacy exported spellings instead of renaming public APIs: `EWMH_DoUpdateWmIcon()` in `fvwm/ewmh_icons.c`, `CMD_Send_ConfigInfo()` in `fvwm/modconf.c`, and `FImageSizeIsSafe()` in `libs/ImageSafety.c` are compatibility-facing names.
- Name test cases `static void test_<behavior>(void)`, and invoke them explicitly from `main()`, following `tests/test_event_loop_policy.c` and `tests/test_image_safety.c`.
- Follow Go conventions in `bin/FvwmPrompt/FvwmPrompt.go`: lower camel case for unexported functions and variables (`handleInput`, `cmdLineArgs`) and upper camel case only for exported fields needed by encoders (`ConnectionProfile`, `VersionInfo`).
- Use snake-case Perl methods and lexical names, such as `show_message`, `$error_msg`, and `$_dialog_tool` in `perllib/FVWM/Module/Toolkit.pm`; use `FVWM::...` package namespaces that match the path.
- Use descriptive lowercase snake case for new C locals and fields, as in `pixel_count` in `libs/ImageSafety.c` and `consecutive_events` in `fvwm/event_loop_policy.c`.
- Keep declarations close to the smallest useful block when matching modern C11 code such as `libs/ImageSafety.c`; in legacy files such as `fvwm/fvwm3.c`, retain the surrounding declaration-before-statements style to avoid unrelated churn.
- Preserve domain types: core/X11 code uses `Bool`, `True`, `False`, `Window`, and `None` in files such as `fvwm/module_runtime.c`, while self-contained standard-C helpers use `bool`, `true`, and `false` in `libs/ImageSafety.c`.
- Avoid introducing new unprefixed globals. Existing globals such as `Scr`, `dpy`, and `fvwm_userdir` in `fvwm/fvwm3.c` reflect legacy interfaces; new state should be passed through parameters or kept `static` as in `fvwm/module_runtime.c`.
- Use lowercase domain names and `_t` suffixes where the subsystem already does so, such as `fvwm_run_state_t` in `fvwm/fvwm3.c`, `fvwm_queue_consumer_t` in `fvwm/event_loop_policy.h`, and `cond_rc_t` in `fvwm/condrc.h`.
- Struct typedefs may use either anonymous structs (`fvwm_queue_drain_state` in `fvwm/event_loop_policy.h`) or named tags needed for self-reference (`fmodule_store` in `fvwm/module_list.h`); follow the local header's pattern.
- Use uppercase snake case for enum members, constants, and macros, as in `FMODULE_INPUT_READY` and `MODULE_QUEUE_MAX_BYTES` in `fvwm/module_list.h`.
- Use uppercase include guards derived from subsystem and filename, such as `FVWM_EVENT_LOOP_POLICY_H` in `fvwm/event_loop_policy.h` and `FVWMLIB_QUEUE_H` in `libs/fqueue.h`.

## Code Style

- Format project-owned C and headers with the repository root `.clang-format`. It is LLVM-based with Mozilla brace placement, tabs of width 8, an 80-column limit, right-aligned pointers, sorted includes, and no one-line functions, blocks, conditionals, or loops.
- Use tabs for indentation in `*.c` and `*.h`, spaces only where alignment requires them, UTF-8, LF endings, no trailing whitespace, and a final newline, as specified by `.editorconfig`.
- Use four spaces for Meson files, as specified for `meson.build` in `.editorconfig`; examples are `tests/meson.build` and `bin/FvwmPrompt/meson.build`.
- Put opening braces on the next line for C functions and control blocks, as demonstrated in `fvwm/event_loop_policy.c` and `tests/test_module_runtime.c`.
- Write pointer declarations with the `*` attached to the variable, for example `fmodule *module` in `fvwm/module_runtime.c`, matching `.clang-format`'s `PointerAlignment: Right`.
- Keep lines at or below 80 columns and indent continuations by one tab/four visual continuation spaces, following calls in `tests/test_event_loop_policy.c` and settings in `.clang-format`.
- Use `gofmt` conventions for `bin/FvwmPrompt/FvwmPrompt.go`; its imports are grouped into standard-library and third-party blocks. No repository-specific Go formatter configuration exists outside the source and `bin/FvwmPrompt/go.mod`.
- Match the surrounding Perl style because no Perl formatter configuration is present. Package modules such as `perllib/FVWM/Module/Toolkit.pm` use tabs, braces on the same line as control statements, `use strict`, and lexical `my` variables.
- Treat the Linux kernel coding guide as the human style baseline, per `dev-docs/DEVELOPERS.md`, while `.clang-format` and `.editorconfig` provide the executable whitespace rules.
- Build as C11 with Meson warning level 2, as configured in `meson.build`; project-wide `-fno-common`, `-Wno-sign-compare`, and `-Wno-unused-parameter` flags also come from `meson.build`.
- Do not rely on warnings being fatal locally because `werror=false` is set in `meson.build`. The GCC CI leg adds selected `-Werror` flags for ODR, LTO type mismatch, and strict aliasing in `.github/workflows/ccpp.yml`.
- No project-owned `clang-tidy`, `cppcheck`, `golangci-lint`, or automated format-check job is configured; `.github/workflows/ccpp.yml` validates compilation and tests. The `.golangci.yml` under `bin/FvwmPrompt/vendor/github.com/sirupsen/logrus/` belongs to vendored code.
- Codacy is an external review signal linked from `README.md` and referenced by `.github/PULL_REQUEST_TEMPLATE.md`; it does not replace the repository's Meson build/test gate in `.github/workflows/ccpp.yml`.

## Import Organization

- C code uses source-root-relative quoted includes such as `"libs/fqueue.h"` and `"fvwm/execcontext.h"`, enabled by include directories in `meson.build`; same-directory headers may use short names such as `"event_loop_policy.h"` in `fvwm/event_loop_policy.c`.
- Go imports use the module path `github.com/fvwmorg/FvwmPrompt` from `bin/FvwmPrompt/go.mod`; external packages are imported by canonical paths in `bin/FvwmPrompt/FvwmPrompt.go`.
- Perl modules use namespace imports corresponding to `perllib/`, such as `use FVWM::Module;` in `perllib/FVWM/Module/Toolkit.pm`.

## Error Handling

- Return explicit status values for recoverable C failures: `NULL`, `False`/`false`, `-1`, or a domain enum. `module_receive()` returns `FMODULE_INPUT_INCOMPLETE`, `FMODULE_INPUT_READY`, or `FMODULE_INPUT_ERROR` in `fvwm/module_runtime.c`; callers switch behavior through assertions in `tests/test_module_runtime.c`.
- Initialize output parameters before parsing and leave them in a safe state on failure, following `FEwmhIconFindBest()` in `libs/ImageSafety.c`, which clears `pixel_start`, `best_width`, and `best_height`.
- Validate sizes before arithmetic or allocation, as in `FImageSizeIsSafe()` in `libs/ImageSafety.c` and `module_output_queue_reserve()` in `fvwm/module_runtime.c`.
- Use a single cleanup/error label when multiple branches share teardown, as in the `err:` path of `module_receive()` in `fvwm/module_runtime.c`; make ownership and close/free behavior explicit.
- Use `fxmalloc()`, `fxcalloc()`, `fxrealloc()`, and `fxstrdup()` from `libs/safemalloc.c` where allocation failure is unrecoverable. These helpers terminate through `err()`/`errx()` rather than returning `NULL`.
- Log recoverable diagnostics with `fvwm_debug(__func__, ...)`, as in `fvwm/module_runtime.c`; do not silently swallow malformed external input when a useful diagnostic can be emitted.
- In Go, return from benign terminal conditions and use the shared `logrus.Logger` for fatal startup/connection failures, following `bin/FvwmPrompt/FvwmPrompt.go`. Check returned errors at call boundaries rather than discarding them in new code.
- In Perl, enable `use strict` and `use warnings` for new scripts, then use `die` for unrecoverable setup errors and `warn`/`STDERR` for nonfatal diagnostics, following `modules/FvwmMFL/test_fmd.pl` and `perllib/FVWM/Module/Toolkit.pm`.

## Logging

- Pass `__func__` as the first argument to C diagnostics so log records identify their origin, as in `fvwm/module_runtime.c` and `fvwm/ewmh_icons.c`.
- Use `fprintf(stderr, ...)` for command-line usage and direct user-facing errors, as in `fvwm/fvwm3.c`; use `fvwm_debug()` for runtime diagnostics governed by FVWM logging state in `libs/log.c`.
- Do not add unconditional debug prints. Existing compile-time/runtime debug switches in files such as `fvwm/events.c` are subsystem-specific and should be preserved rather than copied into new code.
- Keep fatal Go logging at process-boundary failures, such as `net.Dial` and readline initialization in `bin/FvwmPrompt/FvwmPrompt.go`; ordinary control flow should not call `log.Fatal`.

## Comments

- Explain invariants, ownership, compatibility behavior, and non-obvious policy rather than restating operations. Examples include queue locking semantics in `libs/fqueue.c` and input pinning comments in `fvwm/module_list.h`.
- Preserve the GPL notice at the beginning of new C source files and scripts, but not headers, as required by `dev-docs/DEVELOPERS.md`.
- Legacy C files use section banners such as those in `fvwm/fvwm3.c` and `modules/FvwmButtons/dynamic.c`; maintain them when editing those files, but do not add empty banners to compact files such as `fvwm/event_loop_policy.c`.
- Document every user-visible feature and option in the AsciiDoc manual under `doc/`, as required by `dev-docs/DEVELOPERS.md`; module documentation examples include `doc/FvwmPager.adoc` and `doc/FvwmButtons.adoc`.
- Not applicable. Public C APIs use concise block comments near declarations, as in `fvwm/module_list.h`; Perl library documentation uses POD after `__END__`, as in `perllib/FVWM/Module/Toolkit.pm`.

## Function Design

- Pass state explicitly through pointers and callback context, as in `fvwm_bounded_queue_drain()` in `fvwm/event_loop_policy.c`; use `const` for read-only buffers and strings, as in `FEwmhIconFindBest()` in `libs/ImageSafety.c`.
- Use output parameters only when multiple values must be returned, initialize them defensively, and make success/failure visible in the return value, following `FEwmhIconFindBest()` in `libs/ImageSafety.c`.
- Use `void` in parameter lists for no-argument C functions, as in `module_create(void)` in `fvwm/module_runtime.c` and every test case in `tests/test_image_safety.c`.
- Use boolean results for predicates and capacity checks, status enums for multi-state operations, pointers/`NULL` for lookup or ownership transfer, and `0`/negative integer conventions only where the surrounding API already uses them; examples are in `libs/ImageSafety.c`, `fvwm/module_runtime.c`, and `libs/System.c`.
- Keep cleanup idempotent where practical: `module_destroy(NULL)` and `module_input_discard(NULL)` return safely in `fvwm/module_runtime.c`.

## Module Design

<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->

## Architecture

## System Overview

```text

```

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

- Keep X server ownership and all authoritative window state in the single `fvwm3` process (`fvwm/fvwm3.c`, `fvwm/screen.h`, `fvwm/fvwm.h`).
- Route user configuration, bindings, menus, schedules, EWMH-generated actions, and module commands through the same command engine (`fvwm/functions.c:392`, `fvwm/read.c:115`, `fvwm/module_interface.c:724`).
- Extend user-visible behavior with separate module executables communicating over two one-way pipes instead of loading code into the core process (`fvwm/module_list.c:123`, `libs/Module.h:240`).
- Reuse platform, graphics, protocol, and parsing code through the non-installed static library declared in `libs/meson.build:73`.
- Represent managed clients with a large mutable `FvwmWindow` record linked both by management order and stacking order (`fvwm/fvwm.h:717`).
- Carry command origin, triggering event, target window, and originating module explicitly in immutable-style heap contexts (`fvwm/execcontext.h:22`, `fvwm/execcontext.c:121`).

## Layers

- Purpose: Own process startup, X display acquisition, configuration search, initialization order, restart, and shutdown.
- Location: `fvwm/fvwm3.c`, with session support in `fvwm/session.c`.
- Contains: `main()`, signal handlers, X error handlers, default command setup, and `StartupStuff()`.
- Depends on: Every core subsystem and most of `libs/` through explicit headers (`fvwm/fvwm3.c:32-76`).
- Used by: The installed `fvwm3` executable defined in `meson.build:501`.
- Purpose: Convert X events, module bytes, timers, session messages, and deferred signal requests into bounded units of core work.
- Location: `fvwm/events.c`, `fvwm/event_loop_policy.c`, `fvwm/schedule.c`, `fvwm/module_runtime.c`.
- Contains: `select()`-based descriptor polling, X event budgets, module command budgets, event jump tables, and scheduled-command execution.
- Depends on: Xlib/FEvent, module queues, session management, and global `Scr` flags (`fvwm/events.c:4208-4455`).
- Used by: `main()` through `HandleEvents()` (`fvwm/fvwm3.c:2407`).
- Purpose: Normalize all FVWM command text into built-in calls, complex-function execution, module config storage, or module process launch.
- Location: `fvwm/functions.c`, `fvwm/cmdparser_old.c`, `fvwm/functable.c`, `fvwm/functable_complex.c`, `fvwm/commands.h`.
- Contains: Parser hook interface, command metadata table, variable expansion, execution deferral, return-code propagation, and complex-function control flow.
- Depends on: `exec_context_t`, parsing helpers in `libs/Parse.c`, global styles/decor state, and individual `CMD_*` implementations throughout `fvwm/` (`fvwm/functions.c:392-740`).
- Used by: Config loading, X event handlers, schedules, menu actions, bindings, EWMH adapters, and modules (`fvwm/read.c:149`, `fvwm/module_interface.c:771`).
- Purpose: Maintain the authoritative model and implement window-manager behavior.
- Location: `fvwm/`.
- Contains: Client capture, frames, decorations, placement, movement, focus, stacking, styles, menus, virtual desktops, EWMH/ICCCM, icons, and update batching.
- Depends on: Xlib and support wrappers from `libs/`, command execution contexts, `ScreenInfo Scr`, and `FvwmWindow` (`fvwm/fvwm.h`, `fvwm/screen.h`).
- Used by: X event handlers and `CMD_*` built-ins registered in `fvwm/functable.c`.
- Purpose: Keep reusable X11, image, locale, data-structure, memory, logging, and module-protocol machinery outside core policy files.
- Location: `libs/`.
- Contains: `FScreen`, `FEvent`, `FImage`, `Picture*`, `FRender*`, `Flocale*`, `Module`, queues/lists/trees, safe allocation, and portability shims (`libs/meson.build:1-60`).
- Depends on: Required and optional libraries collected as `all_found_deps` in `meson.build:212-416`.
- Used by: The core executable, C modules, `fvwm-root`, and tests (`meson.build:501-508`, `modules/FvwmPager/meson.build`, `bin/meson.build:15-22`).
- Purpose: Add long-running UI and automation features without putting their loops or state into the core process.
- Location: `modules/FvwmAnimate/` through `modules/FvwmScript/`, plus the Perl support in `perllib/`.
- Contains: One executable-oriented subtree per module, typically a `main()`, message mask setup, config parsing, packet handlers, X event loop, and `meson.build` (`modules/FvwmPager/FvwmPager.c:136`, `modules/FvwmAuto/FvwmAuto.c:101`).
- Depends on: The ABI-like protocol in `libs/Module.h`, `libfvwm3`, and module-specific X facilities.
- Used by: `CMD_Module` and unknown-command fallback in the core command engine (`fvwm/functions.c:716`, `fvwm/module_list.c:123`).
- Purpose: Supply command-line helpers, default runtime configuration, generated scripts, man-page sources, and translations.
- Location: `bin/`, `default-config/`, `doc/`, `po/`, `utils/`, `meson-scripts/`.
- Contains: C, Go, Perl, shell, AsciiDoc, image, and gettext assets wired into Meson (`bin/meson.build`, `default-config/meson.build`, `doc/meson.build`, `po/meson.build`).
- Depends on: Configured installation paths from `meson.build:479-492`.
- Used by: Installed users, module processes, and release/documentation builds.

## Data Flow

### Primary Request Path: X Window Map

### Command Execution Flow

### Module Bidirectional Flow

### FvwmMFL JSON Socket Flow

- Treat `ScreenInfo Scr` and `Display *dpy` as the process-wide roots of core state; they are defined in `fvwm/fvwm3.c:137-138` and declared to the rest of `fvwm/` through core headers.
- Store managed clients in `Scr.FvwmRoot.next` and a separate circular stacking ring; update both through lifecycle/stack helpers rather than manually (`fvwm/screen.h:300-307`, `fvwm/add_window.c:2258-2275`, `fvwm/stack.c`).
- Store monitor/output state in global RB/TAILQ collections and attach each `FvwmWindow` to `m`/`m_prev` (`libs/FScreen.h:83-155`, `fvwm/fvwm.h:977-978`).
- Use flag-driven deferred updates for style, decoration, destruction, and redraw work at event-loop boundaries (`fvwm/events.c:4216-4239`, `fvwm/update.c:719`).
- Keep module process records and death-row cleanup private to `fvwm/module_runtime.c:16-18`; use the iterator and lifecycle APIs from `fvwm/module_list.h`.

## Key Abstractions

- Purpose: Authoritative aggregate for one managed X client, including client/frame windows, names, flags, geometry, styles, hints, icons, desktop/layer, EWMH data, and monitor association.
- Examples: `fvwm/fvwm.h:717-979`, construction in `fvwm/add_window.c:2122`.
- Pattern: Large mutable record with access/flag macros; pass `FvwmWindow *` through policy functions and never create partial instances outside the lifecycle code in `fvwm/add_window.c`.
- Purpose: Process-wide owner for root-window data, managed-window root, graphics contexts, defaults, bindings, functions, focus, styles, decorations, and deferred-work flags.
- Examples: `fvwm/screen.h:300-508`, definition in `fvwm/fvwm3.c:137`.
- Pattern: Global application context; initialize through `InitVariables()` and subsystem initializers in `fvwm/fvwm3.c:1072`.
- Purpose: Preserve command provenance and target information across parsing, expansion, built-ins, complex functions, and modules.
- Examples: `fvwm/execcontext.h:22-81`, `fvwm/execcontext.c`.
- Pattern: Allocate with `exc_create_context()`, derive with `exc_clone_context()`, pass as `const`, and pair every allocation with `exc_destroy_context()`.
- Purpose: Map lowercase command keywords to `CMD_*` callbacks, IDs, behavior flags, and target cursors.
- Examples: `fvwm/functable.c:30-55`, declarations in `fvwm/functable.h`, callbacks in `fvwm/commands.h`.
- Pattern: Table-driven command dispatch; register new built-ins in `fvwm/functable.c` and implement them with the shared `F_CMD_ARGS` signature from `fvwm/fvwm.h:61-65`.
- Purpose: Separate command-engine orchestration from parser implementation details.
- Examples: `fvwm/cmdparser_hooks.h`, active implementation `fvwm/cmdparser_old.c`, installation in `fvwm/functions.c:1232-1235`.
- Pattern: Vtable/strategy; keep parser-owned allocations inside create/destroy hooks and expose parser phases through `cmdparser_hooks_t`.
- Purpose: Define the cross-process module protocol and startup contract.
- Examples: `libs/Module.h:16-28`, `libs/Module.h:240-260`, implementation in `libs/Module.c`.
- Pattern: Binary packet stream from core to modules plus framed textual commands from modules to core; maintain protocol field widths and synchronized consumers when changing it.
- Purpose: Represent active XRandR outputs and their per-monitor virtual desktop state.
- Examples: `libs/FScreen.h:59-176`, implementation in `libs/FScreen.c`.
- Pattern: RB tree plus output-order TAILQ; resolve monitors through `monitor_*` helpers and update window ownership through `update_fvwm_monitor()` (`fvwm/update.c:665`).
- Purpose: Encapsulate third-party or X extension APIs behind FVWM-specific interfaces.
- Examples: `libs/FShape.c`, `libs/FRender.c`, `libs/FEvent.c`, `libs/FBidi.c`; naming rule in `dev-docs/DEVELOPERS.md:152-160`.
- Pattern: Put reusable third-party adaptation in `libs/F<Name>.c`/`.h`; reserve the `F` prefix for wrappers.

## Entry Points

- Location: `fvwm/fvwm3.c:1687`.
- Triggers: Desktop session startup or direct execution of installed `fvwm3`.
- Responsibilities: Initialize the parser/support layers, X connection, globals, EWMH/ICCCM, configuration, session state, and event loop.
- Location: One `main()` in each module subtree, such as `modules/FvwmPager/FvwmPager.c:136`, `modules/FvwmAuto/FvwmAuto.c:101`, and `modules/FvwmMFL/FvwmMFL.c:830`.
- Triggers: `Module <name>` or unknown-command fallback through `fvwm/module_list.c:123`.
- Responsibilities: Parse FVWM-provided descriptors, select packet masks, load module config, notify startup completion, and run a module-specific loop.
- Location: `modules/FvwmScript/FvwmScript.c`, generated scanner/parser sources in `modules/FvwmScript/scanner.c` and `modules/FvwmScript/script.c`, widget library in `modules/FvwmScript/Widgets/`.
- Triggers: `Module FvwmScript <script>` through the standard module launcher.
- Responsibilities: Parse FvwmScript language files and render/operate its widget set; build composition is in `modules/FvwmScript/meson.build`.
- Location: `bin/FvwmPrompt/FvwmPrompt.go:122`.
- Triggers: Direct user invocation after `FvwmMFL` publishes `FVWMMFL_SOCKET`.
- Responsibilities: Provide readline history/prompting, protect the `Quit` path with confirmation, and relay commands over the local socket.
- Location: Constructor and event API in `perllib/FVWM/Module.pm.in:91`, module executables such as `modules/FvwmPerl/FvwmPerl.in`.
- Triggers: FVWM module launch or Perl tooling using the installed `FVWM::*` packages.
- Responsibilities: Wrap startup argv, pipe framing, packet/event handling, masks, tracking, and callbacks.
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

### Bypassing the Shared Command Engine

### Editing Protocol Structures in Isolation

### Mutating Core Lists Directly

## Error Handling

- Return `NULL`, `False`, or typed status enums from fallible helpers and let the owning layer decide whether to continue (`fvwm/add_window.c:2158-2171`, `fvwm/module_list.h:106-111`).
- Log through `fvwm_debug(__func__, ...)` in C core paths rather than printing directly (`libs/log.h`, `fvwm/fvwm3.c:2014-2018`).
- Filter expected asynchronous X errors such as `BadWindow` in `FvwmErrorHandler`; terminate only when root redirection fails or the X connection is lost (`fvwm/fvwm3.c:1375-1433`).
- Mark failed modules for death-row cleanup rather than freeing records during iteration or while queued inputs retain references (`fvwm/module_runtime.c:141-165`).
- Enforce protocol bounds before allocation or copying (`fvwm/module_runtime.c:199-210`, `libs/Module.c:79-84`).
- In module executables, report startup misuse and terminate when FVWM-provided argv or pipe input is invalid (`modules/FvwmAuto/FvwmAuto.c:133-145`, `modules/FvwmPager/FvwmPager.c:143-146`).

## Cross-Cutting Concerns

<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->

## Project Skills

No project skills found. Add skills to any of: `.claude/skills/`, `.agents/skills/`, `.cursor/skills/`, `.github/skills/`, or `.codex/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->

## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:

- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->

<!-- GSD:profile-start -->

## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
