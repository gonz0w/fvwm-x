# Test reliability, native setup and protocol audit — 2026-10-12

Owners: CI-01 with MONITOR-01/EWMH-01/SELECTION-01 reconciliation. Status:
**affected local checkpoint qualified; required protocol findings retained**.
No configuration migration, native code change or mandatory dependency is added.
This is not complete EWMH/ICCCM conformance or passing release CI evidence.

Four related stories are complete within this checkpoint:

1. The Script text/list fixture acknowledges prior X input through a bound F12
   event before querying over the separate MFL/module channel. XSync only waits
   for the X server. The acknowledgement runs through the existing Script binding
   dispatcher, without changing focus, text or edit callback counters. Rejected
   typing/paste checks retain exact unchanged-state assertions. A bounded optional
   backlog of 128 harmless F11 keys stresses that ordering; there is no new sleep
   or retry-until-success assertion. This removes the fixture's unacknowledged
   input assumption; it does not prove retrospectively what caused the earlier
   Clang stale snapshot. That observation and its three unchanged controls remain
   in the [prior report](VERIFICATION-2026-10-11-PROMPT-METADATA.md).
2. Linux/BSD wrappers delegate common native setup to `tests/run_native_gate.sh`.
   Feature flags, PNG backport assertion, reconfigure, two-job build, native
   binary inspection and staged standard installs have one owner. A Linux
   sanitizer profile shares setup and does not stage an install. Five contract
   cases check all wrappers, explicit tool/path precedence, paths with spaces,
   profile selection, reconfigure, failure propagation and platform refusal.
3. A new independent X client inventories all 63 advertised atoms and verifies
   property types/formats, the WM-check child/name, desktop basics, map/stack order,
   actual-pixel frame/client sums, client destruction and continued commands.
   It records required-clause findings rather than turning an inventory pass into
   a conformance claim. Every tested profile advertises one initial desktop with
   an empty `_NET_WORKAREA`: this is a reproduced EWMH 3.9 publication gap.
   Seven selected newer hints are absent from both tables and observed output.
4. A fixed [32-row reconciliation](RECONCILIATION-2026-10-12-STAGE3.md) separates
   verified monitor/legacy transfer foundations, required gaps, optional protocols
   and physical/native-CI coverage. [CI candidates](../ci/README.md) record full
   action commits, entry-point/source hashes and update guidance outside live
   workflows. BSD transitive assets and inherited container digests remain open.

## Source and binary scope

Frozen source: `builddir/codex-2026-10-12/coverage-ci/source.tgz`, SHA256
`ebd123efac3bf5096552827af28bab2fbc0a66897a36a594cc1070712da09003`.
There are 1418 files, with only seven owning paths changed from the accepted
`397c` archive: two X fixtures, the native gate contract test, three gate scripts
and the owning test-registration prefix. Another thread's shared Meson edits
are excluded. All C/header inputs match the accepted archive byte-for-byte.

The original affected freeze is SHA256
`4c850cc9c037d54c80cd5f67ed0a12057bc073d85cdb1d8040de83c62a8fd249`.
The only final delta sets the new contract test's finite deadline to 180 seconds.
Five targets passed the identical scripts under the original 30-second limit.
OpenBSD AMD64 reconfigured that registration and passed after its retained
30-second CPU-emulation timeout; only its unfinished checks were dispatched.
No valid earlier platform results were repeated.

Every profile reuses its already qualified native core, MFL and Script binaries.
Their hashes match the corresponding accepted tested identities; no unverified
installed/core binary substitution occurs. New independent fixtures execute those
binaries on actual Linux/BSD kernels and Xvfb. The six native Meson setups validate
registration and platform/dependency discovery; only the new pure contract test
runs through the new registration build. Existing broad compiler/install/default-X
results are reused because this batch changes no C, module protocol or rendering.
No new physical hardware, default-BSD-X or release sanitizer result is claimed.

## Affected results

| Target/profile | X fixture invocations | Native gate contract |
| --- | --- | --- |
| Linux ARM64 GCC, Clang, software-enabled and minimal | 12 / 12 | 5 / 5 cases |
| Linux AMD64 GCC and Clang | 6 / 6 | 5 / 5 cases |
| FreeBSD ARM64 | 3 / 3 | 5 / 5 cases |
| FreeBSD AMD64 | 3 / 3 | 5 / 5 cases |
| OpenBSD ARM64 | 3 / 3 | 5 / 5 cases |
| OpenBSD AMD64 | 3 / 3 | 5 / 5 cases after retained timeout |

Total: 30 X invocations in ten profiles, including 20 ordinary/backlog Script
runs and ten EWMH inventories; six registered contract runs contain 30 individual
cases. All standard/optional X checks keep their 90-second Script and 60-second
inventory deadlines. Shell syntax checks pass natively on all six targets.
Meson registration, runner bytes, log hashes, all source hashes/modes, native
binary identities and complete gzip/archive reads validate. Details are in
`validation-summary.json`, six evidence archives and six native Meson test logs
under the dated build directory. This is an affected checkpoint, not a new full
C release matrix.

Host checks pass the five contract cases, candidate full-SHA/permission contracts
and both YAML documents through the existing Ruby YAML parser. PyYAML is absent;
no package was installed for this check. Candidate YAML parsing is not GitHub
expression/service validation. Four official action references, reviewed entry
points, BSD configurations and failed optional Dockerfile/main.js lookups are
retained under `action-refs`; VM action entry points are `index.js` instead.

## Retained failures and limits

Early test-only Script development used brace strings where Key expects bare
key/modifier arguments and failed before module startup. First and final parse
failure artifacts remain in the development tree; an intermediate attempt reused
that development output directory. The correction precedes the frozen checkpoint.
The first EWMH inventory stopped on the work-area count assertion; its property
observations remain retained. The final inventory explicitly records that required
finding and verifies the other named contracts; it does not mark work-area
conformance as passed.

An initial dispatch used the wrong baseline date and stopped all six targets
before extraction. Its runner/logs remain. The corrected dispatch preserves all
valid five-target evidence; OpenBSD AMD64's original timeout/log and deadline-only
follow-up are retained. Evidence validation initially looked for successful unit
stderr in Meson's summary log; it now checks the collected native `testlog.json`
records, their OK status and all five actual cases. That was a validator assumption,
not a production/test failure.

Open runtime work includes correct desktop/work-area publication, manager
transition deadlines and clause evidence, output-loss managed-window recovery,
transactional scale reload, Script owner target negotiation and bounded INCR.
Optional focus/user-time, fullscreen monitor and ping/sync adoption remain later
protocol stories. Utility ports/Perl retirement, full CI assets/native runs,
physical displays/toolkits and release ownership remain separate tracker items.
