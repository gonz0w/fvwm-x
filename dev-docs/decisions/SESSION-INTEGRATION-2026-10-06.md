# Default session manager and optional D-Bus integration

Owners: SESSION-01, SESSION-02, coordinated with CFG-01/CFG-02, IPC-02,
NOTIFY-01 and TRAY-01.

Status: **product direction and roadmap scope approved on 2026-10-06**.
Implementation, concrete settings/CLI, dependency selection and qualification
remain with the build thread. This record does not claim an implemented session
manager or change the currently installed login entry/configuration.

## User direction and boundary

The default install should provide the strongest practical portable session
experience using bundled FVWM components and existing base-system capabilities.
Session management is a default product feature, not solely a contrib example.
D-Bus-specific integration belongs in an optional contribution that extends
that same session, configuration and UI. The user intends to use the extension
personally and recommend it to most desktop users; make it comprehensive and
easy to enable, without making its external services mandatory.

Use one session capability/settings model and one set of session actions. Do not
create a separate desktop theme, competing session manager, alternative settings
store or parallel command transport for the extended profile. Absent optional
services must not prevent an ordinary FVWM login.

## SESSION-01: portable default session foundation

After the shared desktop primitives, implement a process-isolated lifecycle
owner integrated with the packaged default login path and existing desktop
helper. Keep authoritative X/window state and managed actions in the core;
reuse the shared command engine, configuration registry and module/MFL transport.
Component names and exact startup wiring are not selected by this document.

- Establish a consistent session environment before FVWM/application startup.
  Preserve display/authentication environment and the existing direct `fvwm3`
  invocation, custom themes, commands and session-manager compatibility.
- Provide well-commented default configuration and installed man documentation
  for services, startup order, availability, restart/logout and local overrides.
  Expose validated settings in the existing configuration/session UI rather
  than requiring users to rewrite startup functions.
- Define startup prerequisites, service roles and bounded readiness/failure
  handling. Separate installed, configured/enabled, starting, ready, unavailable
  and failed states. Executable discovery or process launch is not readiness.
- Track explicitly owned children, exits and cleanup. Prevent duplicate starts
  across WM restart; bound retry/backoff and avoid endless crash loops. Reuse
  externally owned services without adopting or terminating them. No name-based
  `killall`/`pkill`, unvalidated PID-file signaling or global process cleanup.
- Offer controlled XDG autostart support, honoring user overrides, `Hidden`,
  `TryExec` and desktop filters. Reuse reviewed desktop-entry/Exec handling,
  not shell evaluation of labels or settings. Do not enable removable-media
  autorun as a consequence of application autostart support.
- Unify the current OS power/locker capability policy, native session UI,
  local notices and available portable tray/display/idle integrations. Installed
  optional tools alone are not consent to start a new daemon or alter displays.
- Distinguish WM restart, session logout and system power transitions. Define
  cooperative application shutdown/cancellation and bounded completion, reusing
  existing XSMP behavior where clients participate. Do not promise restoration
  of arbitrary applications or silently force-close unsaved work.
- Integrate existing OS-maintained lockers without implementing authentication.
  Require a validated locker-specific readiness contract for coordinated
  lock-before-sleep. Reject/report a coordinated request when protection cannot
  be confirmed; do not substitute a fixed delay or claim control over unrelated
  OS sleep/lid events when the backend supplies no inhibition mechanism.
- Keep OS authorization with the existing system services. No automatic
  privilege escalation, forced power action or global authentication changes.

SESSION-01 is portable session lifecycle work, not a new login/display manager,
init system, privilege daemon, compositor or replacement for application tools.
Automatic resume/lid handling and sleep inhibitors remain capability-specific;
report their absence honestly on each target.

## SESSION-02: comprehensive optional D-Bus contribution

The contribution supplies a reviewed enablement/profile path for the default
session foundation. Aim for one documented user-level opt-in through the common
settings/configuration surface, with installed manual examples and predictable
disable/reset behavior. Final setting names and packaging are pending; do not
publish placeholder commands as implemented features.

- Discover and reuse an existing usable session bus. When explicitly enabled,
  an installed supported bus launcher may create a session-scoped bus if needed,
  before FVWM and applications inherit the environment. Track whether it is
  borrowed or owned; never stop a borrowed/system/user-manager bus at logout.
- Distinguish a session bus from the system bus. Never start a system bus or
  enable system services from a user's desktop configuration. Avoid competing
  bus instances, environment mismatch and unconditional login-loop retries.
- With D-Bus absent, preserve the portable session and report the extension as
  unavailable. With a failed/inaccessible bus, report failure and offer the
  supported portable fallback; do not silently report standard services active.
- Integrate available login1/systemd or elogind capability/event/inhibitor
  adapters. BSD keeps its native backend and gains only optional interfaces
  actually supplied there. No mandatory systemd/elogind on any target.
- Extend the same native notification/tray actions with the standard services
  under NOTIFY-01/TRAY-01. Respect existing owners; the contribution is not an
  excuse to replace another notification server, watcher or tray automatically.
- Provide configuration/documentation for already installed authentication
  agents, portal backends and other explicitly selected session services where
  supported. Discover their actual capabilities and coordinate ownership;
  a bus alone does not supply any of them. No new portal, credential store or
  authentication-agent implementation is authorized by this profile.
- Reuse the common session UI for availability, service health, power choices,
  lock/idle state and actionable errors. Keep appearance/live preferences aligned
  with the default desktop, including the existing locker/Autorandr recipes.
- Document display-manager and manual-start paths, prerequisites, environment,
  service ordering, owner conflicts, restart/logout, fallback and troubleshooting
  across Linux, FreeBSD and OpenBSD. Avoid hidden Linux-only defaults on BSD.

Select the shared optional D-Bus client/build/runtime contract before protocol
adoption, jointly with NOTIFY-01/TRAY-01. No library or mandatory package is
selected here. Use a maintained portable client layer, not a hand-written wire
protocol or shell polling in the core. Keep bounded callbacks/queues, peer data
validation and timeouts outside the core; translate only validated operations
through the accepted transport. Remote labels/actions are data, not arbitrary
FVWM or shell command authority.

## Execution and acceptance

Preserve the authoritative tracker order: existing security/responsiveness and
shared primitives first; SESSION-01 then SESSION-02's bootstrap/dependency
contract; NOTIFY-01 then TRAY-01; complete extension integration/qualification
with those owners. RELEASE-01 stays last. Reuse shared parsing, settings,
process, readiness and operation-result work rather than implementing it twice.

Use focused development checks and the combined qualification cadence. Required
coverage includes no-bus portable login, missing optional packages, borrowed vs
owned bus, failed startup/recovery, duplicate/competing owners, WM restart vs
logout, existing XSMP compatibility, autostart overrides and malformed input,
denied power requests, failed locker/readiness, live settings, and native service
integration. Qualify enabled/disabled builds and staged default/contrib installs,
manuals and actual-X behavior on the relevant six-target matrix. Physical
authentication, lid, suspend/resume and hotplug checks remain separate evidence.

This configuration thread records requirements and sequencing; the build thread
owns runtime/build/install changes. The native locker remains deferred. Release
publishing and changes to legacy compatibility policy remain separately owned.
