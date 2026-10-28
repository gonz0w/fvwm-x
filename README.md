# FVWM-X

FVWM-X is an X11 window manager from FVWM3, for Linux, FreeBSD Xorg
and OpenBSD Xenocara on AMD64 and ARM64.

We target responsive, maintainable code and FVWM configuration/theme compatibility.
No compositor or Wayland backend.

**Status:** in development toward R1: a configurable, fast, low-memory desktop
lite. Features target R1; maintenance targets R2/R3. Builds require Go;
installed utilities need no compiler or Python packages. [Build/upgrade notes](INSTALL.md).
Recent features:

- Icons, scaling, wallpapers, high contrast and configuration inspection.
- Atomic button updates, responsive layout and settings/editor.
- Source-aware errors and paged configuration reports.
- Structured IPC queries/actions and scoped settings preview/recovery.
- Keyboard-first window/app/desktop/action palette with native refresh.
- Optional browser: list/grid, selection, filter/sort, watching, properties and
  application actions; live updates retain drafts/selection.
- File jobs: folders, rename, copy/move, links, trash/restore, delete;
  progress, conflicts and cancellation.
- Optional desktop pinboard: saved references, shaped cells and Form management;
  unpin preserves targets. [Support checks](dev-docs/verification/VERIFICATION-2026-10-28-PINBOARD-ACCEPTANCE.md)
  pass; release qualification continues.

Slate: desktop pinboard.

![Slate dark desktop pinboard manager](doc/screenshots/pinboard-acceptance-slate-dark.png)

Dune: 4096-entry grid.

![Dune light file browser showing a selected grid entry](doc/screenshots/browser-dune-grid.png)

[More screenshots, theme/config examples and current limits](doc/development-preview.md).
File-manager, clipboard, accessibility and release qualification remain open.
Folder menus remain default. OpenBSD follows new releases; the current baseline
is 8.0, with 7.9 out of scope.

[R1 resource profiles](dev-docs/decisions/R1-REFERENCE-PROFILES-2026-10-26.md)
and [release scope](dev-docs/decisions/RELEASE-PLAN-2026-10-26.md).

- [Install and build](INSTALL.md)
- [Priorities and evidence](dev-docs/PROJECT-TRACKER.md)
- [Roadmap and review board](https://github.com/users/gonz0w/projects/6)
- [Contribute and validate locally](CONTRIBUTING.md)

Review and validation run locally; GitHub Actions is disabled.
FVWM-X retains FVWM's GPL license and upstream attribution; see [COPYING](COPYING).
