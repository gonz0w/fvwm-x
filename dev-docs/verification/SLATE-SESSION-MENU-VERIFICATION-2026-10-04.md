# Slate session menu — 2026-10-04

The default Session menu now groups session controls, desktop maintenance and
information/help. It is reachable from the bar, Quick panel, traditional menu
and application launcher. Native FVWM menus and FvwmForm provide the interface;
new `session.py` uses the existing Python runtime and its standard library.
No desktop package or library was added. No core C change belongs to this work.

## Behavior

- Lock, Sleep, Hibernate, Restart FVWM, Log out, Reboot and Power off.
- Missing or denied services have disabled rows with reasons. Permission or
  hardware uncertainty stays explicit; finding a binary does not guarantee a
  working power cycle.
- Desktop: Appearance, bar configuration, workspace overview, arrangements,
  refresh panels, refresh applications, redraw and personal configuration folder.
- Information/help: session/backend details, actual configured shortcuts,
  logged-in users, system monitor and display information. Terminal entries
  require existing base tools and a terminal.
- Specific confirmations for all six session/power actions. Return cancels,
  Escape closes, and pointer Cancel preserves the session. Reopening an
  information or confirmation form replaces its existing instance.
- Sleep/hibernate explicitly state that they do not automatically lock. Secure
  locking remains delegated to an existing xlock/slock. No decorative lock screen
  or user-switching login manager is implied.
- Capability discovery runs in bounded helper workers, is cached for 60 seconds,
  and is refreshed on menu opening when stale. Status updates do not rebuild a
  posted menu. Activation rechecks power capabilities, executes owned argv, never
  escalates privileges or retries, and reports failures or timeout uncertainty.
- Submenus now open beside their parent with an 8-pixel gap, rather than covering
  the parent rows. Existing personal configuration remains the override point.

Linux uses running systemd/logind capabilities and systemctl, or an existing
independent shutdown command without systemd. FreeBSD offers reported ACPI S3
through acpiconf and deliberately does not equate S4 advertisement with verified
hibernation. OpenBSD offers zzz/ZZZ when the apmd socket exists, while retaining
hardware/permission uncertainty. BSD reboot/power off use existing shutdown.
Commands and options were checked against primary documentation:
[systemd systemctl](https://github.com/systemd/systemd/blob/main/man/systemctl.xml),
[login1 capability methods](https://github.com/systemd/systemd/blob/main/man/org.freedesktop.login1.xml),
[FreeBSD acpiconf](https://man.freebsd.org/cgi/man.cgi?query=acpiconf&sektion=8),
and [OpenBSD apm/zzz](https://man.openbsd.org/apm.8).

## Verification

There are 17 desktop, 14 launcher and 14 new session policy checks: 45 total.
The new cases cover denial, unsupported services, authorization challenges,
inactive systemd symlinks, BSD backend arguments, explicit hardware uncertainty,
invalid actions, capability rechecks, no retries/escalation, timeout/error
reporting, safe confirmation defaults and stable menu snapshots.

The targeted real-X gate loads the baseline and native module processes, posts
and dismisses the actual menu, opens both information forms, opens all six
confirmations, and cancels using real XTest Return, Escape and pointer input.
Screenshots are native X images. Actual shutdown, restart, logout, lock,
suspend/resume and hibernation requests were not executed. Backend success/failure
policy uses controlled substitutes; this report does not certify hardware power
or authentication support.

| Target | Policy checks | Targeted native session menu/forms |
| --- | --- | --- |
| Linux ARM64 | 45 pass | Pass |
| Linux AMD64, CPU emulation | 45 pass | Pass |
| FreeBSD ARM64 | 45 pass | Xvfb pass |
| OpenBSD ARM64 | 45 pass | Xvfb pass |
| FreeBSD AMD64, CPU emulation | 45 pass | Xvfb pass |
| OpenBSD AMD64, CPU emulation | 45 pass | Xvfb pass |

BSD runs use task-owned QEMU snapshots and their preserved native C builds.
Only the current configuration, helper policy and tests were staged; the helper
shebang and PNG substitution were configured for each guest. The session-only
mode excludes the unrelated launcher-parser rejection test. Linux reconfigured
Meson generates and installs `session.py` beside FvwmDesktop. The session policy
is registered as `desktop-session-policy`; the native session gate is also part
of the existing complete desktop integration test.

The Linux ARM64 complete desktop and two-output Xorg-dummy regression also
passes, including launcher input/actions, Appearance, Quick controls, status,
task list, screenshots, palette/schedule, density and window placement. This
follow-up does not replace the independent core C/sanitizer verification.

Evidence: `builddir/slate-verification/session-review/`. Final Linux captures are
in `linux-arm64-final/` and `linux-amd64-final/`. Four BSD archives retain native
screenshots, core logs, evidence JSON and policy logs. `source-sha256.json`
records the changed helper/configuration and test sources. Initial Linux test
failures were probe assumptions: native FVWM menu windows use save-under rather
than override-redirect, and an expanded button row positions Cancel at its left
edge. Corrected probes pass; initial diagnostic captures are retained.

The existing live desktop was updated without closing its application windows
or replacing its saved preferences. Its browser screenshot is `live-session.jpg`.
