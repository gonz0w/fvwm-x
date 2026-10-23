# Accumulated browser/provider platform checkpoint

Status: **passed**, covering merged PR173–175 before the next feature batch.
The integrated reference is PR175 `74c00f4e8e39607fa645bc41b0cea720486d94a1`.
[Source-bound receipt](VERIFICATION-2026-10-23-BROWSER-PROVIDER-CYCLE.json).

This closes the affected cycle for native live browser adoption, native directory
watching and read-only browser provider queries. The frozen 2315-file source
archive SHA256 is `901106036e74e93538512275e86bed06082838c4b45453f551af94211c126721`.
Guest input guards, complete gzip CRCs, captured file hashes/modes and final
commands/results verify. OpenBSD uses release 8.0 on both architectures; its
normal signed package/syspatch preparation and compiler/PNG receipts remain
explicit. Compiled caches reduce rebuild work without retaining test results
from those caches.

| Target/profile | Selected Form/MFL entries | Go policy / browser native-X |
| --- | ---: | --- |
| Linux ARM64 GCC | 7 fresh Form + 5 retained MFL | 68 cases retained; three themes retained |
| Linux AMD64 GCC + Clang | 24 fresh | 68 cases fresh; browser fixture fresh |
| FreeBSD ARM64 | 7 fresh Form + 5 retained MFL | 67 cases retained; browser fixture retained |
| FreeBSD AMD64 | 12 fresh | 67 cases fresh; browser fixture fresh |
| OpenBSD 8.0 ARM64 | 12 fresh | 67 cases fresh; browser fixture fresh |
| OpenBSD 8.0 AMD64 | 12 fresh | 67 cases fresh; browser fixture fresh |

Total: **84 selected native entries across six targets/seven profiles** (74
fresh, ten retained). Five Go packages per target plus retained Linux race
compose **472 top-case executions / 35 package outcomes**. Vet and helper/client
builds pass. Eight browser native-X runs compose four fresh target runs with
four exact source-bound PR175 runs, including Slate dark, Dune light and the
larger-font classic configuration. All browser runs capture 28 screenshots;
three scoped PR175 leak-enabled guest sanitizer exercises are retained.

Selected entries are form-list/input/command policy, form transport/input/button/
fields native-X, mfl-request/stream policy and MFL requests/settings/operations
native-X. Seven Form entries execute fresh on both ARM64 targets against their
combined current modules; the matching PR175 MFL/Go/browser/race/sanitizer evidence
is retained. A 545-file runtime/test input comparison and explicit proven C
line-wrapping derivations bind that retention to this source. No feature runtime
or fixture repair is required for the cycle. `make verify` passes once for the
shared cycle; native/guest coverage is separate from its portable checks.

The producer tests cover routing, malformed input, owner replacement, pending
wait cancellation/deadline and bounded reply assembly. Browser checks cover
current native selection/stale pages/CLI correlation alongside live draft/window
retention, watching/manual recovery and 4096-entry list/grid behavior. Theme
captures remain the unmodified [PR175 review set](VERIFICATION-2026-10-23-FILE-PROVIDER-QUERIES.md#screenshot-review).
Unchanged themes need no separate screenshot matrix to close this checkpoint.

All owned QEMU jobs shut down. OpenBSD status records normal guest/QEMU completion
and ephemeral job-key removal. Preserved host archives remain canonical; only
verified completed guest duplicates may be retired. No hosted CI, status
publisher or benchmark build is introduced.

PR175 resets the integration counter to **0/3**; the next feature merge becomes
1/3. The independent performance reference remains PR174 at **1/5** after PR175;
the next merge becomes 2/5. The due comparison was completed in PR175 and is not
repeated. Further tuning stays deferred.

This is affected integration, not full milestone or release qualification.
Full install/default-X/compiler/optional feature and owner acceptance, native
Linux CPU sanitizer execution, physical hardware/toolkits/compositors/assistive
coverage, typed external file actions/FileManager1 and protected clipboard-content
handoff retain their separate gates. No broader owner is closed by this report.
