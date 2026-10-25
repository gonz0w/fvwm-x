# FVWM-X

FVWM-X is an X11 window manager derived from FVWM3, for Linux, FreeBSD Xorg
and OpenBSD Xenocara on AMD64 and ARM64.

We target simpler code, responsive interaction, safer resources and FVWM 3.x
configuration/theme compatibility. No compositor or Wayland backend.

**Status:** in-progress development. Features paused for source cleanup and
dependency retirement. Builds require Go. Installed utilities need no
Go compiler or Python packages. The pre-FVWM3 converter is retired.
[Build/upgrade notes](INSTALL.md). Go coordinates the desktop; native Xlib/PNG
capture replaces Python runtime helpers. Six-target controller checks pass;
session qualification remains open.
Recent work includes:

- Atomic button updates, desktop settings/editor and configuration checking.
- Structured IPC queries/actions and scoped settings preview/recovery.
- An optional native file-browser prototype: scrolling list/grid, multi-selection,
  filtering, sorting, observed properties and configured application actions.
  Live updates retain drafts and selection; opt-in watching refreshes folders.

Slate dark: list/properties.

![Slate dark file browser showing selection and observed properties](doc/screenshots/browser-slate-list.png)

Dune light: grid with 4096 entries.

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
