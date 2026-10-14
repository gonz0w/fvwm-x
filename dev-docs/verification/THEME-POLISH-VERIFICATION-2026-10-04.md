# Slate theme polish — 2026-10-04

The default remains one commented Slate layout, with four light/dark palettes.
This pass adds shared typography/layout tokens, persistent Comfortable and
Compact spacing, explicit interaction/feedback colors, and a readable two-by-two
workspace overview. No application toolkit themes or new runtime libraries are
introduced.

## Behavior

- Comfortable stays the default: 36-pixel bar, 32-pixel titles, 16-pixel gaps.
  Compact uses 32, 28 and 12 pixels. Appearance and launcher actions select either
  profile; the private JSON preference survives restart.
- Body and bold heading fonts use existing generic Sans. Font tokens remain
  configurable in local.config and survive density changes. Commented profile
  files expose menu/form spacing, launcher rows, panel and overview geometry.
- Bar labels retain their readable widths at either density. Quick settings
  retains two 32-pixel rows for the native slider so endpoint labels stay visible.
- Pressed title/panel/task controls use a distinct accent fill. Maximized windows
  show a restore mark. Disabled sound controls, battery warnings and outcome
  notices use separate semantic color roles with written labels.
- All eight palette variants meet 4.5:1 text contrast for the added semantic and
  pressed roles. Disabled light Dune text was strengthened after contrast checks.
- The pager uses bold desktop labels and a 720x360 or 640x300 two-by-two layout.
  WindowListSkip surfaces, including its own window and the bar, are omitted.
  A live density change refreshes an open overview; application content remains
  unchanged. Owned launcher/quick-panel windows close during profile rebuilding.
- Panel rebuilding waits for SlatePanel's monitor-width reply. This avoids an
  unnecessary immediate rebuild followed by another refresh.

## Verification

The final policy suite has 16 desktop and 12 launcher checks. New coverage checks
compact bar bounds, preference validation/persistence, all added colorset text
contrast, and visible launcher widget bounds at small and large monitor sizes.

Both Linux architectures pass the full Xvfb desktop regression and two-output
Xorg-dummy window controls. The density gate observes actual bar, launcher,
quick-panel, pager and client geometry, switches light/dark appearance, and
refreshes an open pager. Existing launcher typing, activation, clipboard,
window restoration, status updates, task-list, volume, palette/schedule,
opaque mode, screenshot and core responsiveness checks also pass.

Linux AMD64 was tested under Podman CPU emulation; ARM64 used native containers.
Both builds stage the new profile files successfully. The AMD64 Picom 12.5
legacy XRender preview produced real shadows, but later log review found a
failed XRender picture allocation and an abort during the run. Overlapping
surfaces in the review image have rendering artifacts. This is an unstable
compositor preview, not a passing compositor check. The original gate missed
the late exit; it now checks that Picom remains alive through the review.
The exact artifact cause still needs a controlled compositor comparison.
Picom is optional and was
installed only in the test container, not added to default runtime requirements.

| Target | Policy | Native window controls and density | Install |
| --- | --- | --- | --- |
| Linux AMD64 | 28 pass | Desktop + two-output Xorg pass; Picom preview unstable | Pass |
| Linux ARM64 | 28 pass | Full desktop + two-output Xorg pass | Pass |
| FreeBSD ARM64 | 28 pass | Xvfb pass | Pass |
| OpenBSD ARM64 | 28 pass | Xvfb pass | Pass |
| FreeBSD AMD64 | 28 pass | Xvfb pass | Pass |
| OpenBSD AMD64 | 28 pass | Xvfb pass | Pass |

BSD uses task-owned QEMU snapshots of the existing verification disks. Existing
guests were shut down by their owner during the earlier attempt; their disks were preserved. All four fresh snapshots were rebuilt from the current source because their
saved base binaries predate the current launcher/parser work. All four native
policy, window-control/density and staged-install checks passed.

The private mixer fixture writes atomic JSON. Module probes wait for changed
geometry and swallowed-widget ownership, and tolerate a resource disappearing
between X discovery and its geometry query. These checks handle actual module
replacement without assuming that X resource IDs are unique across restarts.

## Evidence and scope

Evidence lives under `builddir/slate-verification/theme-polish/`: source hashes,
Linux logs/captures, and separate BSD logs/captures. Images are actual X captures,
including intentional test clients; they are not design mockups. The
`linux-amd64/slate-dark-live-launcher.png` and matching light image include the
optional shadow preview; `slate-*-compact-launcher.png` shows Compact.

The user guide is `doc/FvwmDesktop.adoc`. GTK/Qt integration, an icon system,
and palette swatches remain outside this polish pass. Physical GPU/DPI/font and
input hardware still need release testing. The rest of the concurrently changed
core tree was not modified by this theme work; these results are targeted theme
verification, not a claim that every unrelated core gate ran.

## Appearance follow-up

The subsequent [Appearance and interactive review verification](SLATE-APPEARANCE-VERIFICATION-2026-10-04.md)
adds the icon system and palette swatches, resolves the controlled compositor
comparison, and records passing native controls on all six platform targets.
Picom 12.5 legacy XRender also aborts on native ARM64; modern XRender remains
alive and supplies current pixels to the interactive viewer. The optional
profile now uses that modern path. The original failed preview remains retained
as historical evidence.
