# FVWM-X

FVWM-X is an X11 window manager from FVWM3, for Linux, FreeBSD Xorg
and OpenBSD Xenocara on AMD64 and ARM64.

We target simpler code, responsiveness, safer resources and FVWM 3.x
configuration/theme compatibility. No compositor or Wayland backend.

**Status:** in development toward R1, a highly configurable, fast, low-memory
desktop lite. All currently planned features target R1; further maintenance
is scoped into R2/R3. Builds require Go; installed utilities need no compiler
or Python packages. [Build/upgrade notes](INSTALL.md).
Accepted runtime, linkage, SVG and ownership improvements remain.
Recent work:

- Icons, scaling, wallpapers, high contrast and configuration inspection.
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

Next: [R1 resource profiles](dev-docs/decisions/R1-REFERENCE-PROFILES-2026-10-26.md)
and [release scope](dev-docs/decisions/RELEASE-PLAN-2026-10-26.md).

- [Install and build](INSTALL.md)
- [Priorities and evidence](dev-docs/PROJECT-TRACKER.md)
- [Roadmap and review board](https://github.com/users/gonz0w/projects/6)
- [Contribute and validate locally](CONTRIBUTING.md)

Review and validation run locally; GitHub Actions is disabled.
FVWM-X retains FVWM's GPL license and upstream attribution; see [COPYING](COPYING).
