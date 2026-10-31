# Development preview: native desktop controls and themes

FVWM-X is in active development. These examples show native desktop controls
and the optional file-browser prototype. The file manager and clipboard
application remain in development. The [project tracker](../dev-docs/PROJECT-TRACKER.md)
owns current status. The original live-update checkpoint in
[PR #173](https://github.com/gonz0w/fvwm-x/pull/173) is retained in the earlier
theme examples; newer sections record their own source identities.

## Settings preview (in progress)

The [FvwmDesktop manual](FvwmDesktop.adoc#_preview_one_setting) describes the
editor and its one-setting preview. These unmodified native-X captures are from
the gate 31 three-theme workflow, which passed for Slate dark, Dune light and
Classic with enlarged font. The source22 closure was
`509c0317cab676cbdf318c86ada18c6ea2ba9202ab7f343b597ce198c4ff67cd`.
The captures show individual moments, not an accessibility or release result.

Classic enlarged editor:

![Classic enlarged settings editor with the Preview one setting control](screenshots/settings-20261031-classic-editor.png)

Dune light F5 chooser:

![Dune settings preview chooser over the editor](screenshots/settings-20261031-dune-chooser.png)

Slate dark wallpaper preview confirmation:

![Slate wallpaper preview confirmation with Save and Cancel](screenshots/settings-20261031-slate-wallpaper.png)

The raw PNG SHA-256 values in the order above are
`53852c5924c67e9f8f0425fcf2df4b2dd789844634a458b2c665cd1b7bb07e28`,
`e89f49690fb71ddf7f7063f568add4af0db56be294b593b9c1455bc2e42c6ef9`, and
`f305d5151eb29d2b5a99e0a04e713476f9fae3e220b7640f92bbc1555c1bb256`.
The [capture manifest](screenshots/settings-completion-20261031-manifest.json)
binds them to the source archive and configurations.
See the [in-progress evidence](../dev-docs/verification/VERIFICATION-2026-10-31-SETTINGS-COMPLETION.md)
for workflow scope and remaining checks.

## Native application preferences (in progress)

The optional [application preferences editor](FvwmDesktop.adoc#_application_preferences)
shares the existing saved settings. It publishes supported font, icon, cursor
and animation values through an owned native helper. Publication starts disabled;
empty font/theme fields and cursor size zero inherit application fallbacks.
The editor shows observed native and cursor-resource status separately.

Slate dark shows the native editor and two actual GTK3 clients using Sans 22.

![Slate application preferences with actual GTK3 font samples](screenshots/native-preferences-slate-dark.png)

Dune uses the same controls with light theme roles.

![Dune application preferences](screenshots/native-preferences-dune-light.png)

The enlarged Classic customization uses DejaVu Sans Mono 18 for FVWM UI.

![Enlarged Classic application preferences](screenshots/native-preferences-classic-large.png)

The [capture manifest](screenshots/native-preferences-manifest.json) retains
exact [Slate](screenshots/native-preferences-slate-dark.fvwm),
[Dune](screenshots/native-preferences-dune-light.fvwm) and
[Classic](screenshots/native-preferences-classic-large.fvwm) configurations.
These contain temporary lab paths; adapt them before use. Actual GTK3 settings,
notifications, existing/fresh clients, enlarged text and fresh Xcursor lookups
were observed. This does not establish Qt/browser behavior, cached-client cursor
refresh, a settings portal or full platform/release acceptance. See the
[source-bound report](../dev-docs/verification/VERIFICATION-2026-10-31-NATIVE-PREFERENCES.md).

## What is available

Recent checkpoints add atomic button updates, desktop settings/editor and
configuration checking, structured IPC queries/actions and scoped settings
preview/recovery. See the [desktop manual](FvwmDesktop.adoc) and
[MFL manual](FvwmMFL.adoc) for their supported interfaces.

The optional `fvwm-file-browser` uses FvwmForm and FvwmMFL, with no additional
GUI toolkit. It supports local folders, list/grid views, keyboard navigation,
range/multiple selection, label filtering, metadata sorting, observed properties,
reveal requests and configured application actions. Same-folder refresh,
filtering, sorting and status/property updates preserve the native window,
unsubmitted input drafts and surviving selection. Changing folders can rebuild
the view and reset drafts; list/grid switches keep the existing controls.

Newer development adds opt-in directory watching (`--watch auto`, or F8), with
coalesced native invalidations, periodic reconciliation and explicit manual
fallback. F5 remains available; Cancel work disables watching. The four original
[Slate/Dune/Classic captures](#slate-dark-list-and-live-refresh) retain their
PR173 source identity and predate that watch status/control; the watching examples show the newer checkpoint.
See the [browser manual](fvwm-file-browser.adoc#directory-watching) for current
platform and consistency limits.

Limits include 4096 loaded entries and 256 selected entries. Larger folders are
refused as a whole. Properties describe the last admitted metadata snapshot;
they do not read file contents or calculate recursive folder sizes. When no mutation job is active, the prototype
closes after five minutes without a callback, accepted external control or
current-Form interactive feedback. The
[file-browser manual](fvwm-file-browser.adoc) specifies controls, limits,
worker lifetime, literal-path actions and error behavior.

## FileManager1 browser requests — work in progress

The explicit `fvwm-file-browser --filemanager-service` mode accepts
`ShowFolders`, `ShowItems` and `ShowItemProperties` on an existing local session
bus. It reuses the native browser, preserves literal URI/filename bytes and
admits folder groups in order. Success acknowledges admitted work; folder loading
and presentation finish separately. It does not change folder or MIME defaults.
See the [setup and limits](fvwm-file-browser.adoc#filemanager1-service) and
[opt-in configuration example](examples/FvwmFileManager1.fvwm).

Slate shows a grouped reveal request selecting two entries without opening them.

![Slate browser opened through FileManager1 with two revealed entries](screenshots/filemanager-bus-slate-dark.png)

Dune shows a folder opened through the same service.

![Dune browser folder opened through FileManager1](screenshots/filemanager-bus-dune-light.png)

Enlarged Classic uses an explicit navy/ivory/wheat Form configuration and shows
observed properties for two requested entries.

![Classic enlarged browser showing two requested entries and observed properties](screenshots/filemanager-bus-classic-large.png)

The [capture manifest](screenshots/filemanager-bus-manifest.json) links exact
[Slate](screenshots/filemanager-bus-slate-dark.fvwm),
[Dune](screenshots/filemanager-bus-dune-light.fvwm) and
[Classic](screenshots/filemanager-bus-classic-large.fvwm) fixture configurations.
These contain temporary lab paths; adapt them before use. Nine unmodified
captures accompany the three passing native-X workflows. Screenshots do not
establish assistive-technology or physical-hardware acceptance. Broader supported
install/selection qualification remains open under #67/#77; see the
[source-bound report](../dev-docs/verification/VERIFICATION-2026-10-31-FILEMANAGER-BUS.md).

## Standalone XEmbed tray

`FvwmTray` is an opt-in standalone host for a bounded subset of XEmbed tray
clients. The initial host admits root-child windows using the screen's default
visual and depth. Bar/container embedding, StatusNotifier items and menus are
not yet supported. See the [manual](FvwmTray.adoc) and
[standalone configuration example](examples/FvwmTray-Standalone.fvwm).

These unmodified 3200×1080 Xorg captures show the host with synthetic blue
square clients, not real application icons. Slate, Dune and Classic use distinct
palette and font settings:

![Slate dark standalone tray with two synthetic blue clients](screenshots/tray-slate-dark.png)

![Dune light standalone tray with two synthetic blue clients](screenshots/tray-dune-light.png)

![Classic larger-font standalone tray with two synthetic blue clients](screenshots/tray-classic-large.png)

At 2× the host font scales while the synthetic clients retain their 24×20
actual-pixel dimensions:

![Standalone tray at 2× with unchanged synthetic client pixels](screenshots/tray-primary-2x.png)

The [Slate](screenshots/tray-slate-dark.fvwm),
[Dune](screenshots/tray-dune-light.fvwm), and
[Classic](screenshots/tray-classic-large.fvwm) configurations and
[capture manifest](screenshots/tray-manifest.json) record the inputs. Scoped
native Linux model and independent X11 cases passed in normal and sanitizer
builds; the [checkpoint report](../dev-docs/verification/VERIFICATION-2026-10-30-XEMBED-TRAY.md)
records their evidence and limits. Broader operating-system, toolkit and full
tray acceptance remain in progress.

## Session power providers and manual Lock

Session now projects the shared portable or optional login1 power policy,
per-action permission states, an explicit power probe and manual Lock. Poweroff
and Reboot require a confirmation bound to the observed provider and settings.
Changing policy requires a live owner handshake and explicit reprobe. Old private
parent versions need a fresh login after upgrade; see the
[session manual](fvwm-session.adoc).

Slate shows the native status and controls; diagnostic rows elide to fit.

![Slate session provider controls](screenshots/session-providers-slate-dark-panel.png)

Dune shows all four shared session preferences.

![Dune four-field session settings](screenshots/session-providers-dune-light-settings.png)

Enlarged Classic shows the confirmation and an explicit legacy-color Form.

![Classic power confirmation and legacy colors](screenshots/session-providers-classic-large-confirmation.png)

The [capture/configuration manifest](screenshots/session-providers-manifest.json)
binds these unmodified 1440×1000 images to the passing three-theme fixture.
Manual Lock tracks one external locker child across Desktop/core restart and WM
exit; running is not a protection acknowledgment. Coordinated suspend remains
unavailable without qualified secure readiness. These fixtures use harmless
native commands and a private provider bus, not real power or authentication.
The [checkpoint report](../dev-docs/verification/VERIFICATION-2026-10-29-SESSION-PROVIDERS.md)
records checks and broader release gaps. Development continues.

## Session and services

Open **Session → Session and services…** to inspect the owned core, autostart scan,
optional bus, existing notification server and independent application outcomes.
Refresh (F5), Settings (F2), Test notification (F6) and Log out (F3) use the shared
controller. During cooperative logout, Cancel (F4) leaves the desktop running;
applications already closed cannot be restored. Unsupported/refusing or newly
opened applications prevent an unconditional final Quit.

The compact editor saves `session_bus=off|reuse|start`,
`session_notifications=off|external` and `notification_dnd`. Bus policy applies at
next login; notifications and DND use a live owner handshake. The defaults keep
optional integration off. The isolated native bus client requires an explicit
`-Dsession-dbus=enabled` build and an installed existing provider; no daemon or
notification server package is installed by the build.

These commands also work in an existing FVWM configuration:

```fvwm
SendToModule FvwmDesktop session-panel
SendToModule FvwmDesktop edit-settings group:session
```

Slate dark: the native Session inventory and controls.

![Slate Session panel](screenshots/session-services-slate-dark-panel.png)

Dune light: the three-field editor, with separate saved/live policy semantics.

![Dune session settings](screenshots/session-services-dune-light-settings.png)

Classic customization: eighteen-point monospace text, custom borders and a
separate Form using explicit legacy colors while the Session panel keeps its
semantic colorsets. The observed application is waiting for cooperative closure;
Cancel remains available.

![Classic cooperative logout and legacy module colors](screenshots/session-services-classic-large-waiting.png)

The [manifest and raw configuration snapshots](screenshots/session-services-manifest.json)
bind unmodified 1440×1000 captures to their executed inputs. Snapshots contain
private fixture paths; the Classic overlay records the actual legacy color
commands. The [checkpoint report](../dev-docs/verification/VERIFICATION-2026-10-29-SESSION-SERVICES.md)
records real-client, bus-loss/replacement, reconnect and timeout outcomes.
Whole-guest panel counters are not a WM benchmark. Full cross-platform service
qualification, native notification popups/history, broader tray support and
locker/power adapters remain in development. See the [session](fvwm-session.adoc) and
[desktop](FvwmDesktop.adoc) manuals for scope and limits.

## Preview palette or light/dark appearance

In a Desktop settings editor, choose the candidate, then press **Palette (F6)**
or **Appearance (F7)** in the Preview row. Save (Enter) keeps the preview and
saves only that preference. Cancel (Escape) or expiry recovers the previous
owned resources. The returned editor retains other unapplied fields.

These previews require an opaque effective presentation and unchanged resolved
wallpaper choices. Disable transparency, or use high contrast; retain the current
wallpaper or select independent wallpaper choices as appropriate. The controls
explain unmet prerequisites. They do not change those preferences automatically.
Appearance preview accepts Light or Dark; Auto remains available through Apply.
See the [desktop manual](FvwmDesktop.adoc) for conflicts and failed-save recovery.

Slate fixture: a palette candidate with Save and Cancel.

![Native palette preview with Save and Cancel](screenshots/palette-preview-slate-dark.png)

Dune fixture: explicit Dark appearance with the existing wallpaper retained.

![Native Dark appearance preview retaining wallpaper](screenshots/appearance-preview-dune-light.png)

Enlarged Classic: Save returns to the full editor, with the compact F5/F6/F7 row
and the remaining fields still available.

![Enlarged Classic editor after saving only appearance](screenshots/appearance-preview-classic-draft.png)

These unmodified 1440×1000 development captures use three fixture configurations;
the visible colors follow the candidate, so they can differ from the profile
name. The [manifest](screenshots/palette-appearance-preview-manifest.json) binds
each capture to its executed source and configuration. The
[checkpoint report](../dev-docs/verification/VERIFICATION-2026-10-29-PALETTE-APPEARANCE-PREVIEW.md)
records checks and limits. Panel counters are whole-guest values, not a benchmark.
Font, icon, layout and wallpaper preview remain in development.

## Preview high contrast and return to your edits

In Desktop settings, choose **Preview high contrast...**, or press F5 in an
editor containing contrast. Save (Enter) keeps high contrast and saves only
that preference. Cancel (Escape) restores the previous owned resources. The
core also recovers on expiry; closing the dialog leaves recovery to that deadline.
Other editor drafts remain available when the preview ends. See the
[desktop manual](FvwmDesktop.adoc) for admission,
conflict and persistence limits. This was the first scoped resource consumer. Palette and explicit light/dark
appearance now have the conditional previews above; typography, icons and layout
previews remain in development.

Slate dark: the active preview offers Save and Cancel while an existing Form
uses the high-contrast roles.

![Slate dark native high contrast preview](screenshots/contrast-preview-slate-dark.png)

Dune light: an external preference-file edit is preserved, the previous runtime
presentation recovers, and the notice requests a reload.

![Dune light external-edit recovery notice](screenshots/contrast-preview-dune-recovery.png)

Enlarged Classic: Save returns to the captured all-fields draft. Apply remains
available on screen; other edits are still unsaved until Apply.

![Enlarged Classic returned settings draft after saving contrast](screenshots/contrast-preview-classic-draft.png)

These unmodified 1440×1000 captures are a development checkpoint, not complete
accessibility or release qualification. The [manifest](screenshots/contrast-preview-manifest.json)
binds source, configuration and artifacts; the [verification report](../dev-docs/verification/VERIFICATION-2026-10-29-CONTRAST-PREVIEW.md)
records actual checks and remaining limits. Panel counters are whole-guest values,
not a window-manager benchmark.

## Application startup activity

Choose **Launch activity...** from the Applications launcher, or send:

```fvwm
SendToModule FvwmDesktop launch-activity
```

The native form shows a read-only snapshot of observed startup notifications.
F1 and F2 move between retained records, F5 refreshes the snapshot, and Escape
closes it. The view does not refresh in the background. A record can be
`pending`, `protocol-ended`, `window-observed` or `expired`. These are
observations: a `protocol-ended` notification does not establish that an
application started successfully. The [desktop manual](FvwmDesktop.adoc)
describes the bounds, fallback behavior and remaining matching limits.

Slate dark: a pending notification in the snapshot.

![Slate dark launch activity showing a pending notification](screenshots/startup-activity-slate-dark.png)

Dune light: a synthetic notification after it ended and the snapshot was
refreshed.

![Dune light launch activity showing a protocol-ended notification](screenshots/startup-activity-dune-light.png)

Enlarged Classic: the activity form with larger monospace type.

![Enlarged Classic launch activity showing a pending notification](screenshots/startup-activity-classic-large.png)

The Classic fixture requests larger fonts and legacy Form colors. This activity
form explicitly uses Colorset 2, so its visible colors follow the Slate colorset
in this capture. These unmodified images use synthetic notifications; the
[manifest](screenshots/startup-activity-manifest.json) records their source and
configuration identities. The [checkpoint report](../dev-docs/verification/VERIFICATION-2026-10-28-STARTUP-NOTIFICATION.md)
records the checks and remaining qualification. Panel counters show whole-guest
values, not a window-manager benchmark. Broader startup, file integration and
release qualification remain in progress.

## Choose an application for a selected file

Select one regular file and press Alt-O. The chooser shows its observed MIME
type, default and alternatives. Enter launches the selected application, F5
refreshes the choices and Escape closes the chooser. Identification uses an
existing optional `file(1)`; it is not installed automatically and the chooser
does not write defaults. If the file identity changes, refresh the main browser,
select it again and reopen Alt-O.

Slate dark: the observed type and selected default appear with an alternative.

![Slate selected-file chooser with observed MIME type and default](screenshots/selected-file-slate-dark.png)

Dune light: a changed handler is refused, with refresh guidance in browser status.

![Dune selected-file chooser reporting a changed handler](screenshots/selected-file-dune-light.png)

Enlarged Classic: choices appear again after refresh.

![Enlarged Classic selected-file chooser after refresh](screenshots/selected-file-classic-large.png)

The synthetic path and failure detail are shortened in the chooser. These
unmodified captures and their distinct source identities are in the
[manifest](screenshots/selected-file-manifest.json); the
[checkpoint report](../dev-docs/verification/VERIFICATION-2026-10-28-SELECTED-FILE-APPLICATIONS.md)
records the workflows and limits. Panel counters are whole-guest values, not a
benchmark. Broader application-role, file integration and release qualification
remain in progress.

## Choose an application for the current folder

In the optional browser, F12 opens a chooser for the displayed folder. It reads
the existing directory default and alternatives without changing associations.
Choose a handler and press Enter to launch, F5 to refresh or Escape to close.
A changed handler is refused; refresh the choices before trying again. Starting
an application is a handoff, not confirmation of its completion.

Slate dark: the default is selected alongside alternatives.

![Slate folder application chooser with default and alternatives](screenshots/directory-roles-slate-dark.png)

Dune light: a changed handler is refused and the browser asks for refresh.

![Dune folder chooser reporting a changed handler](screenshots/directory-roles-dune-light.png)

Enlarged Classic: the choices are available again after refresh.

![Enlarged Classic folder chooser after refresh](screenshots/directory-roles-classic-large.png)

These unmodified captures use synthetic applications and folder names. Long
folder and failure text is shortened in the chooser; the browser status supplies
the refresh guidance. [Capture identities](screenshots/directory-roles-manifest.json)
and [checkpoint results](../dev-docs/verification/VERIFICATION-2026-10-28-APPLICATION-ROLES.md)
record the evidence. Panel counters are whole-guest values, not a benchmark.
Arbitrary-file MIME detection, D-Bus integration and broader application-role,
file integration and release qualification remain in progress.

## Localized XDG menu actions

The standalone XDG menu groups declared localized actions after their parent.
These unmodified Slate dark, Dune light and enlarged Classic captures use
synthetic colored fixture icons to check localized application, action and
.directory icon resolution. They are test fixtures, not new artwork.

![Slate XDG menu with localized parent and actions](screenshots/xdg-menu-slate-dark.png)

![Dune XDG menu with localized parent and actions](screenshots/xdg-menu-dune-light.png)

![Enlarged Classic XDG menu with localized parent and actions](screenshots/xdg-menu-classic-large.png)

The [capture manifest](screenshots/xdg-menu-manifest.json) records image and
source identities. The [checkpoint results](../dev-docs/verification/VERIFICATION-2026-10-28-XDG-MENU-PARITY.md)
cover regeneration and retained exports. Panel counters are whole-guest values,
not a window-manager benchmark. Broader XDG and file integration, MIME, startup
notification, install and release qualification remain in progress.

## Localized Applications actions

Applications can show declared Desktop Actions as localized *Application — Action*
rows. The launcher expands Desktop Entry field codes into literal arguments;
it does not run them through a shell. A stale menu action is refused and asks
for the menu to be closed and applications refreshed.

These unmodified captures show synthetic French application and action names in
Slate dark, Dune light and an enlarged Classic configuration, respectively.

![Slate localized Applications menu](screenshots/applications-slate-dark.png)

![Dune localized Applications menu](screenshots/applications-dune-light.png)

![Enlarged Classic localized Applications menu](screenshots/applications-classic-large.png)

The [capture manifest](screenshots/applications-manifest.json) records their
identities; the [checkpoint results](../dev-docs/verification/VERIFICATION-2026-10-28-XDG-APPLICATIONS.md)
record the workflows and limits. Panel counters are whole-guest values, not a
window-manager benchmark. XDG integration, MIME associations and release
qualification remain in progress.

## Open the Files folder in the native browser

In Files, navigate to a folder and choose **Open this folder in browser**.
An idle matching browser refreshes and presents its existing window. Folder
menus remain the default. Long menu titles and entry labels use an ellipsis;
navigation and browser requests retain the original full paths.

Slate dark: the explicit browser action in the Files menu.

![Slate Files menu with Open this folder in browser](screenshots/files-browser-slate-dark-menu.png)

Dune light: the file browser, captured after the same-folder reuse workflow.

![Dune browser after the qualified reuse workflow](screenshots/files-browser-dune-light-reused.png)

Enlarged Classic: a monospaced 18-point font and shortened menu labels.

![Enlarged Classic Files menu with shortened title and browser action](screenshots/files-browser-classic-large-menu.png)

These are unmodified development captures. Punctuation in the synthetic folder
name is literal data. A still image does not prove reuse; the
[workflow results](../dev-docs/verification/VERIFICATION-2026-10-28-FILES-BROWSER-HANDOFF.md)
and [capture identities](screenshots/files-browser-handoff-manifest.json) retain
that evidence. Panel counters are whole-guest values, not a WM benchmark.
The [desktop manual](FvwmDesktop.adoc)
explains missing-command and uncertain-handoff recovery. Complete default-role,
FileManager1 and release qualification remain in progress.

## Optional desktop pinboard

`fvwm-pinboard` adds explicitly started, saved local file/folder references.
Its shaped native canvas leaves empty root space available to existing bindings.
The ordinary Form manager adds, relabels, moves, reloads and unpins references;
unpin preserves the target. Missing targets and literal byte paths remain
manageable. Saved placement follows the admitted monitor/work-area/scale and
falls back into the root drawable without rewriting preferences.

Slate dark at 1.25x: the manager exposes full references and a missing target.

![Slate pinboard manager with missing, literal-byte and root references](screenshots/pinboard-acceptance-slate-dark.png)

Dune light at 1x, using installed entries: the file browser remains usable
alongside the pinboard after saved-state recovery.

![Dune file browser with the recovered pinboard behind it](screenshots/pinboard-acceptance-dune-light.png)

Enlarged Classic at 2x: explicit legacy colors and a scalable 18-point mono font.

![Classic enlarged pinboard manager](screenshots/pinboard-acceptance-classic-large.png)

These are development captures. Cell labels can clip; the manager retains full
reference details. The whole-guest panel counters are not a window-manager
resource benchmark. The small black probe window is an integration-test client.
[Current capture identities](screenshots/pinboard-acceptance-manifest.json) bind
these three images to passing theme and recovery checks. The
[acceptance supplement](../dev-docs/verification/VERIFICATION-2026-10-28-PINBOARD-ACCEPTANCE.md)
keeps focused support results separate from release qualification. The
[earlier twelve-capture set](screenshots/pinboard-manifest.json) remains available.

See the [pinboard manual](fvwm-pinboard.adoc) for startup, Font/Colorset controls,
state limits, ownership refusal and recovery. This opt-in data-only mode does
not populate or manage an actual Desktop directory. It preserves the existing
wallpaper and refuses conflicting desktop ownership.

## Bounded file-operation jobs

F9 opens the browser's job editor for its captured selection. Create a folder,
rename, copy, move, link, trash, restore or explicitly delete items. One isolated
worker reports progress and per-item results while browsing remains responsive.
Conflicts offer Skip, New name or Cancel; existing destinations are never
overwritten. Closing the owner cancels active work. Cancellation can leave
partial results and does not undo completed operations.

Slate dark at 1.25x: a real copy conflict waits for a decision.

![Slate file-job conflict with skip, new-name and cancel controls](screenshots/jobs-slate-conflict.png)

Dune light at 1x: completed results remain visible in the job window.

![Dune file jobs showing completed per-item results](screenshots/jobs-dune-completed.png)

Enlarged Classic at 2x: permanent deletion previews the captured target names
before the separate F10 confirmation. The Classic controls use an explicit
legacy palette over the Slate desktop.

![Enlarged Classic permanent-delete preview with two target names](screenshots/jobs-classic-delete.png)

[Capture identities](screenshots/jobs-manifest.json) and
[verification and remaining coverage](../dev-docs/verification/VERIFICATION-2026-10-27-FILE-JOBS.md)
retain the three configurations, all twelve captures and filesystem limits.
See the [job manual](fvwm-file-browser.adoc#_file_jobs) before use:
trash can refuse unsupported locations, move is not universally atomic, and
partial results require inspection. Development and broader file-manager and
clipboard integration remain in progress.

### Actual copy progress

Copy now has its remaining acceptance captures: Slate1.25x, Dune1x and
enlarged Classic2x each show a real running job with nonzero copied bytes,
followed by native cancellation and explicit partial results. The private
fixture briefly pauses the verified copy worker for readable captures; these
images demonstrate behavior, not transfer speed.

![Slate actual running copy progress](screenshots/jobs-progress-slate-dark-running.png)
![Dune cancelled copy with partial results](screenshots/jobs-progress-dune-light-cancelled.png)
![Enlarged Classic actual running copy progress](screenshots/jobs-progress-classic-large-running.png)

[All six capture identities](screenshots/jobs-progress-manifest.json) and
[completion evidence and limits](../dev-docs/verification/VERIFICATION-2026-10-27-FILE-JOBS-COMPLETION.md)
include the actual ARM64 filesystem supplement. Development remains in progress.

## Pointer and paste continuity

Input clicks now choose the nearest visible character boundary using the rendered
font metrics. Middle-button cut-buffer paste advances once per field separator;
a single-field Tab inserts a space. The final destination displays its text and
cursor immediately. These native Slate, Dune and enlarged Classic captures show
`A` pasted into Path and `B` into the filter draft after one Tab. The applied filter
label remains `axbc` until the separate Filter action submits the new draft.

![Slate browser after pointer editing and paste](screenshots/pointer-slate.png)
![Dune browser after pointer editing and paste](screenshots/pointer-dune.png)
![Classic browser after pointer editing and paste](screenshots/pointer-classic.png)

[Capture identities](screenshots/pointer-manifest.json) and
[focused verification](../dev-docs/verification/VERIFICATION-2026-10-27-FORM-POINTER-PASTE.md)
retain exact editing, repaint and scale results. Development is still in progress;
this adds no clipboard-history service or IME/shaping qualification.

## Editable input continuity

Long Path and filter drafts now keep Home/End and editing aligned with the visible
cursor. Field traversal starts at Home, including wrap into short or empty fields.
Drafts and cursor survive resize, list/grid switches and scale changes. These
unmodified native captures show Slate 1.25x, Dune 1x and enlarged Classic 2x:

![Slate browser with long editable drafts](screenshots/input-slate.png)
![Dune browser with long editable drafts](screenshots/input-dune.png)
![Classic browser with long editable drafts](screenshots/input-classic.png)

The Classic control palette is deliberately distinct from its Slate desktop.
[Capture identities](screenshots/input-manifest.json) and
[focused verification](../dev-docs/verification/VERIFICATION-2026-10-27-FORM-INPUT-CONTINUITY.md)
record the resize/scale and exact editing checks. Development remains in progress;
these scenes do not qualify IME, shaping or assistive technology.

## Reserved-area startup

Automatic scaled Forms now use the selected monitor's available area when valid
reservation metadata arrives during startup. Preferred rows fit around reserved
edges; readable control minima take precedence if the available area is smaller. Explicit
Position/Geometry and later user movement keep their authority.

![Slate fractional browser inside the monitor's available area](screenshots/workarea-slate.png)

![Dune browser inside the monitor's available area](screenshots/workarea-dune.png)

![Enlarged Classic browser title now below the reserved desktop panel](screenshots/workarea-classic.png)

The real desktop reserves 36px at the top in these three scenes. The enlarged
Classic title is now fully below that panel. A separate real mixed-scale scene
uses a second monitor at a nonzero origin and a deliberately large configured
reservation, demonstrating that its own available area governs startup:

![Slate browser on a nonzero-origin second monitor with a large configured reservation](screenshots/workarea-nonzero.png)

This remains in-progress development. Existing Monitor reservations have no
shared generation with topology, so stable initial configuration is qualified;
live reservation freshness through pre-ready panel/hotplug changes is not.
[Unmodified capture/source manifest](screenshots/workarea-manifest.json),
[Form configuration](FvwmForm.adoc) and
[verification and limits](../dev-docs/verification/VERIFICATION-2026-10-27-FORM-WORKAREA.md).

## Scaled Form startup

Scaled Forms now retain their configured startup row count when it fits, while
still allowing a one-row viewport when resized smaller. Reflow keeps an already
visible focused entry in view; deliberate wheel scrolling can leave it offscreen.
Drafts, selections and native windows survive those changes.

![Slate at 1.25x with four visible Details fields](screenshots/startup-slate.png)

![Dune at 1x with four visible Details fields](screenshots/startup-dune.png)

![Enlarged Classic at 2x with four visible Details fields](screenshots/startup-classic.png)

Automatic initial sizing measures the managed frame and keeps fitting Forms
inside the physical monitor, without a screenshot-fixture move. Later user
movement and explicit Position/Geometry remain authoritative. These earlier captures predate reserved-area adoption above. Their enlarged
Classic browser reaches the physical top edge and overlaps the desktop panel;
the current stable-startup correction is shown above. First-ready geometry can follow
the first mapped frame; this remains in-progress development.

[Unmodified capture/source manifest](screenshots/startup-manifest.json),
[Form configuration](FvwmForm.adoc) and
[verification and limits](../dev-docs/verification/VERIFICATION-2026-10-27-FORM-STARTUP.md).

## Complete value details

Details (F1) opens a read-only snapshot of the browser's complete admitted
values. Choose a field with the keyboard and Read (Enter). Home/End and arrows
scroll its value; Previous/Next part (F9/F10) cover long values. Opening, paging
and closing retain the main browser's selection, drafts and native windows.
Reopen F1 to capture later state; the existing snapshot stays frozen.

![Slate browser Details showing a long path at 1.25x](screenshots/details-slate-path.png)

![Dune browser Details showing complete observed-name metadata](screenshots/details-dune-name.png)

![Enlarged Classic read-only value scrolled horizontally at 2x](screenshots/details-classic-end.png)

Values use reversible ASCII escapes, including explicit invalid filename bytes.
They are observed metadata, not file content or a clipboard transfer. Bounded
admission refuses oversized snapshots explicitly. Personal Forms can reuse the
additive `InputReadonly name` modifier. Existing editable Inputs keep their
behavior. The native regression also repairs right-edge redraw residue during
read-only scrolling.

These unmodified captures use Slate, Dune and an enlarged 18pt Classic config
with actual RandR scales. The fixture moves fitting Classic windows into the
capture area without changing font, scale, dimensions or IDs; this does not
qualify default placement on every screen. These earlier captures predate the startup sizing
correction above: their chooser viewport can compress to one visible row; arrows
still reach every field.
[Capture/source manifest](screenshots/details-manifest.json),
[manual](fvwm-file-browser.adoc#complete-value-details) and
[results/remaining acceptance](../dev-docs/verification/VERIFICATION-2026-10-27-BROWSER-DISCLOSURE.md).
Broader module/support, physical, assistive and multilingual acceptance remains
in development under #51/#69.

## Live native selection feedback

Selecting, extending a range, moving focus or scrolling now updates the browser's
selected counts and action readiness without waiting for another action. Hidden
selection survives filtering. Feedback uses a bounded change notification and a
fresh native snapshot; it does not rebuild the view, invoke an action or discard
input drafts. Actions still validate their own current selection.

![Slate browser updating selection counts and readiness](screenshots/feedback-slate-selected.png)

![Dune browser updating a selected range](screenshots/feedback-dune-range.png)

The 256-entry combined selection limit includes hidden entries. When native input
would exceed it, the browser retains its admitted state and reports unavailable.
Adjusting the selection restores counts and readiness without reopening it.

![Enlarged Classic browser refusing 257 combined entries](screenshots/feedback-classic-refused.png)

![Enlarged Classic browser recovering at 256 combined entries](screenshots/feedback-classic-recovered.png)

These are unmodified Slate, Dune and enlarged Classic captures. An initial Classic
image showed incomplete static glyphs; it remains in the evidence archive. The
published Classic pair comes from a bounded follow-up with equal quiet status
rasters before and after a producer round trip, and clean manual inspection.
That sampling did not reproduce the anomaly; broader rendering acceptance stays
open. [Capture/source manifest](screenshots/feedback-manifest.json),
[verification and limits](../dev-docs/verification/VERIFICATION-2026-10-27-FORM-FEEDBACK.md)
and [periodic performance comparison](../dev-docs/verification/PERFORMANCE-2026-10-27-FORM-FEEDBACK.md).
These earlier feedback captures predate Complete value details above.
Multilingual fonts and physical/assistive acceptance remain in development.

## Scaled labels and keyboard focus

The browser's single-line status labels now use available width. Essential
selection and action state appears first, and text that still cannot fit ends
with `...`. Widening can reveal more of the retained value; shrinking restores
the narrow view without increasing its minimum size. This adds no text wrapping
or extra list rows. These earlier captures predate Complete value details above.

![Slate browser with expanded status labels at 2x](screenshots/readability-slate-wide.png)

![Dune browser with expanded status labels at 2x](screenshots/readability-dune-wide.png)

![Enlarged Classic browser with expanded labels at 2x](screenshots/readability-classic-wide.png)

Collection focus and input carets use a bounded scale-derived stroke. The
synthetic native control fixture below shows selected keyboard focus and long
label overflow. It is a primitive test Form, not a file-manager workflow.

![Enlarged Classic native controls with selected keyboard focus](screenshots/readability-classic-focus.png)

Personal Forms may add `LabelViewport name` after a named Label, on its own
Line in a single-ListViewport Form. Ordinary Labels retain fixed bounds; a
core/fontset drawing repair now enforces those documented bounds consistently
with Xft. The three-profile checkpoint uses actual RandR scale changes and
unmodified captures, not font-size simulation.
[Capture/source manifest](screenshots/readability-manifest.json),
[Form reference](FvwmForm.adoc),
[browser reference](fvwm-file-browser.adoc) and
[results/limits](../dev-docs/verification/VERIFICATION-2026-10-27-FORM-READABILITY.md).
This remains development work; broader #51/#69 and physical/assistive acceptance
are open. These earlier PR280 captures predate live native selection feedback
above; their summary labels reflect the controller's last admitted callback.
Some synthetic CJK characters show missing-glyph boxes with the configured font;
these captures do not qualify multilingual glyph coverage.

## Native monitor scaling

The browser now follows committed per-monitor UI scales, including fractional
scales. Single-viewport personal Forms may opt into `UIScale monitor`; ordinary
Forms keep their existing default. Fonts and control geometry update together
while native windows, selection, anchors, cursor and unsent text stay in place.
The core continues to own actual-pixel parent geometry.

These captures use actual RandR outputs at 1.25x, 1.5x and 2x, with Slate dark,
Dune light and enlarged Classic configurations. Each is an unmodified desktop
capture; open the full image to inspect the second monitor and controls.

![Slate browser first mapped on a monitor at 1.25x](screenshots/scale-slate-first-1.25.png)

![Dune browser on a monitor at 1.5x](screenshots/scale-dune-1.5.png)

![Enlarged Classic browser with a scaled grid](screenshots/scale-classic-grid-2.png)

If a scaled minimum cannot fit, Form retains its last complete view until real
geometry permits adoption. A grid that needs more room retains the list view;
enlarge and retry F7. Unsupported opt-in Form compositions retain their complete
1x view with a diagnostic. Bitmap font fallback uses the returned glyph metrics;
it does not invent larger bitmap glyphs. Long fixed labels can still clip.

![Slate browser after grid enlargement and a real native refresh](screenshots/scale-slate-grid-2.png)

[Capture/source manifest](screenshots/scale-manifest.json),
[Form manual](FvwmForm.adoc), [browser manual](fvwm-file-browser.adoc) and
[results/limits](../dev-docs/verification/VERIFICATION-2026-10-27-VIEWPORT-SCALE.md).
This consumer checkpoint keeps #51 and #69 open for their remaining acceptance;
physical displays, accessibility and the six-target release gates remain separate.

## Browser view continuity and keyboard actions

F7 now switches the same native browser between list and grid. The Form, input
and collection windows stay in place, retaining both unsent drafts, selection,
focus, range and viewport anchors. The View label describes the observed mode
after a real native acknowledgement. Folder navigation can still rebuild.

![Slate grid in the same browser with keyboard mnemonics](screenshots/continuity-slate-grid.png)

Every browser button has a documented keyboard action: existing F2–F8/Enter/Escape
plus visible Alt-letter shortcuts and Alt-1..4 for configured selection actions.
The four Missing buttons are synthetic configured examples used to test an
unavailable program; they are not default desktop applications.

![Enlarged native browser with list controls and keyboard help](screenshots/continuity-large-resized.png)

A grid that cannot fit the current window refuses the switch and keeps the old
view usable. Enlarge and retry F7. The one-row minimum-height capture below
retains both draft fields; the real callback checks verify their complete bytes.
Long status/properties/count tails can clip, especially with enlarged text;
the readable View button remains available when the status suffix clips.

![Dune browser retaining its old mode and drafts after an undersized grid refusal](screenshots/continuity-dune-refused.png)

Provider loss retains last-good rows and refuses their effects; refresh recovers.
Missing/non-executable programs report failed, never a successful launch.

![Slate browser retaining unavailable data](screenshots/continuity-slate-unavailable.png)

![Dune browser recovered with retained drafts and selection](screenshots/continuity-dune-recovered.png)

![Enlarged successor browser after producer teardown and a fresh lifetime](screenshots/continuity-large-successor.png)

All six unmodified captures come from one source-bound three-theme checkpoint,
with a 1920×1800 nested display and 4,096 synthetic entries. They demonstrate
configured 18pt text; the newer actual monitor-scale checkpoint appears above.
#69 remains open for broader acceptance; #65 action acceptance is complete on
the associated PR's merge.
[Capture manifest](screenshots/continuity-manifest.json),
[manual](fvwm-file-browser.adoc),
[results and limits](../dev-docs/verification/VERIFICATION-2026-10-27-BROWSER-CONTINUITY.md).

## Shared action palette

Super+A opens one optional native palette for windows, applications, advertised
desktops and eight registered desktop actions. Super+P still opens Run. Type a
literal query, press F3 to filter, use Tab/arrows/Space to select and Enter to
activate; F5 refreshes and Escape closes with focus recovery. Existing native
Form controls, application discovery and the shared command engine provide the
behavior without an added daemon, database or GUI toolkit.

![Slate dark action palette with four provider counts](screenshots/palette-slate-ready.png)

![Dune light palette filtered to a managed window](screenshots/palette-dune-filtered.png)

Printable localized names remain visible with enlarged text. Native configuration
metacharacters use visible escapes; original labels and literal action data are
kept separately.

![Enlarged palette showing a localized literal result](screenshots/palette-large-localized.png)

Registered Settings opens the existing desktop settings menu. The menu is not a
new editor, and the example application/window names are synthetic fixtures.

![Dune light existing settings menu opened from the palette](screenshots/palette-dune-settings.png)

Provider loss retains last-good rows with unavailable state and refuses their
actions; the other providers remain usable. Explicit refresh recovers the source.
Long details/reasons can clip, especially with enlarged text. The existing worker
failure notice may temporarily cover result rows in loss/recovery captures.

![Enlarged palette retaining unavailable application results](screenshots/palette-large-unavailable.png)

Windows stop at 512 retained entries after a bounded scan; desktops stop at 128,
applications at 500 and registered actions at 16. Omitted inventory is disclosed.
The core's aggregate truncation flag appears on both window and desktop labels,
even when only the window inventory exceeded its limit.

![Slate dark palette disclosing the bounded 512-window inventory](screenshots/palette-slate-bounded.png)

These unmodified captures are from the same three-profile native checkpoint.
[Source and capture manifest](screenshots/palette-manifest.json),
[manual](FvwmDesktop.adoc#action-palette) and
[results/limits](../dev-docs/verification/VERIFICATION-2026-10-27-ACTION-PALETTE.md).
The desktop is still in development; optional file/notes palette adapters,
physical accessibility and release resource qualification remain separate.

## Committed native bindings and recovery

BIND-01 adds shared bounded revision/lifetime policy for desktop menus and the
optional browser. These are actual captures of the same source in Slate dark,
Dune light and enlarged 18pt monospace configurations. Work is still in progress
toward R1; this does not complete a file manager or clipboard application.
[Contract](../dev-docs/decisions/NATIVE-VIEW-BINDINGS-2026-10-26.md) and
[results](../dev-docs/verification/VERIFICATION-2026-10-26-NATIVE-BINDINGS.md).

The browser distinguishes unknown totals during work from the committed loaded
count. It reports action readiness while retaining last-good entries after loss;
Refresh obtains a new current snapshot. Buttons retain their visual appearance,
and actions still revalidate immediately before effects.

![Slate dark browser with committed progress and action-readiness labels](screenshots/bindings-slate-ready.png)

![Dune light browser showing unknown progress while a real worker is held](screenshots/bindings-dune-loading.png)

![Enlarged browser retaining last-good entries after an injected worker failure](screenshots/bindings-large-unavailable.png)

Desktop application menus use actual Greyed rows after producer loss. Old mapped
application actions carry their snapshot identity and cannot launch a different
application after discovery reorders the list. Closing and reopening converges
to the current menu; the example names are synthetic test applications.

![Dune application menu with an unavailable Greyed application](screenshots/bindings-dune-menu-unavailable.png)

![Enlarged application menu reopened with the latest discovery snapshot](screenshots/bindings-large-menu-latest.png)

![Enlarged browser with complete progress and readable action readiness](screenshots/bindings-large-ready.png)

No per-control polling shell, additional resident, GUI toolkit or persistence
store was added. Directory watching remains opt-in; when off, its ticker stops.
Existing desktop polling/status sampling continues. The observed binding idle
interval and bounds do not establish whole-desktop wakeup or release budgets.

## Slate dark: list and live refresh

The native scrolling list uses the shipped desktop's fonts and Form colors.
Here the last entry in a 4096-entry synthetic folder is selected and its observed
properties are visible. The count describes the complete filtered projection,
including entries outside the viewport.

![Slate dark native list, scrolled to selected entry 4095 with observed properties](screenshots/browser-slate-list.png)

After an explicit refresh, the browser reports additions, updates, removals and
lost selection. This example retains a filter and a separately typed Path draft.
The remaining selected folder is filtered out, so its properties stay visible
while the list shows the matching new file. The temporary paths and files are
synthetic test data. The four extra buttons exercise configured selection
actions; their labels are fixture examples, not built-in applications.

![Slate dark live refresh retaining the filter, Path draft and hidden selection properties](screenshots/browser-slate-refresh.png)

## Dune light: grid

The same browser uses the shipped Dune palette in light mode. Simple native
type marks accompany names; they are not thumbnails or an external icon theme.
This capture shows the final grid rows and selected last entry in a 4096-entry
folder. F7 switches between list and grid while retaining selected item IDs.

![Dune light grid with the last entry selected in a 4096-entry synthetic folder](screenshots/browser-dune-grid.png)

## Classic customization: larger bitmap font and legacy colors

The controlled minimal configuration uses the base-system `9x15` font,
black/wheat Form colors, navy/ivory item colors and different borders. The same
live-refresh case remains usable without the shipped desktop layout or artwork.
The narrow Path field displays part of the retained draft; the native check
verifies its complete bytes independently.

![Classic minimal file browser using 9x15 and explicit legacy Form colors after live refresh](screenshots/browser-classic-refresh.png)

These three actual FVWM configurations pass the focused native-X smoke set.
That does not qualify every theme, scale, accessibility interface or physical
display. The [theme contract](../dev-docs/THEME-STANDARDS.md#primitive-theme-compatibility-smoke-set)
and [verification report](../dev-docs/verification/VERIFICATION-2026-10-23-BROWSER-LIVE-ADOPTION.md)
record the scope and remaining gates.

## Opt-in watching: native refresh and manual fallback

The newer watch checkpoint updates the same window after a synthetic file is
added. Selection, the observed-properties panel and the unrelated Path draft
remain intact. F8 toggles watching; F5 remains an explicit refresh.

![Slate dark file browser automatically refreshed after a directory event, retaining selection and the Path draft](screenshots/browser-watch-slate.png)

Renaming the observed directory stops watching and reports manual fallback.
The previous snapshot stays visible. A successful F5 refresh can re-arm an
enabled watch; the browser does not follow a replacement directory automatically.

![Dune light file browser reporting lost directory watching and the manual recovery controls](screenshots/browser-watch-dune-fallback.png)

These two unmodified captures come from the source-bound
[directory-watch checkpoint](../dev-docs/verification/VERIFICATION-2026-10-23-BROWSER-DIRECTORY-WATCH.md).
Their [separate manifest](screenshots/watch-manifest.json) records the newer
source/archive identity. The classic configuration passes the same cases.

## Responsive panel and collection layout

The newer native layout checkpoint adds measured panel captions and resize-aware
collection viewports. Existing personal grids stay fixed unless they opt in.
The shipped panel uses `Layout content`; its explicit height remains unchanged.
This 18pt example fits the initial panel captions at 1440px:

![Measured panel with enlarged text](screenshots/layout-classic-panel.png)

The browser grid recalculates its columns and visible rows as the same native
window resizes. Selection and focus survive; no entry snapshot is published just
for geometry. This Dune capture shows the expanded viewport:

![Dune browser after native viewport resize](screenshots/layout-dune-responsive.png)

Enlarged text with explicit legacy Form colors uses the same collection contract.
Controls fit the initial window; long fixed metadata labels still clip. Full
multilingual, mixed-scale and assistive acceptance remains open.

![Enlarged browser with legacy Form colors](screenshots/layout-classic-browser.png)

These captures use 4096 synthetic entries and preserve their source identity in
the [layout verification report](../dev-docs/verification/VERIFICATION-2026-10-26-NATIVE-LAYOUT.md).
See the [Buttons](FvwmButtons.adoc) and [Form](FvwmForm.adoc) manuals for opt-in
syntax, bounds and insufficient-space behavior.

## Try the optional browser

Use an installed Go-enabled development build; see [INSTALL.md](../INSTALL.md)
for dependencies and the supported Go compiler baseline. For a new build:

```sh
./build -b builddir-preview
```

Install it using the documented installation procedure. From an FVWM session,
start `Module FvwmMFL` once through FVWM's command interface if it is not already
running. Then launch the browser from a terminal:

```sh
fvwm-file-browser --dir "$HOME" --view grid
```

For a session binding, merge [browser.local.config](examples/file-browser/browser.local.config)
into `$FVWM_USERDIR/local.config` and restart the session. It starts FvwmMFL
and assigns Alt-F9 to the browser. Omit its startup line if your configuration
already starts FvwmMFL; keep only one startup entry.

For the shipped desktop, merge **one** palette snippet into the same local
override file: [Slate dark](examples/file-browser/slate-dark.local.config) or
[Dune light](examples/file-browser/dune-light.local.config). These use the same
palette/appearance/opaque options as the captured smoke profiles. Explicit
module options override saved Appearance choices until removed and the helper
restarted. They leave application toolkit themes alone.

[classic-minimal.config](examples/file-browser/classic-minimal.config) is an
alternative minimal session configuration, not a Slate override. Its font,
colors, focus policy and borders match the tested profile; it adds the browser
binding for convenience. Use it in a separate test session via FVWM's `-f`
option. It requires the `9x15` font and installed modules. It supplies no desktop
bar, menus or default-desktop startup.

Configured actions are optional. For example, if `gvim` is installed:

```sh
fvwm-file-browser --dir "$HOME" --action 'Edit=gvim'
```

The action receives the validated selected absolute paths as separate literal
arguments, including selected entries hidden by the filter. It does not interpret
filenames as shell commands. Configure a program that accepts that argument
interface; this is not a copy/move/delete job API.

## Still in development

The existing desktop Files menu remains the default; this prototype does not
register a system folder handler. Further file-provider integration,
file mutation jobs, thumbnails and complete consumer/install/platform acceptance
remain open. Protected clipboard content, private delivery, retention and storage
also remain separate work. Reusable metadata/selection primitives do not provide
clipboard payload protection. Wider theme, assistive technology, hardware and
release checks retain their own gates.

The four original Slate/Dune/Classic PNGs come from the PR173 Linux ARM64 native-X theme run
at 1440×1000 and UI scale 1. They reuse the existing source-bound checkpoint,
without another runtime or matrix run. The [image manifest](screenshots/manifest.json)
records hashes, profiles, source identity and the canonical capture archive.
All six images were captured on 2026-10-23 at the same geometry/scale.
These are selected functionality examples, not performance measurements or
finished artwork. [Periodic comparisons](../dev-docs/verification/PERFORMANCE-2026-10-23-PR169.md)
track regressions separately.

## Conditional controls from other consumers

The provider also accepts generation-checked selection edits, literal filtering,
observed sorting, list/grid mode and observed-properties presentation. Query
`resource: "files"` with `FvwmCommand --request`, choose the provider, then query
its state and use the returned generation for each typed control. Selection
edits accept up to eight stable IDs per request and retain the total 256-item
cap; hidden selections remain owned. See the [MFL control schema](FvwmMFL.adoc#_conditional_browser_controls).
These controls publish native views; the following worker requests add external
application handoff. Protected clipboard content remains separate.

The [latest control checkpoint](../dev-docs/verification/VERIFICATION-2026-10-23-FILE-PROVIDER-CONTROLS.md)
contains Slate dark, Dune light and classic larger-font examples with exact
source/configuration and capture provenance. Existing images above retain their
original checkpoint identities. The browser remains a development prototype.

## Typed navigation, outcomes and cancellation

Other consumers can now request Home/Up/Back, item/path navigation, refresh,
opening, terminal-here and configured-selection actions through the same files
provider. Query state for the provider and current generation, then send an
`action` request. The immediate reply acknowledges admission; poll the retained
`work` ID for its outcome. Cancellation waits for child reaping and cannot undo
an application handoff. See the [exact schemas](FvwmMFL.adoc#_isolated_navigation_and_application_requests)
and [focused checkpoint](../dev-docs/verification/VERIFICATION-2026-10-23-FILE-PROVIDER-WORK.md).

Slate dark: the configured action started with one validated path. The status
states that application completion is not tracked.

![Slate dark configured action handoff](screenshots/work-slate-dark.png)

Dune light: cancellation has reaped the worker and retained the previous folder.

![Dune light cancelled file work](screenshots/work-dune-light.png)

Classic larger-font configuration: an externally changed item is refused before
an application launch; refreshing can recover its observation.

![Classic theme stale file target refusal](screenshots/work-classic-large.png)

These unmodified captures reuse the same three-theme native-X checkpoint at
1440×1000, scale 1. The [work image manifest](screenshots/work-manifest.json)
records hashes and source/configuration provenance. These examples add no new
widget or default theme. The browser remains in-progress development; mutation,
FileManager1, protected clipboard and full consumer/release acceptance remain open.

## Live reveal and target properties

Consumers can now reveal or inspect targets in a running browser, using a current
provider generation and bounded base64 byte paths. A successful request selects
exactly the target set, clears the admitted filter and brings the first target
into view. Same-folder requests preserve the window and input drafts; a draft
filter can be reapplied explicitly. Missing/mixed-parent targets fail as a whole.
No item is executed by reveal or properties. Explicit hidden/watch setters and
configured action labels/availability let a configuration build its own controls.
See the [target schemas](FvwmMFL.adoc#_isolated_navigation_and_application_requests)
and [checkpoint](../dev-docs/verification/VERIFICATION-2026-10-23-FILE-PROVIDER-TARGETS.md).

Slate dark: observed properties for an explicitly requested hidden file and a
symlink. The inspector follows no link and reads no file content.

![Slate target properties](screenshots/targets-slate-dark.png)

Dune light: two requested items are selected; the first is focused and scrolled
into view through a generation-checked native focus command.

![Dune live target reveal](screenshots/targets-dune-light.png)

Classic larger-font configuration: watching is enabled by a typed request and
reports native observation with periodic reconciliation.

![Classic typed watching](screenshots/targets-classic-large.png)

The [target image manifest](screenshots/targets-manifest.json) records these
unmodified 1440×1000, scale-1 captures and their exact source/configuration/archive
identities. Earlier images keep their original provenance. This remains an
in-progress browser prototype; file jobs, default folder roles/FileManager1,
protected clipboard content and broader consumer/release qualification stay open.


## Portable consumer handoff

`fvwm-file-browser --reuse` can refresh a matching idle folder window or hand
local reveal/properties targets to it, then present that window through the
core. A successful reply acknowledges work admission; its existing work ID
tracks completion. Without a match it opens an independent view. Explicit
customization uses independent launches. Go-enabled installs include an optional
development folder desktop entry; default associations remain user controlled.
See the [manual](fvwm-file-browser.adoc#_portable_folder_handoff) and
[focused checkpoint](../dev-docs/verification/VERIFICATION-2026-10-23-BROWSER-CONSUMER-HANDOFF.md).

Slate dark: the existing iconified browser is presented with two requested
properties targets, including a byte-escaped local URI. No item is executed.

![Slate reused browser properties](screenshots/handoff-slate-dark.png)

Dune light: a positional folder URI refreshes the same provider and native window.

![Dune reused folder](screenshots/handoff-dune-light.png)

Classic larger-font configuration: the same properties handoff retains native
selection and readable controls with 9x15 text.

![Classic reused browser properties](screenshots/handoff-classic-large.png)

The [handoff image manifest](screenshots/handoff-manifest.json) records unmodified
1440×1000, scale-1 captures and source/configuration/archive identities. This
remains in-progress development. File jobs/mutation, default folder roles,
FileManager1 and protected clipboard delivery remain separate.


## Source-aware configuration diagnostics

Runtime command errors now carry an owned source path, physical continuation
span and literal Read chain when available. Selected FvwmDesktop option errors
show the same context at startup, replay and live delivery. Generated commands,
nearest known enclosing invocation sites and unavailable origins are labelled
explicitly;
these are not inferred definition locations. Valid configuration overrides
still require a controller restart; live delivery adds diagnostics.

Slate dark: a live invalid palette spans two physical lines. The notice retains
the full source location and dismisses through its native button.

![Slate runtime error identifying the source file and continued lines 2–3](screenshots/diagnostics-slate-runtime.png)

The existing checker continues to inspect registered literal settings and Read
without executing configuration. Unsupported dynamic syntax stays unchecked.
For example, the offline CLI accepts:

```sh
FvwmDesktop --check-config "$HOME/.fvwm/local.config" --json
```

The native `SendToModule FvwmDesktop check-config` action uses the same supported
subset. Dune light: the first page distinguishes three invalid options from four
unchecked constructs; the source files and action markers are unchanged.

![Dune checker report showing source spans, include callers and Next navigation](screenshots/diagnostics-dune-checker.png)

Enlarged 18pt monospace configuration: a seven-file long-path chain continues
across three pages. Each page contains at most twelve wrapped source/message
rows, preserving the complete paths and leaving Close and navigation visible.

![Enlarged native checker with complete source rows continuing across three pages](screenshots/diagnostics-classic-long-chain.png)

The [image manifest](screenshots/diagnostics-manifest.json) records these
unmodified 1440×1200 captures, source inputs and archive identities. All fifteen
captures from the three configurations were inspected. Notice dismissal uses
actual XTest pointer clicks; report navigation exercises native Form key
handlers. This is in-progress development, with broader effective-value/origin
inspection, text/assistive and release acceptance retaining their own scope.

## Native notifications

The opt-in `FvwmNotify` prototype now serves plain text notifications on the
selected optional session bus, with replacement, expiry, action replies, DND and
bounded memory-only history. Passive popups preserve focus; an explicitly opened
managed center provides keyboard actions. The existing desktop DND preference
is projected after commit. No new GUI toolkit or mandatory desktop package is
required. See the [manual](FvwmNotify.adoc) for setup, controls and initial limits,
and the [focused evidence](../dev-docs/verification/VERIFICATION-2026-10-30-NATIVE-NOTIFICATIONS.md)
for source identities and remaining qualification.

These are synthetic notifications in one owned Linux nested-X environment.
The three native configurations use different colors, font families and border
widths; observed loaded font heights are 17, 20 and 29 pixels at scale 1.0.
This verifies enlarged text is actually loaded. It does not establish mixed-DPI,
physical hardware, accessibility or complete support-profile acceptance. Long
text is bounded and clipped/wrapped; the large-text header can truncate.

Slate, Sans 11, dark palette:

![Slate passive native notification](screenshots/notifications-slate-dark.png)

Dune, Serif 12, light palette, explicit keyboard center:

![Dune native notification center](screenshots/notifications-dune-light-center.png)

Classic, Mono 18, enlarged text and legacy frame borders:

![Classic enlarged native notification center](screenshots/notifications-classic-large-center.png)

The exact module overlays are retained as
[Slate](screenshots/notifications-slate-dark.fvwm),
[Dune](screenshots/notifications-dune-light.fvwm) and
[Classic](screenshots/notifications-classic-large.fvwm). Load an overlay only after
enabling the optional module and choosing a free colorset ID. These examples
change colorset 42 and normal FVWM border width; adapt those to your configuration.
The standalone fixture uses explicit DND commands; its final check also starts
the actual desktop controller and observes committed shared DND updates.

Body/actions are the only advertised capabilities. Image/markup/sound support,
persistent history and automatic service activation are not implemented. The [installed acceptance checkpoint](../dev-docs/verification/VERIFICATION-2026-10-30-NOTIFICATION-ACCEPTANCE.md)
records the component checks and their limits; these screenshots alone do not
establish full support, physical hardware or accessibility qualification.

## Notification lifecycle and mixed scale

The current notification module follows dock reservations and committed monitor
changes while preserving its live popup and keyboard-center windows. The native
Xorg-dummy checkpoint switches the primary output from 1× to 2×, changes actual
DPI to 1.5×, disables/re-enables the output and verifies an application action
after reflow. A 96-pixel dock reservation also moves both views automatically.
These are actual RandR transitions in a synthetic X server; physical hotplug,
assistive-provider and wider release qualification remain open.

These captures use the same live notification/center at 2× with the existing
[Slate](screenshots/notifications-slate-dark.fvwm),
[Dune](screenshots/notifications-dune-light.fvwm) and
[enlarged Classic](screenshots/notifications-classic-large.fvwm) overlays. They
come from the final instrumented UI run, with leak detection enabled. The passive
popup overlaps part of the explicit center in this fixture; it does not take
focus. Text wraps within bounded content, including splitting long words; the
large center header can truncate. Development is still in progress.

![Slate native notification and center at 2×](screenshots/notifications-mixed-scale-slate-dark.png)

![Dune native notification and center at 2×](screenshots/notifications-mixed-scale-dune-light.png)

![Classic enlarged native notification and center at 2×](screenshots/notifications-mixed-scale-classic-large.png)

[Source-bound lifecycle, topology and sanitizer evidence](../dev-docs/verification/VERIFICATION-2026-10-30-NOTIFICATION-LIFECYCLE.md)
records the startup/producer tests, corrected defects and remaining release gates.

## Installed notification controls on OpenBSD

These captures use the installed optional notification module on OpenBSD 8.0
ARM64 with the guest's authenticated default X server. The synthetic fixture
uses the same three configuration overlays above. It measured five 24-pixel
button rows at 1× and exercised the fourth action at its bottom pixel. Its
keyboard cases cover all four actions, reverse traversal, dismissal, Escape and
focus return to a probe using explicit ClickToFocus policy. These observations
are specific to the fixture and do not qualify physical devices, assistive
providers or all themes.

Slate, Sans 11, dark palette:

![Slate dark notification with three controls](screenshots/notifications-acceptance-slate-dark.png)

Dune, Serif 12, light palette and center:

![Dune light notification and keyboard center](screenshots/notifications-acceptance-dune-light-center.png)

Classic, Mono 18 and enlarged text:

![Classic enlarged notification and center](screenshots/notifications-acceptance-classic-large-center.png)

Four actions, with the fourth selected in the keyboard center:

![Four-action keyboard selection in the notification center](screenshots/notifications-acceptance-four-action-focus.png)

The passive popup overlaps the center in these captures; it does not take focus.
The enlarged Classic header can truncate. The [capture manifest](screenshots/notifications-acceptance-manifest.json)
records the configuration hashes, installed module and source identities.
See the [component evidence](../dev-docs/verification/VERIFICATION-2026-10-30-NOTIFICATION-ACCEPTANCE.md)
for the checks and their separate release limits.


## Bar tray popup and saved preferences

The opt-in tray now opens beside its adaptive-bar button. It retains application
icon pixels, offers bounded pages and a keyboard fallback, and survives a bar
rebuild or exit. At this earlier checkpoint four saved preferences shared the settings owner;
`SendToModule FvwmDesktop edit-settings group:tray` opens the compact editor.
This remains in-progress R1 development.

These unmodified 3200×1080 native Linux/Xorg captures use synthetic blue clients.
They show Slate Sans, Dune Serif and enlarged Slate Mono. Host fonts and layout
change; foreign client dimensions remain 24×20 actual pixels. The editor retains
its normal shared font in this enlarged bar/tray example.

![Slate bar popup and tray settings](screenshots/tray-bar/theme-0-bar-popup-settings.png)

![Dune bar popup and tray settings](screenshots/tray-bar/theme-1-bar-popup-settings.png)

![Enlarged Mono bar and popup](screenshots/tray-bar/theme-2-bar-popup-settings.png)

![Secondary-monitor 2× popup](screenshots/tray-bar/secondary-2x-anchor.png)

The [capture/config manifest](screenshots/tray-bar/manifest.json) records exact
fonts, saved preference snapshots, loaded heights and actual client geometry.
Each adjacent `theme-N.fvwm` is the exact captured overlay on the default desktop;
apply the matching `palette`, `appearance`, gap, padding and orientation from the
manifest through FvwmDesktop first, then read that overlay. Enable the optional
tray explicitly. Do not swallow its root-child host in another process.

[Usage and recovery](FvwmTray.adoc) and the
[source-bound checkpoint](../dev-docs/verification/VERIFICATION-2026-10-30-TRAY-BAR.md)
describe the earlier checkpoint and its then-pending work. Current tray work is shown below.


## Native tray completion work in progress

The optional tray supports XEmbed and, when the optional session bus feature is
enabled, StatusNotifier items. StatusNotifier is off by default. Six tray
preferences share the schema 8 settings store. The default bar uses a root
popup; a native FvwmButtons tray container is opt-in. Foreign XEmbed client
pixels retain their actual size. See [FvwmTray](FvwmTray.adoc) for usage.

These unmodified native Linux/Xorg captures use eight affected installed
executables, build-tree fixture peers and a private bus and display.
The theme, contained-menu and tooltip captures use synthetic protocol fixtures.
They show Slate dark with Sans 11, Dune light with Serif 12, and Slate dark with
DejaVu Sans Mono 18. The configuration overlays and exact image hashes are in
the [capture manifest](images/tray-completion-20261030/manifest.json).

![Slate dark popup and settings](images/tray-completion-20261030/modern-theme-0.png)

![Dune light popup and settings](images/tray-completion-20261030/modern-theme-1.png)

![Slate popup with enlarged monospaced tray text](images/tray-completion-20261030/modern-theme-2.png)

![Slate dark contained tray between Before and After buttons](images/tray-completion-20261030/container-theme-0.png)

![Dune light contained tray between Before and After buttons](images/tray-completion-20261030/container-theme-1.png)

![Slate contained tray with enlarged monospaced button labels](images/tray-completion-20261030/container-theme-2.png)

![Synthetic application menu in a contained tray](images/tray-completion-20261030/container-application-menu.png)

![Plain-text tooltip below a tray popup](images/tray-completion-20261030/modern-tooltip.png)

The following capture includes real GTK 3.24.41 and Ayatana 0.5.93 clients.
The GTK fixture loads a 2×2 source image and selects its own actual client
size: 16×16 at the first observation and 2×2 after hide/show.
The earlier black capture came from isolated MIME data preventing PNG loading;
the fixture producer now uses the base MIME paths. Foreign client pixels are
not resampled by the tray.

![GTK and Ayatana tray clients with an application menu](images/tray-completion-20261030/real-gtk-ayatana-clients.png)

These images show presentation at the captured sizes; they do not prove input
behavior or accessibility. Affected native Linux and combined six-profile acceptance passed; final review
and whole-release qualification remain separate.
