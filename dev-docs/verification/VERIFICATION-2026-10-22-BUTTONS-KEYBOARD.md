# Native button keyboard navigation and state — 2026-10-22

Owners: BUTTON-01 #58 / BUTTON-02 #59.
Status: **implemented; focused Linux ARM64 and FreeBSD ARM64 checks pass;
combined qualification pending**.
[Machine receipt](VERIFICATION-2026-10-22-BUTTONS-KEYBOARD.json).

## Feature batch and compatibility

Four related improvements form one checkpoint: module-local traversal and
activation, independent visible keyboard focus, disabled input refusal with
focus recovery, and live enabled/selected/state-palette updates. They implement
BUTTON-01 and the live-presentation part of BUTTON-02. Atomic grouped updates
remain BUTTON-02's next checkpoint; legacy ChangeButton stays incremental.

KeyboardNavigation defaults to 0. Existing Return-under-pointer behavior and
configuration arguments remain usable. Opting in adds Tab/Shift+Tab wrapping
through eligible leaves, directional rectangle navigation and Enter/Space
activation using the current mouse-button-1/default action. Labels, containers,
disabled controls and swallowed applications are skipped. No global key grab
or focus request is added. Held activation, ActionOnPress, focus loss, pointer
click cancellation, all-disabled and re-enabled states have explicit behavior.

Disabled controls refuse pointer, keyboard and PressButton at the shared module
activation boundary. Ordinary labels/icons remain; an optional disabled palette
and small interior cross distinguish them. UI state is not authorization:
managed-window commands still use SendText and the core command engine, and
action handlers remain responsible for validating requests/capabilities.

Focus, selected/pressed and hover palettes have one foreground/background
selector. Focus is also outlined without a special palette. Dynamic changes
preserve record/action identity and current eligible focus. Alternate live title
updates now replace their own strings; previously ActiveTitle/PressTitle wrote
the normal-title pointer. No layout removal, new toolkit or extra mandatory
dependency is introduced. XKB detectable repeat applies only to the module's
connection, with the existing X11 baseline and a traditional repeat fallback.

The [FvwmButtons manual](../../doc/FvwmButtons.adoc) defines exact traversal,
presentation precedence, cancellation, disabled behavior and update semantics.
Native accessibility export, default-theme adoption, general text editing and
module-wide scaling remain their existing owners.

## Qualification scope

Host evidence: `builddir/codex-2026-10-22/buttons-keyboard-state` in the owning
workspace. Reuse owner-controlled incremental builds and verified OpenBSD 8.0
build/PNG caches. Give changed inputs fresh modification times; verify every
source and installed binary/data identity. Run the broad standard profiles
once, with affected optional profiles, staged Buttons/geometry checks,
authenticated BSD default displays and scoped guest ASan/UBSan.

Focused development6 passes three entries on Linux ARM64 GCC and FreeBSD ARM64:
buttons-navigation, buttons-keyboard-xvfb and buttons-transport-xvfb. The real-X
scenario covers nested-grid traversal, reverse/wrap/edge behavior, eligibility,
mouse-specific/default actions, visible focus, independent hover, held activation
and ActionOnPress, disabled pointer/keyboard/PressButton refusal, focus loss,
empty eligibility, live state/palettes/titles, stable actions and legacy behavior.
An unrelated client receives all 200 rapid real key presses and releases while
retaining focus. RGB observations use the server colormap and tolerate RGB565
quantisation on default BSD displays.

Combined profiles, installs, manuals and sanitizer results are pending. Local
VM/container results are not physical-input, native-CPU Linux sanitizer,
toolkit/assistive-client/compositor or release acceptance.

## Retained development failures and cleanup

- Development1 compiled but selected the not-yet-registered live test. The
  registration and fixture were added before qualification.
- Development2 left manual placement enabled in the new fixture and timed out.
  The fixture now requests TileCascadePlacement. Development3 passed feature
  actions but its tree walk raced destroyed decoration windows after KillModule.
  Bounded WM-client-list observation now handles only expected vanished-client
  property errors; all other errors are asserted. These were fixture failures.
- Development4 passed feature assertions but incorrectly awaited an Echo after
  Quit. Direct shutdown fixes that harness error. Development5 and development6
  pass. Failed attempts remain captured; no full matrix was repeated for them.
- Failed fixture logs are canonical on the host. Six exact orphan test processes
  were verified by PID, command and start identity, then stopped. Completed guest
  focused captures/source archives were retired only after canonical hashes and
  payload manifests matched. Sources, incremental builds, stages and VM disks
  remain. These housekeeping actions do not qualify product behavior.

PR144 remains the performance reference. Merged PR145–148 advance its count to
4/5; this unmerged feature checkpoint does not yet advance it. The next merged
implementation checkpoint is due for a lightweight comparison using existing
binaries. No tuning work or benchmark rebuild is added here.
