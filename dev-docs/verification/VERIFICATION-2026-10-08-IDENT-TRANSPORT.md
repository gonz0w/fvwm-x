# Ident popup readiness and packet admission

Status: three related module stories are focused-qualified; full combined
qualification is pending. This source is separate from the running Event/Auto
consumer checkpoint and needs no configuration migration or new library.

The actual popup replaces raw select/fd_set and the descriptor-width query with
two descriptors through the existing cancellation-aware poll wrapper. X events
remain first; pipe hangup drains through the common reader, and termination or
terminal readiness errors return to normal outer control flow. Existing X-event
drains now observe termination. This does not introduce core X work or make the
module packet reader nonblocking.

Both window-list and popup owners validate declared payload sizes before calling
consumers: a complete native ConfigWinPacket, three words for property changes,
and three context words plus a terminator inside the declared body for names and
configuration strings. Refused bodies are diagnosed and discarded, allowing
subsequent valid packets. The obsolete, undefined DeadPipe declaration is removed
after reference and actual-module compile/link checks. These findings came from
source review; the old out-of-range readiness and short-body operations were not
executed.

Frozen source: 1363 files, five owned paths from consumer1361, two new regression
files. SHA-256:
`04d940c30ecca80d517d4977dbb2fe31b873adf405ab3b1fb9a2de2ed94d70e1`.
Evidence: `builddir/codex-2026-10-08/module-ident-production`. The Meson freeze
changes only the Ident block; unrelated shared-chat targets remain untouched.

Normal and scoped ASan/UBSan profiles each pass four pure and six actual-module
checks. Pure checks enable leaks; actual-process checks disable them. The new
actual popup control passes pipe EOF, idle termination, short/nonterminated
startup and dynamic bodies followed by valid packets, and input descriptor 4096.
A native C fixture emits the declared ConfigWinPacket using the project header;
the test does not duplicate native flags layout. Owned X queries establish that
the popup exists before exercising its wait; pixel correctness is not asserted.

Native FreeBSD/OpenBSD ARM64 overlays compile/link the actual module and fixture.
Both pass the existing five configuration cases and all three ordinary popup
cases. FreeBSD also passes descriptor 4096; OpenBSD reports that its inherited
hard limit excludes that case. No host hard limit is changed. High X-connection
descriptors are not separately forced. Three complete focused archives validate
payload hashes/modes and compressed EOF: 25 Linux files, 31 FreeBSD and 32 OpenBSD.

Initial preflight mistakes are retained: the first freeze assumed the Auto block
preceded Ident; the next selected an earlier generic live-X block and lacked the
new selector. Neither reached builds or tests. Post-test Linux artifact collection
initially omitted Meson's tests/ directory and suite prefixes; the correct paths
were read from completed test commands. The initial archive is retained and
collection was repaired without repeating tests or changing source/deadlines.

Packet reads/writes remain blocking with common termination cancellation. Complete
resource teardown, bounded X-event service, other module consumers and native
release CI/physical/toolkit/compositor verification remain open.

Ten combined runners pass source/syntax/selector preflight for the next checkpoint:
required 1424 standard entries/eight profiles, 120 installed identities, 156 BSD
installed/default-X driver invocations, optional 183/163 and 66 scoped checks
(26 pure/40 process/core). These are requirements, not passing matrix results.
Dispatch waits for consumer1361 qualification. The added default-X argument was
corrected to the existing display context during preflight, before dispatch.

## Combined successor — 2026-10-08

The Ident-only full matrix was never dispatched. Its future counts above are
superseded by the six-story [Ident/Pager batch](VERIFICATION-2026-10-08-POPUP-READINESS.md),
which passes final normal/scoped 4+7 and native BSD ARM64 focused controls.
Consumer1361 is now strictly locally qualified; the combined popup matrix can
proceed. This component's source04 evidence remains scoped to its own checks.

The combined popup1363 successor is now locally qualified: 1432 standard entries,
120 identities, 160 BSD drivers, optional 184/164 and 67 scoped checks. Seven
archives strictly validate. Component source04 remains scoped to its earlier
focused evidence; no Ident-only full gate was dispatched.
