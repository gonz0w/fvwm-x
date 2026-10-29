# High contrast resource preview — 2026-10-29

This #60/#70 checkpoint adds the first owned-resource preview consumer: normal
to high contrast through the existing Desktop menu/editor. It batches private
preparation, fourteen colorset owners and two menu-opacity values, outer-boundary
publication/recovery, typed C/Go requests, Save/Cancel, captured editor drafts and
revision-checked persistence. The frozen [tracker contract](../PROJECT-TRACKER.md#r1-high-contrast-resource-preview--2026-10-29)
retains the other settings and domain acceptance; neither whole issue closes.

The core keeps actual image/shape/color references, including old resources
whose source files later change or disappear. It copies at most 64 KiB metadata,
never image pixels. Admission refuses resources without demonstrated ownership,
raw root pixmaps, pending/refused backgrounds and unsupported tint/alpha policy.
All fourteen records and the two scalars publish before notifications at a safe
outer boundary. Nested menus/Wait can mark pending work but cannot replace/free
those resources. Modules repaint independently; confirmation is not simultaneous
rendering completion.

Save confirms runtime first and saves only contrast. Cancel/expiry recover under
matching generations. External changes are preserved on conflict. Failed save
or a concurrent file edit reports runtime kept / preference not saved; unrelated
editor drafts survive. Closing the dialog leaves recovery to the core deadline.
The single existing lease excludes overlapping opacity/resource previews.
Structured client lines have a separate 4096-byte bound; raw commands stay limited to 1000 bytes.

## Focused verification

One reused Linux ARM64 development container runs on native ARM64 CPU in the
owned Linux VM. No BSD/AMD64/compiler/install/release matrix is inferred.

| Selected check | Result and input |
| --- | --- |
| Incremental affected core/modules/Desktop/Command builds | Passed; reused normal and scoped sanitizer builds. |
| Preview policy, MFL request/stream and query formatter C units | Passed on source02; those implementations are unchanged in the final runtime. |
| Go normal/race/vet | Five affected packages pass, composing four original normal package passes with the corrected controller test; race and vet each cover all five on source03. |
| Generated command catalog | Passed after canonical description parity correction in source04; production command behavior unchanged. |
| Popup font reference unit | Normal and ASan/UBSan pass on source09. |
| Actual image/shape ownership and complete resource recovery | Full resource fixture passes normal and scoped ASan/UBSan on source09, including deleted/replaced sources, deadlines, refusal, generation conflicts, held menus/Wait, actor loss and fresh producer identity. |
| Real allocation failures | Separately linked test core passes allocation attempts 1/17/56 without publication; normal core has no fault hook. |
| Existing scalar/core-state/stream/request actual-X regressions | Four suites pass on source09. |
| Actual 1×/2× popup ownership | Source10 passes ten real popups at observed scale quarters 4/8 in each normal and sanitizer profile. Base bitmap fonts need not double their glyph size. Earlier fallback-only runs remain actual 1× evidence. |
| Three theme workflows and containment | Source13 passes Slate dark, Dune light and enlarged Classic: actual Save/Cancel, all-eleven-field draft Apply, contrast-only persistence and external edit recovery, with real client/frame/control bounds. Two affected Go packages pass after the UI hint-only correction. All 33 final captures inspected; three unmodified examples are public. |
| Manuals and review | Three manuals generate; final Desktop wording regenerates. Component source reviews accept source13; exact final integrated candidate/repository checks are recorded in the PR. Roff formatter remains unavailable. |

## Reproduced defect and retained failures

The first full normal run crashed on a menu opening; the unchanged-source scoped
ASan diagnostic identified a cached popup font released by DefaultFont and later
released again by popup binding. The four implicated font/menu source paths are
byte-identical to merged PR301; this is source proof, not a parent-binary run.
A small no-preview control that creates a menu before DefaultFont replacement
then opens it reproduces the diagnostic. The original control, which created
the menu after replacement, passed and remains a green control.

The repair gives every nonnull MenuRootStatic popup font slot one explicit
Flocale reference. Rebinding acquires before releasing, including cache hits and
aliases. Independent static clones retain both slots; shared dynamic copies
share the static owner. Final static destruction releases both slots. No font
registry, command-order change or new preference is introduced.

Preserved setup/oracle failures include a synthetic twelve-key editor draft
corrected to the actual eleven-key UI; mismatched catalog comment; an incorrect
shared-pixmap-ID assumption; query limit; unsupported long-filename token route;
and a Wait log witness that initially ignored structured provenance. Metadata
capacity is instead exercised with genuine supported gradient arguments, actual
pixels and fresh publication. The invalid filename route remains unavailable.
The test wrapper initially omitted existing Xft include flags; two subsequent
launcher path mistakes executed no suite. The first theme run lacked proof of
keyboard focus. A focused rerun still failed: its distinct-XID assumption was
wrong. Actual destruction/replacement witnesses establish six genuine same-ID
reuses across the three themes. Baseline test restoration also initially
exceeded the existing eight-setting IPC bound; setup now uses two requests,
while the native eleven-field Apply assertion remains intact. Semantic themes
then passed, but screenshot inspection found Apply off-screen in enlarged
Classic. Shorter hints and one extra row correct that real UI defect; final
actual geometry assertions and captures pass without reducing fonts or enlarging
the screen. Failed receipts and original expectations remain preserved.

## Limits and continuation

Allocation injection is skipped in ordinary/sanitizer cores and passes only in
the separately linked test core. Deterministic pending/refused decoder-owner and
ownerless-mask admission are not independently synthesized by the live fixture;
policy/source checks and the named genuine refusal cases retain their limits.
No new screen-reader/toolkit/physical-display/compositor or general appearance,
font/icon/layout preview acceptance is claimed. The inherited Form Xft shutdown
allocation finding remains with its existing owner; this checkpoint's scoped
core sanitizer passes do not close it.

Asciidoctor generates the three affected manuals. The selected test image has
neither groff nor mandoc; formatted terminal-page inspection is unavailable, not
passed. No package is installed for that gap. The development profile has
mandoc disabled, so an initial guessed Ninja manual target was unavailable.

Performance is 1/5 from the merged PR299 reference and is not due; this runtime
merge advances it to 2/5. Broader release gates, the remaining #60/#70 domains,
#72 autostart and later #67/#77 FileManager1 remain separate. Parked #257/#258
remain untouched. Temper source review is separate from implementation but is
not human approval; actual backend model/effort metadata is unverified.

## Source and retained evidence

The compact [receipt](VERIFICATION-2026-10-29-CONTRAST-PREVIEW.json) records the
composed inputs and current binary identities. Source guards bracket native runs.
The final core/protocol/font implementation executes at source09 closure
`e99b732881285238a5e5336c71577475fea1a8b475681315c412976a6851e121`;
source10 changes only the two fixtures and manual wording. Final UI source13
closure is `1561eafe649166010873bef3e432c4562c6322d1442c7716b7cfdab760dacfd7`.
Reporting prose and copied screenshots are outside those executed inputs. No
later whole-tree execution or matrix pass is implied by composition.

The verified local archive `final-evidence.tgz` is 6,001,764 bytes, SHA256
`25d5dfe6d845afa3400d57031837a67a5f9d832e5db7b4520b6bc2d66598a592`,
with 1,080 individually checked regular payloads. It retains failed runs, actual
configurations, 33 final screenshots and complete selected command logs. The
[public gallery](../../doc/development-preview.md#preview-high-contrast-and-return-to-your-edits)
contains three original captures and their source manifest.

Verified duplicate guest artifacts/input archives are retired (327,477,139
bytes), preserving current source/builds, private installs, caches and the next
performance reference. The owned container and Linux VM stop normally. Older
host input archives are reduced to verified reconstruction deltas rather than
kept as repeated full source copies. Retention receipts remain with current
review evidence; this is not a claim that executable builds take those bytes.
