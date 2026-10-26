# High-contrast checkpoint

THEME-02 #214: deterministic contrast roles, strongest opaque presentation,
preview admission/recovery and registry/editor/menu/IPC adoption are implemented.
Focused native Linux ARM64 qualification passes; the broader theme owner remains
open. [Structured evidence](VERIFICATION-2026-10-26-HIGH-CONTRAST.json) binds exact
source, commands, binary/capture identities, failures and remaining limits.
[Contract](../decisions/HIGH-CONTRAST-PRESENTATION-2026-10-26.md) defines scope.

## Source and focused results

Base is merged PR264 `c40b27c2c40cfda3eb5f3b6fd6817f8ccf90c9ec`, reviewed tree
`b0c5c180182a865ba5341b80804eaf6fd7ed2116`; canonical signature/author DCO are
verified, branch retired and Project Done. Production Go sources were frozen
before the reused native build/race gate and remained unchanged through all X
runs. Fixture correction and final manual hash scopes are separately recorded.
No C runtime, Go dependency or shipped palette changes occur in this batch.

| Focused check | Result | Seconds |
| --- | --- | ---: |
| Affected incremental build | Pass | 0.472 |
| Settings/controller/checker Go race packages | Pass | 3.115 |
| Reconfigure / new test registration | Pass | 1.107 |
| Existing settings IPC integration | Pass | 15.630 |
| Corrected contrast X: three configurations | Pass | 10.533 |
| Existing normal wallpaper X: Slate | Pass | 2.544 |
| Existing editor/opacity-preview regression | Pass | 23.661 |

Policy tests preserve exact normal role output, measure all eight high-contrast
palette/appearance variants and thresholds around 4.5, reject malformed inputs,
verify map ownership and exact historical schemas 1–4. Shared presentation tests
cover override precedence, requested/resolved/source metadata and retained effects.
Controller tests cover current/drafted high contrast, delayed inspect/start/query/
confirm/save, commit/reload ordering, failed transition preservation and unchanged
normal-mode runtime preview. Local macOS policy tests pass; the first controller
run passed all but one new assertion described below, and its targeted correction
passes. Native race qualification passes the complete affected packages.

## Observed UI and numerical scope

All three actual default configurations exercise high dark and light appearance:
Slate, Dune, and Slate with an explicit 18pt DejaVu Sans Mono fixture font. Bar
pixels become exactly black/white; existing Form and launcher pixels contain the
opposite text and a predominant opaque surface. The same Form window ID survives
both transitions with no DestroyNotify. No unexpected observer X errors occur.
The light Grove wallpaper sample remains `0xdce4d4` through contrast changes.

Saved/requested transparency true and launcher opacity 82 survive high contrast;
metadata resolves false/100 with high-contrast constraints. Leaving/resetting
contrast restores the original tint and 82 opacity. Reload preserves high policy;
external saved high choice is adopted on reload. Both active native saved/runtime
70% previews retire and release their core lease before opaque publication; fresh
native preview requests are refused. An independent generic core lease at 100%
proves release without changing high opacity; the generic core capability remains
available and is separate from helper admission.

Eighteen final editor/menu/Form captures are retained. Lead inspected all twelve
editor/menu views, which also show the persistent probe. New labels, radio choices,
constraint explanation, dark/light surfaces and enlarged editor/menu text fit.
The existing fixed-width 18pt bar clips captions; #212/#214/#75 retain that layout
and shipped-theme gap. The probe's small automatic title decoration truncates its
long fixture title; it is not a tested product layout guarantee. Full theme,
keyboard and assistive-input acceptance are not claimed.

Independent expected-token calculations use the existing project sRGB formula;
Go policy tests separately assert the actual returned roles/pairs. All eight
shipped high variants produce 21:1 normal/muted text and border against the
surface. The minimum accent/surface and selected-text/accent ratio is 5.892:1;
the arbitrary-accent fallback policy enforces at least 4.5:1. Native pixel checks
supply narrower actual-rendering evidence, not every control/state qualification.

## Costs, failures and retention

FvwmDesktop grows from 5,293,199 to 5,300,364 file bytes (+7,165 B, about 0.135%).
ELF text grows 17,292 B, data 192 B and BSS is unchanged. Both executables are
static Go; file/section sizes are not RSS or speed measurements. No mandatory
package, default resident, asset or C/module protocol is added. Exact historical
files load without rewrite; successful save writes schema 4. IPC remains 1 and
composition becomes 2.

The initial new controller assertion matched Module inside KillModule; matching
an actual command boundary fixes the test. The first X fixture required an
incorrect override_redirect menu flag; it now uses the established mapped
save_under predicate with bounded discovery, unchanged pixel checks and diagnostic
identities. Initial sources/logs and corrected passes are retained. Production
was unchanged; unaffected build/race/IPC results were not repeated. Both manuals
render through existing Linux Asciidoctor and host mandoc, with exact source hashes.

The due [PR253→PR264 performance observation](PERFORMANCE-2026-10-26-PR264.md)
uses preserved binaries and five alternating pairs. Timing ranges overlap; no
speed gain is established. Anonymous Script RSS is unchanged, while the distinct
smaps private-dirty aggregate grows 512 KiB. Those rows do not establish allocation
cause or whole-session performance. Counter resets to 0/5 at PR264; this feature
becomes 1/5 on merge. Native C/Script/browser identities remain those measured.

This is focused Linux qualification, not all-OS/compiler/feature/install,
C sanitizers or full R1/theme/accessibility acceptance. Reduced-image behavior is
covered by unchanged branches/policy tests, not a fresh profile campaign. Parked
ownership/pathname work is excluded. All 113 archived file identities verify;
about 20.7 MB of exact guest duplicates/temporary receipts are retired. Source,
build caches and performance references remain. The owned lab is restored stopped
after both container connections show no other active containers.

Separate final reviewer `/root/r1_theme_review` verifies all 113 archive entries,
exact final source/manual hashes, binary/18 capture identities and composed gates.
Numeric contrast and performance medians reproduce independently; all twelve
editor/menu views were inspected again. The performance placeholder labels are
corrected while raw samples remain immutable. No unresolved findings remain.
Requested Sol/high routing is recorded without independent runtime attestation;
this is not a human or GitHub approving review. Repository/Markdown-link, fixture
AST and whitespace checks pass.
