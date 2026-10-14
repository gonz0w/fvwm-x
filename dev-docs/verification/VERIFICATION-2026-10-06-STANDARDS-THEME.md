# Standards documentation and focused theme review — 2026-10-06

Owners: STANDARDS-01 / THEME-01. This is a documentation and configuration
checkpoint, **not complete desktop, WCAG or native UI qualification**.

## Scope and documentation handoff

Added [STANDARDS.md](../STANDARDS.md) and
[THEME-STANDARDS.md](../THEME-STANDARDS.md), with primary specification links,
requirement/profile/evidence records and independent acceptance tests. Ranked
STANDARDS/EWMH/SELECTION/XDG/TEXT/SETTINGS/A11Y/THEME stories in the authoritative
tracker. Existing session, notification, tray, monitor, security, manual and
release owners retain their responsibilities. Development/support/documentation
guides and the final release gate now reference these requirements.

Native UI follows WCAG2ICT-adapted project policy; actual HTML exports target
applicable WCAG 2.2 AA. Required, optional, delegated and unsupported behavior
must be recorded honestly. No new mandatory toolkit, browser engine or package
is introduced. Runtime implementations and full qualification remain queued.

## Measured finding and configuration adjustment

Captured actual commands from `Desktop.theme`, `appearance_state` and
`session_colors` without an X connection, command dispatch or settings writes.
Each of eight palette/mode variants produced 19 meaningful base text pairs,
eight preview pairs and six representative enabled/disabled Session state pairs:
264 opaque checks. The latter exercises one enabled and one disabled action;
all seven actions share that state-color formula. Decorative colorsets 3/4
were excluded from text checks, not from future native boundary/focus review.
Separately checked 19 bootstrap text pairs in `default-config/config`.

Relative luminance uses normalized sRGB, the 0.04045 transfer breakpoint,
weights 0.2126/0.7152/0.0722 and `(Lmax + 0.05)/(Lmin + 0.05)`. Acceptance uses
unrounded values. The [retained data](STANDARDS-THEME-CONTRAST-2026-10-06.json)
includes individual foreground/background pairs, exact ratios, source SHA-256
and generated icon identities.

All original base text roles passed 4.5:1. Disabled Session labels on raised
surfaces measured 4.471642 (light Dune), 4.323570 (light Grove) and 4.203613
(light Ink). Inactive controls have WCAG exceptions; this finding is against
the project's deliberately stronger disabled-label readability policy.

Only these light muted tokens changed:

| Palette | Before | After | Disabled Session text after |
| --- | --- | --- | --- |
| Dune | `#72665b` | `#706459` | 4.607433:1 |
| Grove | `#62735a` | `#5b6b54` | 4.845018:1 |
| Ink | `#60728d` | `#586982` | 4.797580:1 |

Regenerated icons with the existing `python3 -B default-config/generate_icons.py`.
All 368 existing PNG paths remain; exactly 21 PNGs changed: seven
disabled Session symbols in each adjusted light palette. The other 347 remain
byte-identical. Wallpapers do not consume the muted token and need no rebuild.
Generator/helper/runtime source, Slate colors and layout are unchanged.
Added source comments and user-manual guidance on color and motion scope.

## Post-adjustment numeric review

| Palette / mode | Base text minimum | Disabled Session | Accent on raised | Menu model minimum | Bar model minimum |
| --- | --- | --- | --- | --- | --- |
| Slate dark | 6.132945 | 6.154612 | 5.497577 | 5.233563 | 8.434788 |
| Slate light | 5.268157 | 4.819266 | 4.999627 | 4.665998 | 9.163455 |
| Dune dark | 6.150073 | 6.096342 | 5.538456 | 5.279284 | 8.309989 |
| Dune light | 5.058967 | 4.607433 | 4.818905 | 4.667036 | 9.127602 |
| Grove dark | 5.632269 | 5.678144 | 5.767462 | 5.765089 | 7.748929 |
| Grove light | 5.284749 | 4.845018 | 5.106127 | 4.667797 | 8.420615 |
| Ink dark | 5.833337 | 5.930334 | 5.902943 | 6.037721 | 8.101585 |
| Ink light | 5.250779 | 4.797580 | 5.482065 | 4.642376 | 8.299854 |

All 264 emitted opaque text pairs and 19 bootstrap pairs pass 4.5:1. Bootstrap
minimum is 6.132945:1. Accent marks against raised surfaces exceed 3:1 in every
variant. Decorative border/surface ratios remain 2.331020–2.876276; this does
not qualify a required control boundary wherever such a boundary is used.

The transparency columns are **a model, not native pixel evidence**. Source
inspection confirms MenuStyle Translucent 94 tints a root snapshot with the
menu background; bar Colorset 10 uses surface Tint 88. Model each channel as
`tint * percentage + foreign * (1 - percentage)`, using black/white extrema.
Menus check text/muted/accent on the modeled surface; bar checks normal text.
The model exposed original light Grove/Ink muted-menu values of 4.165422 and
4.067625, now above 4.5. This models monotone sRGB channel blending, not linear
luminance. Actual XRender/fallback/depth quantization, antialiasing, selected
surfaces and external compositor pixels remain unqualified.

## Focused checks and remaining evidence

- `python3 -B tests/test_desktop.py`: **17/17 pass**. Existing palette/appearance,
  emitted-role contrast, preference and desktop policy checks run on this host.
- Command-capture numeric review: **264/264** opaque text pairs; separate
  bootstrap review: **19/19**. These are Python/configuration checks, not native
  X tests. Asset hashes confirm the bounded 21-image change and stable paths.
- Documentation local-link, story-ID/order and LF checks are recorded at final
  review below. External references were reviewed against primary sources.

Source layout observations: comfortable/compact bars are 36/32 pixels high and
reserve 112/96 pixels for four workspace cells before exceptional narrow-screen
fallback. Session's 736×624 fixed 46×26 grid gives nominal 16×24 pixel cells;
ordinary tiles span multiple cells. Appearance's 640×480 / 16×15 grid gives
nominal 40×32 cells. Launcher primary text bands include 23-pixel heights; their
combined label/description/icon hit regions need actual measurement before
declaring the 24-unit target policy met. These observations are not screenshots
or transformed per-monitor hit-test measurements.

THEME-01 still needs actual opaque/translucent native screenshots, retained and
foreign background checks, no-PNG/bootstrap rendering, selected Picom, both
densities, target/focus geometry, narrow/long text, 200% font and supported scale
review. BUTTON-01/02 and A11Y-01 own remaining keyboard/control/accessibility
primitives; existing shortcuts do not prove full focus navigation. TEXT-01 owns
complex input and SETTINGS-01 the toolkit/browser preference bridge. No runtime
fix is attempted here.

No fresh man/HTML render, full six-target matrix, screen reader, native sanitizer
CI or physical display/compositor/IME qualification ran for this focused change.
The native Linux/BSD build thread retains its builds, source changes and evidence.
This review does not supersede their checkpoints. Asciidoctor remains absent on
the local host; the manual source change awaits DOCS-01's affected render gate.

## Final documentation review

Final local checks pass: 14 edited guide/configuration/manual text files have
valid UTF-8, LF and final newlines; 227 local link targets resolve. All 65 active
backlog IDs are unique, the eight new owner rows each appear once and execution
stages remain 1–8. All retained source/icon hashes match. The 21 adjusted PNGs
decode as nonempty 32×32 RGBA symbols with exact muted palette ink for every
nontransparent pixel. These checks do not validate remote links, native target
geometry, installed manuals or runtime protocol conformance.
