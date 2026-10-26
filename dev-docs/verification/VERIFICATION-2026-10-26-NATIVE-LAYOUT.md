# Responsive native layout checkpoint — 2026-10-26

LAYOUT-01 #212, FILE-VIEW-01 #69 and THEME-01 #75 native adoption.
Base is merged PR266 `84fe2a3cb2c7d56652d671f37cb8296c4e9c79b4`, reviewed tree
`8dec46b8c359adedc6a7bc29993d859aeb591dcd`; canonical signature/author DCO
verified and branch retired. The [receipt](VERIFICATION-2026-10-26-NATIVE-LAYOUT.json)
binds 24 exact input hashes, commands, binaries, profiles, captures and failures.
The [decision](../decisions/NATIVE-LAYOUT-2026-10-26.md) freezes the finite scope.

## Delivered behavior

Five related outcomes use existing owners: checked row/grid policy, measured
flat Buttons rows, reflow after accepted dynamic publication, an expanding Form
collection with browser adoption, and actual three-configuration interaction.
Configured legacy geometry remains the default. Content rows keep every action
in a positive rectangle under compression; captions may clip. The configured
height and shipped strut remain aligned. Form viewport resize preserves native
windows, snapshot generation, selection, focus and the surviving scroll anchor.

## Focused results

One reused Linux ARM64 GCC 13.3.0 / Go 1.27.2 / Meson 1.7.0 cache. Native
Linux execution occurs inside the Podman VM. No new all-platform/install campaign.

| Check | Outcome | Seconds |
|---|---|---:|
| ui-layout | Pass | 0.002 |
| buttons-keyboard-x | Pass | 14.169 |
| configure | Pass | 1.013 |
| build | Pass | 0.896 |
| go-browser-race | Pass | 8.855 |
| form-list | Pass | 0.002 |
| form-input | Pass | 0.002 |
| buttons-navigation | Pass | 0.001 |
| buttons-updates | Pass | 0.002 |
| asan-configure | Pass | 1.101 |
| asan-build | Pass | 0.171 |
| asan-ui-layout | Pass | 0.008 |
| asan-form-list | Pass | 0.004 |
| form-geometry-x | Pass | 2.263 |
| browser-regression-x-corrected | Pass | 11.442 |
| native-layout-x-refresh | Pass | 11.847 |

The frozen panel policy and existing Buttons atomic/keyboard regression ran
once while the related Form consumer finished. The combined run reused them.
Affected normal builds, browser/collection race, Form list/input and Buttons
policy pass. Geometry/list policy also pass scoped native ASan/UBSan with leak
detection. This is not full module sanitizer or native release-CI qualification.

The final three-profile probe passes exact adjacent panel rectangles, weighted
surplus, native pointer/keyboard actions at 160/700/1440px, refused group retention,
single/group title reflow and compressed icon/state-caption targets. The browser
loads 4096 entries, keeps the same children and published state during ordinary
and three-request burst resize, and publishes a changed metadata refresh without
losing selection/focus. A separate four-column Form verifies top anchoring and
fixed Choice square windows. Owned module retirement and X observer checks pass.

All twelve final captures were visually inspected. The initial 18pt bar now
shows its Applications, clock, resource and session captions within the available
width. Browser controls fit the initial enlarged window. Fixed metadata labels
and deliberately narrow collection cells still clip, as documented; the enlarged
resized window extends below the 1200px capture because the fixture retains its
position. Child containment passes, but that capture is not a whole-screen fit
or complete text/accessibility guarantee. Three representative images are
published in the [development preview](../../doc/development-preview.md#responsive-panel-and-collection-layout).
Three AsciiDoc manuals render with Asciidoctor and host mandoc.

## Observations and static cost

The probe records elapsed input/resize completion including fixture polling and
round trips, not physical display latency. Across the three profiles, pointer
action medians are about 33.8–36.2ms, individual resize medians 54.1–55.5ms and
burst medians 54.1–55.1ms. There is no paired baseline or speed-gain claim.
At 100 CPU ticks/second, the browser Form uses five ticks through the workload;
all three 0.4-second idle observations have unchanged ticks. Sampled Form RSS
after the workload is 6,940–7,068 KiB. These short per-process samples do not
establish a whole-session peak, wakeup budget or leak result. Exact owner/Buttons
RSS/HWM and before/after counters are retained in each profile receipt.

| Binary | Before bytes | After bytes | Delta |
|---|---:|---:|---:|
| fvwm3 | 5,387,608 | 5,387,608 | +0 |
| FvwmButtons | 1,724,208 | 1,740,048 | +15,840 |
| FvwmForm | 1,627,040 | 1,635,872 | +8,832 |
| fvwm-file-browser | 5,623,589 | 5,623,709 | +120 |

Baseline identities were captured before the early panel compile; the later
pre-combined snapshot already contains the new panel and is not the baseline.
ELF sections and linked dependencies are retained separately. No new dependency,
default resident, toolkit, installed entry or module packet format is added.
PR264 periodic comparison counter is 2/5 after PR266, 3/5 on this merge; no
periodic whole-WM comparison is due.

## Failures, corrections and remaining acceptance

The first early compile selected system Go 1.22.2 and stopped at the version
gate before C compilation. Selecting the existing approved Go 1.27.2 SDK fixes
the command environment. Existing fallthrough warnings remain in Buttons.
A continuation script quoting error stopped before running its checks and was
corrected; no product assertion was bypassed. README word-limit checks initially
failed during the status edit and pass after shortening it.

The browser regression retained an old 12-row page assumption; the adopted
compact list requests eight. The corrected test checks the exact focus and bitmap.
Its old fixed-grid expectations now use the observed native row step with exact
range/one-cell assertions; separate geometry and native viewport checks cover
the layout. Initial layout probes assumed root padding that Buttons actually
applies to children, then required a native generation change for a no-op folder
refresh. Corrected probes use the actual full root rectangle and change one
fixture file in place before refresh; IDs/ordering remain stable. Original
source variants, hashes, failed logs and corrected passes are retained. Production
source did not change during these fixture-only continuations.

Source review identified coalesced final resize dimensions and Choice child
width hazards; opt-in coalescing and move-only fixed controls fix them. Parent
clearing prevents stale core-font captions. Prepared icon masks are clipped for
compressed content rows. Full shaping/IME, translation, mixed-scale, Clay and
broader theme/assistive/support/profile/install gates remain with their finite
owners. No whole #212/#69/#75 closure or R1 release acceptance is inferred.
Accumulated risk is addressed here with real legacy/opt-in consumers, race and
scoped sanitizers; unchanged X11 APIs/private geometry do not trigger a BSD boot
or repeat matrix. Combined release qualification remains required.

## Evidence and review

Canonical archive: `/Users/cam/DEV/fvwm3/builddir/codex-2026-10-26/r1-native-layout/native-evidence.tgz`.
SHA-256 `51f90148f0f05728f76746406419556f3929367ae412be794f45179b8261eff5`, 1,942,806 bytes; all 181 inventoried files verify.

Requested routing follows PROJECT.md: Astra/high scope analysis, Sol/high
bounded panel/collection/fixture implementations, separate Sol/high review and
lead-owned native execution. Requested settings are not independently attested
runtime metadata. Final independent review by `/root/r1_layout_review` has no unresolved findings.
It verifies all 24 final hashes, unchanged production, 181 archive payloads,
commands/failures/manuals/costs/telemetry and screenshot provenance; three published
images were independently inspected. No suites or native/resource operations were
run by the reviewer. The receipt records exact review scope and input-map hash.

All 181 guest inventory files were rechecked before removing only the duplicate
guest directory/archive (4,826,090 bytes). Host archive, source/build caches,
VM disks and performance references remain. Both Podman connections had no
running containers; the owned container and test VM are stopped.
