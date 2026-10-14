# External Integrations

**Analysis Date:** 2026-08-29

## APIs & External Services

**X Window System:**
- X11 server - FVWM3 opens and owns an X display/root window, manages client windows, receives input/window events, and draws decorations directly.
  - SDK/Client: Xlib, Xt, Xft, and X resource APIs included through `libs/fvwm_x11.h` and used by `fvwm/fvwm3.c`, `fvwm/events.c`, and `fvwm/add_window.c`.
  - Auth: delegated to the X server connection selected by `DISPLAY` or `fvwm3 -d`; the repository implements no separate X credential store.
  - Transport: whatever Xlib resolves from `DISPLAY` (normally a local Unix socket, but X11 can also use remote transports); `fvwm/fvwm3.c` exposes `HOSTDISPLAY` to child processes.
- ICCCM - Core window-manager/client conventions, focus, colormaps, protocols, and session-related properties are handled in `fvwm/icccm2.c`, `fvwm/add_window.c`, `fvwm/events.c`, and `fvwm/fvwm3.c`.
- EWMH/NetWM - Freedesktop window types, states, desktop metadata, client lists, work areas, names, and client messages are implemented in `fvwm/ewmh.c`, `fvwm/ewmh_events.c`, `fvwm/ewmh_conf.c`, `fvwm/ewmh_icons.c`, and `fvwm/ewmh_names.c`.
- XRandR 1.5+ - Monitor inventory, geometry, focus, enable/disable/change events, and multi-monitor layout are consumed in `libs/FScreen.c`, `fvwm/events.c`, and `modules/FvwmPager/fpmonitor.c`.
- X extensions - Shape is required through `libs/FShape.h`; optional XRender, Xcursor, XFixes, XPM, XShm, and XKB support is wrapped in `libs/fvwm_x11.h`, `libs/FRenderInit.c`, `fvwm/cursor.c`, and `libs/PictureImageLoader.c`.

**Local FVWM Command/Event APIs:**
- Native module protocol - FVWM3 starts modules as child processes and passes two pipe file descriptors plus window/config context on their command lines.
  - SDK/Client: C packet/text helpers in `libs/Module.h` and `libs/Module.c`; Perl client abstraction in `perllib/FVWM/Module.pm.in`.
  - Protocol: binary `FvwmPacket` event frames from FVWM to modules and text commands from modules to FVWM, with message masks and startup/unlock responses defined in `libs/Module.h`.
  - Lifecycle: pipe creation, nonblocking mode, close-on-exec, `fork`, and `execvp` are implemented in `fvwm/module_list.c`; framing and dispatch are handled in `fvwm/module_interface.c` and `fvwm/module_runtime.c`.
  - Auth: process ancestry and inherited file descriptors; there is no network listener or token for this internal protocol.
- FvwmMFL - Bridges the internal module protocol to external local clients over a Unix-domain stream socket.
  - SDK/Client: libevent listener/bufferevent APIs and vendored cJSON in `modules/FvwmMFL/FvwmMFL.c`; clients include `bin/FvwmCommand.in` and `bin/FvwmPrompt/FvwmPrompt.go`.
  - Protocol: clients send plain command/subscription text; FvwmMFL sends newline-terminated JSON connection and event objects described in `doc/FvwmMFL.adoc`.
  - Endpoint: exported as `FVWMMFL_SOCKET`; the directory is selected from `FVWMMFL_SOCKET_PATH`, `XDG_RUNTIME_DIR`, `TMPDIR`, or `/tmp` in `modules/FvwmMFL/FvwmMFL.c`.
  - Auth: local filesystem ownership and permissions. `modules/FvwmMFL/FvwmMFL.c` requires an owner-only socket directory and sets the socket to mode `0600`; no application-level authentication exists.
- Status FIFO - `fvwm/builtins.c` creates `$TMPDIR/fvwm3.pipe`, exports `FVWM_STATUS_PIPE`, and writes JSON status lines; `contrib/status/read_status.pl` is an example consumer that also invokes `xrandr` and formats output for lemonbar.

**Desktop Session Management:**
- XSMP/ICE session manager - Optional client connection, save/restart state, shutdown callbacks, and ICE message processing are implemented in `fvwm/session.c` using SMlib/ICElib.
  - SDK/Client: `sm`, `ice`, and `xtrans` dependencies declared in `meson.build`; wrappers/types are exposed through `libs/FSMlib.h` and `libs/fvwm_x11.h`.
  - Discovery: `SESSION_MANAGER`, `SESSION_MANAGER_NAME`, and `SM_SAVE_DIR` environment variables consumed by `fvwm/session.c`.
  - Auth: ICE `MIT-MAGIC-COOKIE-1`; `libs/fsm.c` generates cookies, creates owner-private temporary authorization files, and applies/removes them via the external `iceauth` command.
- Embedded/proxy session manager - `libs/fsm.c` can initialize an XSMP server, publish `SESSION_MANAGER`, and proxy swallowed applications for modules such as `modules/FvwmButtons/FvwmButtons.c` and `modules/FvwmScript/Widgets/Swallow.c`.

**Freedesktop Desktop Integration:**
- X session registration - `contrib/xsession/fvwm3.desktop.in` is configured and installed to the X session directory by `meson.build`, launching the installed `fvwm3` binary.
- XDG application menus - `bin/fvwm-menu-desktop.in` reads Freedesktop menu files, desktop entries, icon themes, locale, and base directories through `python-xdg`, then emits FVWM menu commands.
  - SDK/Client: `xdg.Menu`, `xdg.IconTheme`, `xdg.Locale`, `xdg.DesktopEntry`, and `xdg.BaseDirectory` imported by `bin/fvwm-menu-desktop.in`.
  - Auth: none; reads per-user and system XDG files.
- X resources - Window placement and client options consume Xrm data in `fvwm/add_window.c` and `fvwm/placement.c`.

**Rendering, Fonts, and Localization:**
- Fontconfig/Xft - Font parsing, matching, fallback, rotation, and UTF-8 glyph rendering in `libs/Fft.c`.
- Image libraries - PNG, XPM, and SVG files are decoded in-process by `libs/PictureImageLoader.c`; SVG rendering uses librsvg with a Cairo-compatible backend configured in `meson.build`.
- Text conversion - optional iconv conversion is implemented in `libs/Ficonv.c` and `libs/Flocale.c`; optional bidirectional layout uses FriBidi in `libs/FBidi.c` and `libs/BidiJoin.c`.
- Gettext - catalogs under `po/` are installed by `po/meson.build`; runtime domain/locale lookup is implemented in `libs/FGettext.c`.

**Optional External Desktop Programs:**
- ImageMagick or Pillow - `bin/fvwm-menu-desktop.in` uses Pillow when importable, otherwise calls ImageMagick `identify` and `convert`, otherwise keeps original icon sizes.
- `xlock` - `bin/fvwm-menu-xlock.in` parses `xlock -help` output and emits menu actions that execute xlock modes.
- Dialog programs - `perllib/FVWM/Module/Toolkit.pm` probes for `gdialog`, `Xdialog`, `zenity`, `message`, or `kdialog`, falling back to `xterm` for user-visible errors.
- Terminal/editor/shell commands - `bin/fvwm-menu-directory.in`, `perllib/FVWM/Module/Terminal.pm`, and the default configuration invoke user-configurable commands, with `xterm`, `$SHELL`, and `$EDITOR` as defaults/fallbacks.
- `iceauth` - called by `libs/fsm.c` when optional embedded session-manager authentication is active.
- These programs are optional runtime integrations, not linked dependencies; callers must tolerate their absence according to the fallback paths in the cited scripts.

**Repository and Release Services:**
- GitHub - Source hosting, issue/PR events, Actions CI, generated release notes, tags, and release assets are automated under `.github/workflows/`.
  - SDK/Client: `actions/checkout@v5`, `gh`, `curl`, `jq`, and GitHub REST endpoint `releases/generate-notes` in `.github/workflows/release.yml`.
  - Auth: GitHub-managed `GITHUB_TOKEN`; referenced only by name in `.github/workflows/release.yml`.
- Blacksmith - Hosted GitHub Actions runners named `blacksmith-4vcpu-ubuntu-2204` and `blacksmith-4vcpu-ubuntu-2404` execute CI, IRC notification, and release jobs in `.github/workflows/*.yml`.
- Docker Hub/container registry - CI and `Dockerfile` consume `fvwmorg/fvwm3-build:latest` and `fvwmorg/fvwm3-build-alpine:latest`; no registry credentials are declared in the repository.
- Libera.Chat IRC - Issue and pull-request events are sent to `irc.libera.chat` channel `#fvwm` by `Gottox/irc-message-action@v2.1.3` in `.github/workflows/issues.yml`.
- FVWM website repository - Release automation checks out and updates `fvwmorg/fvwmorg.github.io` using `meson-scripts/generate-manpages-website.sh` from `.github/workflows/release.yml`.
  - Auth: GitHub Actions secret `FVWM_PUBLISH_DOCS`, referenced by name in `.github/workflows/release.yml`.
- Codacy - A repository quality badge links to Codacy in `README.md`; no Codacy workflow, API client, token, or checked-in Codacy configuration was detected.
- Dependabot - Monthly GitHub Actions and Go module updates are configured in `.github/dependabot.yml`.

## Data Storage

**Databases:**
- Not detected - There is no database server, ORM, SQL schema, or persistent service client in the build graph rooted at `meson.build`.

**File Storage:**
- Local filesystem only - System defaults and assets are installed from `default-config/` according to `default-config/meson.build`; user configuration and module state are read under `FVWM_USERDIR` by files such as `modules/FvwmScript/FvwmScript.c` and `perllib/FVWM/Module.pm.in`.
- Session state - Optional XSMP save/restore files are created under `SM_SAVE_DIR`, `$HOME`, or the passwd home directory by `fvwm/session.c`.
- Logs - Opt-in logs go to stderr or a local file selected by `-o`/`FVWM3_LOGFILE`, with relative paths rooted in `FVWM_USERDIR`, in `libs/log.c`.
- Command history - FvwmPrompt stores `.FvwmConsole-History` under `FVWM_USERDIR` in `bin/FvwmPrompt/FvwmPrompt.go`.
- IPC files - FvwmMFL creates a Unix socket and lock file in an owner-private runtime directory in `modules/FvwmMFL/FvwmMFL.c`; status output creates a FIFO in `fvwm/builtins.c`.
- Generated menu icons - `bin/fvwm-menu-desktop.in` can persist resized icons under its user-configurable icon directory, defaulting to `~/.fvwm/icons`.
- Translation catalogs - source and compiled catalogs are committed under `po/` and installed through `po/meson.build`.

**Caching:**
- No external cache service - No Redis, Memcached, or equivalent client is present in `meson.build` or application sources.
- In-process font cache - `libs/Fft.c` keeps a fixed-size Fontconfig match cache for fallback glyph rendering.
- Filesystem reuse - `bin/fvwm-menu-desktop.in` reuses generated icons when source timestamps and theme state permit; `perllib/General/FileSystem.pm` contains optional local file-operation caching.

## Authentication & Identity

**Auth Provider:**
- No user-account authentication - FVWM3 is a per-user desktop process and exposes no login/session identity service of its own.
  - X identity is delegated to the X server selected by `DISPLAY` in `fvwm/fvwm3.c`.
  - Internal module identity is inherited through child process file descriptors in `fvwm/module_list.c`.
  - FvwmMFL access is restricted by owner-only directory/socket permissions in `modules/FvwmMFL/FvwmMFL.c`; any process running as that user can connect.
  - Optional XSMP/ICE identity uses `MIT-MAGIC-COOKIE-1` generated and installed through `libs/fsm.c` and `iceauth`.
  - CI/release identity uses GitHub Actions secrets referenced in `.github/workflows/release.yml`; no secret values are stored in repository files.

## Monitoring & Observability

**Error Tracking:**
- No runtime error-tracking SaaS is integrated - Native failures and diagnostics use stderr or the local logger in `libs/log.c`; FvwmPrompt uses local Logrus output in `bin/FvwmPrompt/FvwmPrompt.go`.
- Codacy supplies an external code-quality badge only through `README.md`; it is not application telemetry.

**Logs:**
- FVWM native logging is disabled unless verbose mode is enabled; `libs/log.c` writes timestamped, line-buffered entries to stderr or a configurable local file.
- Modules generally report fatal/startup failures to stderr, exemplified by `modules/FvwmMFL/FvwmMFL.c`.
- GitHub Actions build and test output is the CI observability path in `.github/workflows/ccpp.yml`, including `meson test --print-errorlogs` and sanitizer jobs.
- No metrics exporter, tracing SDK, crash reporter, or remote log sink was detected in `meson.build`, `fvwm/`, `libs/`, or `modules/`.

## CI/CD & Deployment

**Hosting:**
- GitHub Releases hosts version tags and `fvwm3-<version>.tar.gz` plus SHA-256 checksum artifacts generated by `meson dist` in `.github/workflows/release.yml`.
- The separate `fvwmorg/fvwmorg.github.io` repository hosts generated HTML manual pages; updates are built by `meson-scripts/generate-manpages-website.sh` and pushed by `.github/workflows/release.yml`.
- Runtime deployment is via Unix/Linux/BSD distribution packaging or local `meson install`; the install graph is defined in `meson.build`, component `meson.build` files, and `contrib/xsession/fvwm3.desktop.in`.

**CI Pipeline:**
- `.github/workflows/ccpp.yml` builds and tests GCC and Clang combinations against Ubuntu- and Alpine-based FVWM build containers on Blacksmith runners.
- `.github/workflows/ccpp.yml` separately compiles/tests C code with AddressSanitizer and UndefinedBehaviorSanitizer and disables the Go component for that job.
- `.github/workflows/release.yml` validates versions, generates notes via the GitHub API, creates/tag/pushes the release commit, builds documentation and a source archive, uploads release assets, updates the website, and advances `.current-version`/`.release-status`.
- `.github/workflows/issues.yml` publishes issue and pull-request lifecycle notifications to Libera.Chat IRC.
- `.github/dependabot.yml` requests monthly updates for GitHub Actions and the Go module in `bin/FvwmPrompt/`.
- `.disabled-travis.yml` is retained but disabled legacy Travis CI configuration; it is excluded from release archives by `meson-scripts/dist-tarball.sh` and is not an active pipeline.

## Environment Configuration

**Required env vars:**
- `DISPLAY` - Required for normal X11 operation and checked by `fvwm/fvwm3.c`, `bin/FvwmCommand.in`, and X-aware modules; it can alternatively be supplied to `fvwm3` with the display command-line option.
- `HOME` or an available passwd home - Used as fallback for user/session files in `fvwm/session.c`, `bin/fvwm-menu-directory.in`, and Perl helpers.
- No API keys, database URLs, or remote-service credentials are required by the installed runtime; runtime integrations are local/X11/ICE based.

**Optional runtime env vars:**
- `FVWM_USERDIR`, `FVWM_DATADIR`, `FVWM_MODULEDIR` - User data, installed data, and module locations used throughout `modules/`, `perllib/FVWM/Module.pm.in`, and configured scripts.
- `FVWMMFL_SOCKET`, `FVWMMFL_SOCKET_PATH`, `XDG_RUNTIME_DIR`, `TMPDIR` - FvwmMFL and status IPC placement/discovery in `modules/FvwmMFL/FvwmMFL.c`, `bin/FvwmCommand.in`, `bin/FvwmPrompt/FvwmPrompt.go`, and `fvwm/builtins.c`.
- `SESSION_MANAGER`, `SESSION_MANAGER_NAME`, `SM_SAVE_DIR` - Optional XSMP/ICE integration in `fvwm/session.c`, `libs/fsm.c`, and modules that proxy swallowed applications.
- `FVWM3_LOGFILE` - Local log destination in `libs/log.c`.
- `FVWM_STATUS_PIPE` - Exported status FIFO consumed by `contrib/status/read_status.pl`.
- `FVWM_MODULE_DEBUG`, `FVWM_MODULE_TERMINAL_CLIENT`, `FVWM_VISUALID`, and `FVWM_COLORMAP` - Module diagnostics/terminal state and child X visual context in `perllib/FVWM/Module.pm.in`, `perllib/FVWM/Module/Terminal.pm`, and `fvwm/module_list.c`.
- `CC`, `CFLAGS`, and `CC_LD` - CI/compiler selection variables used by `.github/workflows/ccpp.yml`, not application runtime configuration.

**Secrets location:**
- GitHub Actions managed secrets only - `GITHUB_TOKEN` and `FVWM_PUBLISH_DOCS` are referenced by name in `.github/workflows/release.yml`.
- No `.env` files or checked-in runtime credential files were detected.

## Webhooks & Callbacks

**Incoming:**
- Installed application: none - FVWM3 exposes no HTTP endpoint or incoming webhooks; external runtime control enters through X11 events, inherited module pipes, the local FvwmMFL Unix socket, and the status FIFO in `fvwm/events.c`, `fvwm/module_interface.c`, `modules/FvwmMFL/FvwmMFL.c`, and `fvwm/builtins.c`.
- Repository automation: GitHub issue, pull-request, push, and manual workflow-dispatch events trigger `.github/workflows/issues.yml`, `.github/workflows/ccpp.yml`, and `.github/workflows/release.yml` within GitHub Actions.
- Session callbacks: XSMP save, die, completion, and shutdown cancellation callbacks are registered in `fvwm/session.c`; embedded session-manager callbacks are implemented in `libs/fsm.c`.

**Outgoing:**
- Runtime: no HTTP callbacks - FVWM sends X11 property/client messages, binary module packets, text commands, JSON over local Unix sockets/FIFOs, and optional ICE/XSMP messages through the implementations under `fvwm/`, `libs/Module.c`, `modules/FvwmMFL/FvwmMFL.c`, and `libs/fsm.c`.
- CI issue/PR notifications: `Gottox/irc-message-action@v2.1.3` sends formatted messages to Libera.Chat from `.github/workflows/issues.yml`.
- Release notes: `.github/workflows/release.yml` posts to GitHub's `releases/generate-notes` REST endpoint using the workflow token.
- Release publication: `.github/workflows/release.yml` pushes tags/commits, uploads source/checksum assets with `gh`, and pushes generated documentation to `fvwmorg/fvwmorg.github.io`.

---

*Integration audit: 2026-08-29*
