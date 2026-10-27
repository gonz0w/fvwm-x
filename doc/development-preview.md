# Development preview: native file browsing and themes

FVWM-X is in active development. These examples show the optional native
file-browser prototype and the shared controls behind it, not a completed file
manager or clipboard application. The [project tracker](../dev-docs/PROJECT-TRACKER.md)
owns current status; [PR #173](https://github.com/gonz0w/fvwm-x/pull/173) contains
the live-update checkpoint shown here.

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
fallback. F5 remains available; Cancel work disables watching. The first four
captures below retain their PR173 source identity and predate that watch
status/control; the watching examples show the newer checkpoint.
See the [browser manual](fvwm-file-browser.adoc#directory-watching) for current
platform and consistency limits.

Limits include 4096 loaded entries and 256 selected entries. Larger folders are
refused as a whole. Properties describe the last admitted metadata snapshot;
they do not read file contents or calculate recursive folder sizes. The prototype
closes after five minutes without a callback, accepted external control or
current-Form interactive feedback. The
[file-browser manual](fvwm-file-browser.adoc) specifies controls, limits,
worker lifetime, literal-path actions and error behavior.

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
movement and explicit Position/Geometry remain authoritative. The enlarged
Classic browser reaches the physical top edge and can overlap the desktop panel;
reserved work-area placement remains unqualified. First-ready geometry can follow
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

The first four unmodified PNGs come from the PR173 Linux ARM64 native-X theme run
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
