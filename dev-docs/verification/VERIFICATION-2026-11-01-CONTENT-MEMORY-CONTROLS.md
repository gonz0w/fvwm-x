# Content memory controls — source checkpoint

Owner Cam; #74 with #63/#71; base merged PR333
`5cd95cef80f4425aa0db143a9a7cee279af8f9dd`. This prepares one private native
comparison checkpoint. It selects no backend, allocator, persistent store,
capture service, installed capacity or dependency.

The [contract](../decisions/CONTENT-MEMORY-EVALUATION-2026-11-01.md) defines
non-forking public-buffer access controls, the separately owned direct-child
supervisor, strict unprivileged lock refusal and the capture-only oracle.
[Exact receipts](VERIFICATION-2026-11-01-CONTENT-MEMORY-CONTROLS.json) retain
source/command/compiler/artifact identities and actual check outcomes.

Four optimized (`-O2`) strict macOS ARM64 Apple Clang 21.0.0 compile/link commands
pass: policy object, base probe, libsodium 1.0.22 probe and supervisor. C11,
`-Wall -Wextra -Werror -pedantic`, minimal feature-independent config header;
existing pkg-config library only, no installation. All input hashes match before
and after. Nine pure capture-oracle tests pass in 0.002 seconds, covering native
signal differences, readiness, ordinary exit 139, wait/deadline/cancel failure,
positive cleanup, real-versus-injected/early lock refusal, privileges and strict
bounded capture parsing. These test fabricated receipts, not OS enforcement.

Separate source review corrected an unexecuted unavailable-lock output-shape
mismatch and bounded capture reads through the checked regular descriptor.
Strict compile02 and the affected oracle tests pass after those changes.

One reviewed host boundary invocation runs the base probe under the supervisor:
exit 77, confirmed wait, no signal/timeout/cancel, 78.764 ms supervised wall.
It emits the actual eight-record unavailable protocol (profile header, six
wipe-unavailable rows, summary), with zero producer bytes and completed ledger
cleanup. This confirms the corrected output shape; it is not a protected
positive control or strict lock-refusal qualification. No expected-fault mode,
libsodium candidate or supported-target memory control ran. The optimized
compilation receipt itself records zero runtime invocations.

Source review precedes the actual assembled supported-profile run;
configured source/package attestation, real unprivileged locking, region/dump
observations, CPU/RSS/cost and backend decision remain explicit requirements.
Expected-fault termination does not prove explicit wiping or cleanup. Unsupported
preparation/inspection is unavailable; zero core-file limits do not describe all
external crash collectors. No crash dumps or system-wide changes are selected.

Repository/link and whitespace checks pass. Separate source and signed-delivery
review remain required. Full WM/install/default-X/theme/native-host/release gates
remain with their owners, without a broad rerun for each private helper. No new
UI/screenshots. The original LIB-06 helper and all its observations are unchanged.

Owner defaults now include CLIPBOARD-only capture, PRIMARY synchronization off
and retaining history while locked. Logout ends session history; recognized
sensitive items stay excluded and persistence stays off. Capture/payload-access
behavior during lock, backend guarantees and installed resource budgets are not
inferred from retention. #63/#71/#74 and R1 stay open.
