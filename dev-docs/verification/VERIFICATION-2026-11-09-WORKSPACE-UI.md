# Workspace tools UI checkpoint

The PR374 Linux ARM64 Xvfb/XTest checkpoint passed 30 cases using the retained
production binaries. Slate dark passed 14 cases. Dune light and enlarged
Classic passed eight each. The runner verified the retained build, policy,
Go, race, scoped sanitizer and manual receipts and ran the UI fixture only.
The owned container and VM shut down. The fixture reported no cleanup failures
or unexpected observer X errors.

[Source and binary hashes, cases, screenshot hashes and shutdown receipt](VERIFICATION-2026-11-09-WORKSPACE-UI.json) ·
[Earlier build and policy evidence](VERIFICATION-2026-11-08-WORKSPACE-TOOLS.md)

The existing two-stage Desktop Entry serializer qualified when the fixture
committed its application catalog and entered the new workspace scene.
The first UI attempt then exposed a gap in the observer. The Desktop helper
received its scratchpad configuration and sent status requests, while the
proxy omitted directed core replies from its log. The proxy now records those
replies for this fixture within its existing bounds. The next attempt passed
seven Slate cases and reached the manager's Next page, where the fixture
incorrectly assumed the replacement Form would have a different X window ID.
The final attempt confirmed each new Form lifetime and its expected page
configuration before continuing with keyboard input.

| Theme | Overview | Manager | Scratchpad application |
| --- | --- | --- | --- |
| Slate dark | [Marked window](../../doc/screenshots/workspace-overview-marked-slate-dark.png) | [Manager](../../doc/screenshots/workspace-scratchpad-manager-slate-dark.png) | [Summoned app](../../doc/screenshots/workspace-scratchpads-slate-dark.png) |
| Dune light | [Marked window](../../doc/screenshots/workspace-overview-marked-dune-light.png) | [Manager](../../doc/screenshots/workspace-scratchpad-manager-dune-light.png) | [Summoned app](../../doc/screenshots/workspace-scratchpads-dune-light.png) |
| Enlarged Classic | [Marked window](../../doc/screenshots/workspace-overview-marked-classic-large.png) | [Manager](../../doc/screenshots/workspace-scratchpad-manager-classic-large.png) | [Summoned app](../../doc/screenshots/workspace-scratchpads-classic-large.png) |

#221 has real-X evidence for literal launch, rapid-toggle coalescing, placement,
size preservation, keyboard summon/dismiss, focus return, manager paging,
sticky/fullscreen refusal, missing executable handling and helper/core restart.
The terminal path used a strict-size-hints fixture because the guest lacked
xterm. Real terminal, live transient, actual XID reuse and mixed-monitor/scale
coverage remain open.

#223 has real-X evidence for searchable window/workspace facts, keyboard
activation and focus return, movement to another workspace at page (0,0), pointer Move with observed
frame displacement, stale target refusal and teardown. The native scene ran
without a compositor. Large collections reuse the existing palette evidence.
Overview-specific bounded update cost under topology and scale changes remains
open. A live overview-driven desktop switch and nonzero-page relocation
remain unqualified. The labels-first scope and deferred thumbnail evaluation are recorded in
the [behavior contract](../decisions/WORKSPACE-TOOLS-2026-11-07.md).

Visual inspection found readable controls in Slate dark and Dune light.
The enlarged Classic manager fits, while the overview extends beyond the
right edge of the 1440 × 1200 display. The Pointer Move control and right
title-bar controls are clipped. The enlarged fixture application's title
bar is also clipped. Keyboard dispatch passed independently of this
visual defect.

PR374 remains draft. The next checkpoint needs a compact overview layout
and a focused enlarged Classic capture that verifies the full frame and
all controls fit. The UI attempt budget is exhausted. Both issue maps
remain open. Platform/install, physical input and assistive technology
coverage retain their separate gates.
