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
[Font/picture/monitor record boundaries](dev-docs/verification/VERIFICATION-2026-10-19-UI-TYPE-BOUNDARIES.md) reduce ARM GCC
layout recompilations from 252/289/226 to 49/66/92 objects. The affected local
checkpoint passes 170 entries across six targets/ten profiles with unchanged
compiled code/data. Required OpenBSD gaps remain [documented](dev-docs/verification/VERIFICATION-2026-10-19-UI-TYPE-BOUNDARIES.md).
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
