# Private drawing evaluation: source and affected compile

Date: 2026-11-03. Owner: GRAPHICS-02 [#211](https://github.com/gonz0w/fvwm-x/issues/211).
Status: affected compilation passed; runtime qualification and adoption pending.
[Source disposition](../decisions/DRAWING-COMPARISON-PROTOTYPE-2026-11-03.md).
[Receipts and measured ELF costs](VERIFICATION-2026-11-03-DRAWING-EVALUATION.json).

The batch adds a bounded operation-local comparison painter, pure geometry and
transition policy, private Form/Buttons adapters and optional targets. Ordinary
builds keep their renderer and configuration. The new `graphics-eval` feature is
disabled by default. No optional evaluation executable or archive is installed.

## What passed

One separately reviewed plan used the existing native Linux ARM64 GCC 13.3.0
profile, with PNG/SVG/session D-Bus enabled and NLS/docs disabled. Cairo 1.18.0
and X11 1.8.7 were already present; no packages were installed. The direct Cairo
Xlib API link check passed independently of SVG. Default-off introspection had
none of the seven optional targets and retained the standalone pure-policy test.
Default-on introspection had exactly the seven non-installed optional targets.

Two ordinary modules and the policy test compiled, followed by four private
module variants and the real-X shape probe. The initial compile commands passed on their first
execution attempt. Configuration and build commands total about 6.6 seconds;
preparation including metadata took 6.87 seconds. The initial dispatch took 11.50
seconds, excluding VM startup/shutdown. Final source review then caught a
ConfigureNotify comparison using content dimensions. One adapter correction uses
existing cached actual client dimensions and rebuilds only the two private
Buttons variants. That focused repair passed; seven other ELFs, header/profile
and the PR352 reference controls remain unchanged. Its dispatch took 2.03 seconds. Every guest command's owned process group
was absent before continuation. The container was stopped before bounded receipt
export; final container and machine shutdown are confirmed. No product, unit or
UI test ran in this compile plan, and there was no install or full WM build.

`make check` passed the repository foundation checks. Working-tree whitespace
checks passed. These static checks do not establish native product behavior. The builds retain
pre-existing Buttons fallthrough and Form partial XWMHints/XSizeHints initializer
warnings at unchanged source sites; this is not a warning-free build claim.
The earlier native macOS ARM64 pure-policy unit passed before a subsequent
whitespace/comment reflow. Its original plan/source pins remain in the receipt;
current exact policy source was compiled on Linux and has not been rerun. Do not
promote that historical pass into exact-current-source runtime qualification.

The later staged check included newly added files and caught one leading
space-before-tab indentation error in the Form adapter. The correction removes
only that indentation tab; its exact byte substitution against the compiled
frozen overlay is recorded in JSON. The other 23 executed source pins stay
unchanged. No non-whitespace source byte changed and no native build or runtime
check was repeated for this correction. The failed staged check is retained;
a working-tree diff check alone had excluded the then-untracked new files.

## Observed executable cost

These are debugoptimized ELF contents on this one profile, not package size,
resident memory, allocation totals or performance results. Private native twins
include the same transition/adaptation work as the Cairo variants; retain the
ordinary modules as a separate baseline so that glue costs remain visible.

| Module | Variant | ELF bytes | Text | Data | BSS |
| --- | --- | ---: | ---: | ---: | ---: |
| Form | ordinary | 1,923,008 | 452,141 | 70,096 | 14,272 |
| Buttons | ordinary | 1,857,824 | 453,206 | 13,872 | 14,808 |
| Form | native evaluation | 1,958,016 | 458,693 | 70,096 | 14,544 |
| Form | Cairo evaluation | 1,972,816 | 467,793 | 70,344 | 14,544 |
| Buttons | native evaluation | 1,893,192 | 459,942 | 13,872 | 15,080 |
| Buttons | Cairo evaluation | 1,907,984 | 464,946 | 14,120 | 15,080 |

Only the private Cairo module variants add a direct `libcairo.so.2` dependency.
Ordinary modules and private native twins do not. The core ELF is byte-identical
to the PR352 input. Complete direct/transitive source, build, runtime and install
costs on all six targets remain pending; this table does not substitute for them.
No startup, latency, CPU, RSS, idle, X-request or roundtrip improvement is claimed.
The periodic production-performance ledger remains 0/5 after PR352.

## Authored oracles and queued shared qualification

The shape probe checks preflight/refusal leaves requests and pixels unchanged,
all four shape types with antialiasing on/off, patterned-background preservation,
per-operation footprint confinement, clip confinement, native fallback, explicit
copy/resource balance and zero X errors through cleanup/destruction. It compiled;
none of these real-X expectations has been executed for this candidate.

Pure-policy coverage includes geometry/area limits, empty/intersecting clips,
eight-paint/120ms limits, skipped frames, stale-token isolation, repeated target
updates, replacement, clock regression, reduced motion, visibility and identity/
generation retirement. Linux runtime remains queued.

The G2–G5 checkpoint must exercise ordinary/native-evaluation/Cairo-evaluation
views of the existing three-profile workload, plus these module lifecycle oracles:

| Stimulus | Required observation |
| --- | --- |
| Form choice changed twice before deadline | Selection/action state changes immediately; at most one live transition; final bevel matches current selection, with no accumulated antialiasing. |
| Buttons focus moves between direct-root content leaves with distinct normal/focus foregrounds | Current focus is immediate; old leaf settles; no retained record pointer or idle timer; native/Cairo twins share transition policy. |
| Enter/Space, admitted pointer press, disabled/empty pointer target, hover or effective PressColorset during focus transition | Existing dispatch/action remains immediate; superseded color cannot paint over the current role; rejected pointer input cannot strand intermediate pixels. |
| Same-size position change or live interruption | Settle the current live control through its ordinary redraw, without replaying stale geometry. |
| Resize/reflow/scale/palette/background change | Invalidate before publication; subsequent draws use current owner geometry/resources and reconstructed background. |
| Target or frame unmap/destroy/teardown | Cancel without touching the retired drawable; no later callback, stale paint or X error. |
| Reduced motion, equal colors, invisible control, or absent full-motion harness request | Immediate final state and no animation timeout. |
| Unsupported visual, oversized geometry or injected preparation refusal | Same-site native fallback; no partial candidate commit. |

Use Slate dark, Dune light and enlarged Classic; capture actual new UI only when
those runtime cases execute. Choose distinct valid foregrounds on a common
background for the focus-color comparison and include immediate/pressed controls.
No screenshots were generated by this compile-only checkpoint. Full quarter/mixed-
monitor, resource lifetime, accessibility/optical, native sanitizer, repeated cost
and six-target/default-server/install acceptance remain under their existing
owners. `NextRequest` is an issued-request delta, not an implicit-roundtrip count;
a complete roundtrip observation method remains a prerequisite to that claim.

## Review and remaining limits

Separate source review found and corrected per-operation antialiasing bounds,
background-preservation oracles, explicit copy/Cairo-object counters, narrow Xlib
header dependencies, and live-versus-retired cancellation/subscription paths.
Execution-input review corrected the optional-target classifier and made failed
or unconfirmed shutdown a failed plan result. The execution-plan corrections preceded dispatch. The final source dimension
correction followed the successful initial build and has its own focused receipt;
no failed native attempt is hidden. No automatic library/renderer adoption follows.

The implementer role remained unavailable, so the authorized lead fallback wrote
the source. Separate reviewer/analyst inspections and product documentation were
used; actual backend model selection remains unverified. Final integrated and
signed-head review are delivery gates. #211 remains open with its frozen scope.
