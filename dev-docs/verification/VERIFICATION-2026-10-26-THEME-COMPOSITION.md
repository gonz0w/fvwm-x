# Theme composition checkpoint

THEME-02 #214: shared semantic color projection, independent wallpaper preferences
and adoption by the existing controller/editor/IPC are implemented. Focused native
Linux ARM64 checks pass. The complete theme issue remains open for typography,
density/geometry, icon/high-contrast choices and asset/profile acceptance.
[Structured evidence](VERIFICATION-2026-10-26-THEME-COMPOSITION.json) binds source,
commands, failures, native identities, captures and costs.

## Inputs and results

Base is merged PR263 `794014e01e13f498539d64df9ec70426cc76a128`, reviewed tree
`95fe8c5eb30bde13c454944fc28e76fa42a1a911`; canonical signature and author DCO
are verified. The merged branch is retired. Runtime/test input hashes were checked
in the reused Linux cache before and after execution. C core, MFL, Form, capture
helper, palettes and Go dependencies are unchanged.

| Focused check | Result | Seconds |
| --- | --- | ---: |
| Reconfigure / affected incremental build | Pass | 1.161 / 0.423 |
| Settings/controller/checker Go race packages | Pass | 3.065 |
| Existing settings IPC integration | Pass | 15.736 |
| Corrected three-profile composition fixture | Pass | 6.650 |
| Corrected settings editor/opacity preview regression | Pass | 23.585 |
| Final wallpaper UI build / policy race | Pass | 0.318 / 1.377 |
| Final three-profile composition and captures | Pass | 7.875 |

Pure coverage includes every palette variant/colorset mapping, malformed inputs,
map ownership, exact v1/v2/v3 field sets, refusal/preservation, 1,080 wallpaper
precedence/schedule combinations and stable metadata. Controller tests cover
preflight-before-commit, wallpaper-only invalidation and preserved opacity-preview
positions. Native macOS settings/controller tests also pass. Final menu resolution
rows and the retained-editor hint changed only presentation: affected policy race
and all three native composition profiles were repeated; prior controller, IPC and
preview results bind the explicitly recorded pre-hint binary. No full matrix was
repeated for these rows. The final Meson worker dependency declaration was validated
with a separate reconfigure/introspection receipt after runtime archival.

Actual root pixels change from Slate/Dune to Grove light (`0xdce4d4`) and then
Dune dark (`0x2e2925`). Wallpaper-only updates preserve the sampled bar pixel;
UI palette changes preserve explicit wallpaper pixels. Retention masks newly saved
choices through reload; release applies them, wallpaper-group reset restores
follow, and module override after helper restart leaves saved bytes unchanged.
All final profiles report zero unexpected observer X errors. Private state saves
schema 3; the legacy schema behavior is covered by policy/owner tests.

Slate dark and Dune light use shipped full default configurations; classic-large
uses the same Slate configuration with an explicit 18pt DejaVu Sans Mono fixture
font. Six final editor/retained-state screenshots were inspected. New editor rows,
radio choices, override source and retention hint fit in all three. Existing
fixed-width bar captions clip under that enlarged font; this is a retained
layout/typography gap for #212/#214/#75, not full enlarged-desktop acceptance.
Capture identities and local paths are in the JSON report.

Existing palette contrast checks pass across all four palettes/two appearances.
Using their existing luminance formula, minimum ratios are 10.902 for text/surface,
4.645 for muted/surface and 5.892 for on-accent/accent. These numeric text/selection
pairs do not qualify every native state or the complete #75 accessibility contract.
Decorative same-color roles are not text contrast pairs.

## Costs, failures and limits

FvwmDesktop grows from 5,289,833 to 5,293,199 file bytes (+3,366 B, about 0.064%).
ELF text grows 8,151 B, data 416 B, BSS is unchanged. Both are static Go executables;
`ldd` reports not dynamic. These sizes include normal build/debug data and are not
RSS, wakeups or speed measurements. No new package/default resident is introduced.
PR253's periodic WM comparison is 4/5 after PR263; this runtime merge reaches 5/5,
so the next combined checkpoint owes that comparison. No speed improvement is
claimed by this feature checkpoint.

The first composition fixture assumed a new module directive would alter an
already running helper. It now uses the documented restart boundary. The old
preview fixture had a hard-coded thirteen-radio expectation; it now derives the
count from registry metadata. Both initial failures and corrected passes are
preserved; production was unchanged by those fixture repairs. Valid earlier
passes were retained. Manuals render through existing Linux Asciidoctor and host
mandoc; no formatter was installed. Final prose-only edits remove an obsolete
registered-setting count and clarify the unchanged eight-key IPC request limit;
rendered and final manual source hashes are recorded separately.

This is focused Linux ARM64 native qualification, not six-target/compiler/feature,
install, C sanitizer or release acceptance. The unchanged no-image solid fallback
branch is retained but no reduced-image profile was freshly run. Scheduled paint
uses existing ticks; metadata does not claim clock-specific observation. Broader
THEME-02 and profile/theme/release owners remain open. Parked ownership/pathname
work was not inspected or changed.

The canonical host archive verifies all 97 inventoried file identities and its
SHA-256. About 28 MB of this batch's exact guest payload/archive duplicates was
removed after comparison; source/build caches and other owners' resources remain.
The owned container/VM were restored stopped after both container connections
showed no other active containers. Final separate source/evidence review is
recorded below; no independent human or GitHub approval is implied.

The separate reviewer found one stale evidence-summary count in the existing IPC
fixture: its assertions checked ten settings, but the summary still said eight.
The final fixture derives the label from its initial all-settings snapshot. This
changes no assertion or execution path, so the passing native gate was retained;
the original archive remains immutable and the correction is separately bound in
the JSON report.

Separate final reviewer `/root/r1_theme_review` verifies the source and composed
qualification, all 97 archive entries, three binary identities, six captures and
manual/registration/final-fixture hashes. No unresolved findings remain. Requested
Sol/high routing is recorded without claiming independently attested execution
metadata. Repository/Markdown-link, Python AST and whitespace checks pass.
