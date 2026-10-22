# FVWM-X

FVWM-X is an X11 window manager derived from FVWM3, for Linux, FreeBSD Xorg
and OpenBSD Xenocara on AMD64 and ARM64. It offers a configurable desktop.

Our goals are a simpler codebase, responsive interaction, secure resource and
process handling, and continued use of existing FVWM configurations and themes.
Desktop services build on FVWM's native modules and existing system facilities.
File managers and other applications remain your choice; there is no built-in
compositor or native Wayland backend.

**Status:** development. The
[safety closeout](dev-docs/PROJECT-TRACKER.md#hardening-closeout), manager-target
prerequisite, buttons/atomic groups, settings/editor and non-executing
configuration checker are merged.
[IPC-02](dev-docs/PROJECT-TRACKER.md#ipc-02-structured-queries-and-operation-results)
has core/settings queries, actions, validation and wait controls. Topology,
workspace/global-binding queries pass platform checks; menu/placement policy
inspection passes focused checks. Preview, polish,
maintenance and tuning remain deferred.
[OpenBSD 8.0 qualification](dev-docs/verification/VERIFICATION-2026-10-20-OPENBSD-80.md)
passes on both VM architectures. OpenBSD follows new releases; 8.0 is the
current qualified baseline and 7.9 is out of scope.
[Protocol requirement inventory](dev-docs/verification/STANDARDS-2026-10-21-PROTOCOL-REQUIREMENTS.md)
records remaining runtime acceptance.
[Periodic performance comparisons](dev-docs/verification/PERFORMANCE-2026-10-22-PR154.md)
track regressions every five implementation batches using existing builds.
Go builds use a patched compiler baseline. Desktop/artwork publication,
physical hardware, toolkits/compositors, native-CPU sanitizers and release
acceptance remain open.

- [Install and build](INSTALL.md)
- [Current priorities and evidence](dev-docs/PROJECT-TRACKER.md)
- [Roadmap and review board](https://github.com/users/gonz0w/projects/6)
- [Contribute and validate locally](CONTRIBUTING.md)

Review, tests, lint and validation run locally. GitHub hosts code, issues and
PR review; GitHub Actions is disabled. Preserve original failed evidence and
identify the exact source and platform behind every passing result.

FVWM-X retains FVWM's GPL license and upstream attribution; see [COPYING](COPYING).
