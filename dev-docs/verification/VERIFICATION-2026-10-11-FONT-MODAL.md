# Script fonts and modal service — 2026-10-11

Owner: SCRIPT-01 / LOOP-01 shared module readiness and font rendering.
Status: strictly locally qualified. The full frozen 3bb3 baseline and affected
69af resource-revision correction pass their combined acceptance gates. Native
Linux release sanitizer CI and physical desktop checks remain separate.

## Source and stories

Frozen baseline candidate `3bb3f059e4c143e026882de22849378261ecf1ec349704371591e28824699d74`
contains 1403 files, derived from accepted I18N source f183 with 23 owning paths.
Evidence lives in `builddir/codex-2026-10-11/font-modal/`. Other threads' current
build targets, configuration and test registrations are excluded; the accepted
Meson test graph receives only the affected link options and two new tests.

1. Script main and modal loops share one operation-owned packet continuation,
   session readiness state and initialized monotonic periodic clock. Normal X
   dispatch and expose/configure collection have finite event budgets. Module
   callbacks never reenter the borrowed reader or an executing script block.
2. Button, menu, text-selection, scrollbar, MiniScroll and List waits use finite
   readiness. Popup polling no longer spins; List no longer uses select(32).
   Escape, termination and transport EOF cancel waits. Serviced changes to the
   active widget cancel its old interaction before cached font/title/layout use.
3. Modal non-input events use the ordinary dispatcher without recursively invoking
   another widget. Paste and script selection waits use an optional cancellation
   service; the existing request API and private requestor/deadline remain intact.
   Popup grab failure cleans up; caller-owned cursor IDs are released after the
   window retains the resource. Unused double-click/delay wrappers are removed.
4. UTF-8 scalar decoding is bounded, validates continuation/overlong/surrogate
   cases and supports four-byte offsets. Core STRING16 replaces supplementary
   scalars and invalid bytes; trailing fragments never leave uninitialized glyphs.
   The legacy Big5 ASCII fallback advances. The combining buffer holds a full
   four-byte character plus NUL; position/allocation arithmetic is wide.
5. Font-set width uses bounded wide-character batches for long input while
   preserving locale conversion state. Core/Xft measurement remains checked.
   Charset conversion returns its owned converted buffer; a non-ASCII one-byte
   string takes conversion rather than the ASCII shortcut.
6. Native unit coverage checks every Unicode scalar and truncated prefix. Actual
   font controls cover offsets, font-set accumulation, Big5 progress, configured
   Latin-1/Xft conversion and combining/supplementary rendering. Thirteen real-X
   scenarios check held command/timer service and mutation/release cancellation
   with inherited descriptors through 80. The Script manual documents behavior.

The [implementation contract](../decisions/FONT-MODAL-SERVICE-2026-10-11.md)
records the configuration-preserving policy. No additional dependency is introduced.
This does not add grapheme editing, shaping/IME or a whole-font resource ceiling;
those retain TEXT-01 and broader resource-budget owners.

The final ownership audit adds a four-path correction, frozen as
`69af035b70d1605a14fce45bb1ecc1743e5b99f434c42aa27e29d6b8adb2b002`,
under `builddir/codex-2026-10-11/font-modal-lifetime/`. Each widget owns a
resource revision, initialized by its existing zeroed allocation. Successful
font/title replacements advance it; modal and selection snapshots compare it
before reusing cached state. A bounded Script callback cannot perform enough
replacements to wrap the 64-bit revision back to its starting value. This closes
the pointer-identity gap if allocation or font caching reuses an address, including
multiple replacements in one callback. The fixture adds an identical-title-only
held-button case, for fourteen scenarios. Two retained baseline controls pass:
neither reproduces allocator reuse, and this report makes no reproduction claim.
The correction passes affected eight-compiler, optional, scoped and default-BSD-X
checks against unchanged baseline consumers; the full baseline matrix is retained.
Final source still contains 1403 files; combined ownership is 24 paths. This is
baseline-plus-correction qualification, not a claim that all full-suite entries
were repeated on 69af. Only the Script structure/setters and modal regression
change after the baseline; shared font and selection implementations stay exact.

The legacy BMP combining path still substitutes `?` for unsupported four-byte
input. Its replacement shrinks rather than expands that input; the final audit
does not find the suspected expansion. The new direct STRING16 scalar conversion
uses U+FFFD. Neither path implements supplementary-plane shaping or font fallback;
the supplementary draw check is a lifetime/bounds control, not a glyph fidelity
claim. TEXT-01 retains those rendering capabilities.

## Focused evidence and retained attempts

The initial Linux candidate passes three existing font/widget/text-list tests.
Its first build dispatch used an ambiguous Meson fvwm3 target name and stopped
before compilation; explicit executable selection corrected the harness.
The subsequent candidate passes those three tests. Its first new modal fixture
stops on an expected window-discovery race after module termination. The corrected
fixture accepts BadWindow only during tree/name scans, keeps all other X errors
fatal, and passes all thirteen scenarios. The next Linux candidate passes all
four affected integration tests. FreeBSD/OpenBSD ARM64 each pass the four affected
integration tests and exhaustive native font-text unit test from frozen 3bb3.

Original source snapshots, logs and executed runners are retained. These focused
checks alone are not the complete support matrix. The final combined records
cover full registered suites, compiler profiles, staged executable identities,
default BSD X servers, optional features and scoped sanitizers. Native release
Linux sanitizer CI, actual composed input/toolkits and physical hardware remain
separate requirements.

## Accepted combined evidence

| Gate | Passing scope |
| --- | --- |
| Full baseline, eight standard compiler profiles | 1832 registered entries: 235 per Linux GCC/Clang profile on AMD64/ARM64; 223 per FreeBSD/OpenBSD target on AMD64/ARM64. |
| Baseline optional Linux ARM64 | Minimal 183; enabled software 214. Total full/optional baseline: 2229 entries, 150 staged code identities and 99 regenerated catalogs. |
| Installed baseline on default BSD X | 52 driver invocations, thirteen per target: FreeBSD scfb on both architectures, OpenBSD ARM64 wsfb with the existing root fallback, OpenBSD AMD64 vesa. |
| Final resource revision, eight standard profiles | 104 affected driver invocations against unchanged baseline consumers. |
| Final optional and scoped profiles | 23 affected optional invocations (minimal ten; software thirteen), plus thirteen actual-module ASan/UBSan invocations with teardown leak checking disabled. |
| Final staged module on default BSD X | 52 affected driver invocations; fourteen modal scenarios per modal fixture. Eleven exact module stages cover standard, optional and scoped profiles. |

Six full baseline archives, five baseline companion archives and six correction
archives validate declared source hashes/modes, exact executed runner versions,
registered fixture derivation and original deadlines, staged bytes and gzip
footers. The correction proves that imported consumers match the baseline's
tested binaries. Where native build commands reuse the unchanged core library,
configuration, archive and thin-member hashes match and Meson's commands are
rechecked. Each module stage matches its Meson install specification/RPATH
transform; this module-only staging does not claim a fresh entire install.
All source/build trees and original attempts remain separate and retained.
The final acceptance receipt is
`builddir/codex-2026-10-11/font-modal-lifetime/completion.json`.

Eight scoped Linux ARM64 Clang ASan/UBSan checks pass: exhaustive font-text policy
and the production font probe with leak detection enabled, plus six actual-module
checks with inherited teardown leak detection disabled. This is local scoped
coverage, not native release CI. The affected man and HTML render with existing
Linux Asciidoctor; native FreeBSD mandoc presents baseline and candidate roff.

The first whole-source collector embedded the prior I18N runner filename.
Read-only recollection preserves the original archive and includes the actual
executed font/modal runner; runtime gates are not repeated. Its first OpenBSD
archive move encounters the separate /tmp and /home filesystems; verified byte/
mode copying preserves the original before replacement. The first host validator
also assumed no generated Python bytecode; it now validates every declared
source hash/mode and permits only additional __pycache__/*.pyc files.

Correction harness recovery is also retained: its first minimal-profile selector
assumes thirteen fixtures although the feature-disabled registration contains ten.
It stops before those tests run; completed default/Clang results are preserved and
only remaining profiles resume. The first resume encounters the already-created
stage directory and is corrected without changing source. The first baseline
consumer collector counts configured script outputs alongside native targets;
it now uses Meson's native install specifications. Finally, native AMD64 library
reuse initially omits thin-archive members and fails at link time. Copying and
hash-checking the unchanged member objects repairs that build-only preparation.
Executed initial/resumed runners and logs remain part of the evidence; no runtime
source, fixture deadlines or dependency requirements change during these repairs.
