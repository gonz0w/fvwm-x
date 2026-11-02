# A1 desktop component metrics: source/build checkpoint

THEME-02 #214 gains three bounded independent settings, a versioned pure role
projection, scoped native consumers and a compact editor/manual. Work remains
in progress. [Contract](../decisions/DESKTOP-COMPONENT-METRICS-2026-11-02.md) ·
[Source/build receipts](VERIFICATION-2026-11-02-DESKTOP-METRICS.json).

Base is merged PR345 `ea6098d692fc459afa7e2d661dc00296d6198175`.
The existing private settings store reads exact schemas1–9 without rewriting
and saves schema10. Defaults inherit; module overrides retain precedence.
Literal font policy rejects bytes native commands cannot preserve. The existing
application-font transport keeps its broader independent contract.

Only the named launcher style and future controller-owned Slate forms consume
metrics. Existing forms retain resources until reopened. Panel, tray, global
font/decoration and personal configuration are outside this slice. Initial inherit
emits no new metric override; explicit-to-inherit restores only documented
profile baselines. A separate submitted tuple preserves the eight-position color
preview key and prevents desired metadata from implying submission. New forms
also use that submitted tuple while recovery is pending. No font/geometry claim
is added to the native preview lease or PreviewKey.

## Observed

Four final affected offline Go1.27.2 compile/link commands pass: desktopsettings,
desktopcontroller and desktopcheck test binaries plus FvwmDesktop. Existing
vendored dependencies, approved host SDK/cache and CGO-disabled profile were
reused. No toolchain/package download, native build or lab start occurred.
The first controller compilation caught a nonexistent fixture method; the call
was corrected to existing editSettings. The original failure receipt remains.
Separate source review found that a metrics-only edit could replay the full
color/panel/tray projection. The corrected unchanged-color path edits only
launcher metrics, including forced recovery replay; unchanged effective metrics
do not repaint. Two affected controller/executable builds pass after correction;
two settings/check artifacts retain their unchanged inputs from compile03.
Repository/whitespace checks and gofmt pass. Host Asciidoctor is unavailable;
rendering and installation remain queued. Separate source/evidence review is
clear after the scope correction; exact signed-delivery review follows.

## Authored, unexecuted acceptance

Cases specify literal-font refusal, independent role values, exact historical
schemas and future-key substitution refusal, scoped editor/encoded callback,
module override precedence, requested-versus-submitted facts, native command
order/reset, unchanged projection, independent aliases and preview publication
barriers. A synthetic release exercises the replay seam; it is not actual core
resource recovery. These cases were compiled, **not run**.

There are **zero runtime invocations and zero lab starts** in A1. Runtime/race,
real-X font/spacing/border and actual preview recovery/conflict observations,
three materially different themes including enlarged text and screenshots join
the named A1–A4 shared appearance checkpoint under the approved cadence. No new
screenshot or rendered success is claimed. The separate C2–C5 clipboard queue
remains open; capture stays off. Full support/compiler/install/default-X and
release/physical/accessibility acceptance remain with their owners. No issue
closes and no speed improvement is claimed. Performance is2/5 before delivery.
