# Desktop-provider observation — IPC-02

Three features pass focused merge checks: desktop-provider capabilities and
configuration-state queries, opt-in preference invalidations, and conditional
settings snapshots. [Exact source/command receipt](VERIFICATION-2026-10-22-IPC-PROVIDER-OBSERVATION.json).
Base: merged PR158, `61b6d70ccd876c1d9dc3443f0ee8c972b5378f0b`.

The existing FvwmDesktop owner reports starting/ready configuration state,
supported preference operations and a known load error. This is explicitly
`lifecycle_scope: desktop-preferences`, not SESSION process ownership, death
detection, successful persistence or painted theme completion. Conditional
queries require exact provider identity and validate scope/token; matching
preference generations omit registry values, while mismatches refresh the scope.

One fixed watch/filter per connection forwards identity/generation invalidations
without values, action text, cache or retained history. Register then snapshot;
reconnects and raced/malformed notices require a fresh snapshot. A replacement
helper announces its new identity to unfiltered observers. Missing owners still
time out; no liveness is inferred. Queue failures disconnect only affected
consumers. Watch/unwatch is read-only and remains available at full pending
capacity. Existing ordering, slots, cancellation and fragment lifetime remain.
See the [contract](../decisions/IPC-PROVIDER-OBSERVATION-2026-10-22.md) and
[primary manual](../../doc/FvwmMFL.adoc).

## Focused validation

One reused Linux ARM64 GCC profile passes seven registered entries:
`desktop-policy` (54 cases), `mfl-request`, `mfl-stream`, `mfl-settings-xvfb`,
`mfl-request-xvfb`, `mfl-core-state-xvfb` and `mfl-stream-xvfb`. The frozen
selected test command takes 22.228 seconds; all gate compile/test commands,
including sanitizers, total 39.252 seconds. Known negative-path deadlines account
for most integration time. No VM boots, installs, broad builds or matrix repeats.

Policy checks cover strict provider/watch/conditional schemas, missing identity,
literal bounded notice grammar, initialized refusal outputs, startup state,
ready announcement, unchanged responses, invalid scope, stale refresh and no
query publication. Actual core/MFL/helper integration observes two identities,
read-only watches, exact filtering, notification payload exclusion, unwatch,
malformed-notice cache invalidation, local admission at eight pending waits and
missing-provider deadlines without a cache. Prior mutation/validation/cancel/
partial-fragment/disconnect and stream isolation controls pass. New notifications
use the same bounded queue exercised by existing stream slow-reader tests;
there is no independent notification cache or queue.

Two scoped leak-enabled Clang ASan/UBSan/LSan exercises pass: native request
policy and the expanded provider/settings actual-X fixture with instrumented
MFL and current uninstrumented GCC core. Leak detection remains enabled without
suppressions or diagnostics. The core binary remains byte-identical to PR158.
Both final compile logs contain no warnings. Emulated guest passes are separate
from native-CPU and whole-core sanitizer acceptance; existing Form exit-cleanup
gaps remain CLEAN-01 / Later and are not requalified.

The first new host policy assertion incorrectly expected keep_wallpaper in the
effects scope. The registry places it in appearance; the fixture assertion is
corrected before freeze, with the original log/hash retained. Both focused guest
development runs and final frozen checks pass. No runtime/fixture correction
follows the final freeze. Final publication changes affect only documentation,
status and reports. Portable maintenance verification retains the completed
PR157 cycle because its harness inputs are unchanged; make check/link/diff and
local source review pass here.

Both affected manuals render/format. FvwmMFL mandoc remains status 4 with
inherited mso/LINKSTYLE/section-spacing diagnostics and one added subsection
spacing warning; FvwmDesktop stays status 3 with identical inherited diagnostics.
No clean-lint or full manual/install qualification is claimed. No visible UI
changes require screenshots.

All 2188 frozen source files match in the guest before and after checks. Seven
changed runtime/test hashes and six relevant core/helper/module/policy binary
identities bind this checkpoint. The 132,129-byte canonical archive has 41 files;
gzip EOF/CRC and every payload hash/mode are verified. Exact completed guest
capture and duplicate evidence/source archives are retired after canonical
proof. Reusable sources/builds/stages/references/disks and host canonicals stay.

## Deferred checks and next boundary

PR157 remains integrated reference. PR158 is **1/3 merged**; this batch becomes
**2/3 on merge**. Affected six-target/alternate-profile qualification is due at
shared closure; full compiler/optional-feature/install/default-X and owner/
milestone/release acceptance remain at their required boundaries. No new OS API,
core/native ABI, dispatch, install/discovery or server-input behavior requires
an early platform gate. Native-CPU/hardware/toolkit/compositor checks are
separate; deferred coverage is not a pass.

IPC-02 remains open for other adapters and broader provider/state acceptance,
domain actions beyond preferences, progress/decisions/cancellation, SESSION
lifecycle adoption and shared-core generations/CFG-01 recovery. The new provider
query does not complete those contracts. Performance reference PR154 is
**4/5 merged**, **5/5 on this merge**; the next run should compare preserved
binaries without adding a broad test run or reopening performance tuning.
