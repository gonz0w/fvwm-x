# Configured selection actions and observed properties

Status: **focused Linux / early FreeBSD checks pass; ready for review**.
Owners: FILE-ACTION-01 / FILE-DATA-01 / FILE-VIEW-01; portable request handoff.
Base: merged PR170 `94c2169026c0f926e297f9ad1ab466ed3b92803a`.
[Contract](../decisions/FILE-SELECTION-ACTIONS-2026-10-23.md),
[source-bound receipt](VERIFICATION-2026-10-23-FILE-SELECTION-ACTIONS.json).

## Delivered batch

- Configured `--action LABEL=EXECUTABLE` buttons (up to four), generation-checked
  selected IDs and one literal-argument process launch. The isolated worker
  validates the entire selection before starting any application: unique known
  IDs, file/folder types, parent/entry identity, direct-child names, 4096-byte
  paths and 32 KiB combined arguments. Stale/unsupported/over-budget selections
  fail as a whole; a busy worker never queues another side effect.
- Properties for 1..256 entries, including offscreen/filtered selections, plus
  repeated same-parent `--properties` startup requests. Single-entry detail stays
  intact; multiple entries show type counts and checked regular-file totals,
  excluding recursive folder sizes/link targets. Any disappeared inspected ID
  retires the old panel. No filesystem/content work runs in the UI owner.
- Consistent cancelled results when cancellation precedes worker startup, and
  explicit configured-action process-creation/path-count feedback. Launch is a
  handoff, not application completion. Existing one-worker/reap ownership stays.

No filename is reconstructed from a label or evaluated by a shell/FVWM command.
Default Open selected still requires one file; configured programs explicitly
accept the multiple-path interface. The feature is ordinary file metadata and
literal actions, not protected clipboard delivery or file-operation jobs.
No mandatory dependency, native C/module packet, default theme or existing
configuration contract changes. SQLite remains deferred.

## Verification and scoped follow-up

Initial frozen source passes both five-package policy sets, Linux race/vet,
three one-action theme drivers and the manual. Review adds the pre-copy root
length check and a four-action layout fixture. That source passes **five Go
packages / 51 top-level cases on both Linux ARM64 and FreeBSD ARM64**, plus the
separate Linux race run (**153 final top-case executions**), vet and builds.
Actual child processes preserve reversed selection order, cwd, quotes, dollar/
backtick/newline names and invalid UTF-8 filename bytes on both target systems.
A changed final selected entry prevents any configured program from launching.
Limits, ambiguous IDs, symlinks, replaced roots, summary overflow, immediate
cancellation, busy refusal and initial multi-properties ownership are covered.
Host APFS uses a valid-name substitute; it does not establish raw-byte pathname
execution. Both supported native target runs use the original byte cases.

Slate dark and Dune light pass the **maximum four-button** actual-X fixture.
Classic's old lowest-wide-control heuristic selects a low action button instead
of the path input. Its original failed archive stays preserved. The observer
alone changes to identify the wider 40-column path input; the 40-column input is wider than the fixture action labels in these
equal-font profiles. Full source guards and an exact one-fixture
difference retain passing policy/race/FreeBSD/Slate/Dune results. Only the classic
native-X driver reruns. It passes with unchanged product sources/binary hashes.

Final fixture source: **2267 files**, archive
`881d7115da6e8031f858838e4d5feb9253499c0b056f0bf0bf3942c8cda0d4da`. The three composed drivers load actual shipped
Slate/Dune configuration/desktop paths and classic 9x15/legacy colors. They
exercise empty/4096 list/grid, focus/ranges/selection, filter/refresh/worker
cancellation and new summary/action controls. A real button launches exactly
one fixture application with the two original selected byte paths; replaying
its old callback does not launch again. No action runs on startup. Each driver
captures **15 screenshots** with config/font/scale/server inputs. New selection
summary and four-action views in Dune/classic are inspected for fit and contrast.

The authoritative optional-Go Meson profile is reconfigured to register the new
source files; only the helper/manual targets build. Its Go cache resolves to the
existing qualified shared cache, avoiding a new cold SDK cache. The resulting
helper is **byte-identical to the tested helper**. The generated manual renders
with host mandoc. Current repository/link/whitespace checks pass. PR170's portable
maintenance qualification is retained as the cycle reference, not rerun or
presented as fresh feature evidence.

All five archives validate compressed EOF/CRC, complete inventories and every
hash/mode. Explicit successes and the observer failure remain source-scoped:
initial Linux/FreeBSD **90/11 files**, reviewed Linux/FreeBSD **82/11**, final
observer **32**. Canonical evidence/screenshots: `/Users/cam/DEV/fvwm3/builddir/codex-2026-10-23/file-selection-actions`.
Completed guest capture/source-archive duplicates retire only after host proof;
sources, builds, qualified caches/SDKs, binaries and VM disks remain preserved.

## Remaining qualification

PR169 is the integrated/performance reference. Merged PR170 makes **1/3 and
1/5**; this checkpoint becomes **2/3 and 2/5 only on merge**. No periodic
performance comparison is due and tuning remains deferred. Use the next shared
cycle for accumulated affected platforms. Full install/default-X/compiler-feature,
owner/release and native-CPU/hardware/toolkit/compositor/assistive acceptance stay
at their boundary. No new native-C allocation/lifetime change requires another
C sanitizer run. In-place native updates, external providers, watchers, mutation
jobs and protected clipboard content/private delivery/storage remain open.
