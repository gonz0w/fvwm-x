# Slate native window controls — 2026-10-04

The baseline now demonstrates native geometry, conditional targeting, directional
focus, RandR monitor transfer, on-demand rearrangement, workspace overview and
window inspection. It adds no runtime library or external desktop package.
`default-config/windows.fvwm` contains the commented functions and bindings;
`examples.fvwm` contains optional application rules and a State-31 scratchpad
recipe. The examples are installed but not automatically read. Personal
`local.config` remains the last configuration hook.

## Behavior

- Super+arrows: halves; Super+Alt+1..4: quarters in reading order.
- Super+Ctrl+arrows: directional focus, raise and update normal window-list order.
- Super+Shift+Left/Right: previous/next monitor, wrapping by RandR numbering.
- Super+Ctrl+C/G/R: center, grow and restore saved normal geometry.
- Super+Ctrl+T: one-shot gapped tiling; Super+O: toggle workspace overview.
- Super+Ctrl+I: inspect focused window; the title menu preserves its own target.
- Workspace tools and launcher actions expose overview, tile, cascade and Pick
  inspection. Pager miniatures support desktop selection and middle-button
  window dragging. Native cursors use the X cursor font.

Half/quarter placement uses the target monitor's working area, including the
bar and outer insets, with frame dimensions and internal gaps. It preserves
normal geometry via ResizeMoveMaximize. Tiling/cascading changes normal geometry
and has no layout-undo command. Ordinary client size constraints remain honored.
Per-application workspace assignments and scratchpad shortcuts stay opt-in.

## Verification obtained

| Target | Policy | Actual X controls | Full desktop regression |
| --- | --- | --- | --- |
| Linux AMD64, Podman CPU emulation | 11 launcher + 15 desktop pass | Xvfb and two-output Xorg-dummy pass | 3/3 targeted Meson tests; 61.63 s desktop gate |
| Linux ARM64, native container | 11 launcher + 15 desktop pass | Xvfb and two-output Xorg-dummy pass | 3/3 targeted Meson tests; 32.37 s desktop gate |
| FreeBSD AMD64, QEMU CPU emulation | 11 launcher + 15 desktop pass | Xvfb pass | Targeted controls only |
| FreeBSD ARM64, QEMU hardware acceleration | 11 launcher + 15 desktop pass | Xvfb pass | Targeted controls only |
| OpenBSD AMD64, QEMU CPU emulation | 11 launcher + 15 desktop pass | Xvfb pass | Targeted controls only |
| OpenBSD ARM64, QEMU hardware acceleration | 11 launcher + 15 desktop pass | Xvfb pass | Targeted controls only |

The controls gate generates real XTest shortcuts and observes frame geometry
and input focus. It covers eight placements, normal geometry restoration,
center/grow working-area boundaries, directional focus without pointer warp,
explicit menu targeting, fixed-client and minimized/panel exclusions, gapped
tiling, cascade without resize, normal/minimized pager toggling, inspection and
the unchanged optional scratchpad recipe across desktops.

The Linux Xorg gate uses two actual RandR outputs of different sizes. It checks
next/previous transfer, negative-index wrapping, destination centering, placement
on the owning monitor while the pointer remains elsewhere, per-monitor insets,
normal-size restoration, focus retention and unchanged pointer position.

The full desktop regression continues to exercise palette switches, appearance,
launcher input/activation, task/quick controls, compact layouts and screenshots.
Linux AMD64's full run preceded the final continuation-line formatting and
minimized-pager condition; the final controls-only Xvfb/Xorg follow-up passes
with those changes and the expanded safeguard assertions. Linux ARM64 and the
four final BSD controls gates used that final configuration and test helper.

Meson's existing desktop test now depends on FvwmPager, FvwmIdent and
FvwmRearrange. Linux CI adds the two-output Xorg check; BSD CI retains the native
Xvfb controls check. These are configured CI gates, not evidence of hosted CI
execution. Staged Linux installation contains both new default files.
Python compilation, local policy checks and `git diff --check` pass.

## Evidence and boundaries

Evidence lives under `builddir/slate-verification/window-controls/`: full Linux
desktop artifacts, final AMD64 follow-up, four BSD native evidence archives and
logs, staged-install log, input archive and SHA-256 source manifest. Initial
diagnostics are retained separately. Native checks corrected Maximize flag
ordering and replaced a root-context Any/Close action with explicit All targets.
Pager probes use its stable X class because its title follows desktop labels.

BSD tests used already-built native WM/module executables in the existing
disposable verification guests and an isolated source/helper directory; they
did not modify those guests' other work or rebuild unrelated C changes. Linux
tests built snapshots of the current checkout. This is a targeted desktop gate,
not a verification claim for concurrent core changes. Only this task's Linux
containers and volumes are removed after copying evidence; existing VMs remain
running.

Physical GPUs, physical monitor hotplug/mixed-DPI transitions, compositor
effects, secure lock/power permissions and native Linux sanitizer CI remain
outside this configuration gate. The scratchpad recipe requires an installed
terminal with an appropriate resource-name option; repeated requests before its
first asynchronous map can start duplicates. Application assignments and that
recipe are commented examples, rather than baseline policy.
