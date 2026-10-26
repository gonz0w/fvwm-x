# M5 unused IconMan private interfaces — 2026-10-25

Owner: CLEAN-01 #34. Base merged PR250:
`ba52057e7364ee26987477572d18ce8b08282d5f`.
**Focused checks passed; broader owner/release qualification remains pending.**

Remove exactly two unused function bodies and seven private declarations across
five IconMan files. `unmap_manager` and `set_win_state` have no callers; their
bodies and declarations retire. Five declaration-only orphans retire too:
`which_box`, `draw_added_icon`, `draw_deleted_icon`, `expand_weighted_sorts` and
`update_window_stuff`. There are no replacement functions or new tests.

The active source/configuration/manual search finds only nine original definition/
declaration lines. Before-object inventories across ten actual IconMan objects
contain the two definitions and no undefined references to any retired name;
the five orphan prototypes already have no compiled symbol. The private headers
are not installed: Meson installs the module executable and its configuration.
These interfaces are not an external module ABI or configuration contract.

`map_manager`, `set_manager_window_mapping`, `add_win_state`, `del_win_state`,
`xy_to_button`, `draw_manager` and every other retained source byte remain.
The exact removed spans reconstruct the five candidate files from the baseline.
After compilation, the global defined-symbol set falls from 116 to 114, removing
only the two named text symbols. No new definitions or retired undefined references
appear; every other defined name/type survives. This compares symbol contracts,
not binary addresses, debug line tables or performance.

## One focused checkpoint

The 565 baseline input hashes/modes match the merged source, with three additional
unchanged fixture helpers checked before upload. All 568 final identities match
before and after the gate. The reused normal GCC 13.3.0 Linux ARM64 profile builds
IconMan and the existing UI packet/producer fixtures. UI packet unit, IconMan
service X and transport X checks all pass on their first candidate invocation.
Recorded commands total 1.587 seconds, excluding engineering/evidence work.

Service coverage queues 257 keys and 80 exposes and observes queued-X continuation
without new input. Transport covers popup EOF/TERM, fragmented packets, tooltip
delay, long-tooltip termination and high descriptors. The [receipt](VERIFICATION-2026-10-25-M5-ICONMAN-INTERFACES.json)
retains raw case results, exact commands/options/compile commands, source/output
identities and symbol comparison. No UI/configuration/manual behavior is added.
There is no reason for a new fixture, sanitizer campaign, install or automatic
platform matrix solely for this exact unreferenced-code deletion.

Canonical private archive: October 25 `m5-iconman-interfaces/native-evidence.tgz`,
54 payloads plus manifest, 337,070 bytes; SHA256
`122886e4ead0e8ae1189f2aa3e60ed1e6062f8396e6e99812b1a36408f389035`.
Every payload hash, size and mode is verified. Baseline sources/objects, caller
inventory, exact spans, raw before/after symbols, focused logs/profile/output and
X results survive. The 55 verified guest capture files and duplicate archive are
retired after exact manifest and no-active-reference checks. Canonical host
evidence, source/build caches, stages, VM disks and performance references remain.

Separate final source/caller/exact-span/raw-evidence/report review passes with no
remaining findings. All source identities, payloads, symbol sets and raw passing
results were independently checked. Repository checks/lint pass; signed author-DCO
delivery follows. Analysis
and review request the selected Temper models; actual runtime metadata is
unavailable. The lead performs the tiny exact-deletion implementation.
No new UI/theme/screenshot or benchmark. PR248 reference is 2/5 after PR250 and
3/5 on this merge. No fresh BSD/AMD64/install/compiler-feature/default-server,
native-CPU/hardware/toolkit/compositor or release qualification is inferred.
Broad #34 remains open; #31/#35/#36 remain completed and #93 not planned.

## Separate parked checkpoint

The earlier Backer admission candidate is incomplete and unpublished, on local
WIP branch `codex/m5-backer-command-admission` at `a4c0933c0`. Its combined/valid
unit terminal assertion failed; individual rejection scenarios passed. Delegated
diagnosis was blocked by the security/Daybreak filter and was not retried through
another agent/executor. No Backer X/sanitizer/manual acceptance is claimed.

Its existing work/evidence is preserved in a local WIP commit and the private
`m5-backer-admission/parked-native-evidence.tgz`: 60 payloads, 2,367,386 bytes,
SHA256 `d590d39371a67e0115737dc5f90ce375c0462df2f921a8a2136e37e07bb8ad17`.
The 61 verified raw guest files and duplicate archive are retired after preserving
and verifying the host copy. Source is restored to the merged baseline before
this independent IconMan work. This PR contains no parked Backer code, tests or
manual changes and does not diagnose or resolve that failure.
