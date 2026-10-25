# Native notifications and system tray — roadmap scope approved

Later scope update, 2026-10-25: [LOCK-01 #225](https://github.com/gonz0w/fvwm-x/issues/225)
now owns qualified external-locker readiness, idle/display-power/suspend
coordination after SESSION-01. This supersedes deferral of that integration
only; a new native authentication implementation remains unselected. Portable
session/bus ownership and current accepted components stay unchanged. See the
[modernization order](MODERNIZATION-ORDER-2026-10-25.md).

Owners: NOTIFY-01 and TRAY-01. Status: the user approved adding both services
to the roadmap on 2026-10-06. Runtime implementation, concrete command syntax
and dependency selection remain pending in the build thread. The locker is
deferred and is not added to the implementation queue.

## Placement and shared design

The later approved [session direction](SESSION-INTEGRATION-2026-10-06.md)
adds a portable default lifecycle owner (SESSION-01) and comprehensive optional
D-Bus contribution (SESSION-02). Coordinate bus/dependency selection, service
ownership, settings and UI with those owners; do not create separate launchers
or settings stores here. This supplements the sequence below with the session
foundation and shared bus contract before notification/modern-tray adoption.

Follow the authoritative [execution order](../PROJECT-TRACKER.md#execution-order):
existing hardening, responsiveness and desktop primitives first, then NOTIFY-01,
TRAY-01's XEmbed checkpoint, and TRAY-01's StatusNotifier checkpoint. The final
public-release gate remains last. Urgent reproduced defects retain precedence.

Use separate native processes with existing Xlib/Xft drawing, colorsets, picture
handling, monitor/scale policy and module transport. Proposed component names
are FvwmNotify and FvwmTray; they are not existing executables or finalized APIs.
Keep authoritative managed-window state in the core. Reuse module-local bounded
event/timer handling; external service work must not block the core event loop.

Use BUTTON-01/BUTTON-02 where shared focus/action or dynamic-update functionality
is needed. Reuse CFG-01's typed preferences and IPC-02's correlated queries;
do not create another settings registry, window-state authority or transport.
Support validated live appearance/layout updates without restarting applications
or rewriting personal configuration. Preserve existing module/config contracts.

## Dependency decision before protocol implementation

Standard application notifications and StatusNotifier use a D-Bus session bus.
The required FVWM build stack currently has no D-Bus client layer. Evaluate a
maintained, portable system library and package availability across all six
targets; select the optional feature/build/runtime contract before adoption.
This roadmap approval does not select a library or authorize new mandatory
desktop packages, GTK/Qt, a bus implementation or a compositor.

A build/session without D-Bus can retain FVWM-local notices and an XEmbed tray;
it must not advertise the standard D-Bus notification or StatusNotifier services.
Document capability availability. Respect another notification server or tray
owner; do not silently replace it. Service activation is opt-in until a separately
reviewed default policy is accepted.

## NOTIFY-01: native notification service

- Implement the freedesktop notification interface: server information,
  capabilities, notification IDs/replacement, closure, urgency, expiration,
  action replies and corresponding signals. Advertise only implemented features.
- Provide non-focus-stealing popups with wrapping, dismissal, monitor/work-area
  placement and scale-aware fonts/images. Reuse palette colorsets and dynamic
  configuration for spacing, placement and presentation.
- Add dismiss-all, do-not-disturb and optional bounded in-memory history.
  Define reload/reset and restart behavior; persistent history is not implicit.
- Bound message/text/image sizes, notification/history counts, work per turn,
  pending replies and flood behavior. Validate UTF-8, hints, image strides and
  dimensions. Apply existing image acquisition/lifetime policies to file images.
- Notification labels, markup, paths and action IDs are data, never FVWM/shell
  commands. Action activation sends the protocol response to the application.
  Start with a documented limited markup/capability policy, not an HTML engine.
- Acceptance: native policy/protocol tests plus real-X replacement/expiry/actions,
  focus preservation, flood/refusal/recovery, reconnect/owner conflicts, live
  palette/settings changes, monitor hotplug and mixed-DPI behavior.

Protocol: [Desktop Notifications specification](https://specifications.freedesktop.org/notification/latest-single/).

## TRAY-01: native system tray in two checkpoints

1. **XEmbed:** own the appropriate tray selection, publish manager discovery,
   handle dock requests and embedded-client mapping/focus/resize/lifetime.
   Provide a standalone host with an optional FvwmButtons container. Verify
   integration before relying on swallowing; existing swallowing and legacy
   KDE tray bookkeeping are not full protocol implementations.
2. **StatusNotifier:** add watcher/host registration, application disappearance,
   icon/state changes, activation, tooltips and supported application menus.
   Define supported D-Bus menu behavior and test with representative real clients;
   an XEmbed-only checkpoint must not claim modern tray coverage.

Use dynamic visibility, icon size/spacing, monitor placement and layout updates.
Integrate a tray widget with the desktop helper's adaptive bar without replacing
the bar or changing existing configured widgets. Define empty/overflow handling,
keyboard access, restart/reconnect ownership and settings persistence.

Treat clients, properties, icons and menu labels as untrusted data. Bound client
counts, icon/menu payloads, reply timeouts and work queues; handle destroyed/reused
XIDs and bus-owner changes. Menus dispatch protocol actions, not label-derived
commands. Preserve tray-client pixels rather than pretending embedded foreign
windows are FVWM-owned scalable UI.

Acceptance: native protocol/lifecycle tests plus actual-X docking, focus/input,
resize, destruction, competing owners, restart, overflow and bar integration.
The modern checkpoint adds real application registration/menu/icon checks and
bounded malformed/slow/disconnected-client recovery.

Protocols: [System Tray](https://specifications.freedesktop.org/systemtray/latest-single/),
[XEmbed](https://specifications.freedesktop.org/xembed/latest/),
[StatusNotifier](https://specifications.freedesktop.org/status-notifier-item/latest/status-notifier-item.html).

## Qualification and exclusions

Use focused development checks and combined batch qualification from
[WORKFLOW.md](../WORKFLOW.md#implementation-batches-and-test-cadence). Required
Linux/FreeBSD/OpenBSD AMD64/ARM64 builds, native tests, relevant nested/default-X
integration, feature-enabled/disabled installs and installed man pages remain
acceptance gates. Record unavailable optional profiles honestly. Native Linux
sanitizer CI and physical compositor/hardware qualification remain distinct.

No locker/authentication feature, broad theme redesign, external application
replacement or release publication is authorized by these roadmap entries.
