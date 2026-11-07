# PR368 UI and manual closeout

Continuation of the [T1 source checkpoint](VERIFICATION-2026-11-07-THEME-RELOAD.md)
on `codex/r1-live-theme-reload`, base `22ee9ff545a9dcc8aeee31985e8f4b7058591a55`.
Owner: Cam. Scope: qualify the existing reload/inspector batch for PR review;
no additional feature, platform, dependency, install or performance campaign.

## Results

Native Linux ARM64, existing owner-controlled PNG/SVG profile. The five passing
Go packages and both configured helper builds were reused after source/binary
identity checks: 796 helper-source files match the passing inputs. No Go tests,
Go command or rebuild was executed in this continuation.

| Profile | Initial panel | Reloaded panel | Wallpaper after reload/refusal |
| --- | --- | --- | --- |
| Slate dark | `#202a31` | `#34304d` | Exact original root object and pixels |
| Dune light | `#faf6ee` | `#ded4ec` | Exact original root object and pixels |
| Classic, enlarged text | `#202a31` | `#34304d` | Exact original root object and pixels |

All three actual runs verify only colorsets 0–8 and 10–14 are published. Browser
provider, generation, selected count, window and child IDs survive; editor IDs,
focus, saved/effective preferences, settings generation/revision and saved bytes
remain unchanged. An unsubmitted space survives successful and refused reload,
then its actual F3 callback is rejected as an invalid font without persistence.
Invalid palette input reports source context and publishes no color resources.
The inspector's actual F2 callback displays ordered literal include ancestry.
No unexpected observer X errors or cleanup errors occur; all fixture children exit.

All four unmodified captures were inspected by the lead and separate reviewer:
[Slate](../../doc/screenshots/theme-reload-slate-dark.png),
[Dune](../../doc/screenshots/theme-reload-dune-light.png),
[18pt Classic](../../doc/screenshots/theme-reload-classic-large.png) and
[include ancestry](../../doc/screenshots/theme-reload-include-ancestry.png).
These show deliberate custom color edits, not unchanged shipped palettes.
Classic combines the Slate palette with enlarged text and legacy Form overrides.
[Gallery and context](../../doc/development-preview.md#live-theme-color-reload-in-progress) ·
[Source/capture manifest](../../doc/screenshots/theme-reload-manifest.json).

The successful UI command took 4.198 seconds, excluding lab startup and input
admission. This is functional execution time, not a performance comparison.
All 94 export files and archive hashes verify; the sole native command group
retired. Both owned container and machine stopped. The one other VM was not
controlled or modified. Existing build/cache/profile resources remain reusable.

## Diagnosis and repaired fixture

The committed cleanup fix first recovered the original failure: a ten-second
wait for root wallpaper ownership. PNG and solid background preparation also
use the image worker. The build-tree fixture had not selected that worker,
unlike the working composition fixture. Explicitly select its existing executable
and require Backer, image worker and browser before starting X. Their current
binary/library/target identities are recorded and rechecked; no historical byte
pin or fresh rebuild provenance is invented for these extra helpers.

That repairs setup, then exposes a second fixture precondition: the first valid
owned root may still be an intermediate startup image. Startup submits light
then dark sources, while a current root can coexist with pending replacement.
The earlier comparison did not retain its two root objects, so the exact old
field difference remains unavailable.

The final baseline follows editor/browser setup and a desktop barrier. It requires
existing core ownership/colorset-idle observations plus independent selected-asset
corner pixels, bound to the admitted PNG hashes at fixed 1440×1200 geometry.
Every root object and readiness response is stored before assertions. Exact
root dictionary equality—including pixmap IDs, full pixel hashes and corners—
remains required after successful and refused reload. No sleep, raised timeout,
weaker equality, new decoder or product-code change is used to obtain a pass.
The corrected three-profile result qualifies the repaired fixture; previous
failures remain recorded as failures, not retroactively converted to passes.

## Manuals

Candidate and PR367-baseline FvwmDesktop manuals render independently with native
Asciidoctor, both exit0 and empty renderer diagnostics. Generation runs before UI
and is reused by later UI jobs. Host mandoc renders at 60 and 80 columns were
inspected, including the literal command, profile-root setup, bounds, preview
recovery, submission limits and retained state.

Mandoc lint is **not clean**: both return3 for the existing GNU `mso`/`LINKSTYLE`
preamble and existing long-input/paragraph families. Counts are baseline/candidate:
`mso`1/1, `LINKSTYLE`1/1, long-input110/120, paragraph18/19. The extra section
has the same generated paragraph warning; no new formatter family or broken
example was found. Broader generator cleanup remains with DOCS-01.

## Evidence and remaining scope

Artifact root: `builddir/codex-2026-11-07/r1-theme-ui-resume`.
Diagnostic plan: `adf370d6253ad9a548d1a1ea3efd17bd1e8d24a2b7deee779599674e24107401`.
Worker-selection correction: `67e1e7fd88ec006e7c6382258d6baaf92a72e8565122742ad0747a02e7db376f`.
Root-readiness plan: `4a9dfa029299b823186cf330f9fa945d8f2f7b64a96f50805bbc69813418302b`.
Executed driver SHA256: `69ddd60207f629be8290b2c01556846d7a7dcc78ec533d65c6a767775159b7d6`.
Successful export archive SHA256: `eebb9f570774fdd2ef3c717392895f130aed9c239f46939f06377c2fd4d4ce2a`.
The 19-file and 32-file diagnostic exports and archives also verify; their three
and five command groups retire and owned shutdown is confirmed.

This completes PR368's focused UI/manual gate, subject to final signed delivery
review. No whole-issue closure or all-platform/accessibility/physical/sanitizer/
release acceptance is claimed. #214/#215/#217/#218 retain their frozen broader
owners and affected-profile checkpoints. The periodic performance comparison
remains due for the shared checkpoint; no speed or RSS improvement is asserted.
