# Theme generation and default configuration standards

Approved direction: 2026-10-06. Owner: THEME-01, coordinated with A11Y-01,
BUTTON-01/02, CFG-01, SETTINGS-01 and TEXT-01. This is generation/review guidance,
not a declaration that the existing theme passes every requirement.

Use [WCAG2ICT](https://www.w3.org/TR/wcag2ict-22/) to apply suitable web-accessibility
principles to native software. The source criteria are [WCAG 2.2](https://www.w3.org/TR/WCAG22/);
its Understanding pages are informative explanations. Native FVWM configuration
is not HTML/CSS, and passing the checks below is not full WCAG conformance.

## Generation contract

Keep the existing Slate layout and palette names, light/dark/auto choices,
configuration syntax and local overrides. Generate by semantic roles:
background, surface, raised surface, text, muted text, control outline, focus,
accent/on-accent and warning/error/success. Include normal, hover, pressed,
selected, focused, inactive and disabled states and the no-image/no-compositor
bootstrap. State meanings must not depend solely on hue.

Use the existing configuration/asset pipeline and role IDs; do not introduce a
new theme engine, GUI toolkit, executable palette format or mandatory icon font.
Comments explain role meaning, units, defaults and where supported overrides
belong. Preserve compatible image/titlebar styles; artwork has no accessibility
exemption when it conveys information. Decorative detail may stay subtle.

## Measurable default requirements

| Check | Default acceptance | Review scope |
| --- | --- | --- |
| Text contrast | At least 4.5:1 for normal-size information text in every active state; project defaults also retain readable inactive/disabled labels. | Body/title/menu/status/placeholder text, tooltip/error messages, accent text and on-accent text. Large-text exceptions must be justified, not guessed from a font string. |
| Meaningful non-text contrast | At least 3:1 against adjacent colors where a mark/outline is necessary to identify a control or state. | Icon glyphs, sliders, required keylines and focus/selected indicators. Decorative window outlines are not automatically essential control boundaries. |
| Transparency/gradients | Check effective background extrema under the actual blend; retain an accessible opaque choice. | Bar root tint, menu snapshots, selection/hover/pressed surfaces, shipped wallpapers, retained cross-palette wallpaper and worst-case foreign backgrounds. An opaque-token pass is insufficient. |
| Keyboard and focus | Every desktop action reachable by keyboard; visible focus, logical order, Escape/cancel and focus return without traps. | Launcher, menus, quick/settings/session/appearance controls, tray and notifications. Hover is not keyboard focus; disabled actions refuse activation. |
| Focus geometry | Project goal: a distinct indicator roughly two logical UI units thick with at least 3:1 contrast where needed. | A stronger native design goal, not a claim that WCAG AA requires a two-pixel perimeter. Test obscuration by popups/panels and low-resolution rendering. |
| Pointer targets | Native project floor: 24×24 UI units at the documented 1× baseline; prefer 32×32 or more for ordinary desktop controls. | Measure hit regions, not just glyphs. Compact mode must retain usable targets and spacing; tiny resize edges need alternative keyboard/menu actions. |
| Text/layout growth | Preserve content/actions at enlarged fonts and supported scale factors; wrap, scroll or provide equivalent reachable actions. | Long labels/localization, launcher results, errors and narrow monitors. Increase text to 200% in review; test existing 1×/1.25×/1.5×/2× UI scale paths separately. |
| Motion/transparency choice | Keep motion off by default; support disabling configured motion and translucency. | Native shade/menu effects and optional Picom profile independently. No flashing or unrequested focus-stealing animation; do not promise a native toggle changes an external compositor. |
| Semantics and feedback | Labels plus shape/text/state for selected, unavailable and destructive actions; clear error/recovery feedback. | Icons supplement names; do not convey success/error solely through green/red. Expose actual service availability without decorative fake-lock states. |
| Assistive input/output | Preserve keyboard usability and supported accessible names/roles/states/events. | AT-SPI coverage is runtime work under A11Y-01; a theme cannot supply an accessibility tree or fix missing IME/grapheme support. |

Text thresholds and exceptions are explained in
[Contrast Minimum](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html).
Control/indicator contrast uses
[Non-text Contrast](https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html).
Keyboard/focus review follows
[Keyboard](https://www.w3.org/WAI/WCAG22/Understanding/keyboard.html) and
[Focus Visible](https://www.w3.org/WAI/WCAG22/Understanding/focus-visible.html).

[Target Size Minimum](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html)
uses CSS pixels and contains spacing/equivalent-control exceptions. The native
24-unit floor above is a deliberate project adaptation, not a conversion of
CSS pixels into X11 physical pixels. Document current density-token and module
geometry semantics; the wire remains actual pixels. Enlarged native UI is not
a demand to scale client contents or reproduce web-page reflow inside the WM.

## Contrast calculation and asset review

For opaque sRGB tokens, normalize channels to 0–1, linearize with the standard
piecewise sRGB transfer, then compute relative luminance
`L = 0.2126 R + 0.7152 G + 0.0722 B`. Contrast is
`(lighter L + 0.05) / (darker L + 0.05)`. Keep full precision for acceptance;
rounded display values cannot turn a failure into a pass. Use configured colors
for the numeric check and inspect actual glyph weight/rendering separately.

For transparency, first establish the actual XRender/Colorset/Picom blend and
background, then measure the resulting pair. Do not assume a tint percentage is
linear luminance or that FVWM's snapshot translucency equals CSS opacity. Check
all effective gradient/background extrema. Distinguish meaningful control edges
from decorative separators before applying a threshold; never change a label's
status to decorative merely to avoid fixing its contrast.

Image/font/vector icons need the same state contrast and readable scale review.
Keep essential labels as text, avoid text baked into images and verify fallback
assets/fonts available in the supported install. Track generated asset/source
parity so a palette edit cannot leave old-colored PNG icons or wallpapers behind.
Runtime-derived pressed/disabled/semantic colors need their own checks.

## Primitive theme compatibility smoke set

User direction, 2026-10-23: keep a small, repeatable set of two or three actual
FVWM configurations for the evolving file-manager/clipboard primitives. Initial
profiles are:

| Profile | Variation exercised |
| --- | --- |
| Shipped Slate dark | Current default configuration, native module colors/fonts and normal desktop integration. |
| Shipped Dune light | A different shipped palette in light mode, including backgrounds and selected/focused state visibility. |
| Classic/minimal custom configuration | Explicit legacy Fore/Back/ItemFore/ItemBack module settings, larger base-system fonts and different configured control/border styling. |

Load the actual configuration through its normal FVWM/module/theme path. A
palette name in a receipt, changing only a screenshot background, or recoloring
the isolated fixture does not qualify the full configuration. Keep controlled
configuration snapshots/hashes, generated inputs, available fonts, scale and
server geometry in the evidence. User-selected configurations can replace a
profile later while retaining useful variation. Do not change shipped defaults
or another owner's theme configuration to make a test pass.

Exercise list/grid and subsequent native consumers with long/escaped labels,
empty and large collections, normal/selected/focused states, keyboard traversal,
mouse hit regions, scrolling and view switches. Inspect text/icon contrast,
clipping/overlap, focus visibility, selection distinction and action reachability
at the actual font/scale. Capture comparable screenshots for each profile and
separate product defects from probe geometry assumptions. Use existing native
fonts/resources; no extra desktop toolkit or mandatory font package is implied.
Clipboard examples use synthetic metadata and preserve the private-content
boundary; theme testing does not authorize payloads in ordinary UI broadcasts.

Follow the shared UI cadence in [WORKFLOW.md](WORKFLOW.md#representative-theme-compatibility).
This smoke set catches customization regressions and does not replace the wider
THEME-01/A11Y-01 matrix below or prove support for every user configuration.
The current browser checkpoint passes all three configurations, including live
refresh, selection/properties and list/grid checks. See the
[source-bound report](verification/VERIFICATION-2026-10-23-BROWSER-LIVE-ADOPTION.md)
and [public screenshots/config examples](../doc/development-preview.md).
Earlier isolated-fixture results retain their original scope; the wider matrix
below remains open.

## Review matrix and handoff

Review all four shipped palettes × light/dark × comfortable/compact × opaque/
translucent, then relevant states/scales. Include bootstrap/no-PNG, no compositor,
selected Picom, keyboard-only, large-font/long-label and narrow-monitor cases.
Reuse existing tests/live_x/desktop_* controllers and the desktop policy tests;
new contrast/layout assertions belong to the owning build/tooling checkpoint.
Do not make automated screenshot OCR the only proof of text/layout quality.

Save a dated report with palette/config hashes, colors/formula, role/state pairs,
exact numeric results, target/geometry observations, screenshots for the actual
native UI, available services and remaining gaps. Native toolkit/client behavior,
screen readers and physical displays require their own evidence. AI mockups may
explain an idea but cannot qualify implementation. One focused review after a
coherent theme adjustment is enough; broader checks follow changed behavior.

Configuration/artwork review is permitted in this thread after the standards
documentation checkpoint. Record runtime gaps under BUTTON/CFG/A11Y/TEXT/etc;
do not implement C/helper/asset-generator rewrites here or change another
thread's build/default ownership without coordination.

## Shared preferences and web output

Use SETTINGS-01 to bridge selected light/dark, text, contrast and motion
preferences to supported XSETTINGS/toolkits and optional portal/browser paths.
Theme tokens alone do not implement CSS `prefers-color-scheme` or
`prefers-reduced-motion`; browser and site support must be verified. GTK/Qt theme
creation remains separately deferred. For any later web export use equivalent
semantic tokens, responsive layout and semantic HTML; add ARIA only where native
HTML semantics are insufficient. Follow DOCS-01 for offline lightweight HTML.

WCAG 2.2 AA is the web-output target; the native target is the documented adapted
policy plus supported assistive interfaces. Do not label the whole desktop AA
compliant based solely on color ratios or a theme-generation template.
