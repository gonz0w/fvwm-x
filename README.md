# FVWM-X

FVWM-X is an X11 window manager derived from FVWM3, for Linux, FreeBSD Xorg
and OpenBSD 8.0 Xenocara on AMD64 and ARM64. It is for people who want a fast,
configurable desktop and control over their own setup.

Our goals are a simpler codebase, responsive interaction, secure resource and
process handling, and continued use of existing FVWM configurations and themes.
Desktop services build on FVWM's native modules and existing system facilities.
File managers and other applications remain your choice; there is no built-in
compositor or native Wayland backend.

**Status:** development. Core/module hardening, responsiveness and optional
utility ports have locally qualified checkpoints.
[OpenBSD 8.0 qualification](dev-docs/verification/VERIFICATION-2026-10-20-OPENBSD-80.md)
passes on both VM architectures; 8.0 is the sole OpenBSD release target.
[Session ownership](dev-docs/verification/VERIFICATION-2026-10-20-SESSION-CALLBACKS.md)
retains its source-scoped results and earlier failures.
[Periodic performance comparisons](dev-docs/verification/PERFORMANCE-2026-10-20.md)
track latency, CPU, memory and executable size every five implementation batches.
Optional Go builds use a patched compiler baseline. Desktop/artwork publication,
physical hardware, toolkits/compositors, native-CPU sanitizers and release
acceptance remain open. This is not a stable release.

- [Install and build](INSTALL.md)
- [Current priorities and evidence](dev-docs/PROJECT-TRACKER.md)
- [Roadmap and review board](https://github.com/users/gonz0w/projects/6)
- [Contribute and validate locally](CONTRIBUTING.md)

Review, tests, lint and validation run locally. GitHub hosts code, issues and
PR review; GitHub Actions is disabled. Preserve original failed evidence and
identify the exact source and platform behind every passing result.

FVWM-X retains FVWM's GPL license and upstream attribution; see [COPYING](COPYING).
