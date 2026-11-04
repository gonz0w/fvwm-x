# Drawing and layout: composed Linux checkpoint

The scoped G2–G5 Linux ARM64 comparison is complete. It covers all 54 selected
cases, nine screenshots and fifteen performance runs. One additional Cairo
positive requalifies the repaired painter, for 55 successful GUI invocations.
GRAPHICS-02 [#211](https://github.com/gonz0w/fvwm-x/issues/211) and LAYOUT-01
[#212](https://github.com/gonz0w/fvwm-x/issues/212) remain open for complete
support/cost evidence and their adoption decisions. All prototypes remain
**default-disabled and non-installed**. No production renderer/layout is adopted.
[Measurements, input identities, receipts and retained failures](VERIFICATION-2026-11-04-DRAWING-LAYOUT-CHECKPOINT.json).

## Changes and observed correctness

Four related outcomes complete this comparison batch:

- Clay's maximum-extent regression now quantizes every shared boundary by the
  same nearest-pixel rule, preserving finite/identity/adjacency/minimum/atomic
  output checks and exact integer coverage. The original 1–256-control, 32767px
  cases, captured 190-control endpoint and half-pixel negatives pass.
- Cairo allocates scratch pixmaps from the validated default-screen root,
  independently of a disappearing destination. The deterministic real-X probe
  permits only two matching destination-copy BadDrawable errors; all scratch,
  GC, surface and context resources retire, with zero unexpected probe errors.
  Existing clipping, preservation, shape, antialias and refusal checks remain.
- The fixture validates outer prepared rectangles, records bounded replies before
  failure and observes Form choice state through its documented conditional
  expansion. Layout timing now requires a real 800/880px resize and fresh queried
  rectangles covering the changed client, rather than timing an action alone.
- Composed native-X checks cover semantic actions, stable child identities and
  collection state, replacement/press/move/resize/palette/unmap/destroy,
  reduced/equal/refusal, choice-only teardown, and native/Clay weighted,
  compressed, updated and refused layouts. The expected updated metadata tag
  qualifies actual committed title updates even when Clay keeps equalized widths.

Five native C/sanitizer executions pass: drawing policy, observer, Clay, and
scoped native Linux ASan/UBSan Clay and observer checks. Nine focused Python
methods pass. These are native ARM64 CPU runs in the existing Ubuntu 24.04
container on the owned Linux AppleHV VM, GCC 13.3.0 debugoptimized, with SVG,
PNG and session D-Bus enabled; NLS/docs are off. The server is Xvfb 1440×900×24.
The JSON binds source21/22/23 manifests, generated configuration, twenty binary/
configuration inputs, runtime libraries, tools, fonts and per-case owned PIDs.
Ordinary/native controls and the core retain their original ELF identities.

## Failures, corrections and evidence reuse

The first Clay C run failed at 190 controls. One observation-only diagnostic
found last floating endpoint 32767.130859375, despite a valid nearest-pixel
endpoint. The correction retains the 0.125px adjacency/y/height tolerance;
no vendor edit, arbitrary epsilon increase, clamp or reduced range was used.
One affected Clay consumer/oracle/sanitizer build and qualification followed.

The first GUI baseline exposed an incorrect fixture assumption: global Frame/
Padding are child defaults, while queried outer rectangles cover the full root
client. Child content insets are not query geometry. The source-only G3 claim
has an explicit erratum. The next press failure exposed a second fixture error:
`$(flag)` always emits the configured choice value; `$(flag?yes)` observes on/off.
Exact actual and expected receipts are retained before assertions. The initial
ten direct-value receipts therefore do not prove selection state; later
conditional initial-on/final-off cases supply that evidence.

The destroy case then lacked the retired Buttons observer stream. One unchanged
case with existing verbose logging established fatal BadGC on FreeGC (opcode60),
following scratch allocation from a destroyed window. That diagnostic's later
JSON sequence error came from interleaved diagnostic logging; it is not a product
pass. The stable-root correction and deterministic X probe address the dependent
resource cascade without a new synchronization barrier or error suppression.

Separate source and frozen-input reviews preceded each execution. A proposed
probe-in-destroy plan was rejected before dispatch because the existing CLI
admits the probe only in a positive case. The final plan uses one affected Cairo
positive followed by the same 39 failed/unrun cases. No CLI change was needed.
All failures remain retained. Fifteen completed GUI cases, five C/sanitizer
passes, nine Python methods and three captures were reused where their contracts
were unchanged; old Cairo results retain their old ELF/source bindings. New
probe, destroy, theme and performance cases cover the changed painter. Only
three private Cairo targets rebuilt (0.437 seconds); seventeen other compiled/
configuration anchors and the library closure remained unchanged. Each phase
retired its owned process groups/UID tasks and confirmed container/VM shutdown.
No package, disk, install or full OS matrix change was made.

## Performance observations

Three fixed orders compare ordinary, native-graphics, Cairo, native-layout and
Clay-layout. Each variant has three runs with twelve action samples per module;
layout variants also have twelve real resize samples per run. Observation is off.
The table shows the median of run medians, with their minimum–maximum in brackets,
in milliseconds. Samples include harness, IPC and server scheduling overhead.

| Variant | Buttons action | Form action | Layout reflow |
| --- | ---: | ---: | ---: |
| Ordinary | 0.955 [0.881–0.969] | 14.801 [14.455–15.945] | — |
| Native graphics | 0.970 [0.922–1.005] | 13.383 [13.361–13.720] | — |
| Cairo | 1.010 [0.943–1.175] | 14.054 [12.701–14.513] | — |
| Native layout | 0.924 [0.895–0.974] | 13.530 [13.090–14.130] | 55.639 [55.051–59.613] |
| Clay layout | 1.035 [0.941–1.087] | 14.188 [12.405–15.382] | 59.631 [59.513–59.956] |

These small descriptive samples do not establish a causal speed gain. Native
and Cairo graphics use the same transition policy; ordinary has no new motion.
Layout reflow ends only after actual client geometry and fresh token-bound
prepared rectangles agree. It is neither solver-only nor input-to-photon time.
The unchanged Form consumer's varying timings also show run-to-run variation.

Observed Buttons RSS after the workload was 6528–6544 KiB ordinary,
6536–6548 native graphics, 7856 Cairo, 6544–6548 native layout and 6652–6660 Clay.
Cairo therefore added roughly 1.3 MiB over native graphics in this profile.
Clay requested a 414400-byte arena; RSS is not allocated-byte accounting. Its
module adds 102584 text, 456 data and 1256 BSS bytes over the native layout
control. The final native/Clay layout ELFs are 1802576/2129264 bytes; Cairo
Buttons is 1927336 bytes. Debug ELF size is not installed or runtime memory cost.
The vendor header and license add 303701+891 source bytes plus the adapter,
provenance and upgrade burden; zero production code is removed.

Workload CPU observations are 0–0.02 seconds per owned process at 100 ticks/sec.
All selected 0.4-second idle observations add zero measured CPU ticks. This is
not a zero-wakeup or zero-CPU guarantee. Issued X requests are not implicit
synchronous roundtrips: the complete roundtrip collector remains unavailable.
No whole-desktop performance or six-target/package cost claim follows. The
periodic whole-WM ledger stays 3/5 before delivery, 4/5 on implementation merge;
this component comparison does not reset it.

## Theme examples and remaining gates

These are controlled native-layout subsets, not full theme or application/toolkit
qualification. Ordinary and Cairo Slate captures are byte-identical; the Form
region is also identical in Clay. Clay intentionally redistributes row widths.
Enlarged Classic retains a clipped managed-window title and a low-contrast
black choice label on its dark Form surface. First-column list labels appear
clipped at tile edges in all profiles. These shared fixture/baseline limits are
follow-up candidates, not new Cairo/Clay regressions or passing accessibility
qualification. No unrelated visual polish joins the completed comparison.

| Profile | Ordinary | Cairo | Clay layout |
| --- | --- | --- | --- |
| Slate dark | [PNG](drawing-layout-2026-11-04/slate-dark-ordinary.png) | [PNG](drawing-layout-2026-11-04/slate-dark-cairo.png) | [PNG](drawing-layout-2026-11-04/slate-dark-clay-layout.png) |
| Dune light | [PNG](drawing-layout-2026-11-04/dune-light-ordinary.png) | [PNG](drawing-layout-2026-11-04/dune-light-cairo.png) | [PNG](drawing-layout-2026-11-04/dune-light-clay-layout.png) |
| Enlarged Classic | [PNG](drawing-layout-2026-11-04/classic-large-ordinary.png) | [PNG](drawing-layout-2026-11-04/classic-large-cairo.png) | [PNG](drawing-layout-2026-11-04/classic-large-clay-layout.png) |

Complete six-target support/cost, required roundtrip measurement and each frozen
adopt/defer/reject decision remain open. Physical displays, compositors,
default-server/install and release acceptance are separate. Separate peer-agent
review is not independent human assurance. All requested roles are available;
actual backend model routing remains unverified. No release or milestone closure
is claimed by this Linux checkpoint.
