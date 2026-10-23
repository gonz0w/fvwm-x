# Accumulated browser action platform checkpoint

Status: **passed**, closing merged PR176–178 before the next feature batch.
PR178 `e2f61e65b0e5277dedcf28fe6c602efa8b2d1762` is the new integrated
reference. [Source-bound receipt](VERIFICATION-2026-10-23-BROWSER-ACTIONS-CYCLE.json).
The 2345-file source archive SHA256 is
`f4a55caef10121f56586efa98c15034630fb236a236bfafb370dc31a338d14f6`.

The cycle composes conditional collection controls, isolated file/navigation
requests with correlated outcomes/cancellation, live reveal/target properties,
explicit hidden/watch controls and versioned native focus. Full guest input
guards and host gzip EOF/CRC/hash/mode verification bind the archives.

| Target/profile | Selected Form/MFL entries | Go policy and browser native-X |
| --- | ---: | --- |
| Linux ARM64 GCC | 12 retained | 80 cases plus race/three themes retained |
| Linux AMD64 GCC + Clang | 24 fresh | 80 cases and browser fixture fresh |
| FreeBSD ARM64 | 12 retained | 79 cases/browser fixture retained |
| FreeBSD AMD64 | 12 fresh | 79 cases; scoped UI completion passes |
| OpenBSD 8.0 ARM64 | 12 fresh | 79 cases/browser fixture fresh |
| OpenBSD 8.0 AMD64 | 12 fresh | 79 cases/browser fixture fresh |

Total: **84 native entries, six targets/seven profiles**, with 60 fresh and
24 retained. Five Go packages on each target plus retained Linux race compose
**556 top-case executions / 35 package outcomes**; vet/helper/client builds pass.
Eight normal browser runs include the three retained themes. The retained
sanitized-Form browser adds a ninth browser run, within five retained scoped
sanitizer exercises. Each accepted browser run has 39 screenshots.

The native entries cover Form list/input/command policy and transport/input/
button/fields integration, plus MFL request/stream policy and requests/settings/
operations integration. A comparison of 1493 unchanged runtime/test inputs binds
PR178's ARM64 qualification to the merged source. Its policy/race, three themes,
FreeBSD fixture and leak-enabled scoped sanitizer evidence are retained; the
new cycle does not rebuild ARM64 solely to create another checkpoint.

FreeBSD AMD64's first browser run queries selection before synthetic X input
has been consumed. Native/Go checks pass. A bounded observation now waits for
the same expected native selection before asserting or submitting a conditional
control. XSync from another client does not acknowledge the Form event loop.
The exact old/new fixture hashes are recorded. Only that browser run repeats;
no product/deadline/oracle changes or policy/matrix repeats are required. The
original failure archive is preserved. Native binary hashes match the original
profile; helper/client hashes are captured before/after the UI-only completion,
with no intervening rebuild and exact guarded product sources.

Both OpenBSD architectures use release 8.0, normal signed package updates and
syspatch preparation. Verified PR175 compiled caches include Go dependencies;
these caches retain no current test passes. Source mtimes are normalized only
for exact matching inputs. Both jobs complete normally, shut down QEMU and
remove ephemeral job keys. Linux/FreeBSD AMD64 owned guests also shut down.
Canonical host archives remain preserved. No hosted CI or status publisher is
introduced. Portable `make verify` passes once (177 passes, 14 skips); native
qualification remains separate from those portable checks.

PR178 resets the integration counter to **0/3**; the next feature merge becomes
1/3. Performance reference PR174 is **4/5**; the next feature reaches the
comparison checkpoint. No new performance claim is made by this platform cycle.
The [PR178 captures](VERIFICATION-2026-10-23-FILE-PROVIDER-TARGETS.md#captures-and-storage)
remain the representative theme review set.

This is affected integration, not full consumer/milestone/release acceptance.
Full compiler/optional/install/default-X and owner gates remain at their required
boundary, with native Linux CPU sanitizer, physical hardware/toolkit/compositor/
assistive coverage separate. File mutation/jobs, default roles/FileManager1 and
protected clipboard content/retention/storage remain open. The cycle closes
before implementation of the following feature batch.
