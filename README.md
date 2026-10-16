# FVWM-X

FVWM-X is an X11 window manager derived from FVWM3, for Linux, FreeBSD Xorg
and OpenBSD Xenocara on AMD64 and ARM64. It is for people who want a fast,
configurable desktop and control over their own setup.

Our goals are a simpler codebase, responsive interaction, secure resource and
process handling, and continued use of existing FVWM configurations and themes.
Desktop services build on FVWM's native modules and existing system facilities.
File managers and other applications remain your choice; there is no built-in
compositor or native Wayland backend.

**Status:** development, with locally qualified core responsiveness,
hardening and utility-port checkpoints. Desktop work, physical-hardware qualification and
release acceptance remain open. Hosted source is the qualified core/utility
snapshot; separate desktop/artwork publication is tracked in the guides.
Optional Go builds use a patched compiler baseline. The image-cache and core-
readiness checkpoints are locally qualified, including ordinary, interactive
and synchronous waits. EWMH state and window-update checks pass on six
targets; broader platform failures and fresh-install gaps remain documented.
OpenBSD AMD64 VESA remains
deferred. This is not a stable release.
[Style wildcard optimization](dev-docs/verification/VERIFICATION-2026-10-16-STYLE-WILDCARDS.md)
has focused passes; combined qualification is pending.

- [Install and build](INSTALL.md)
- [Current priorities and evidence](dev-docs/PROJECT-TRACKER.md)
- [Roadmap and review board](https://github.com/users/gonz0w/projects/6)
- [Contribute and validate locally](CONTRIBUTING.md)

Review, tests, lint and validation run locally. GitHub hosts code, issues and
PR review; GitHub Actions is disabled. Preserve original failed evidence and
identify the exact source and platform behind every passing result.

FVWM-X retains FVWM's GPL license and upstream attribution; see [COPYING](COPYING).
