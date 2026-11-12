# Durable docking profiles

The combined Slate Xorg dummy scene passed all nine cases on Linux ARM64.
It published a private data-only profile from the desktop controls and
observed read-only preview. Newer workspace, panel, settings and window
changes were refused or skipped as appropriate. Restore selected the
per-monitor workspace, moved the panel and its strut before applying the
window offsets, and retained client pixel sizes. Removing the destination
refused the old preview and a fresh preview used the primary fallback.
After replug and a same-process FVWM restart with a new core incarnation,
restore reopened the persisted profile and matched a replacement window
by its exact WM_CLASS.

The affected core and Desktop build, docking policy, module runtime and
Go controller and command metadata checks passed. The final run reused
both binaries and 43 prior passing command receipts. Earlier runs
exposed two fixture assumptions about cached workspace rows and startup
desktop mode. The production correction also checks the exact workspace
revision and viewport after desk notifications and uses one active monitor
selector for panel geometry and struts. The owned guest and container
shut down cleanly.

The screenshot is the final Slate fixture after restore. Physical
docking, additional themes and the full support matrix remain release
qualification for #222.

[Results and hashes](VERIFICATION-2026-11-11-DURABLE-DOCKING.json) ·
[Finished Slate scene](../../doc/screenshots/workspace-docking-profile-slate-dark.png)
