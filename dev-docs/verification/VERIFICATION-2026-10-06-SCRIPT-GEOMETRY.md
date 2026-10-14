# Script geometry — 2026-10-06

Owner: SCRIPT-01. Status: implemented and qualified on 2026-10-06.
The user approved refusal of unrepresentable geometry with preservation of the
previous runtime state. [Decision](../decisions/SCRIPT-GEOMETRY-PROPOSAL.md).

## Implementation scope

- ChangeSize checks both dimensions against 1–65535 before either stored field
  changes or an X request is sent. ChangePosition does the same for coordinates
  −32768–32767. Rejection produces a diagnostic and permits later instructions.
- Main-window monitor-relative positions use wide addition before validation.
  Widget startup preserves zero/omitted extents as one pixel. Font/icon layout
  is prepared before widget creation; the shared pure policy preserves ordinary
  minimums, fixed dimensions, rounding and Menu placement. Ten initializers no
  longer repeat font loading and unchecked size/resize calculations.
- Menu, PopupMenu and PushButton popups share checked size/translation/placement.
  Invalid geometry is refused before allocation or grabbing the pointer. The
  ordinary placement policy is retained; screen fitting and modal servicing are
  separate work.
- ChangeFont validates a candidate before replacing the old name/font. A valid
  change retains the existing widget dimensions. Shutdown only destroys widgets
  which finished initialization, and does not unmap an uncreated main window.
- Shared font metric/shadow additions are checked. Xft glyph advances accumulate
  in a wide type; core-font width measurement bounds each Xlib accumulation and
  combines chunks in a wide type. Unrepresentable width results saturate rather
  than wrap. Empty quoted font names no longer index before the string. These
  changes preserve representable results and do not establish complete font
  rendering, Unicode, font-set or resource-budget safety.

## Source and environment

Evidence: `builddir/codex-2026-10-06/script-geometry/`. The frozen snapshot has
1205 source hashes and modes, based on the accepted 1199-file SVG checkpoint.
Twenty-five files differ; unrelated desktop/tooling/doc-default changes remain
outside this runtime scope. Root-owned files are checked against the frozen
snapshot. No new mandatory library, GUI toolkit or core thread is introduced.

Reuse the SVG checkpoint's five private QEMU guests and init-backed ARM64
container. Full-system Linux AMD64 uses Debian 13; AMD64 qemu-user cannot enforce
RLIMIT_AS and is not the qualified profile. FreeBSD uses scfb on both targets;
OpenBSD uses wsfb on ARM64 and vesa on AMD64. Patched PNG profiles, pinning and
source provenance are inherited from the accepted SVG qualification. Original
user guests/shared containers remain untouched.

## Qualification

Final archive validation passes all seven compiler legs:

| Platform | Compiler | Full suite | Default BSD server |
| --- | --- | --- | --- |
| Linux ARM64 | GCC 13 | 90/90 | — |
| Linux ARM64 | Clang 18 | 90/90 | — |
| Linux AMD64 | GCC 14 | 90/90 | — |
| FreeBSD ARM64 | Native Clang 19 | 79/79 | scfb, 24-bit |
| FreeBSD AMD64 | Native Clang 19 | 79/79 | scfb, 24-bit |
| OpenBSD ARM64 | Native Clang 19 | 79/79 | wsfb, 24-bit |
| OpenBSD AMD64 | Native Clang 19 | 79/79 | vesa, 24-bit |

That is 586 suite executions. All six final archives match all 1205 frozen
source hashes and modes. Fifteen staged native executables per compiler leg
(core, native modules and image worker), 105 identities in total, match the
tested build with only Meson's expected install RPATH transformation.

Separate staged Linux checks pass 27 geometry outcomes and 96 prior widget and
text/list outcomes. Installed checks on all four default BSD servers pass 36
geometry outcomes and 128 prior widget and text/list outcomes. The geometry
controller covers complete-operation refusal, startup refusal, zero/omitted
widget sizing, X11 endpoints, popup refusal without grabs, valid replacement
and later-command recovery. Seven additional staged font pixel comparisons
confirm that two refused replacements retain the old pixels and a valid Xft
font replacement changes them without resizing the widget.

`validate.py` checks suite status, source/mode identity, staged binary hashes,
installed outcomes, font pixels, default server drivers and scoped sanitizer
evidence. Its final totals are recorded in `accepted-results.json`.

Three local Clang ASan/UBSan checks pass: the pure policy, production font probe
and actual module. The font probe explicitly releases Fontconfig's process cache.
Leak checking is enabled for the pure policy/probe and disabled for the actual
Script/core/MFL run, whose inherited process teardown retains allocations.
Native Linux release sanitizer CI remains configured but unrun. A local virtual
profile is not evidence of that CI result.

The actual FvwmScript roff was generated and rendered with native FreeBSD
mandoc. Baseline and candidate both have two inherited macro errors and 21
warnings; style diagnostics change from eight to nine. Both lint exits are 3.
The new geometry ranges, refusal behavior and font state/dimension contract are
present in the rendered text. This is not a clean lint or manual rewrite claim.

## Retained failures and corrections

- The first snapshot accidentally supplied both Instructions.c and lowercase
  instructions.c to a case-sensitive guest. The corrected snapshot uses the
  existing canonical Instructions.c and matches the root's same file on macOS.
- The initial core-font test dereferenced an absent core font when the platform
  chose a font set. The synthetic metric fixture now owns a separate XFontStruct
  and temporarily preserves/restores both original core-font and font-set fields.
- The focused target name initially collided with the static library fvwm3;
  the runner uses Meson's fvwm3:executable target.
- The popup fixture initially omitted Menu's parser-required Position property.
  The corrected fixture supplies it, even though Menu computes its own position.
- LeakSanitizer reported 320 bytes retained by Fontconfig's process cache. The
  test probe releases that cache after closing the display; the final probe runs
  with leak detection enabled. No production cache teardown is inferred.
- OpenBSD AMD64 exposed a supervisor report race: the reader saw an empty status
  file before JSON writing completed. The fixture now closes a temporary status
  file and atomically renames it. Earlier partial evidence is not acceptance.
- An installed OpenBSD AMD64 check started before a successful staged install
  existed and failed at process launch. Its final run followed the corrected
  successful full suite and install and passes on the default vesa server.

## Remaining work

Subsequent checkpoint: the user-approved shadow work limit, direct border
iteration and shared consumer handling are now qualified in the
[font-shadow report](VERIFICATION-2026-10-06-FONT-SHADOW.md). The observations
below describe this earlier geometry checkpoint.

The geometry policy is not a window pixel-area, shadow-work, font-cache or
individual library-call budget. The non-rendering shadow review confirms that
Center shadow size 64 has 16640 positions; a large accepted Center font's square
counter exceeds signed int, without executing that iterator. The separate
[shadow work proposal](../decisions/FONT-SHADOW-WORK-PROPOSAL.md) needs a user
compatibility decision before a smaller work limit is enabled. Shared consumers
also require failure/ownership review.

SVG parent waiting remains synchronous P1 work. Script modal servicing, wider
font/Unicode/resource lifetimes, package review, native Linux sanitizer CI and
physical hardware/compositor/toolkit qualification remain open. The earlier
unexplained FreeBSD ARM64 interaction timing observation remains in VERIFY-01.
