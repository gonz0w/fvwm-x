# FVWM-X

FVWM-X is an X11 window manager from FVWM3, for Linux, FreeBSD Xorg
and OpenBSD Xenocara on AMD64 and ARM64.

Preserves FVWM configurations. No compositor or Wayland.

**Status:** R1 development: configurable, fast, low-memory desktop
lite. Builds require Go. [Build/upgrade notes](INSTALL.md).

- Session controls, logout, services and scaling.
- Opt-in notifications, actions, keyboard center and bounded history;
  [scope and theme screenshots](doc/development-preview.md#native-notifications).
- Atomic buttons, responsive layout and settings/editor;
  [opt-in application font/icon/cursor preferences](doc/FvwmDesktop.adoc#_application_preferences).
- Source-aware errors and configuration reports.
- Structured IPC and eight-setting preview/recovery; optional
  [read-only reduced-motion portal bridge](doc/fvwm-settings-portal.adoc).
- [Localized application actions](dev-docs/verification/VERIFICATION-2026-10-29-XDG-INSTALLED.md),
  palette and startup activity.
- Optional browser: list/grid, watching, Files-menu handoff, application choosers
  and live updates; [opt-in FileManager1](doc/fvwm-file-browser.adoc#filemanager1-service).
  [SQLite/FTS5 deferred](dev-docs/decisions/DIRECTORY-BACKEND-EVALUATION-2026-11-02.md).
- Script selection metadata and bounded INCR paste;
  [preview](doc/development-preview.md#script-selection-and-paste-in-progress).
- Opt-in file Copy/Cut/Paste and XDND;
  [preview](doc/development-preview.md#file-clipboard-and-drag-work-in-progress).
- File jobs: folders, rename, copy/move, links, trash/restore, delete;
  progress, conflicts and cancellation.
- Native tray: XEmbed, opt-in StatusNotifier, bar popup and optional
  Buttons slot; [usage](doc/FvwmTray.adoc) and [current captures](doc/development-preview.md#native-tray-completion-work-in-progress).
- Optional Lucide menu glyphs; [theme examples](doc/development-preview.md#optional-lucide-menu-glyphs-in-progress).
- Optional pinboard: saved references, shaped cells and management;
  unpin preserves targets.

Slate: desktop pinboard.

![Slate dark desktop pinboard manager](doc/screenshots/pinboard-acceptance-slate-dark.png)

Dune: 4096-entry grid.

![Dune light file browser showing a selected grid entry](doc/screenshots/browser-dune-grid.png)

[More screenshots, theme/config examples and current limits](doc/development-preview.md).
[Clipboard](doc/fvwm-clipboard.adoc), file-manager and release qualification remain open.
OpenBSD baseline: 8.0.

[R1 resource profiles](dev-docs/decisions/R1-REFERENCE-PROFILES-2026-10-26.md)
and [release scope](dev-docs/decisions/RELEASE-PLAN-2026-10-26.md).

- [Install and build](INSTALL.md)
- [Priorities and evidence](dev-docs/PROJECT-TRACKER.md)
- [Roadmap and review board](https://github.com/users/gonz0w/projects/6)
- [Contribute and validate locally](CONTRIBUTING.md)

Local validation; GitHub Actions disabled.
GPL and upstream attribution: see [COPYING](COPYING).
