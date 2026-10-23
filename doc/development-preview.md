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

Limits include 4096 loaded entries and 256 selected entries. Larger folders are
refused as a whole. Properties describe the last admitted metadata snapshot;
they do not read file contents or calculate recursive folder sizes. The prototype
closes after five minutes without a callback. The
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

## Try the optional browser

Use an installed Go-enabled development build; see [INSTALL.md](../INSTALL.md)
for dependencies and the supported Go compiler baseline. For a new build:

```sh
./build -b builddir-preview -Dgolang=enabled
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
register a system folder handler. General file-provider IPC, automatic watching,
file mutation jobs, thumbnails and complete consumer/install/platform acceptance
remain open. Protected clipboard content, private delivery, retention and storage
also remain separate work. Reusable metadata/selection primitives do not provide
clipboard payload protection. Wider theme, assistive technology, hardware and
release checks retain their own gates.

All four unmodified PNGs come from the 2026-10-23 Linux ARM64 native-X theme run
at 1440×1000 and UI scale 1. They reuse the existing source-bound checkpoint,
without another runtime or matrix run. The [image manifest](screenshots/manifest.json)
records hashes, profiles, source identity and the canonical capture archive.
These are selected functionality examples, not performance measurements or
finished artwork. [Periodic comparisons](../dev-docs/verification/PERFORMANCE-2026-10-23-PR169.md)
track regressions separately.
