# Binding scope inspection — IPC-02

Three features pass focused merge checks: regular/fallback menu records,
normalized placement finish controls, and context/modifier interpretation with
policy-sensitive generations. [Exact source/command receipt](VERIFICATION-2026-10-22-IPC-BINDING-SCOPES.json).
Base: merged PR157, `3f4f7052151f110ba6f398a3451e727af4ecf8c4`.

The core remains the only binding authority. Menu lists are borrowed only during
synchronous serialization; fallback defaults remain distinct from regular
records. Placement reports its two existing finish masks instead of invented
binding history. The policy snapshot names raw context/X modifier masks and
reports actual ignored modifiers. All scopes share conservative incarnation/
revision checks; a real IgnoreModifiers change invalidates earlier observations.
Omitted scope preserves the previous global result. These are observations,
not a per-event winner resolver or an action execution interface.

No configuration syntax, packet ABI, command ordering, install/discovery, OS API
or server-dependent input behavior changes. Existing strict request admission,
read-only mode, pending limits, deadline/cancellation and bounded directed reply
assembly remain in use. No early BSD/install/default-X gate is required for
this scope. See the [selection](../decisions/IPC-BINDING-SCOPES-2026-10-22.md)
and [manual](../../doc/FvwmMFL.adoc) for exact contracts.

## Focused evidence

One reused Linux ARM64 GCC profile passes seven registered entries:
`core-query-json`, `mfl-request`, `mfl-stream`, `mfl-core-state-xvfb`,
`mfl-request-xvfb`, `mfl-settings-xvfb` and `mfl-stream-xvfb`. The selected run
takes 21.993 seconds; incremental compile plus scoped sanitizer work brings
the first frozen gate commands to 23.738 seconds. A test-only review follow-up
adds 0.887 seconds of compile/check commands. Existing five-second negative-path
waits account for most fixture time. No VM boots, full builds, installs or broad
matrix runs occur for this batch.

Policy coverage checks exact scope-to-core routing, valid paged menu scopes,
malformed/duplicate/unknown scopes, wrong types, scalar paging refusal, stale
conditional snapshots, empty menu lists, raw values and allocation-failure
cleanup at every allocation in all five query serializers. Actual-X evidence
observes seven fallback records separately from a regular menu override, verifies
removing the override preserves defaults, checks stored literal text without
execution, and compares placement masks to known configuration changes. Modifier
policy changes invalidate all domains; repeated identical policy is stable.
Prior global binding/topology/workspace and request/settings/stream controls pass.

Three leak-enabled Clang ASan/UBSan/LSan exercises pass: query policy, request
policy and the expanded actual-X core-state fixture with instrumented MFL and
current uninstrumented GCC core. There are no suppressions or sanitizer
diagnostics. These are emulated guest results, not native-CPU qualification or
instrumented whole-core execution. Existing Form exit-cleanup gaps remain with
CLEAN-01 / Later and are not requalified.

The first two development attempts fail the same new menu assertion: the fixture
sent an unescaped variable through ordinary legacy command admission, which
correctly expanded it before storage. The final fixture escapes that variable,
then verifies the stored literal survives query presentation. An isolated affected
pass and the final frozen checks pass. Original logs/hashes remain canonical.
Development dependency rebuilds also surface inherited binding-debug, switch
fallthrough and placement-initializer compiler warnings; changed query/MFL code
adds no diagnosed warning. The final incremental compile logs contain none.

The changed manual renders and formats; mandoc stays status 4 with identical
inherited mso/LINKSTYLE/section-spacing diagnostics. Repository/link/diff checks
and local source review pass. No visible UI changes or screenshots are claimed.
Portable harness verification is retained at the completed PR157 cycle; its
inputs are unchanged, so it is not repeated here.

Review simplifies the allocation-failure test's selection logic and wraps test
vectors to the C line-width convention. Only the two changed policy entries
rerun on GCC and the scoped sanitizer profile. Five unchanged focused entries
and one sanitizer X exercise are retained by exact product/fixture inputs and
core/GCC-MFL/ASan-MFL identities; no full integration repeat. The final composed
counts remain seven focused passes and three scoped sanitizer exercises.

All 2185 first frozen source files match in the guest before/after the original
checks; final policy checks guard that same tree with two exact test-source
overrides. The final snapshot contains 2187 files including reports. Fourteen
changed runtime/test hashes and eight binary identities are recorded. Final
publication edits affect only documentation/status/report files. The 131,913-byte
canonical evidence archive has 41 files; gzip EOF/CRC, all payload hashes and
modes are verified. A second 223,730-byte archive binds 17 final-policy files,
with the same complete payload/mode validation. Only exact completed guest captures and duplicate
evidence/source archives are retired after canonical checks. Sources, reusable
builds, install stages, performance references, VM disks and host canonicals stay.

## Deferred coverage and cadence

PR157 is the new integrated reference: **0/3 merged**, **1/3 after this PR**.
Affected six-target/alternate-profile checks are due at shared cycle closure.
Full compiler/optional-feature/install/default-X and owner/milestone/release
acceptance remain at their required boundary. Native-CPU/hardware/toolkit/
compositor checks are separate. Deferred gates are not passes.

IPC-02 remains open for other providers and broader state acceptance, typed
actions beyond settings, domain progress/decisions/cancellation, service lifecycle
adoption and observable shared-core generations/CFG-01 recovery. Performance
reference PR154 is **3/5 merged**, **4/5 after this PR**; no comparison is due.
