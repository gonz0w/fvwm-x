# Palette and appearance preview checkpoint

Date: 2026-10-29. Owner: Cam. CFG-01 #60 / IPC-02 #70 remain open.
Base: merged PR302 `2f6b48df6c57843320bf5d228b3d5d79bf9c6631`.
The [tracker contract](../PROJECT-TRACKER.md#r1-palette-and-appearance-preview--2026-10-29)
was frozen before implementation. [Structured results](VERIFICATION-2026-10-29-PALETTE-APPEARANCE-PREVIEW.json)
retain commands, source bindings, failures, compiler profiles and artifact hashes.

## Delivered behavior

One shared resource-candidate controller now serves high contrast, the four
existing palettes and explicit light/dark appearance. Version2 `opaque-colors`
replaces the same fourteen role colors while preserving both captured actual
menu-opacity scalars; version1 high contrast retains force-100 behavior. Core
capability, native encoding, strict public MFL parsing and consumers agree.

F6/F7 capture the whole existing editor draft, preview only the selected field,
then Save only that preference after native confirmation. Other draft values
return unchanged. Cancel, expiry and conflicts use the existing owned-resource
recovery. Admission requires opaque effective presentation and the same resolved
wallpaper pair; transparency, wallpaper-follow changes, Auto, overrides and
unchanged candidates have explicit refusals. Preview never changes those
preferences to satisfy its prerequisites.

Confirmed but unsaved contrast/palette/appearance values use a bounded three-key
overlay. Later previews, ticks and unrelated changes preserve other held keys;
explicit replacement/save clears its key, and reload or configuration reset
clears the overlay.
Saved/effective/source and persistence failures remain distinct. No full theme
replay, image job, new resident, toolkit or dependency was added.

## Checks performed

Linux ARM64 ran in the existing owned Podman VM. Normal GCC 13.3.0 and scoped
Clang 18.1.3 core/MFL ASan+UBSan profiles reused incremental builds. The resource
receipts explicitly make no native-CPU claim; their requested profile labels do
not supply a separate CPU attestation. Physical-hardware, native-CPU and AMD64
qualification remain unclaimed. Go used the retained 1.27.2 SDK with network
module access off.

| Check | Actual result |
| --- | --- |
| Paired native plan policy | Identical small fixture against merged302 accepts v1/refuses v2; changed policy accepts both. Fixture SHA256 `92a9cd30e2585ce9b1cc959936f5515fefa3450f1d880811542cb0866244fe29`. |
| Affected incremental build | Normal 2.361 s; scoped sanitizer 1.402 s, excluding configure. |
| C policy/MFL units | Both normal and sanitizer profiles pass. |
| Go controller/settings | Two packages, 280 passing test/subtest records each in ordinary and race runs; vet passes. |
| Actual-X resource ownership | Final normal and scoped core/MFL sanitizer runs pass, 28.357/28.753 s; leak detection and undefined-behavior halt enabled. |
| Actual-X coverage | Both plan kinds, distinct captured menu scalars, retained backing resources, cancel/expiry/conflict/owner recovery and observer transitions. Core-state capability test passes. |
| Three theme workflows | Slate/Dune source02 plus Classic source04 pass. Each has 29 captures, normal/high palette paths, light Cancel/dark Save, full-draft Apply, native entries and refusals. No unexpected observer X errors. |
| Existing F5 consumer | Real Save/Cancel/all-fields regression passes on Slate with source04, 8.562 s. |
| Catalog and fixture syntax | Pass. |
| Manuals | Both affected English AsciiDoc pages generate on Linux and format with host mandoc at width80, exit0/no diagnostics. New sections inspected. This is not full-corpus/install qualification. |

Theme assertions observe actual fourteen-role colors, both menu scalars,
wallpaper identity and sample pixels, mapped frame/client/control containment,
focus and destroyed-window identities. The enlarged Classic editor remains
inside 1440×1000. Native entry commands are the same commands supplied by menu
rows; the fixture does not physically select each nested menu item. Independent
Go plan assertions cover the generated entry commands.

All 87 profile screenshots were inspected as contact sheets. The three published
[gallery examples](../../doc/development-preview.md#preview-palette-or-lightdark-appearance)
were also inspected at original resolution; they are unchanged PNGs. Their
[manifest](../../doc/screenshots/palette-appearance-preview-manifest.json) records
individual configuration/source identities. These checks do not establish full
accessibility, compositor, toolkit or release qualification.

## Failures and evidence composition

Production C/Go bytes did not change after source01. Later changes are directed
fixture corrections; prior affected unit/build/race passes remain applicable.
The raw receipts preserve failed commands alongside later scoped passes:

- Source01 three-theme run stopped at an intentional setup write with a stale
  generation after native Save/full-draft Apply. The fixture now fetches a fresh
  settings snapshot before the next separate setup operation; no automatic
  mutation retry was added.
- Separate review found that a persistent observer could satisfy recovery from
  old rows without witnessing candidate publication. Source02 requires the
  candidate rows before recovery for both plan kinds. Both normal and sanitizer
  full resource fixtures were rerun and pass with the stronger witness.
- Source02 completed Slate and Dune, then Classic sampled a probe covered by
  the returned large editor. XGetImage documents undefined obscured window
  contents without backing store ([Xorg manual](https://xorg.freedesktop.org/archive/X11R7.5/doc/man/man3/XGetImage.3.html)).
  Classic now gives only its private probe the same layer as the Form, raises
  it through the command engine and checks actual viewable rectangles/root
  stacking before reading pixels. The actor is restored before screenshots.
- Source03's screenshot guard incorrectly required cross-layer ordering even
  for disjoint rectangles. Source04 accepts objectively nonintersecting frames;
  overlapping frames still require the actor above the probe. Missing/unviewable
  frames and obscured pixel reads remain failures. Classic alone was rerun and
  passes; unchanged Slate/Dune paths retain source02 results.

Source04 executed tree: `53bede89e721f0bb6763f8004390c8cfbf63c060`, closure
`705cfd703dd4cf3b4f05f9a0d61fc2abea077a664ced28162fc4a785296f9176`.
Final reporting/gallery changes are subsequent non-runtime inputs. The canonical
host evidence archive contains 734 verified regular payload files, 5,988,227
compressed bytes, SHA256
`5a0e0630570575aa69452fd215897a4423fdccbb60034d0a89c45012d026d43c`.
Its local path and exact per-run bindings are in the structured report.

## Handoff and limits

Separate source/evidence review uses `temper_reviewer`, which authored none of
this implementation. Requested backend model/effort metadata remains unverified;
the review is an agent examination, not human approval. The earlier observer
finding is resolved by the passing source02 witness. Final integrated tree
`0c22710dcec27f146cbc393eecbc47facdee8312` is accepted with no remaining actionable
findings. Repository, source-byte guards and whitespace checks pass. Subsequent
reporting annotations record that result without changing executed inputs.

Keep current source/build profiles, SDK/dependency caches, the useful installed
prefix and next performance reference. Retire duplicate guest evidence and old
full input copies after preserving the current review inputs; keep compact
failure receipts and reconstruction deltas. Stop only the owned lab normally.
Actual cleanup retired 119,686,656 guest bytes and replaced three host full
input archives with verified reconstruction deltas, recovering 72,697,738 bytes.
The owned container and Podman VM stopped normally. The local and structured
cleanup receipts record the exact paths; current builds and performance reference
remain intact. The current host archive/source04 input remain for PR review.

No full support/install/default-X matrix was rerun for this scoped checkpoint.
Broader #60/#70 acceptance, #72 once-per-session lifecycle, transparent/image/
wallpaper previews and font/icon/layout work remain separate. Parked #257/#258
remain untouched. Performance remains 2/5 from merged PR299 and advances to 3/5
only on this runtime checkpoint's merge; no comparison is due and no speed or
memory improvement is claimed here.
