# Slate Session panel — 2026-10-05

Session now opens a native FvwmButtons panel instead of crowded menu rows.
The commented baseline lives in `default-config/session.fvwm`. The 736×624
surface uses a fixed grid, 32-pixel outer margins, separate status captions,
large action tiles, and seven original 32-pixel symbols. Light/dark and all four
palettes update the current panel, including hover and pressed states. Dedicated
colorsets 40–60 allow availability changes without rebuilding a posted panel.
The title has its own configurable font; buttons use the existing heading font.

Lock/pause, session/power, and desktop tools form three visual groups. Appearance,
desktop maintenance, information and shortcuts remain accessible. Escape/Close
and repeated Session activation dismiss the panel. Alt-letter shortcuts use the
same capability guard as pointer input. Selecting an available action closes the
panel before its confirmation or next interface. Missing services appear muted
with a short caption; full diagnostics remain in Information or the rejection
notice. Existing safe confirmations default to Cancel.

The core provides current-monitor dimensions. Below 800×700, Session opens the
traditional native menu with generous horizontal/vertical padding. The padded
menu is also explicitly available through `session-menu-native`. No toolkit,
icon font, package, library, compositor or core C change was added by this work.
Images are bundled; builds without PNG retain text buttons. Optional Picom in
the existing review environment remains independent of the default desktop.

## Verification

All six Linux/FreeBSD/OpenBSD × ARM64/AMD64 targets pass 48 policy checks
(17 desktop, 14 launcher, 17 session) and the targeted native-X Session gate.
The actual panel is mapped at 736×624, toggled and dismissed, captured in light
and dark, and activated through pointer and Alt-key input. Information forms,
small-screen fallback, and all six confirmation forms are exercised. Return,
Escape and pointer cancellation preserve the session. No actual lock, sleep,
hibernation, shutdown, reboot, logout or WM restart was accepted by the tests.
Service availability therefore does not certify authentication or hardware power
support; that policy is unchanged from the prior session menu report.

Linux ARM64 also passes the complete desktop integration gate, including rapid
launcher typing, declared application actions and real result-row clicks,
Appearance, window controls, native metrics/tasks, screenshots, density and
configuration parsing. AMD64 passes the targeted Session gate; two full desktop retries failed in
the pre-existing Appearance click checks (schedule opening and task-list toggle
restoration). No complete AMD64 desktop pass is claimed for this follow-up. Linux uses existing native builds; AMD64 CPU execution is
emulated. BSD uses task-owned QEMU snapshots and preserved native C builds,
with only helper/configuration/asset/test sources staged for this change.
The snapshots were stopped after final evidence collection.

A Linux staged install includes `session.fvwm` and all 168 new Session PNGs
(seven actions × eight palette/modes × normal/selected/disabled states), bringing
the bundled icon total to 368. Local Python compile checks and policy tests pass.

Initial full desktop runs exposed reordered ordinary characters after adding
plain-letter WM shortcuts scoped to the panel. A retained actual-X launcher
capture shows the malformed query. Switching Session shortcuts to Alt-letter
avoids grabbing ordinary typing; the unchanged launcher behavior passes the
full ARM64 gate. The intermediate wildcard replay and window-context experiments
were removed. An early BSD rerun exposed a stale-cache test that assumed guest
uptime exceeded 60 seconds; its fixture now explicitly subtracts 61 seconds from
monotonic time. Final BSD checks pass. AMD64 full runs missed Appearance
clicks, including one run after stopping the BSD guests. Their cause remains
unresolved; separate retained evidence records both failures.

Evidence is in `builddir/slate-verification/session-panel/`. Four BSD archives
retain native screenshots, logs and evidence JSON; per-target policy logs and
`source-sha256.json` record the final sources. Final Linux evidence is retained
alongside the initial regressions. `light.png` and `dark.png` are actual X captures
of the updated live desktop, not design mockups. The live review remains at
localhost:6080 with automatic appearance restored and the Session panel open.
