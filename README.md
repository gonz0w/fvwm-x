# FVWM-X

FVWM-X is an X11 window manager derived from FVWM3, for Linux, FreeBSD Xorg
and OpenBSD Xenocara on AMD64 and ARM64. It is for people who want a fast,
configurable desktop and control over their own setup.

Our goals are a simpler codebase, responsive interaction, secure resource and
process handling, and continued use of existing FVWM configurations and themes.
Desktop services build on FVWM's native modules and existing system facilities.
File managers and other applications remain your choice; there is no built-in
compositor or native Wayland backend.

**Status:** development. The hosted core/utility snapshot has locally qualified
hardening, responsiveness and utility-port checkpoints.
[Window-tree walks](dev-docs/verification/VERIFICATION-2026-10-20-WINDOW-TREE-WALKS.md) bound ancestor and pointer lookups and keep
IconMan frame offsets unchanged when lookup fails. One combined checkpoint
covers six targets; required gaps remain explicit.
Optional Go builds use a patched compiler baseline. Separate desktop/artwork publication is tracked in the guides.
Physical hardware, toolkits/compositors, native-CPU sanitizers and release
acceptance remain open. Earlier source-scope failures stay documented.
OpenBSD AMD64 VESA cause work is deferred. This is not a stable release.

- [Install and build](INSTALL.md)
- [Current priorities and evidence](dev-docs/PROJECT-TRACKER.md)
- [Roadmap and review board](https://github.com/users/gonz0w/projects/6)
- [Contribute and validate locally](CONTRIBUTING.md)

Review, tests, lint and validation run locally. GitHub hosts code, issues and
PR review; GitHub Actions is disabled. Preserve original failed evidence and
identify the exact source and platform behind every passing result.

FVWM-X retains FVWM's GPL license and upstream attribution; see [COPYING](COPYING).
