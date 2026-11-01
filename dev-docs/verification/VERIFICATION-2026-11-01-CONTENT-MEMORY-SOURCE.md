# Content memory evaluation — source checkpoint

Owner: Cam; #74/#63. Base PR328 `42e00b03f2fde83b01cf99391597b31fe8e8a1c9`.
This records source preparation and compilation, not protected-memory adoption.
The [finite contract](../decisions/CONTENT-MEMORY-EVALUATION-2026-11-01.md)
defines the two candidates, failure semantics, resource models and remaining
copy/protection/dependency evidence.

## Actual checks

- Repository foundation/link and working/staged whitespace checks pass at the
  recorded checkpoints; native product acceptance remains separate.
- Native macOS ARM64 Apple Clang 21.0.0 strict C11 compilation/link passes:
  production policy object, authored test object and link, then baseline and
  libsodium synthetic-probe builds. `-Wall -Wextra -Werror -pedantic`, assertions
  enabled, minimal feature-independent `config.h`, `_DEFAULT_SOURCE=1`.
- Existing host pkg-config reports libsodium 1.0.22; no package was installed.
  Its flags are captured in the owner receipt. This is a compile dependency
  observation, not complete runtime/transitive/install cost or source attestation.
- Compile02 guards all four affected input hashes before and after, and records
  actual argv/exits, compiler/platform and three output binary identities.
- Separate source review found one P3 fixture-contract issue: six fresh owner
  rows reused each repetition's epoch. A bounded ordinal now assigns unique
  epochs across all size/repetition rows and reports them. The reviewer confirms
  the fix and finds no remaining source blocker; no observed ABA is claimed.

Private owner receipt:
`builddir/codex-2026-11-01/r1-protected-content/compile02/result.json`.
Compile01 predates the reviewed epoch correction and final formatting; it is
historical. The final signed delivery review remains required before merge.

## Authored and queued

Production-linked content cases add checked physical page-cost boundaries,
per-item/aggregate mapped/locked caps, overlapping generation retention, exact
live lease admission and expired/retired/foreign/released ticket refusal.
The two probes author public synthetic prepare/access/retire/cleanup and
injected refusal observations with the actual content ledger. They are not
installed or registered as routine runtime tests. A zero exit is completed
observation/accounting, not proof all preparations or protections succeeded.

No new policy/probe executable, native-X, sanitizer, theme, install or whole
support matrix ran for this source checkpoint. Native Linux/BSD and exact
configured Meson qualification remain pending. No new UI/screenshots.
Candidate memory/CPU/latency/RSS, package closure and native protection failure
comparison are unexecuted; no library/base-system adopt decision is made.
Unobservable library free failures, OpenBSD configured mapping attestation and
stack/register/transport/decoder copies remain explicit. No confidentiality
claim against other display clients or privileged process/memory access.

Run the assembled content/memory/manager runtime gate once after this source
group, preserving each target/profile result. Full release/install/default-X
qualification stays with R1 acceptance. Existing Script Fontconfig clean-exit
and Q67 infrastructure/results are separate and remain failed/pending until
their own finite controls complete. No historical pass qualifies changed code.
Periodic core performance is 3/5 after PR328, 4/5 after this implementation merges;
candidate comparison is a different measurement and does not reset that count.

## Routing and review boundary

Lead implementation uses the recorded adequate-lead fallback after two
implementer agents reached quota; no retry or model substitution. Selected
workflow inventory, analyst design/Q67 planning and separate reviewer source
inspection are bounded read-only work. Requested Temper role/model settings
remain in policy; actual backend model/effort metadata is unavailable/unverified.
Agent review is not independent human assurance or a GitHub approving review.
