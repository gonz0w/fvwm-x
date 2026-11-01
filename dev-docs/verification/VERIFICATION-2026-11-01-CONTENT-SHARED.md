# Manager, content and Script shared checkpoint

Owner: Cam; #22/#63/#74. The combined Linux ARM64 checkpoint passes **14
manager/content entries and two affected Script owner/consumer entries**.
[Source-bound receipt](VERIFICATION-2026-11-01-CONTENT-SHARED.json) records exact
sources, artifacts, command results and private evidence identities. This closes
the retained Script sanitizer clean-exit defect, not all of #22 or clipboard
storage/capture. [Q67](VERIFICATION-2026-11-01-Q67-NOBUS.md) is independent
source34 installed-service evidence supporting #67 closure.

## Results and retained failures

| Scope | Result |
| --- | --- |
| Normal manager/content policies | Two original passes retained by exact unchanged inputs/artifacts. |
| Normal manager X11 cases | Eight fresh passes: two-screen ownership, values, MULTIPLE, admission, refusal, announcement, replacement and failed handoff. |
| Scoped sanitizer manager/content | Four fresh passes: both policies, two-screen ownership and actual conversion refusal. |
| Actual Script owner/consumer | Two fresh passes, normal and leak-enabled ASan/UBSan, including newer-owner survival and shutdown/grace assertions. |

The first ownership fixture failed because it spelled a display suffix `.0`
with a period; MFL filters punctuation to `_`. The fixture now follows that
existing rule. No product socket behavior, deadline or assertion was weakened.
The two earlier normal policy passes are retained; the remaining twelve cases
ran once with the corrected fixture. Original failure evidence remains.

The unmodified sanitized Script again failed its normal-exit assertion with
320 bytes in three Fontconfig-initialization allocations. A separate native
font probe follows Script's locale/graphics initialization, requires actual Xft
and pairs unload/display-close with explicit finalization. Both modes measure
76×16 text metrics. The equivalent mode reproduces 320/3 without selection or
widget work; finalization exits 0 without a leak report. Matched font file,
Xft/Fontconfig/Expat hashes and locale context are retained. This identifies a
missing process-font-finalization step; it does not blame an upstream library.
The outer unchanged Script fixture exits 1; its native child diagnostic is a
separate nested log, so no precise native signal exit is inferred.

The correction keeps `Quit`'s exported signature, moves its implementation to
the process owner and marks ordinary termination there and on MainLoop return.
Shutdown snapshots/resets that private flag before QuitFunc, retires existing
widgets/fonts/resources and closes the display, then calls `FftFini` only for
that normal entry. Fatal allocation exits can interrupt unregistered temporary
font acquisition and remain excluded. The dead-pipe early return remains;
recursive exit from an exit handler is unsupported. No suppression or general
font rewrite is introduced. Separate source review cleared the correction.
Only two objects and one link per profile rebuilt before both actual Script
fixtures passed with their original 120-second deadline. Earlier paired font
controls and unchanged manager/content results were retained.

The diagnostic executable is non-installed, excluded from default builds and
not registered as an always-passing test. Its failing attribution mode is an
explicit manual comparison, not a waived test.

## Profile and boundaries

One existing owned Podman/AppleHV Linux ARM64 guest, GCC 13.3.0 debugoptimized
and Clang 18.1.3 address/undefined profile with compiler-rt. Build options,
generated configuration, compiler identity and source/artifact hashes verify
before/after dispatch. Sanitizer options are `detect_leaks=1:abort_on_error=1`
and `halt_on_error=1:print_stacktrace=1`. External Xvfb/font libraries are not
instrumented. This is scoped guest execution, not native Linux hardware
sanitizer qualification or a six-target/install/default-X release pass.

The registered commands ran through an owned bounded runner, not a whole Meson
suite. Default core startup can launch MFL before the configured module path;
these cases prove affected core behavior but do not establish which duplicate
MFL startup won from a fixture `--mfl` argument alone. No MFL code changed.
No new UI, theme/screenshots or broad support repeat was needed.

The content ledger now has executed policy evidence in both profiles. Payload
storage, exact dependency/protection comparison, capture, native transport,
installed capacities and clipboard-manager/SAVE_TARGETS adoption remain open
under #63/#71/#74/CLIPBOARD-01. X11 confidentiality, physical hardware and
release-wide acceptance remain separate. Prior reports retain their historical
failures; this report supplies the scoped continuation.
