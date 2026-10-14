# FVWM-X

FVWM-X is an X11 window manager derived from FVWM3, for Linux, FreeBSD Xorg
and OpenBSD Xenocara on AMD64 and ARM64. It is for people who want a fast,
configurable desktop and control over their own setup.

Our goals are a simpler codebase, responsive interaction, secure resource and
process handling, and continued use of existing FVWM configurations and themes.
Desktop services build on FVWM's native modules and existing system facilities.
File managers and other applications remain your choice; there is no built-in
compositor or native Wayland backend.

**Status:** active development, with locally qualified core responsiveness,
hardening and utility-port checkpoints. The local PNG/QEMU build, install and
BSD default-X checkpoint is complete. Broader desktop work, physical-hardware
qualification and release acceptance remain open. This is not a stable release.

- [Install and build](INSTALL.md)
- [Current priorities and evidence](dev-docs/PROJECT-TRACKER.md)
- [Roadmap and review board](https://github.com/users/gonz0w/projects/6)
- [Contribute and validate locally](CONTRIBUTING.md)

Review, tests, lint and validation run locally. GitHub hosts code, issues and
PR review; GitHub Actions is disabled. Preserve original failed evidence and
identify the exact source and platform behind every passing result.

FVWM-X retains FVWM's GPL license and upstream attribution; see [COPYING](COPYING).
