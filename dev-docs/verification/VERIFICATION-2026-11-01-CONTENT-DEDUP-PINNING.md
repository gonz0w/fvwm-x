# Native content comparison and pinning — shared Linux Q1

Owner Cam; #63/#71/#74; base merged PR336
`e44e156e436cbcaf0a40594bab7c37f643dd02da`. Four related stories extend the
existing native owner: numeric description/pinning, bounded original-byte
comparison, explicit compatible deduplication commit, and cursor/failure/replay
coverage. [Contract](../decisions/CONTENT-PAYLOAD-OWNER-2026-11-01.md#comparison-pinning-and-deduplication)
and [source/profile-bound observations](VERIFICATION-2026-11-01-CONTENT-DEDUP-PINNING.json)
record the boundary. No installed clipboard service, capture/transport, UI,
Go/Lua payload, capacity default, crypto or persistence is adopted.

Comparison matches representation ID sets/kinds/actual encoded lengths,
independent of order, then compares original bytes directly in steps of at most
4 KiB. Both exact leases and read-only scopes close before progress commits;
no temporary payload copy or retained lease exists between calls. Current LIVE
refs and strict hint/deadline compatibility are rechecked. Clear, replacement,
expiry and failed access invalidate later work. Native cursors are trusted C
continuations, not serialized/authenticated proofs. Commit refuses a pinned drop,
preserves the survivor's pin/expiry and retains drop charges until cleanup.

## Observed checks

The finite shared Linux Q1 covers the five PR336 owner stories plus this batch.
All **four normal/scoped-sanitizer executables and four production controls**
pass in one run. The existing stopped Linux ARM64 lab was reused: Ubuntu 24.04.5
userspace, GCC 13.3.0, Clang 18.1.3/compiler-rt, Fedora
`6.18.10-200.fc43.aarch64` kernel in AppleHV. This is native Linux guest C
execution, not physical desktop qualification or another target's evidence.
Feature-independent helpers compile directly from the frozen source; configured
Meson registration is inspected, not represented as a full configured WM build.

| Check | Observed result |
| --- | --- |
| Metadata policy, GCC normal | Exit 0, confirmed wait, no diagnostics |
| Native owner including comparison/pin cases, GCC normal | Exit 0, confirmed wait, no diagnostics |
| Metadata policy, Clang ASan/UBSan | Exit 0, leak detection enabled, no diagnostics |
| Native owner, Clang ASan/UBSan | Exit 0, leak detection enabled, no diagnostics |
| Production owner read/write | Public binary bytes published/read; scopes closed, all charges explicitly cleaned |
| Production owner no-access write | Actual native SIGSEGV 11, confirmed wait; numeric exit 139 alone is insufficient |
| Production owner read-only write | Actual native SIGSEGV 11 inside the scoped reader, confirmed wait |
| Production owner zero-lock limit | Matched existing nonroot positive; verified soft/hard limits zero; genuine lock refusal errno 1; explicit cleanup |

Production controls use UID/EUID 65534, checked absence of effective Linux
IPC-lock capability, 4-KiB pages and an 8-MiB ordinary lock limit. One reserved
representation charges 12,288 mapped/4,096 locked model bytes. On verified
zero-limit refusal, no bytes were produced, the region was closed, work/leases
were zero and the item remained retired/charged until cleanup. Fault controls
terminate at the expected access; their resources end through process teardown,
not an explicit-wipe claim. Sanitizer interception of mlock does not establish
OS locking; those checks use the unsanitized production variant separately.
Process guards and regional restrictions remain distinct from observed universal
dump exclusion or protection against privileged inspection.

Eleven small native compile/link commands take 2.159 seconds; the eight-leaf
runtime phase takes 0.098 seconds. Build/run/quiesce/collection take 3.610 seconds
once the existing lab is ready. These are qualification timings, not a WM speed
benchmark. Each non-forking leaf has a five-second deadline, one-second TERM
grace and five-second kill/reap bound; the whole native phase is bounded to
30 seconds. No timeouts, skips, unavailable controls, sanitizer diagnostics or
native failures occurred. Source and binary identities match around every leaf.
The exact owned container was stopped/verified before collection; independent
holder receipts confirm both container and machine stopped. No packages, disk
expansion, credentials or shared build changes were needed.

Strict optimized macOS ARM64 affected compilation/link also passes with C11,
O2, `-Wall -Wextra -Werror -pedantic`, reusing matching policy objects. This is
source checking, not Linux/BSD runtime acceptance. Six pure captured-observation
oracle tests pass in 0.001 seconds, including forged numeric exits, missing
readiness/cleanup, mismatched nonroot positives and unconfirmed/timed waits.
Repository/source-link and whitespace checks pass; integrated and signed-delivery
review remain required before merge.

## Review and limits

Separate source review found an understated native fixture lock prerequisite:
the equal-comparison fixture can hold four rounded 8,193-byte regions. Preflight
now derives peak rounded storage for the actual page size instead of assuming
eight pages suffice everywhere. This was corrected before runtime; no native
red/green failure is claimed. Dispatch review also corrected repeated-cancel
cleanup and failure-collection ordering before execution. Failed/uncertain child
termination must quiesce the owned container before collecting from its stopped
filesystem. Prepare failures retain compiler logs and cannot qualify.

No new UI exists, so themes/screenshots are not applicable. Other architectures,
BSD execution of this actual owner, full support/install/default-X/hardware,
real-X selection transfer and installed history acceptance remain queued at
their owning feature/release boundaries. The earlier six-target allocator
comparison retains its exact source scope; it does not qualify this owner by
inference. #63/#71/#74 remain open. Parked #257/#258 are unchanged.

Session-only history, recognized sensitive exclusion, persistence off,
CLIPBOARD-only capture, PRIMARY sync off and retained history while locked remain
fixed. Lock-time capture/access and allowed transport copies remain separate.
Existing Xlib/property/INCR heap copies are not claimed protected by this owner.
