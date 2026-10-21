# Core record dependencies — 2026-10-20

MAINT-02 #14 / CLEAN-01 #34. Base: merged [PR #134](https://github.com/gonz0w/fvwm-x/pull/134),
`3bb05ac2e79edfac9ae4bb0cec5ed36d2baf1f7e`. Three related development-speed improvements.

## Result

Three module/icon-box/event-filter dependency improvements pass 185 affected native checks across six targets/ten profiles (19 focused retained / 166 fresh). All 5981 object pairs, ten WM executables and 249 install-output pairs preserve allocated code/data or non-ELF bytes. Ten scoped stages verify 249 identities. ARM GCC layout fanout falls 68/216/37 to 30/6/2 objects. Broader PR134 runtime evidence and inherited OpenBSD gaps remain source-scoped.

| Layout owner | ARM GCC previous dependencies | Final dependencies / dry compile tasks |
| --- | --- | --- |
| Module transport/input records | 68 through `module_list.h` | 30 through `module_records.h` |
| Icon-box records | 216 through `fvwm.h` | 6 through `icon_boxes.h` |
| Property-event filter arguments | 37 through `events.h` | 2 through `event_filter.h` |

Module transport, input, stored-list and deferred packet records retain their
existing tags, typedefs, fields and macros. The API retains its iterator state
and opaque pointer declarations. Production and test field consumers explicitly
include the complete records. Icon-box storage remains owned by style, icon
placement and window construction/retirement; general window records only borrow
its pointer. Property-event filtering keeps the same operation-local scratch
state beside its event owner, including the actual-core fixture compilation.

All 24 changed C files alter includes and blank separators only, preserving
physical line counts and every runtime statement. No configuration syntax,
packet/native ABI layout, flags, error barrier, lifetime, callback/dispatch order,
allocation or test registration changes. Public module function declarations
remain byte-identical. Exact record extraction and consumer review pass.
Ninja dependency sets and default dry compile/link tasks are observed separately;
header probe mtimes are restored exactly and every final no-op check passes.
This reduces rebuild scope; no wall-clock development or WM latency claim is made.

## Qualification

- 185 affected native checks pass across ten profiles. Linux profiles
  have 19; the session-disabled minimal profile has 18; each BSD default profile
  has 18 because the backpressure fixture is registered only on Linux.
- Nineteen Linux ARM GCC focused results are retained by exact frozen source,
  captured testlog and postchecked object/production-output hashes after the
  other independent profiles finish. The other 166 results execute fresh.
  Coverage includes module construction/queues/packets/iteration/storage/output,
  context ownership, window/style updates, retirement, scheduling and sessions.
- All 5981 old/current object pairs and ten WM executables retain
  exact allocated ELF section types/flags/sizes/content, excluding build-ID
  notes. No assertion-line exception, normalization or comparison-only compile
  is used. Every previous object target is explicitly built, including tests
  excluded from default targets. All 249 install-output
  pairs preserve code/data or exact non-ELF bytes.
- Ten fresh scoped install stages verify 249 identities,
  including native Meson RPATH adjustments. Installed versions pass. This scoped
  OpenBSD AMD64 install does not close its broader full-install/geometry gap.
- All 2675 registered names preserve ordering; compile-command
  databases remain unchanged. Assertion-enabled core fixtures retain 75 objects
  per profile; Linux retains two XRandR fault-wrapper objects per profile.
- All 2085 frozen regular input hashes/modes pass six native
  postchecks. Baseline WM hashes match PR134's final canonical identities.
  Canonical archives pass gzip EOF/CRC and payload/receipt validation.
- Portable `make verify` runs once: 166 passes / eleven native-tool capability
  skips. Local declaration/consumer review and changed-header diagnostics pass.

Focused development retained three compile-only failures: one private include
was initially inside a function, two test consumers needed explicit private includes,
and a queue fixture depended transitively on the old record exposure.
The independent queue-limit wrapper was included in the final consumer audit.
No failed test execution is relabeled as a pass. The combined BSD harness then
requested a Linux-only registration; BSD-only continuations retain completed
builds and finish the 18 registered affected checks without a matrix repeat.
All original logs and archives remain canonical evidence.

## Evidence and remaining scope

Signed author-DCO implementation: `cb515d726cba7897e60c36175d684b9209a40d46`;
tree `d70f9c751b94ed001928604854af6e7d3c135937`. Frozen archive SHA256:
`e49d9dd7c313c3601cb5ae6f99a641eb36b4f22d0c6e774bec563cce482c9200` (2085 regular inputs).
[Machine receipt](VERIFICATION-2026-10-20-CORE-RECORD-BOUNDARIES-COMBINED.json) binds source, runners,
profile results, dependency probes, object/install comparisons, portable checks
and review. Raw canonical evidence: `builddir/codex-2026-10-20/core-record-boundaries`.
Final guide/evidence upkeep changes no compiled input.

Exact allocated-code/data parity retains [PR134's broader runtime/default-X/
scoped-sanitizer evidence](VERIFICATION-2026-10-20-SESSION-CALLBACKS.md) in its
original source/profile scope. Those suites are not rerun for this declaration
and include checkpoint. Three required OpenBSD AMD64 CI/SVG/legacy-property
omissions and its broader full-install/two staged-geometry gap remain without
retries. Native-CPU Linux sanitizers, physical GPU/input/mixed-monitor hardware,
actual toolkits/compositors and release acceptance remain separate.

Performance reference PR132 now has two subsequent merged implementation
checkpoints (PR133 and PR134); this unmerged batch does not increment the count.
The next regular comparison is due after three more merged implementation
checkpoints. No runtime speed/size comparison is due or performed here.
Verified completed guest capture duplicates are retired after canonical-host
validation; source, reusable builds, install/performance stages and VM disks
remain intact. Retirement identities are recorded in the receipt. Review VNC
stays closed, Actions disabled and VESA #93 deferred. Broad owners remain open.
