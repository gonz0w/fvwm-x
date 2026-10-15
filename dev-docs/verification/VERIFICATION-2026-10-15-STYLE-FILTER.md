# Distinct style-ID filter — 2026-10-15

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38).
Status: all affected checks pass; [PR #100](https://github.com/gonz0w/fvwm-x/pull/100)
ready for review. Four broader OpenBSD AMD64 failures and its fresh-install
gap remain open; full platform qualification is incomplete.
[Baseline](VERIFICATION-2026-10-15-STYLE-PROFILE.md),
[ordering contract](../decisions/WINDOW-UPDATE-BARRIERS-2026-10-15.md),
[machine receipt](VERIFICATION-2026-10-15-STYLE-FILTER-COMBINED.json).
PR #99 is merged. The signed runtime implementation remained unchanged throughout
qualification. VESA cause investigation #93 remains deferred.

## Three-story change

1. `_simplify_style_list` builds a fixed 4-KiB operation-owned summary before
   its reverse traversal. Two 8192-slot bit domains record seen
   and repeated hash slots. An ID cannot repeat when either domain's repeated
   bit is absent, so that rule skips its whole backward interference/merge
   scan. Empty/singleton lists return immediately. All merge decisions and
   backward scans for possible repeats use the existing code unchanged.
2. `style_id_filter.c` hashes the same active selector flags/fields used by
   style equality, including compatibility names and all 64 window-ID bits.
   Inactive data and pointer addresses never distinguish equivalent IDs.
   The pure native policy test covers the empty ID and all 63 nonempty flag
   combinations with independent string storage and varied inactive fields,
   caller-string mutation, 32768 repeated IDs and saturated fallback.
3. The actual-core probe runs both normal filtering and a link wrapper that
   always answers "may repeat", retaining the original traversal as a control.
   Both paths check wildcard/exact precedence, resource/class/name/icon
   selectors, WindowID, title-format ownership, both merge directions,
   retirement and repeated passes, rule deletion and changed lookup identities.
   Existing frame/EWMH geometry, pending-style/clean-repeat and independent
   outer-loop/death-row checks remain enabled.

## Correctness and ownership

The filter is zero initialized and populated from the complete current list.
It owns only bits; it allocates nothing and retains no rule/string pointers.
Equivalent IDs always hash alike. Hash collisions or saturated bits can retain
an unnecessary scan, never suppress a required merge. The existing equality
check remains authoritative whenever a scan runs. No match result, style flags,
resource pointer or window plan is cached.

During simplification, merges/removals affect equal IDs only. Moving `cur` to
an older equal rule or replacing its contents therefore leaves its hash class
unchanged. The original summary can overstate repetition after removals, which
is safe. A later pass builds a fresh summary. There is no callback/X operation
inside the filter, and it introduces no lifetime across a command/event turn.

The unchanged scan still accumulates interference from every relevant rule
when an ID can repeat. Skipping the scan of a unique ID does not remove that
rule from another repeated ID's interference scan. Ordered matching, invalidation,
style-list priority, command/module visibility, server/pointer grabs, focus,
geometry/resource generation and X synchronization barriers stay with their
existing owners. No configuration migration, dependency, allocation limit or
new user option is introduced.

The filter is a conservative optimization, not a hard latency or complexity
bound. Repeated IDs, collisions, saturation and adversarial rule sets can retain
quadratic work. Hashing costs two linear passes over active ID bytes. Lookup
and bulk-update matching are outside this change; their baseline counts remain
controls, not claimed improvements. Wider indexes/grab redesign remain open.

## Measurements and source

For 64/128/256 distinct compatibility rules, backward ID string comparisons
fall from 2016/8128/32640 in the preserved-traversal control to zero. The fixed
summary's preparation and each reverse lookup remain part of measured elapsed
work. The reference path includes summary construction too; it is a behavioral
control with the original scan, not byte-identical predecessor timing. The
[PR #99 baseline](VERIFICATION-2026-10-15-STYLE-PROFILE.md) retains the original
measurements separately. No elapsed ratio is an acceptance threshold or a
physical responsiveness claim. Instrumented/emulated/ASan samples are not
native-CPU performance or release sanitizer evidence.

Runtime input archive: `b3c2bea39ec1b291710431c758ac7faeee6e1133501711de0ccbd32ced5a8fc0`,
Every regular-file hash/mode matches signed runtime commit
`4a65f590b5fbc1e92d243cf97edf253eab28b07c` (tree
`d3b665bff519a511c77d8920841737b4ba9688d2`). There are
1957 regular files, based on merged `c8cde63e3` plus eight affected production/
build/test paths recorded in the receipt. Every hash/mode is checked before
configure/build and after the combined run. Changed bytes receive fresh mtimes
in owner-controlled incremental trees. Later report/tracker upkeep is outside
the runtime archive. Unrelated shared-root edits and predecessor source,
builds, installations, VM disks and logs are preserved.

## Qualification and preserved attempts

Focused Linux ARM64 GCC passes the policy unit, actual-core paired profiling
and existing window-update regression. The eleven workload shapes each run
once with filtering and once with rejection disabled. The new fixture's first
structured-selector assertion expected stripped TitleFormat quotes; the parser
preserves the supplied text. The test command now supplies unquoted format
text. The original failing logs/input and the unchanged passing policy/update
checks are preserved. A generated combined runner had an indentation error;
all six invocations stopped before any native configure/build/test. Its source
and dispatch logs remain; the corrected runner was syntax checked before launch.

All three affected entries (`style-id-filter`, `style-profile-xvfb` and
`window-update-xvfb`) pass every one of the ten compiler/feature profiles.
The full runs execute 1968 standard and 431 optional entries. Originally,
1961 standard entries pass and seven fail; all optional entries pass. After
one serialized unchanged control invocation per affected target, the latest
case composition contains 1964 standard passes and four failures. Exactly the
seven originally failed records are replaced by their control observations;
every other original record stays intact. The original runs remain failed.
Source, binaries, commands and deadlines are identical between each original
and its control; the receipt retains both.

| Profile | Original full pass/fail | Single control pass/fail | Latest pass/fail |
| --- | --- | --- | --- |
| freebsd-amd64/default | 239/1 | 1/0 | 240/0 |
| freebsd-arm64/default | 240/0 | 0/0 | 240/0 |
| fvwm-x-linux-arm64-svg/clang | 252/0 | 0/0 | 252/0 |
| fvwm-x-linux-arm64-svg/gcc | 252/0 | 0/0 | 252/0 |
| fvwm-x-linux-arm64-svg/minimal | 200/0 | 0/0 | 200/0 |
| fvwm-x-linux-arm64-svg/software | 231/0 | 0/0 | 231/0 |
| linux-amd64/clang | 252/0 | 0/0 | 252/0 |
| linux-amd64/gcc | 252/0 | 0/0 | 252/0 |
| openbsd-amd64/default | 234/6 | 2/4 | 236/4 |
| openbsd-arm64/default | 240/0 | 0/0 | 240/0 |

The controls are observations, not determinations of timing root cause:

- FreeBSD AMD64: `ci-asset-contract` fails its unchanged two-second negative
  worker deadline originally and passes the single isolated control. Only
  after that passing composition is its fresh stage produced.
- OpenBSD AMD64: `module-selection-xvfb` (Script/Form ten-second readiness)
  and `interaction-frame-sync-xvfb` (90-second outer timeout) fail originally
  and pass the single control. Four cases still fail: `desktop-baseline-xvfb`
  (MFL connection refused originally, MFL echo deadline in the control),
  `live-x-harness-self-test` (one-second stubborn-child readiness),
  `verification-profile` (15-second mock-entry subprocess deadlines), and
  `ci-asset-contract` (two-second negative-worker deadline). Their sources are
  unchanged. No further retries, deadline increases or VM changes are made.

These broad failures remain with VERIFY-01 #5 / CI-01 #17. OpenBSD AMD64's fresh
installation is prevented, and its staged-WM geometry checks are not performed.
The new compiled core fixtures pass on its authenticated default X server,
using 25 retained worker/module/catalog dependencies verified byte-identical
against the current build. Its predecessor staged WM is not claimed to be the
new core. Full platform qualification remains incomplete.

Nine profiles have fresh installations: 135 code and 88 catalog identities
validate against tested binaries and Meson's authoritative install metadata,
including expected rpath processing. Only the nine staged WM executables
change; the other 214 identities match the predecessor. All ten profiles build
and test new WM binaries. Staged version checks pass on all nine fresh stages.
Eighteen staged geometry invocations at 100%/150% cover 126 named pointer,
configure-feedback and resize scenarios, using actual staged WM/MFL bytes.
The four BSD targets pass both default-server core fixtures; three also pass
fresh staged geometry. This yields 14 fixture invocations across seven
owned authenticated server runs, seven unauthorized-client refusals, seven
clean exits and zero forced cleanup. Normal profiles, default-X probes and
scoped sanitizer yield 330 paired workload records and 120 independent
window-update observations. There are eleven policy unit invocations.

Linux ARM64 Clang ASan/UBSan passes the three affected entries. The unit retains
leak checking; the two `_exit` core fixtures disable it. No general leak or
native-CPU release sanitizer qualification is claimed. Emulated Linux/BSD
results remain distinct from native-CPU and physical-hardware acceptance.
The prior Linux AMD64 GCC topology timing failure remains historical in the
[parent report](VERIFICATION-2026-10-15-WINDOW-UPDATES.md); the current full run
passes that test with its existing limit. The OpenBSD negative-worker failure
persists. A passing observation does not establish a prior failure's cause.
Read-only resource observations show no guest paging or exhausted owner-build
disk space; they do not establish why the OpenBSD deadlines fail. Other chats'
VMs, builds, disks and evidence are preserved.

Portable `make verify` passes 166 checks with eleven explicit native-Meson skips.
Repository links, whitespace and source review pass. Review checks hash/equality
consistency, retained interference rules, unchanged merge order, bounded bit
indexes/shifts and mutation/ownership as described above. OpenBSD retains legacy
linker warnings from unchanged string helpers; no new compiler diagnostic is
introduced. Native-CPU Linux sanitizer, physical input/GPU/toolkit and release
acceptance remain separate. No VESA cause investigation is included.

Local owner evidence: `builddir/codex-2026-10-15/style-distinct-id-filter/`.

```sh
meson compile -C builddir -j 2
meson test -C builddir --no-rebuild --num-processes 1 --print-errorlogs
```

For focused checks, select `style-id-filter`, `style-profile-xvfb` and
`window-update-xvfb`. The policy unit needs no X server. The live probe accepts
`--display` for an owned authenticated display and writes raw counts/times plus
core/server logs. All review/tests/lint/validation run locally; Actions stays off.
