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
unsubmitted input drafts and surviving selection. Changing folders or switching
list/grid layout can rebuild the view and reset drafts.

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
closes after five minutes without a callback or accepted external control. The
[file-browser manual](fvwm-file-browser.adoc) specifies controls, limits,
worker lifetime, literal-path actions and error behavior.

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
