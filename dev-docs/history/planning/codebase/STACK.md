# Technology Stack

**Analysis Date:** 2026-08-29

## Languages

**Primary:**
- C11 - The `fvwm3` window-manager process, shared static library, native utilities, and 13 core modules are compiled from `fvwm/`, `libs/`, `modules/`, and `bin/fvwm-root.c`; the standard is fixed by `c_std=c11` in `meson.build`.
- Project version 1.1.6 - The release version comes from `.current-version` and is injected into the Meson project by `meson.build`.

**Secondary:**
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

**Environment:**
- Native Unix process on an X11 display - `fvwm/fvwm3.c` opens the display with `XOpenDisplay`, takes the root-window manager role, and drives a file-descriptor/event loop; this is an X11 window manager, not a Wayland compositor.
- Unix-like operating systems - Linux is the primary CI platform in `.github/workflows/ccpp.yml`; portability branches for Darwin and BSD exist in `meson.build`, `config_defines.h`, and `libs/meson.build`.
- X server connection - `DISPLAY` or the `-d`/`--display` option is required for an operational session, as enforced in `fvwm/fvwm3.c` and `bin/FvwmCommand.in`.
- Multi-process module model - `fvwm/module_list.c` starts modules with `fork`/`execvp` and communicates through two nonblocking pipes using the packet contract in `libs/Module.h`.
- Optional Go runtime component is statically built as a native executable by `bin/FvwmPrompt/meson.build`; it does not require a Go runtime after compilation.

**Package Manager:**
- Native dependencies are discovered through Meson/pkg-config in `meson.build` and supplied by the operating-system package manager; there is no repository-level native dependency lockfile.
- Go modules manage `bin/FvwmPrompt/` dependencies using `bin/FvwmPrompt/go.mod` and `bin/FvwmPrompt/go.sum`.
- Go lock/checksum data: present at `bin/FvwmPrompt/go.sum`; builds use the committed vendor tree under `bin/FvwmPrompt/vendor/` via `-mod=vendor` in `bin/FvwmPrompt/meson.build`.
- Nix development dependencies are listed in `shell.nix`, but there is no `flake.lock` or other Nix pin.

## Frameworks

**Core:**
- Xlib/Xt/Xft - Required native X11 UI and window-management foundation, used directly throughout `fvwm/`, `libs/fvwm_x11.h`, and native modules under `modules/`; there is no higher-level GUI application framework.
- libevent 2.0+ - Event dispatch, Unix-domain socket listeners, buffered clients, timers, and signal events in `modules/FvwmMFL/FvwmMFL.c` and `modules/FvwmForm/FvwmForm.c`.
- Internal FVWM module framework - Binary event packets and text commands are defined by `libs/Module.h` and implemented by `libs/Module.c`, while process lifecycle and pipe transport are managed in `fvwm/module_list.c` and `fvwm/module_interface.c`.
- Internal widget toolkit - `modules/FvwmScript/Widgets/` is compiled into a static library by `modules/FvwmScript/meson.build`; widgets draw directly through X11 rather than using GTK or Qt.

**Testing:**
- Meson test runner - Native assertion-based C executables are registered in `tests/meson.build` for image-size safety, module framing/runtime, and event-loop policy.
- GCC/Clang sanitizers - AddressSanitizer and UndefinedBehaviorSanitizer runs are configured in `.github/workflows/ccpp.yml`.

**Build/Dev:**
- Meson 1.5.1+ - Configures native feature detection, generated files, install layout, tests, documentation, and distribution archives from `meson.build` and `meson.options`.
- Ninja - Executes builds through `build` and is the backend expected by `INSTALL.md`; no minimum version is declared.
- pkg-config - Resolves native X11, rendering, font, internationalization, and event dependencies declared by `dependency()` calls in `meson.build`.
- GCC and Clang - Both compilers are exercised on Ubuntu and Alpine in `.github/workflows/ccpp.yml`.
- Asciidoctor - Optional HTML and man-page generation in `doc/meson.build`; enabled by `-Dhtmldoc=true` and/or `-Dmandoc=true` from `meson.options`.
- Docker - `Dockerfile` builds inside `fvwmorg/fvwm3-build:latest`; CI chooses that image or `fvwmorg/fvwm3-build-alpine:latest` in `.github/workflows/ccpp.yml`.
- Nix shell - `shell.nix` offers a local dependency environment equivalent to the documented native build prerequisites.
- clang-format/EditorConfig - C formatting and whitespace defaults are recorded in `.clang-format` and `.editorconfig`.

## Key Dependencies

**Critical:**
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

**Optional Native Features:**
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

**Go Subproject:**
- `github.com/ergochat/readline` 0.1.3 - Interactive line editing and history in `bin/FvwmPrompt/FvwmPrompt.go`.
- `github.com/fatih/color` 1.13.0 - Colored interactive prompt output in `bin/FvwmPrompt/FvwmPrompt.go`.
- `github.com/sirupsen/logrus` 1.9.4 - FvwmPrompt error logging in `bin/FvwmPrompt/FvwmPrompt.go`.
- `github.com/mattn/go-colorable` 0.1.9, `github.com/mattn/go-isatty` 0.0.14, `golang.org/x/sys` 0.15.0, and `golang.org/x/text` 0.9.0 - Indirect, vendored dependencies pinned by `bin/FvwmPrompt/go.mod` and `bin/FvwmPrompt/go.sum`.

**Optional Script Integrations:**
- `python-xdg` - Parses Freedesktop menu, desktop-entry, icon-theme, locale, and base-directory data in `bin/fvwm-menu-desktop.in`.
- Pillow or ImageMagick (`identify`/`convert`) - Optional icon scaling fallback selected by `bin/fvwm-menu-desktop.in`.
- Core/CPAN Perl modules - Standard modules such as `Getopt::Long`, `IO::Select`, and `Config` are used in `bin/` and `perllib/`; the example status consumer additionally needs `JSON::XS` in `contrib/status/read_status.pl`.

## Configuration

**Environment:**
- Runtime display selection uses `DISPLAY`, which `fvwm/fvwm3.c` normalizes and republishes together with `HOSTDISPLAY` for child processes.
- User data defaults to the FVWM user directory and is exposed as `FVWM_USERDIR`; installed shared data/module locations are compiled from Meson install paths in `meson.build` and made available to scripts through configured templates such as `perllib/FVWM/Module.pm.in`.
- Default startup behavior and assets are installed from `default-config/config` and `default-config/images/` according to `default-config/meson.build`.
- FvwmMFL endpoint discovery uses `FVWMMFL_SOCKET`, `FVWMMFL_SOCKET_PATH`, `XDG_RUNTIME_DIR`, and `TMPDIR` in `modules/FvwmMFL/FvwmMFL.c`, `bin/FvwmCommand.in`, and `bin/FvwmPrompt/FvwmPrompt.go`.
- Optional session management uses `SESSION_MANAGER`, `SESSION_MANAGER_NAME`, and `SM_SAVE_DIR` in `fvwm/session.c` and `libs/fsm.c`.
- Logging is opt-in through `-v`, `-o`, or `FVWM3_LOGFILE`, implemented in `fvwm/fvwm3.c` and `libs/log.c`.
- No `.env` files are present; this native desktop application uses process environment variables and FVWM configuration files rather than dotenv loading.

**Build:**
- `meson.build` is the authoritative build graph and dependency detector; `meson.options` controls optional text, image, session, Go, and documentation features.
- `build` is the preferred wrapper for Meson setup, Ninja compilation, documentation flags, install prefix, and installation, as documented in `INSTALL.md`.
- `config_defines.h` is combined with Meson feature probes to generate build-directory `config.h` through `meson-scripts/conf-cmd.sh`.
- `.current-version` and `utils/fvwm-version-str.sh` supply release and VCS version strings to `meson.build`.
- `Dockerfile` provides the container build path; `shell.nix` provides an alternative local dependency shell.
- `.editorconfig` and `.clang-format` define repository formatting; no separate lint framework configuration is present.
- `modules/FvwmScript/script.c` and `modules/FvwmScript/scanner.c` are committed generated Bison 2.3/Flex outputs, so Bison and Flex are not normal build prerequisites.

## Platform Requirements

**Development:**
- Install Meson >=1.5.1, Ninja, pkg-config, a C11 compiler, Python 3, Perl, sed, and the required X11/libevent development packages listed in `INSTALL.md` and detected by `meson.build`.
- Use `./build` for routine builds, or `meson setup <dir>` followed by `meson compile -C <dir>`; test execution is `meson test -C <dir>` as used by `.github/workflows/ccpp.yml`.
- Go >=1.21 is required only when `-Dgolang=enabled` or automatic detection builds `bin/FvwmPrompt/`; disable it with `-Dgolang=disabled` through `meson.options`.
- Asciidoctor is required only for `-Dmandoc=true` or `-Dhtmldoc=true`, as implemented in `doc/meson.build`.
- Linux and BSD are intended developer platforms per `dev-docs/DEVELOPERS.md`; Darwin-specific build handling exists in `meson.build` and `libs/meson.build`.

**Production:**
- Deploy as installed native executables, modules, scripts, catalogs, assets, and an X session entry; install paths are summarized and generated by `meson.build`, including `bin/`, versioned `libexec/fvwm3/<version>/`, `share/fvwm3/`, locale data, and `share/xsessions/fvwm3.desktop`.
- Requires a functioning X11/Xorg-compatible display and the required shared libraries resolved at build/package time; no application server, database, container orchestrator, or cloud runtime is involved.
- Wayland is not a native target; any use under a Wayland desktop requires an external X11 compatibility server because core operation is implemented through Xlib in `fvwm/fvwm3.c` and `libs/fvwm_x11.h`.
- Release artifacts are source tarballs produced by `meson dist` in `.github/workflows/release.yml`; distribution packaging is expected to supply native libraries and optional features.

---

*Stack analysis: 2026-08-29*
