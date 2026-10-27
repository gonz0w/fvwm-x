# Configuration diagnostics: CFG-02 closeout

Owner: Cam. Base: merged PR267, `5bfcc3e5a394d8ed22e7a8bc8e5e21c7f81fc3dc`.
Candidate: `codex/r1-config-diagnostics`. Status: all finite #61 criteria locally
qualified for review. Issue #61 closes on this PR’s merge. All eight selected profiles pass on the six actual targets.
INSPECT-01 #218 remains open for its separate effective-origin/domain scope.

[Design and protocol](../decisions/CONFIGURATION-DIAGNOSTICS-2026-10-26.md),
[manual](../../doc/FvwmDesktop.adoc),
[actual screenshots](../../doc/development-preview.md#source-aware-configuration-diagnostics).
This is a coherent batch of source/span ownership, selected module provenance,
literal native error presentation and complete paged checker reports.

## Frozen contract and acceptance map

The 2026-10-26 finite #61 release contract supplies these criteria. No criterion
is weakened or moved to another issue to obtain closure.

| Criterion | Implementation and observed result |
| --- | --- |
| Runtime filename, continued span and nested include context for unknown commands, malformed arguments/options and selected module errors | Context-owned immutable snapshots; native fixture independently specifies repeated Read chains, no-final-newline input, unknown commands, malformed DesktopSize/Read, generated/function invocation and unavailable module/event/Schedule origins. Selected FvwmDesktop errors retain source through initial configuration, restart replay and live delivery. All eight affected profiles pass; three actual Linux UI configurations pass. |
| Finite supported declarative grammar | Freeze existing `desktop-settings-literals-and-Read-v1`: registered literal settings and literal Read with optional Quiet. Reuse the settings registry and accepted checker; no new language interpreter or competing grammar. |
| Dynamic forms visibly unchecked, no execution | CLI JSON/text and native checker exercise Exec, PipeRead, module and expansion inputs as unchecked. The fixture independently verifies source bytes remain unchanged and action markers never appear. Existing broader negative/limit policy coverage remains; changed Go packages run on each default target. |
| Bounds and offline/native reports with accurate source locations | Runtime logical and physical input cap 1 MiB; source JSON 1536 bytes, 16 include callers and uint32 lines; retained module-source storage 1 MiB, including retired entries. Native tests prove cap refusal and recovery after deletion, and synchronous deletion during replay. Strict one-shot source pairing rejects ambiguous/stale/malformed metadata. Offline schema version 1 stays unchanged; native reports preserve full long paths across pages of at most twelve wrapped rows. |
| Source-bound policy, real-X, theme and affected-profile qualification | Linux normal/race/scoped sanitizers and three actual configurations pass; all fifteen captures inspected independently. All eight affected profile gates below pass. Broader physical/assistive/release work retains its owners; #61 closes on merge. |

A fresh event/module Read opens a known file root without inventing a source for
its caller. A Read beneath a non-file invocation/generated/unknown parent has
unavailable complete-chain provenance. Optional generated/function locations
identify the nearest known enclosing invocation, possibly an outer call; inner
dynamic steps and definition locations are not reconstructed. Existing command/
Silent ordering and configuration syntax remain.
Live Desktop input is diagnostic-only after startup: valid configured overrides
still require controller restart; saved/requested/effective state is unchanged.

The checker retains its separate 4 MiB combined input, 1 MiB line, 64 file visits,
eight include depth, 10000 lines, 64 diagnostics and cooperative three-second
budget. A budget cannot preempt a blocking filesystem operation. Checking remains
isolated, never validating by executing configuration.

## Source and execution receipts

Canonical frozen source archive:
`1cabcfc3b412142c9fddd5501f8f2937ecebf3e5a5d6b05c71fdc4a2f508a3f6`,
2640 regular inputs. The private manifest records each SHA-256 and mode, guards
source before/after execution and rejects extra Go package source files.
Documentation/gallery/evidence completion after this freeze does not change
tested implementation, build, protocol or fixture inputs; final equivalence
is checked before publication.

The canonical Linux development archive is
`9ae0d1e75e8338f9813010b2443a30e96aac38d3b4b61c8875498ea8ff48ff17`
(1,248,976 bytes). Host validation recomputed archive identity, gzip CRC to EOF,
all 209 payload hashes/sizes/modes, all fifteen capture identities and final
fixture/runtime hashes against the frozen platform source. Initial failures
and the final passing runs are retained together. The [acceptance receipt](VERIFICATION-2026-10-26-CONFIGURATION-DIAGNOSTICS.json)
records all eight archive identities, compilers/SDKs, 64 selected policy executions,
18 actual-OS Go package executions and 1663 final implementation-input checks.

Focused development results:

- Normal C logical-line/source/context, module runtime/queue/packet/config-line
  tests pass, as do generated C/Perl metadata parity and maintenance metadata tests.
- Native Linux Go race checks pass for controller, checker and desktop CLI;
  the final changed controller rerun passes after live subscription adoption.
- Scoped ASan/UBSan logical-line, source and context tests pass with leak detection.
  The final actual-X core fixture passes in 0.795 seconds with
  `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and UBSan halt enabled. This does
  not claim whole-core leak freedom, native-CPU release CI or physical hardware.
- The final Slate dark, Dune light and enlarged 18pt monospace run passes in
  7.279 seconds. Startup/replay/live notices, offline text/JSON, no-execution
  markers, both ordinary report pages and complete three-page long chains pass.
  Every report control is mapped and inside the 1440×1200 display. The largest
  enlarged long-chain client is 1106×566 pixels.
- Notices dismiss through actual XTest pointer clicks. Report navigation and
  Close exercise the native Form KeyPress handler via XSendEvent. These results
  do not claim global focus-return or physical-keyboard/assistive acceptance.
- Separate final source, protocol, lifetime, evidence and fifteen-capture review
  has no unresolved findings. Requested bounded implementation/review delegation
  used the project-selected Sol/high roles and Astra/high scope analysis; the
  requested routing is recorded, not independently attested model execution.

## One affected combined platform checkpoint

Reuse owner-controlled builds; run native binaries on their actual OS X11
stack. Eight selected entries per profile cover config line/source/context,
module runtime/queue/packet/config-line and C/Perl packet metadata. Each profile
also runs the final core-only real-X source/budget/replay fixture. Six defaults
run the three changed Go packages on the actual OS/architecture. Linux Clang and
reduced retain their same-target default Go-policy result.

| Target/profile | Result |
| --- | --- |
| Linux ARM64 GCC default | Pass |
| Linux ARM64 Clang | Pass |
| Linux ARM64 reduced: PNG/SVG/XPM/BiDi/NLS and auto optional features disabled | Pass |
| Linux AMD64 GCC default | Pass |
| FreeBSD 15.1 ARM64 default | Pass |
| FreeBSD 15.1 AMD64 default | Pass |
| OpenBSD 8.0 ARM64 default | Pass |
| OpenBSD 8.0 AMD64 default | Pass |

Installed protocol changes are checked through native C-to-Perl decoding of all
eleven extended events, generator/export parity, and four metadata files copied
to their exact Meson-introspected install destinations with matching bytes/modes
and Perl syntax checks. This is a scoped metadata stage, not a repeated full
installation. No new installed entry, resident, dependency or GUI toolkit is
introduced. Accepted PR153 checker six-target/ten-profile installs/default-X and
M2 Go-port evidence retain their original scope; they are not relabelled as
execution of this changed runtime. Full unrelated install/default-X/compiler,
provider, physical and release qualification remains separate.

Manuals are rendered with the existing Asciidoctor manpage backend and reviewed
in actual mandoc output at 60/80 columns. Rendering/formatting passes. Mandoc
lint exits 3: both merged-base and candidate output have the same diagnostic
families (GNU `mso`/`LINKSTYLE` header compatibility, missing-date/paragraph
macro warnings and generated-input line-length style notices). New paragraphs
increase style/paragraph counts; no clean-lint or complete DOCS-01 acceptance is
claimed. Baseline and final candidate archives retain all messages; final
archive `8f49608577705ef280dbe815baf6bb2d36f8075e6a1ce0d57b0f8bb165c964f3`.

Both OpenBSD jobs use pinned 8.0 images/cache/SDK and the existing disposable
runner, verified upload handoff and normal ephemeral-key cleanup. Historical
7.9 evidence is not used. AMD64 emulation and VM OS execution are distinguished
from native CPU/hardware qualification.

## Corrections, costs and limits

Initial fixtures incorrectly relied on an unquoted punctuation filename through
`-f`, omitted verbose logging, used a shell newline escape removed by FVWM
parsing, imposed notice focus ownership, expected unquoted CLI paths, and treated
X IDs as page generations. Corrections keep source/error assertions intact:
quoted nested Read, existing `-v -o -`, no-newline printf, actual Dismiss clicks,
quoted CLI expectations and observed page callbacks. Passing builds/policy
checks were retained; only affected native checks repeated.

Real checks/review also fixed two missing subscriptions/contracts: the new
source bit is excluded from legacy default extended masks until selected, and
Desktop selects live M_SENDCONFIG with validation-only handling after startup.
Review corrected pagination by rendered rows and pinned module-config entries
through synchronous replay/broadcast callbacks before deletion frees them.
No unresolved product defect is presented as a passing result.

| Linux GCC artifact | File-byte change | text+data+bss change |
| --- | ---: | ---: |
| Core | +91,840 | +18,171 |
| MFL | +368 | +168 |
| Form | +368 | +160 |
| Desktop controller | +5,371 | +11,841 |
| Desktop worker | 0, exact same hash | 0 |

These are same-cache file/section observations, not RSS or a speed measurement.
Debug information and Go reserved sections are not resident-memory accounting.
Retained provenance has a separate 1 MiB bound. No periodic performance repeat
is due: PR264 is the reference, 3/5 after PR267 and 4/5 on this batch's merge.

The host preserves raw captures and per-target archives/manifests. Linux guest
duplicates were removed only after exact host/archive/tree verification;
sources, incremental builds, performance references and VM disks were preserved.
Per-target receipts retain normal guest shutdown and disposable-runner key
removal. Publication uses signed author-DCO commits and local review; Actions
remains disabled. No full R1, #218, theme, physical or release acceptance follows
from this bounded #61 closure.
