# FVWM-X

FVWM-X is an X11 window manager derived from FVWM3, for Linux, FreeBSD Xorg
and OpenBSD Xenocara on AMD64 and ARM64.

Our goals are simpler code, responsive interaction, safer resource handling
and continued use of FVWM 3.x configurations and themes.
Desktop services use FVWM's native modules and system facilities. There is no
built-in compositor or native Wayland backend.

**Status:** in-progress development. Features paused for source cleanup and
dependency retirement. Recent work includes:

- Atomic button updates, desktop settings/editor and configuration checking.
- Structured IPC queries/actions and scoped settings preview/recovery.
- An optional native file-browser prototype: scrolling list/grid, multi-selection,
  filtering, sorting, observed properties and configured application actions.
  Live updates retain drafts and selection; opt-in watching refreshes folders.
  IPC includes live reveal/properties, cancellable actions and optional folder handoff.

Slate dark: native list and observed properties.

![Slate dark file browser showing selection and observed properties](doc/screenshots/browser-slate-list.png)

Dune light: native grid, scrolled to the end of 4096 synthetic entries.

![Dune light file browser showing a selected grid entry](doc/screenshots/browser-dune-grid.png)

[More screenshots, theme/config examples and current limits](doc/development-preview.md).
Full file-manager workflows, protected clipboard handoff, broader accessibility and release qualification
remain open. Existing folder menus remain the default. OpenBSD follows new
releases; 8.0 is the current qualified baseline, with 7.9 out of scope.

Next: [batched legacy, dependency and tooling cleanup](dev-docs/decisions/MAINTENANCE-FIRST-BATCHES-2026-10-23.md).

- [Install and build](INSTALL.md)
- [Current priorities and evidence](dev-docs/PROJECT-TRACKER.md)
- [Roadmap and review board](https://github.com/users/gonz0w/projects/6)
- [Contribute and validate locally](CONTRIBUTING.md)

Review and validation run locally; GitHub Actions is disabled.
FVWM-X retains FVWM's GPL license and upstream attribution; see [COPYING](COPYING).
