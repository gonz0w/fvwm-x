# M5 Script widget-icon ownership

Status: focused checks pass; broader qualification pending. Owners: SCRIPT-01
#4 / CLEAN-01 #34. Neither broad issue closes with this finite family.
PR241 is merged as `109facd8546439e6c8ba1cd85cc5c330996f48d0`;
canonical signature/DCO and reviewed tree
`f869fa49a33e87c48f22fe02f8e7a10691e8e8da` verified. Its branch is
retired and Project/labels Done. #35/#36 remain completed; #93 remains superseded.

## Outcome and before-control

The actual LoadIcon owner released allocated X color cells but discarded the
separately allocated pixel vector without freeing it. Canonical picture cleanup
already releases both. A new control links the unchanged module objects, loads
three pixmaps and a two-cell vector, then replaces with a NULL name. It reports
`vectors=1 dimensions=17x23 pixmap_releases=3 color_releases=1` before its
expected assertion failure. Both production sources match canonical PR241.
The original control source, hashes, binary identity and failed log are retained.

One `script_icon_clear` operation now owns all three pixmaps, color-cell
references, the heap vector and loaded dimensions. Free a vector even with no
allocated cells; clear counts and dimensions on replacement or refusal. Decoder
refusal also clears any retained outputs. Layout requires an actual loaded
pixmap with positive dimensions, rather than a requested filename. Successful
lookup, decoding, bitmap conversion and configured command syntax are preserved.

Shutdown keeps its existing QuitFunc, message-drain and widget-destructor order,
then clears icon owners in the fixed 1000 initially-NULL widget slots. This also
covers the current allocated widget whose preparation refused before the
initialized count advanced. The operation does not own the widget record,
window, GC or font; the dead-pipe early return remains. No interpreter-wide
teardown, new budget, cache, asynchronous image path or Form/font exit repair.
The manual clarifies missing/undecodable image removal and explicit retry.

## One focused checkpoint, with affected continuations

Reuse Linux ARM64 caches: normal GCC 13.3 and scoped Clang 18.1.3 with
address/undefined sanitizers. The normal group compiles the actual module,
owner control and affected integration dependencies. Three units pass in each
profile: script-icon-owner, script-geometry and script-widget-math. Scoped
ASan uses `detect_leaks=1:halt_on_error=1`; UBSan uses
`halt_on_error=1:print_stacktrace=1`. This is scoped owner coverage, not a
whole-module exit-leak claim or native-CPU qualification.

The actual-object owner control covers 100 cycles of replacement, repeated
clear, zero-cell vector, missing image, retained outputs on refusal and retry.
It verifies 1503 distinct pixmap releases and 401 color-reference releases in
all, with no outstanding vectors or changes to borrowed window/GC/font state.
Loaded/missing/invalid-extent layout checks use the production layout helper.

The real-X replacement process passes nine observations: existing text,
black/white XBM conversion/replacement, missing and undecodable image clearing,
same-path retry, expose redraw, repeated replacement and normal exit. A separate
partial-startup process loads icons, then refuses widget 2's over-wide fixed-font
title and exits 1 with the expected geometry diagnostic. Static source review
checks the cleanup call sites; the live exit test alone is not a heap-free proof.
Existing geometry, widget and modal fixtures also pass; the latter covers held
input, resource mutation and termination. The affected man page renders with
Asciidoctor. Repository/link/whitespace checks complete the delivery gate.

Retain original failures and run only affected continuations:

- The extended test first omitted the complete FlocaleFont type include.
  Correct only the test include; reuse compiled production objects.
- Initial Icon properties and then ChangeIcon arguments incorrectly used braced
  synthetic paths. The existing grammar requires STR for both; fix the fixture.
- The current XBM reader does not consume pixel bytes on its declaration line.
  Match the existing multiline XBM controls. The observed green pixel (65280)
  versus required black (0), including a diagnostic screenshot, stays recorded;
  pixel expectations and deadlines are unchanged.
- A Shadow font option did not force startup refusal. Retain all nine passed
  replacement observations/screenshots. Replace only the refusal trigger with
  a 12,000-character title whose fixed-font width exceeds X11's limit. Add case
  selection and execute only partial startup, not the passed replacement case.

No production change follows these test corrections. Three normal units and
successful compilation do not repeat; the final source-bound result composes
those passes with the corrected missing checks. The original aggregate icon
invocation stays failed because its partial-startup fixture timed out.

## Evidence and limits

Private evidence is `builddir/codex-2026-10-25/m5-script-resources`.
Nine captured candidate inputs and 277 relevant C/header/build/fixture inputs
match host/guest hashes and modes. Final runtime input identities, both profile
configurations and eleven selected executable hashes are retained. The canonical
`native-evidence.tgz` contains 178 regular payloads including its manifest;
every payload and gzip EOF/CRC is validated. The
[machine receipt](VERIFICATION-2026-10-25-M5-SCRIPT-ICONS.json) records exact
commands, original failures, continuations, output and archive identities.

Three native 800x600 screenshots are captured in the existing Script regression
run: valid, missing and retried icon. Inspection confirms icon clearing and
recovery with the final existing text label. These use explicit legacy colors
and fixed font; they are not a fresh Slate/Dune/classic full-theme qualification.
No new UI/primitive is added. Existing theme evidence retains its historical scope.

No fresh BSD, AMD64, install or full feature/renderer matrix is claimed. No OS
API, native packet, dependency, installed-path or supported-platform contract
changes; accumulated milestone/release, physical/toolkit/compositor and native
sanitizer qualification remain with their owners. Broader resource, text/input,
interaction and data audits remain open. No speed improvement is claimed.
Periodic reference PR238 is 3/5 after PR241, 4/5 on this merge; no comparison due.

Separate final source/evidence review is clear: source identities, raw outcomes
and archive payloads are verified. Guest and host extracted duplicates retire
after exact archive comparison and active-reference checks; canonical archive
and three review images remain. Signed author-DCO delivery is required. Requested
Astra/high analysis, Sol/high implementation and separate Sol/high review follow
project routing; runtime model metadata remains unverified.
