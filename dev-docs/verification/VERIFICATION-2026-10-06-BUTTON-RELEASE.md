# INPUT-01 bounded common button-release waits — 2026-10-06

Status: locally qualified on Linux/FreeBSD/OpenBSD AMD64/ARM64. Required
physical and native Linux release-CI checks remain separate and unrun.

The user approved bounded audited servicing and 20-ms pointer/wait-cursor timing
in the [decision](../decisions/BUTTON-RELEASE-SERVICE-PROPOSAL.md). No additional
ordering exception or asynchronous image-owner publication is introduced.

## Related implementation batch

| Story | Result |
| --- | --- |
| Release-state policy | Validate button 1–5 before shift arithmetic. Invalid numbers retain previous physical state; keyboard modifiers never keep a released button waiting. Native policy tests cover zero, oversized numbers, multiple buttons, modifiers and finite monotonic deadline boundaries/failure/regression. |
| Readiness and timing | Replace microsecond spinning with the existing audited command/timer/image-transport pump and finite waits. Pointer fallback runs at most once per 20 ms; cursor delay uses monotonic elapsed time. A 32-event budget ensures continuous selected input cannot starve service. Existing grabs, selected keys/releases/exposes and termination are retained. |
| Ordering and common owners | Enclosing functions and active producer tails remain behind physical release. Root/title/focus/menu/paging callers reuse the same helper; no new dispatcher, topology/lifecycle reentry, image starts or owner commits. |
| Integration acceptance | Move/Resize Escape, ordinary release, continuous motion, unsafe producers/timers, synchronous receivers, common callers, Expose traffic, pending image completion/deadline and termination have focused nested-X coverage. |

The existing monotonic event clock is shared with the release wait. This is a
local helper rename, not a monitor scheduling change. Caller window records stay
borrowed; the audited pump cannot run arbitrary mutating commands. Broader target
lifetime/reentrancy work remains open under INPUT-01.

## Focused source and results

Evidence: `builddir/codex-2026-10-06/button-release-service/` and the isolated
Linux source `/tmp/fvwm-button-release-source` in the existing development
container. The complete affected build/policy and original/new servicing checks
pass: interaction policy, ordinary held Move/Resize, Escape-to-release Move/Resize,
and common-caller integration. The common fixture additionally covers worker
completion/reap while owners remain unpublished, worker deadline/reap with final
fallback only after unwind, and SIGTERM while the physical button remains held.

Two fixture assumptions were corrected and their failures retained: focus clicks
must explicitly be swallowed to enter that caller's release wait; final image
fallback/refusal belongs after outer unwind, while deadline/reap is transport-only.
Test-target registration failures were corrected before tests ran. These failures
did not require production behavior changes.

The combined frozen source contains 1262 files, SHA-256
`7b34d93a57f03f7e072e82c0af7a1750290808e9f2ace89e85baf4d63d9da6af`.
`source-button-release-final.tgz` extends the locally qualified image activation
source with eight owned source/manual/test changes. It excludes newer unrelated
work in the shared checkout. Source sync verifies the complete prior baseline and
updates timestamps only for changed files before incremental builds. Prior image
qualification stays archived separately.

## Combined acceptance

All eight standard compiler profiles pass 924 registered entries: Linux
ARM64/AMD64 GCC and Clang each 121, four BSD Clang profiles each 110.
Each profile's 15 staged code identities match the tested binaries after only
Meson's declared RPATH changes (120 identities). Every registered test name,
result, metadata/source hash and installed byte identity is checked in
`qualification-summary.json`; all seven source archives match 1262 hashes/modes,
and every evidence payload has been read to EOF to validate archive integrity.

The four installed default BSD X servers each pass five driver invocations
(20 total): held Move/Resize, Escape-to-final-wait Move/Resize and the common
root/title/focus/menu/paging/image/deadline/shutdown driver. FreeBSD uses scfb,
OpenBSD ARM64 wsfb with the approved root fallback, OpenBSD AMD64 vesa. An initial
OpenBSD ARM64 default-server run lacked the qualified private libpng runtime
path and failed before startup; the failed environment attempt is archived,
the path was supplied and the unchanged source rerun passes.

Optional XRender-disabled software/Xcursor and SVG-disabled profiles pass
120/102 full entries. Scoped local Clang ASan/UBSan passes one leak-enabled pure
policy entry and four actual-core entries with leak detection disabled (held
interactions, final release servicing, common callers and cancellation). No
full-process leak-cleanliness or native Linux CI claim is made.

The separately approved active viewport batch changes later source and has
its own [report](VERIFICATION-2026-10-06-VIEWPORT-SERVICE.md). This acceptance
applies only to the frozen common-release source, not all dirty checkout files.
Native Linux release sanitizer CI and physical hardware remain separate unrun
gates. The initial hold/CPU sample is functional evidence, not a performance claim.
