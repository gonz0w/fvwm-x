# Configuration source diagnostics

Status: implementation contract for the CFG-02 #61 closeout checkpoint, owned
by Cam. This records the bounded design, not passing verification or issue
closure. The [project tracker](../PROJECT-TRACKER.md) owns acceptance and status.
INSPECT-01 #218 may reuse this metadata and the existing checker report; its
broader configuration authoring, binding and theme-origin work remains open.

## Runtime source ownership

[Logical-line reading](../../libs/config_line.c) reports inclusive, one-based
physical first/last lines, including continued lines and a final unterminated
line. The original `fvwm_config_line` interface remains a wrapper. Both reusable
logical storage and consumed physical input per logical command have a 1 MiB
bound; continuation-only input cannot evade the physical byte limit. Refusal
initializes span outputs to zero and dispatches no partial command. Completed
commands retain their original execution order.

[Read](../../fvwm/read.c) creates an immutable bounded
[source snapshot](../../libs/config_origin.c) for each complete command.
[Execution contexts](../../fvwm/execcontext.c) retain/release these snapshots
through clones; they do not borrow a mutable current-Read pointer. Resolving the
opened path for diagnostics does not change existing current-file/directory
expansion variables or lookup behavior.

The source kinds are:

- `file`: a known opened absolute path and exact physical span. `include`
  contains the complete literal Read invocation chain, root first.
- `generated`: a PipeRead command. An optional path/span identifies the nearest
  known enclosing invocation, not a file containing the generated command.
- `invocation`: a function body whose definition source is not retained. An
  optional path/span identifies the nearest known enclosing invocation.
  Nested generated/function steps are not reconstructed: this may be an outer
  call, never a claimed exact direct inner call or definition location.
- `unavailable`: no exact source can be represented, including invalid text,
  exhausted bounds or an include chain through a non-file origin.

A Read issued by a fresh event/module context with a NULL source opens a new
known file root. This does not infer a source for the event/module command.
A non-NULL generated, invocation or unavailable parent cannot become an exact
literal Read chain: a file read beneath it has unavailable provenance. The
currently open Read file is never substituted for an unrelated event, module
command or scheduled action. Schedule contexts currently have no retained
source. A deferred command that retains/clones its execution context retains
that command's snapshot; this does not reconstruct a binding definition.

The [shared logger](../../libs/log.c) accepts only a borrowed generic scoped
context string; it has no dependency on core types. The command engine installs
the snapshot's JSON before parsing/dispatch and restores the previous scope on
return. Unknown-command and existing malformed option/argument diagnostics
using this logger gain the prefix without changing their dispatch or Silent
rules. Nested X-event dispatch and event/readiness waits clear the suspended
command's scope, restoring it after service. Stream refusal reports an
unavailable span and names the failing stream rather than attributing failure
to its caller's line.

## Optional native module metadata

[Retained module configuration](../../fvwm/modconf.c) stores bounded source JSON
alongside each entry. Text, source and alias matching stay together through
startup, replay, live publication and deletion. A 1 MiB aggregate retained
provenance budget includes entries awaiting retirement. Exhaustion preserves
configuration execution/text and substitutes explicit unavailable metadata;
it does not truncate an origin and claim that it is exact.

A synchronous config callback can delete the current or a later list entry.
Deletion unlinks immediately and marks the entry retired. Active replay and
broadcast traversals retain its data and original iteration link until the
outermost traversal returns. A separate retirement link owns cleanup; retired
entries are skipped and their data/source are freed once. This delays storage
release, not callback dispatch or command ordering.

The selected consumer is FvwmDesktop. [Module constants](../../libs/Module.h)
define `MX_CONFIG_SOURCE = ((1UL << 11) | M_EXTENDED_MSG)`. A consumer selects
this in a separate extended SET_MASK call, as required for extended messages,
and selects M_CONFIG_INFO through the normal mask. For retained entries:

| Packet | Native body words 0, 1, 2 | String at body word 3 |
| --- | --- | --- |
| MX_CONFIG_SOURCE | nonzero opaque correlation key, 0, 0 | source JSON |
| immediately following M_CONFIG_INFO | same key, 0, 0 | unchanged config text |

The key is not an X Window identifier. Headers and words use the existing native
unsigned-long ABI, not a portable network format. The
[packet sender](../../fvwm/module_interface.c) copies both bodies before any
callback can delete the retained entry. Metadata adds no synchronization
barrier: it is queued immediately before the config packet, whose existing
synchronous behavior remains in effect. Installed
[Perl constants](../../perllib/FVWM/Constants.pm) and
[packet metadata](../../perllib/FVWM/EventNames.pm) describe the same contract.
Constants are regenerated by the existing maintenance generator.

Unselected receivers retain body word 0 equal to zero and receive no additional
packet. Module initialization preserves the existing default extended bits but
excludes the new source bit until an explicit extended SET_MASK selects it.
Synthesized configuration such as LoggingFD, desktop geometry or image paths has no inferred retained origin and keeps its existing source-free
packet. No new configuration command or second transport is introduced.

A consumer owns at most one pending source. Only the immediately following
complete matching M_CONFIG_INFO can consume it, once. An intervening packet,
mismatched key, malformed metadata or completed pair clears it. Partial native
framing may finish before this decision; text equality never establishes a
source association. The
[selected parser](../../bin/FvwmPrompt/internal/desktopcontroller/source.go)
validates metadata before native presentation. Source text is displayed through
literal encoded fields and bounded wrapping, not executed as configuration.
FvwmDesktop also selects M_SENDCONFIG for live error diagnostics. Startup/replay
retains the existing override application. After startup, live options are
validated and errors are reported without mutating saved, requested or effective
settings; valid configuration overrides still require controller restart. This
is diagnostic adoption, not an additional hot-reload or preview authority.

## Source JSON version 1

The UTF-8 JSON string is at most 1536 bytes, excluding its terminating NUL.
The fields are `version`, `kind`, `path`, `first`, `last`, `include`, `reason`.
Version is integer 1; kind is one of the four values above. File sources require
an absolute nonempty path and integer lines in 1..4294967295, with last >= first.
Include entries contain exactly path/first/last, use the same range rules and
are ordered root first. At most 16 include frames are representable. A complete
chain either fits both depth and total wire bounds or becomes unavailable;
a partial chain is never emitted as exact.

Other kinds may omit site fields and include/reason, with empty/zero defaults.
If a site is supplied, path/first/last must appear together. Include frames
require a site. Optional sites on generated/invocation records identify the
enclosing caller only. Unknown or duplicate fields, null values, wrong types, fractional
or out-of-range lines, malformed UTF-8/Unicode escapes and control characters
in path/reason fields are refused. Invalid or oversized producer origins become
explicit unavailable records. The schema does not carry command text or
instructions to replay source.

## Frozen checking and reuse

The [non-executing checker](../../bin/FvwmPrompt/internal/desktopcheck/check.go)
retains coverage `desktop-settings-literals-and-Read-v1`: registered literal
FvwmDesktop settings and literal Read filenames with optional Quiet. Its
existing continuation, lookup, cycle and source-hash behavior remains. Terminal
provider/invocation checks, expansion, PipeRead, Exec, functions, conditionals,
bindings, theme evaluation and other unsupported syntax are unchecked rather
than evaluated. No checker may validate configuration by executing it.

Existing checker defaults bound combined bytes to 4 MiB, a logical line to
1 MiB, file visits to 64, include depth to 8, lines to 10000 and diagnostics to
64, with a cooperative three-second traversal budget. These are checker limits,
separate from the runtime source and retained module budgets. A time budget
cannot preempt a blocking filesystem operation; acquisition remains isolated
from the core owner.

Offline text/JSON reports and the native paginated report share this checked
subset and source locations. Runtime metadata can improve selected module error
reports and be reused by #218 without claiming effective binding/theme origins
or widening the checker grammar. #61 closure requires its own criterion-to-
evidence mapping and qualification; this decision alone closes neither issue.
