# R1 icon asset choice

Status: evaluation complete; **proposed asset choice, awaiting Cam**.
Owner: ICON-01 #215, coordinated with THEME-02 #214. This document selects no
artwork for installation and changes no runtime/default/dependency. Base is
PR350 `85bef183253e1ec175b1762c4934e09b987acd75`.
[Exact inventory and source evidence](../verification/EVALUATION-2026-11-02-ICON-ASSETS.json).

## Recommendation

Bundle a small **optional** Lucide-derived glyph theme for explicitly owned menu
roles, with complete upstream ISC/MIT notices. Keep current defaults, native
Vector window controls, and unrelated full-color application/tray/pinboard icons.
Keep Papirus an optional user-installed application theme; do not vendor its full
package or add a required desktop package.

The proposed set has 16 source glyphs totaling 5843 bytes and one complete
3208-byte upstream license/notice: **9051 raw bytes**. This is measured source
content, not installed filesystem usage or a runtime memory estimate. The
selected implementation must measure index/provenance/generated/install overhead;
use a 32 KiB file-content planning cap for the optional set and notices, with
actual package/filesystem and runtime costs recorded separately under #29/#226.
No new process, renderer, icon font or toolkit is proposed.

An alternative is to retain current assets/text and installed themes for R1,
keeping this evaluated subset unadopted. No candidate is mandatory merely because
it was evaluated. The release contract calls for an explicitly accepted asset
choice and states “No automatic adoption of Lucide/Papirus” in
[ICON-01's frozen contract](RELEASE-CONTRACTS-2026-10-26.json).

## Exact candidate and licensing

The primary Lucide snapshot is
[`4b203ec9b6002fd74fd5dadc8612220e64289305`](https://github.com/lucide-icons/lucide/commit/4b203ec9b6002fd74fd5dadc8612220e64289305).
Its [license](https://github.com/lucide-icons/lucide/blob/4b203ec9b6002fd74fd5dadc8612220e64289305/LICENSE)
uses ISC generally and separately identifies Feather-derived icons under MIT.
The subset includes both; retain the complete copyright/license notice and
record any derivation. Hashes, exact source URLs and per-icon license bases are
in the inventory. No project-wide relicensing is selected here.

| Proposed glyph | Intended role | Raw SVG bytes |
| --- | --- | ---: |
| folder | Closed directory | 342 |
| folder-open | Open directory/menu title | 401 |
| file | Generic file | 373 |
| file-text | Document/file action | 444 |
| image | Image-file label | 358 |
| clipboard | Clipboard heading | 354 |
| copy | Copy action | 339 |
| scissors | Cut action | 371 |
| clipboard-paste | Paste action | 435 |
| search | Search action | 275 |
| refresh-cw | Refresh action | 392 |
| arrow-left | Back | 262 |
| arrow-right | Forward | 261 |
| arrow-up | Parent directory | 261 |
| settings | Settings action | 586 |
| trash | Remove action | 389 |

`trash-2.svg` was requested during collection but is absent at the pinned
revision. The separately collected `trash.svg` is proposed explicitly; it is not
a silent alias, import or substitution. The other 15 requested files total
5454 bytes. The complete selected-source/notice total above includes trash.

The [Lucide source convention](https://github.com/lucide-icons/lucide/blob/4b203ec9b6002fd74fd5dadc8612220e64289305/docs/contribute/icons/code-conventions.md)
uses 24×24 geometry, no fill, a two-unit stroke, round joins/caps and currentColor.
Static inspection verifies these attributes on the collected sources; it is
not a rendering result.

Papirus is pinned at
[`072a55a432f075acd4932bb4aab6a154746b784d`](https://github.com/PapirusDevelopmentTeam/papirus-icon-theme/commit/072a55a432f075acd4932bb4aab6a154746b784d).
The repository [root license](https://github.com/PapirusDevelopmentTeam/papirus-icon-theme/blob/072a55a432f075acd4932bb4aab6a154746b784d/LICENSE)
is GPL-3.0. Its [README](https://github.com/PapirusDevelopmentTeam/papirus-icon-theme/blob/072a55a432f075acd4932bb4aab6a154746b784d/README.md)
describes optional installation and theme families; its claimed variant count
and listed variants disagree, so complete variant coverage is unavailable here.
Only that README and license were collected. No theme archive/package was
installed or measured, and repository size is not an installed-byte measure.
The existing named resolver can select installed themes; that source capability
establishes neither visual consistency nor profile/package availability.

## Existing primitives and finite integration

The current SVG worker renders bytes and geometry without injecting a native
Colorset foreground. currentColor therefore does not automatically inherit
menu/tray/pinboard text color. The existing renderer preserves alpha.

Native menus already pass the applicable normal/selected Colorset to
[menu picture rendering](../../fvwm/menuitem.c), which applies existing
[IconTint/IconAlpha](../../libs/PictureGraphics.c). This is the smallest supported
recoloring route for a monochrome menu subset. Use an explicit self-contained
source color or verify the selected currentColor resolution, preserve the
upstream vectors/notices, and record source/derived parity. Tint only menus whose
glyphs intentionally belong to those owned generic roles. Menus with unrelated
full-color application artwork are outside that tint scope.

The [tray draw path](../../modules/FvwmTray/tray_sni_ui.c) supplies no picture tint
attributes, while [pinboard](../../modules/FvwmPinboard/pinboard.c) copies picture
colors separately from labels. This proposal adds no automatic symbolic/color
semantics to their third-party icons or overlays. Their existing text and
other-enabled-format fallback remains. The existing optional SVG feature uses
librsvg/Cairo; it is not enabled or made mandatory by this choice.

If Cam accepts the subset, select one bounded adoption batch:

1. Add only these pinned source/derived glyphs, notice/provenance and a private
   optional theme index through the existing asset/install pipeline. Keep default
   theme selections and native Vector controls. Measure exact install artifacts.
2. Add opt-in owned-menu examples using existing resolver requests and scoped
   per-state IconTint, retaining labels/actions and user customization. Do not
   tint arbitrary application artwork or introduce a second resolver/decoder.
3. Join the A5–A8 shared checkpoint: actual Slate dark, Dune light and enlarged
   custom configurations; normal/selected/disabled/focus; transparent and
   antialiased edges; supported quarter scales; numeric meaningful-icon 3:1 and
   ordinary-text 4.5:1 requirements plus inspected stroke weight. Confirm actual
   glyph pixels follow tint while background and unrelated images remain intact.
   Missing/disabled SVG must preserve labels, actions and keyboard access through
   existing fallback. Retain wider support/install/release gates separately.

This evaluation performs no native rendering, contrast/accessibility, package or
release qualification and imports no artwork into shipped paths. An accepted
subset still requires the named implementation and acceptance; an evaluation
merge is not that acceptance. Shared exported-asset retirement and broader panel
roles remain separate #215/#214 work.
