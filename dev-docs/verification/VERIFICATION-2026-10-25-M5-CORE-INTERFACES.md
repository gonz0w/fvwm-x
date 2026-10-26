# M5 private core focus/geometry interfaces — 2026-10-25

Owner: CLEAN-01 #34. Base merged PR252:
`987cdcf34a287bffa30c432f043241a7f6dd34a6`.
**Focused checks pass; broader owner/release qualification remains pending.**

Remove four uncalled bodies: _ForceDeleteFocus,
focus_grab_buttons_on_pointer_window, get_shaded_client_window_pos and
update_relative_geometry. Their prototypes, the unused SetFocusWindowClientEntered
and ForceDeleteFocus macros, and orphan focus_get_transientfor_fwin/get_title_gravity
prototypes retire too. Four bodies, six prototypes and two macros across four
private files remove exactly 97 lines, including exclusively associated comments.

Source/configuration/address inventory finds only the definitions, declarations
and unused macro definitions. All bodies are unconditional; core headers are not
installed. Actual 78 core objects plus libfvwm3 have four selected definitions
and no undefined consumers; the two orphan declarations already produce no symbol.
No configuration name, command, dependency or installed interface is removed.

Exact deletion spans reconstruct every retained source byte. _SetFocusWindow's
four-argument signature and client_entered parameter remain. Active focus set/
return/delete, client-enter/layer/all-window button grabs, restoration, shaded/
client geometry and absolute conversions remain. No ordering barrier, event
policy, geometry calculation or authoritative state moves or changes.

## One combined checkpoint

The dispatcher verifies all 575 baseline/final input hashes and modes. The same
map keys survive with only the four intended identities changed. One reused
normal GCC 13.3.0 Linux ARM64 profile incrementally builds the core and fixture
prerequisites. Build, geometry unit, existing geometry Xorg dummy fixture and
stack-layer X fixture pass on the first candidate invocation. The build log
retains warnings in untouched inputs; no candidate gate fails.

The geometry fixture runs 5/6/8-quarter scales, each with 32 shaded round trips
plus maximized/fullscreen restoration: 34 cases per scale, 102 total, no smoke
shortcut. Fixed/workarea observations and MFL echo round trips are retained.
Layer/focus checks cover parent/child state, stacking/focus/unrelated-window
behavior and destroyed-parent survivor publication. Recorded commands total
51.130 seconds, excluding engineering/evidence work. No new test, sanitizer
campaign or automatic OS/install matrix is added for unreferenced-code deletion.

Object/library and executable global defined-symbol names/types are compared:

| Inventory | Before | After |
| --- | ---: | ---: |
| Core objects + libfvwm3 | 1,721 | 1,717 |
| Core executable | 1,699 | 1,695 |

Only the four selected text symbols disappear. No definitions are added and
every other defined name/type survives; no retired undefined reference appears.
Addresses/debug metadata may differ. No speed or whole-desktop memory gain is
inferred from this structural proof. The [receipt](VERIFICATION-2026-10-25-M5-CORE-INTERFACES.json)
binds source, commands/options/compile commands, outputs, symbol sets and scenario
summaries; full raw case coordinates remain in the archive.

Canonical private archive: October 25 `m5-core-interfaces/native-evidence.tgz`,
71 payloads plus manifest, 421,787 bytes; SHA256
`28a1f78ec0e20c878ddc3e14f361d105fde0e2c724aa28958de72226d76f07d6`.
Every payload hash, size and mode is verified. Baseline sources/objects, caller
inventory, exact spans, source/profile/output identities and raw unit/X/symbol
results survive. The 72 verified guest capture files and duplicate archive are
retired after exact manifest/no-active-reference checks. Canonical host evidence,
source/build caches, stages, VM disks and performance references remain.

Separate final source/caller/exact-span/raw-evidence/report review passes with
no remaining findings. The reviewer independently binds the four changed
files and selected unchanged fixtures, all archive payloads, raw symbol sets and
passing results; unchanged manifest maps and executed hash-check scripts establish
other identity checks without opening excluded source contents. Repository checks/
lint pass; signed author-DCO delivery follows. Requested Temper analysis/review model
settings are unverified runtime metadata; the lead makes the exact deletions.

Broader #34 acceptance remains open and completed #31/#35/#36 stay closed. All
Daybreak-blocked work stays parked under explicit user direction. No new UI/theme/
screenshot or benchmark. PR248 reference is 4/5 after PR252; this candidate merge
makes 5/5 and the next periodic comparison due. No fresh BSD/AMD64/compiler-feature/
install/default-server, native-CPU/hardware/toolkit/compositor or release claim.
