# FvwmScript FIFO processing proposal — 2026-10-05

Status: completed after the user approved a five-second overall deadline with
an override. All seven local Linux/BSD build/test/install gates and four
default BSD X servers pass. See the [final report](../verification/VERIFICATION-2026-10-05-SCRIPT-FIFO.md).
This follows the sparse colorset migration in SCRIPT-01.

The documented SendMsgAndGet protocol uses two named pipes in FVWM_USERDIR.
Modes 0 and 1 wait for helper startup; mode 1 also waits for its reply. Mode 2
is an immediate, atomic send. Keep the protocol filenames, return values and
module process isolation, with no added runtime dependency.

## Findings

The legacy loops check pathname accessibility before opening. The subsequent
blocking open, write and stdio read can wait indefinitely, even though the
manual describes a 20-second reply wait. Open failure is checked against zero
rather than -1; write errors can subtract from progress. The reply allocation
has no spare terminator when the complete 32000-byte allowance is filled.
The path is built from an unchecked communication identifier, and endpoint
opens do not establish the named-pipe type and owner before use. Mode 2 also
needs checked close-on-exec and endpoint validation.

These are source findings from modules/FvwmScript/Instructions.c,
FuncSendMsgAndGet. No external target or oversized allocation was exercised.

## Proposed implementation

Use checked nonblocking close-on-exec descriptors, a monotonic overall deadline
and bounded poll-based progress. Reuse the existing process termination policy;
release all owned descriptors on every exit. Accept only owned named-pipe
endpoints with final symlinks refused, and a bounded basename communication
identifier. Validate mode conversion. Preserve mode 2's single atomic write
and immediate refusal when a reader or pipe capacity is unavailable.

Cap replies at the existing 32000 bytes plus a separate NUL byte. Preserve the
existing failure value 0 and successful mode-0/mode-2 value 1. Keep helper
startup retries bounded by the same overall call deadline as the write/reply,
so a missing or unresponsive helper cannot trap the module indefinitely.
A timeout ends the current communication attempt; it does not terminate the
external helper. Partial writes cannot be rolled back; a helper remains
responsible for its documented framing/recovery behavior.

Introduce *FvwmScript: FIFOTimeout milliseconds with a finite validated override,
using the same 1–3600000 millisecond range as CommandTimeout. This changes the
previous independently counted startup/reply waits to one enforced deadline.
The existing CommandTimeout remains specific to GetOutput subprocesses.

## Compatibility decision

The user approved a **5000 ms default overall deadline with a configurable
finite override** on 2026-10-05. This includes helper startup, command writing
and reply collection. It matches the approved GetOutput default. Themes with
slower helpers can explicitly configure FIFOTimeout 20000 or another value
from 1 through 3600000 milliseconds before launching the module.

This replaces the old independently counted waits and its advertised
20-second reply period. The operation remains inside the isolated Script
module, outside the window manager's authoritative event loop.

## Verification

Native policy/transport tests cover missing readers, delayed startup, backpressure,
interrupted deadlines, complete/empty replies, the 32000-byte boundary,
descriptor zero and close-on-exec, cancellation, broken peers and caller signal
state, endpoint refusal and terminated text. Actual Script checks verify the
protocol, return conventions, five-second default, explicit override, later
widget events and termination. Final full gates pass 75/75 on three Linux
compiler/architecture legs and 64/64 on four BSD targets, with ten FIFO checks
on every default BSD X server and matching fresh installations. Native Linux
sanitizer CI remains configured but unrun. Corrections and provenance are in
the final report.
