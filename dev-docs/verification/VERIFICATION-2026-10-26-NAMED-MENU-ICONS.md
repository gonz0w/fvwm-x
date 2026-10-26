# Named-menu icon checkpoint

ICON-01 #215: typed requests, bounded shared presentation, application-menu
adoption and opt-in directory-menu adoption are implemented. One incremental
Linux ARM64 cache passes five affected Go packages with race instrumentation,
both existing real-X menu regressions and the new native image/theme cases.
#215 remains open for its finite asset/profile and disk-generation remainders.
[Structured evidence](VERIFICATION-2026-10-26-NAMED-MENU-ICONS.json) binds inputs,
commands, failures, binary costs and composed theme receipts.

## Inputs and focused results

Base: merged PR262 `43b6343c81c282234d11890600cf0ea19253e44c`, reviewed
tree `e844f6dabbcb7a2e879e2d2ec1ec747228c647f4`; canonical signature and
author DCO verified, branch retired, PR/Project Done. Candidate inputs are frozen
in the JSON report and verified in the guest before/after execution. C core,
native image worker, Form and MFL binaries are unchanged by this implementation.
No Go module/vendor dependency changes are made.

Normal Linux ARM64 GCC cache: reconfigure 1.013 seconds, affected build
0.467 seconds, five-package Go race gate 7.056 seconds. The existing application
and directory X regressions pass in 0.373/0.535 seconds. The corrected ten-case
native fixture passes in 2.414 seconds; the enlarged classic-theme subset passes
in 1.480 seconds. Native macOS tests also pass the five affected packages;
only corrected scopes were repeated during review. No all-OS/compiler/feature
matrix, install campaign or C sanitizer run was selected for pure Go changes.

The native fixture checks actual pixels, not only generated filenames: one
48x48 application icon has 2,304 matching pixels; two directory icons have 4,608.
The 24-at-1.25 request produces a 30x30 icon with 900 matching pixels. Two
same-physical-size source directories compete; the matching 24-at-2x directory
wins over 48-at-1x. Same-name content replacement changes the published identity
and visible color. Missing icons retain the application label.

Slate dark and Dune light use shipped palette values in controlled menu configs;
classic uses explicit legacy colors and 18pt DejaVu Sans Mono. These are actual
FVWM menu configurations, not full desktop-session/theme acceptance. Six main
consumer captures plus scale/refresh/fallback views are retained. Green/red/blue
squares are deliberate solid-color scale fixtures, not a newly adopted icon set.
Slate, Dune, enlarged classic and quarter-scale images were inspected for full
icons, readable labels, unclipped enlarged text and distinct appearance.

## Costs and limits

| Executable | Before | After | File delta |
| --- | ---: | ---: | ---: |
| fvwm-menu-desktop | 5,435,490 B | 5,440,516 B | +5,026 B |
| fvwm-menu-directory | 3,107,769 B | 5,342,842 B | +2,235,073 B |

These normal Go executable file sizes include their existing debug/build data;
ELF text/data/bss are separate rows in the JSON. Both utilities are statically
linked (`ldd`: not a dynamic executable). Directory mode now includes the shared
standard-library image/hash/resolver code. Its BSS grows by about 32 MiB, including
the Go 1.27.2 `crypto/internal/fips140/drbg.memory` symbol. BSS is a zero-fill
section, not a measurement of resident memory. No external package or always-on
process is added.

Three alternating legacy directory invocations over the same 32 files produce
byte-identical output. Elapsed ranges overlap: baseline 0.672–3.786 ms, candidate
0.690–0.871 ms. This tiny warm-cache observation establishes no speed gain.
`wait4` peak RSS includes the launch child's pre-exec footprint, so the recorded
15,788–15,916 KiB launch-child peaks do **not** establish Go-only or desktop
memory parity. Those costs
remain unavailable here; #29/#226 retain complete profile acceptance.

Source and published-image bytes each have a 128 MiB job budget; request work,
depth, cache entries/string bytes and worker output also have explicit limits.
The directory worker resolves four roles with a five-second parent deadline.
These per-job limits do not bound all historical disk generations: automatic
retirement, asset choice and wider supported/disabled-image-profile qualification
remain explicit #215 work. XPM keeps natural dimensions. No browser image
transport, new service, toolkit, client scaling or renderer replacement is claimed.

PR253's regular WM-performance reference remains at 3/5 after PR256. Developer
tool/reference PR262 does not advance it; this implementation becomes 4/5 on
merge. The utility spot observation is separate from that periodic comparison.

## Failures, review and retained evidence

Initial reconfigure stopped because two already-merged profile test sources
had not been copied to the guest; copying them fixed preparation before runtime
execution. The first new X fixture incorrectly demanded an image repaint inside
a modal menu; bounded close/reopen now exposes the existing image commit boundary.
A second fixture hit BadWindow during menu retirement and Xlib exited before
cleanup. The fixture now uses coherent brief inventory and the existing owned
error handler. Three exact fixture child identities were recorded and retired
with pidfds; no unowned process was signalled. Final handlers report no unexpected
X errors. Production was unchanged by these fixture corrections; valid build,
race and regression passes were retained. Initial classic was repeated alone
with a known enlarged font. Failure logs remain in the archive; early failed
fixture source snapshots are not all independently complete.

Separate Sol/high review found and verified corrections for absolute backslash
filenames and exact native area assertions. Lead/implementation review also fixed
new worker signal lifetime, aggregate publication bytes and the old `--icon-t`
alias. Requested routing metadata is not independently attested model execution.
Final source/evidence review independently verifies all 20 source inputs, all
159 archive entries, binary costs, native pixel counts and inspected captures;
no implementation findings remain. It corrected one report phrase to retain the
actual 15,788–15,916 KiB launch-child range. Repository/link and working/staged
whitespace checks pass; these prose corrections do not rerun native tests.

Both modified AsciiDoc manuals render with the existing Linux Asciidoctor;
macOS mandoc renders their roff at width 80. Options, size bounds, fallback and
callback examples were inspected. The initial host Asciidoctor was unavailable;
the existing guest formatter was reused without installing a package.

Canonical host evidence is under outer-workspace
`builddir/codex-2026-10-26/r1-named-icons/`. `native-evidence.tgz` is
10,993,240 bytes, SHA256
`056fa72ce79bbb998dfc87ba489320b12f55806ba2a3392224e417feff879a43`.
All 159 regular-file inventory identities verify, including baseline/candidate
binaries, screenshots, raw receipts, logs and failed-fixture retirement state.
Source/build caches and other owners' stages remain preserved.
After canonical host verification, this batch's guest payload/archive and exact
failed runtime were retired (30,685,975 regular-file bytes). Final post-signal
runtime log bytes are independently preserved in `late-retirement.tgz`. The
owned lab is restored stopped; no other owner's data was removed.
