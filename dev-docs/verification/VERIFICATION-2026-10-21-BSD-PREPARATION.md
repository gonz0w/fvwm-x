# BSD preparation and evidence ownership

CI-01 #17 / MAINT-02 #14 / CLEAN-01 #34. Three related tooling stories pass
one affected checkpoint after merged PR138. All 684 C/header inputs are
byte-exact to that merged checkpoint. Its complete WM/runtime qualification
retains its original scope; no WM build or full runtime matrix is repeated.

## Behavior

1. Validated source extraction explicitly preserves archive permissions inside
   the private disposable workspace. The private umask previously changed 644/
   755 inputs to 600/700 and stopped PR138's source-identity gate before tests.
   Both file and directory modes now survive a private-umask regression fixture.
   Link, special-file, special-permission, duplicate/path and size admission
   checks remain. The OpenBSD `/home` boundary and job-key ownership remain.
2. QEMU publishes a complete private `status.json` at each phase boundary using
   a new temporary file and atomic replacement. Readers see acquisition, boot,
   source-copy, preparation, execution and collection, with phase-start times.
   Final status includes a finish time. `failed_phase` retains a failed run even
   when evidence collection succeeds afterward; cleanup failures retain their
   own diagnosis without discarding an earlier failed phase. Phase status is
   not an ETA, proof of within-phase progress or power-loss durability guarantee.
3. PNG loader checks own separate `png-verify-built/` and
   `png-verify-installed/` evidence directories. Built-only then installed checks
   can run on one existing build without overwriting earlier logs. Repetition
   uses a fresh explicit `--evidence-directory`; an existing directory refuses.
   Failed checks preserve logs without a success receipt. The prefix receipt,
   transitive-loader hashes and four built/staged consumer requirements remain.

Developer recipes and the architecture map describe the output paths and live
snapshot semantics. These helpers are not installed WM/configuration commands;
no user configuration migration or runtime manual option is introduced.

## Verification and source identity

Frozen tooling commit: `d2db98fc782cda284d0eabeb144dddcfc0456a05`.
Archive SHA-256: `0d1c1d127a31b1ec0ca2a312bcffe8e389eb54a6d8def6f42a63506d3792871b`.
Its 2,097 regular inputs bind exactly to that Git tree. Final evidence/navigation
upkeep does not change the tested helpers. The [machine receipt](VERIFICATION-2026-10-21-BSD-PREPARATION.json)
retains per-target payload hashes, commands, source maps and phase/cleanup state.

| Scope | Result |
| --- | --- |
| macOS portable checkpoint, run once | 188 registered; 177 pass / 11 native-capability skips |
| Linux ARM64/AMD64, FreeBSD ARM64/AMD64, OpenBSD 8.0 ARM64/AMD64 | 47 native policy cases per target; 282 total |
| Real OpenBSD disposable runner | Two fixed-image jobs, with complete source hashes/modes before/after checks |
| Actual PNG/core/SVG loader checks | Built-only, installed and explicit repeat on each OpenBSD architecture; 20 loader observations across six successful helper invocations |
| Original loader evidence | Three original log/receipt files per OpenBSD job remain byte-exact |
| Restored WM build/install identities | All 807 original ELF outputs and 85 staged regular files per OpenBSD target remain exact |
| Live snapshot reader | Complete JSON at every observed read; required phase transitions observed in both jobs |
| Teardown | QEMU exits zero and removes both ephemeral key files on both targets |

The two affected policy suites run against isolated copies of all eight required
helper/test/pin inputs on existing Linux/FreeBSD targets. They include actual
subprocess limits/cleanup, atomic replacement failure, private-umask extraction,
loader refusal and proof coexistence/repetition. Both OpenBSD jobs execute the
same suites against the full frozen source. FreeBSD source-copy/status behavior
is covered natively by those controls; this checkpoint does not claim a fresh
FreeBSD pinned-image boot. Those QEMU profile/firmware inputs are unchanged.

Both fresh OpenBSD preparations restore PR138's canonical, fully verified native
build/stage archives into the same paths. Before use, every regular file's hash,
size and permission bits and the compiler identity match. The qualified PNG
source pin, all twelve installed-file identities, retained 36/36 upstream results
and a fresh native probe are checked. This is source-bound operational reuse,
not a new cache API or dependency qualification inferred from package names.
Host comparison also proves original ELF/stage bytes survive the new checks.
No 270-entry native WM suite is rerun.

After all AMD64 checks pass and evidence is written, its run script deliberately
returns 23. The runner must collect evidence, preserve `run_status=23` and
`failed_phase=run`, and shut down cleanly; its final overall status must be failed.
ARM64 is the successful-run control. This expected negative case is not hidden
as a successful production run or described as a WM failure.

The existing PR138 failures provide the original source-mode and PNG-log-collision
findings. One initial periodic-benchmark guard confuses the old receipt's source
archive field with the native evidence archive and refuses before measurement.
The corrected guard checks the published canonical evidence identity plus both
explicit binary hashes; no measurement or native test is repeated. A host-only
aggregate parser initially expects eight portable invocations rather than seven;
correcting that bookkeeping yields the actual 188/177/11 totals. Operational
Python staging emits a tar-filter deprecation warning; all pinned inputs and
payload identities still verify. No deadline, assertion or acceptance target is
relaxed to obtain passing evidence.

The [periodic comparison](PERFORMANCE-2026-10-21.md) is due after the five merged
implementation checkpoints PR133–136/138. It uses existing PR132/138 binaries,
without rebuilds or a correctness matrix. It records timing variation, size and
source changes independently of this tooling checkpoint.

## Review, cleanup and limits

Local review checks permission boundaries, snapshot interruption/failure paths,
key teardown, consumer mode selection, existing-output refusal and caller/recipe
coverage. Full gzip integrity, safe bounded member paths/types, complete captured
payload hashes and frozen-source maps validate before evidence is accepted.
Canonical captures live under
`builddir/codex-2026-10-21/bsd-preparation-evidence/` and are identified in the
machine receipt; old WM qualification remains in PR138's source-bound report.

Following the user's storage direction, exact file/hash/mode/size and path guards
retire 168349 bytes of completed guest policy/benchmark duplicate evidence.
Source inputs, reusable builds, stages, PR132/138 performance references, VM disks
and canonical host evidence remain. Both new disposable guests are shut down;
no authentication persistence, scheduled work or VNC session is introduced.

This checkpoint changes no production C/header, WM build option, X state/order,
package-signature rule or fixed release input. Native-CPU sanitizer, physical
hardware, toolkit/compositor and release acceptance remain separate. Broader
CI/maintenance/cleanup owners stay open. Future dependency/build caching remains
a separate candidate, not an adopted feature of this batch.
