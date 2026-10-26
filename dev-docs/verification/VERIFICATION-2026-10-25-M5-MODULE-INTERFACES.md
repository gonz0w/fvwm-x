# M5 private module drawing/keyboard interfaces — 2026-10-25

Owner: CLEAN-01 #34. Base merged PR251:
`5093a0fa8aa6ff294545de2342519eace81d4530`.
**Focused checks pass; broader owner/release qualification remains pending.**

Remove three uncalled bodies and five private declarations across six files:
Script DrawArrowE/W and their prototypes; Buttons buttons_keyboard_enabled getter
and prototype; orphan Buttons expand_action and Ident change_defaults prototypes.
Exact deleted spans remove 96 source/header lines. There are no new functions,
replacement helpers, tests, settings, dependencies or installed interface changes.

The active source/configuration inventory contains only eight original definition/
declaration lines. No calls or address references exist. The private executable
headers are not installed, and the bodies are unconditional. Before-symbol scans
cover Script module objects plus its private Widgets archive and Buttons/Ident
objects. They contain exactly three selected definitions, no undefined consumers
and no symbol for either orphan prototype.

Exact span reconstruction preserves every retained source byte. Active Script
DrawArrowN/S, List/MiniScroll consumers and HScrollBar registration remain. Buttons
navigation state/setter/event/focus/pressed paths and module_expand_action calls
remain. Ident configuration and packet callbacks remain. No configuration migration,
rendering/layout policy or dispatch ordering change is introduced.

## One combined checkpoint

All 572 baseline and final source identities match hashes/modes. One reused GCC
13.3.0 Linux ARM64 normal profile incrementally builds Script, Buttons and Ident,
including core/MFL executable prerequisites for the existing fixtures. Those
executables relink during the combined build; their source is unchanged. All four native-X
fixtures pass on the first candidate invocation: Script widgets, Script text/list,
Buttons keyboard and Ident configuration. They record 16, 16, 10 and 5 named
behavior observations respectively; the Buttons summary status is not a separate
case. Recorded commands total 31.508 seconds, excluding engineering/evidence work.

Coverage includes MiniScroll/List arrows and repeat/release behavior, text/list
selection and bounded-input recovery, keyboard navigation/activation/focus,
atomic Button updates and existing configuration/termination handling. Raw results
remain in the [receipt](VERIFICATION-2026-10-25-M5-MODULE-INTERFACES.json) and canonical archive.
No new deletion-mirroring test, sanitizer campaign or automatic platform/install
matrix is warranted for removing unreferenced unconditional definitions.

Both object/archive and executable global defined-symbol names/types are checked:

| Module | Objects/archive before → after | Executable before → after |
| --- | ---: | ---: |
| Script | 288 → 286 | 962 → 960 |
| Buttons | 119 → 118 | 775 → 774 |
| Ident | 18 → 18 | 655 → 655 |

Only DrawArrowE, DrawArrowW and buttons_keyboard_enabled text symbols disappear.
No definitions are added, every other defined name/type survives, and there are
no retired undefined references. This does not compare addresses/debug metadata
or establish a speed improvement.

Canonical private archive: October 25 `m5-module-interfaces/native-evidence.tgz`,
107 payloads plus manifest, 238,737 bytes; SHA256
`b91c4ba2fdbff94fd8305a0729d0397d1a7001edb6a00337a4ff8b5fdb9f9a11`.
Every hash, size and mode is verified. It retains baseline sources, caller and
before/after object/archive/binary symbol inventories, exact spans, source/profile/
output identities and raw passing X results. The 108 verified guest capture files
and duplicate archive are retired after exact manifest/no-active-reference checks;
canonical host evidence, source/build caches, stages, VM disks and performance
references remain.

The host convenience extraction initially assumed the two Script fixtures used
`evidence.json`; their existing result files are named `results.json`. Corrected
host extraction reads the verified archive without repeating runtime checks.
This collection-only correction leaves source, binaries and all passing gates
unchanged. No candidate runtime/build gate failed.

Separate final source/caller/exact-span/raw-evidence/report review passes with no
remaining findings. Source identities, all payloads, symbol sets and raw passing
results were independently verified. Repository checks/lint pass; signed author-DCO
delivery follows. Requested
Temper analysis/closure/review settings follow PROJECT.md; actual runtime model
metadata is unavailable. The lead implements the exact deletions.

The closure audit finds broader acceptance remaining under #6/#9/#25/#34, so none
closes from this slice alone. #31/#35/#36 remain completed; #93 remains not planned.
Daybreak-blocked work stays parked per explicit user direction and is neither
inspected nor advanced. No new UI/theme/screenshot or benchmark. PR248 reference
is 3/5 after PR251 and 4/5 on this merge. No fresh BSD/AMD64/compiler-feature/
install/default-server, native-CPU/hardware/toolkit/compositor or release claim.
