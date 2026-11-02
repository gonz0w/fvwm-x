# Borrowed selection delivery and native producer — source checkpoint

Owner Cam; #22/#63/#71/#74; base merged PR337
`67169fce0f57d92488ecbba223d6b46556be3fee`. Four related stories implement
bounded borrowed property delivery, exact direct/INCR sink transport, source/
native-producer fences and independent fixtures. [Contract and copy proposal](../decisions/CONTENT-SELECTION-SINK-2026-11-01.md)
and [exact source/build receipts](VERIFICATION-2026-11-02-CONTENT-SELECTION-SOURCE.json)
retain scope. No installed capture, service, history default, new dependency,
crypto, persistence or UI is adopted.

The new path holds one complete bounded Xlib reply and delivers slices of at
most 4 KiB without an extra project payload allocation or INCR aggregate. It
preserves embedded NULs, validates before delivery and wipes bounded accessible
reply bytes before XFree, including refusal. This does not establish protection
before Xlib receives bytes, remove its internal/server/source-client copies or
approve the installed copy boundary. Source fixtures use explicit public bytes.
Existing aggregate/text wrappers retain their paths and configured behavior.

A pre-admitted sink cap in 0..32 KiB bounds property/INCR reads; zero permits
empty only. Native adaptation derives it from the already prepared encoded
capacity. Source owner/generation and deadlines are rechecked around delivery,
service and final completion. INCR ACK follows consumption/closed producer
access; a 4097th nonempty chunk cannot reach the sink. Exact producer prefixes
and a fresh clock after the last observer gate publication. Failed completion
retires only this ref, preserving charges for scheduled cleanup. Primary and
retirement diagnostics remain separate; cleanup-pending includes retirement.
A LIVE or invalid producer cannot be taken or retired by preflight.

## Observed source checks

Native Linux ARM64 **ten object compilations and four links pass** on the
recorded source snapshot:
five changed source/fixture files with both GCC 13.3.0 and Clang 18.1.3. C11,
O2/g, Wall/Wextra/Werror/pedantic; existing unused-parameter conventions are
excluded. Both property and new native-selection fixtures link against the
cached native selection target's unchanged dependency closure. Exact prior Q1
production-owner/policy objects are reused after source/header and object checks.
A final fixture-only owner-disconnect case then passes one strict macOS object
check; that final fixture delta has no native-target/runtime qualification yet.
Production source/header hashes remain unchanged from the native build. No warm
build/source is modified. The cached archive is a link dependency,
not a new qualification of every source inside it. Configured whole-WM/Meson
feature builds remain separate from these feature-independent checks.

The reused lab has Ubuntu 24.04.5 ARM64 userspace and the Fedora
`6.18.10-200.fc43.aarch64` AppleHV kernel. Native compile/link work takes
1.026 seconds; guarded staging/build/quiesce/export takes 2.299 seconds after
lab readiness. **Zero candidate executable invocations and zero X tests.**
The controller's historical `run` phase performs identity inspection only.
All source/reused-object/binary identities match; all 34 exported files verify.
The exact owned container is stopped before collection, and independent holder
receipts confirm container and machine stopped. No packages or disk expansion.
These timings are build qualification cost, not a WM performance improvement.

An initial preflight refused the existing cached `-pie` flag before any compiler
command. Its exact blueprint, original failure and stopped-lab receipts are
preserved. The reviewed second attempt pins that complete blueprint, including
PIE/RELRO/NOW/noexecstack and the existing private PNG SDK paths. A finite
argument grammar still rejects unknown options; fresh commands use one output
under the owned cwd and the exact hashed archive. No inherited link command is
executed or warm directory written. This was a preflight correction, not a
source/compiler test failure or a passing result from the original attempt.

Four earlier strict macOS object checks passed during development. The host
lacks Xrandr headers for FSelection, so final affected compilation uses the real
Linux target instead of a stub header or a claimed Mac runtime pass. Repository,
source/link and whitespace checks and Python AST parsing pass. No new user
syntax requires a manual change; private C contracts are documented with their
owners. Final integrated and signed-delivery review remain required.

## Authored runtime cases and queue

The new independent owner fixture authors fifteen raw-sink cases and nine
native-adapter cases: exact binary direct/INCR, empty/maximum/over-cap input,
malformed later chunks, source token/owner loss/replacement/disconnection, refused consumption,
stalled owner, cancellation/flood bounds, fixed capacity, final clock failure,
source-observer expiry, changed sensitive hint, service clear and exact cleanup.
It also authors LIVE-ref preflight refusal and unrelated-event FIFO checks.
Property cases add chunk/empty/refusal/type/format/size boundaries. The existing
private-X harness accepts an optional content probe; no separate test framework
or always-passing expected-fault test is introduced.

Separate preliminary source review caught a stale expiry sample taken before
final source observation. It now samples after that callback, immediately before
native prefix/publication gates. The deterministic fixture advances its fake
clock to expiry during the last observer; this is authored coverage, not an
executed red/green claim. Fixture owner generation tokens are synthetic; no
qualified server/XFixes observation is claimed. Only exact expected BadWindow
from its retired requestor is tolerated by the independent owner child.

Run these property/selection/adapter cases, existing affected consumers and
warranted scoped sanitizers once at the named shared clipboard transfer
checkpoint, after ordered source observation and capture/retention integration
and before first installed capture/consumer acceptance. This spans the related
source batches; do not repeat unchanged Q1 or the all-OS/install/theme matrix
per helper/PR. Other-target/current-owner, full support/install/default-X/
hardware and actual consumer acceptance stay queued honestly. No new UI exists,
so screenshots/theme comparisons are not applicable to this batch.

Before installed capture, resolve the allowed Xlib transient-copy contract and
qualify ordered owner-generation observation, including same-window reacquisition.
XGetSelectionOwner alone and a caller token cannot establish that history. The
separate lock-time capture/access decision remains pending; retained history
while locked is already fixed. #22/#63/#71/#74 remain open. PR337's completed
#63 native criteria 1/3 and exact Q1 evidence remain accepted; parked #257/#258
are unchanged.
