# Combined core responsiveness qualification — 2026-10-11

Status: strictly locally qualified. Tracker owners: PERF-01 /
QUEUE-01 / CLEAN-01 and inherited monitor/Pager checkpoint qualification.

## Source and scope

The user requested completion of the current responsiveness milestone on
2026-10-11. Four performance stories are already implemented and focused-
qualified. Their frozen source is
`005f5eb698d4a2115dc202c80d32c7480ab49f216515c73a539ff285454bb5bd`:
1409 files, twelve owning paths relative to `3074`. No production source was
changed to begin this qualification. Existing configuration names, arguments,
bindings, functions and theme format require no migration.

One whole-source matrix covers `005f` and its inherited `a653`/`3074` changes.
Separate redundant predecessor matrices are not dispatched. The accepted
`3c37` archives, original guest disks and shared checkout/builds are preserved.
Each current guest uses an isolated source/build tree; restarted QEMU guests
use new persistent writable overlays. OpenBSD trees reside on `/home`.

The changes and limited emulated performance observations are in the
[focused performance report](VERIFICATION-2026-10-08-CORE-PERFORMANCE.md).
This report records qualification, not a physical-desktop latency guarantee.

## Final test-only source

Three concrete fixture repairs extend `005f` to final source
`01c20db815285b27ea04b4dda7287293d02e321cc9b70a013d77689447cfd3c9`, still
1409 files. Only `pager_startup_live.py`, `pager_layout_live.py` and the owning
module lookup block in `desktop_live.py` change. Other chats' newer desktop
changes are excluded. Every production/compiler input and configuration/manual
source is byte-identical to `005f`.

- FreeBSD's prepared default runner omitted the new Pager fixture argument.
  Its preceding successful drivers remain; only the failed component and
  outstanding performance drivers run after correction.
- OpenBSD ARM64 default wsfb reports zero RandR monitors but a synthetic output
  Identifier. The fixtures used that output, while production correctly uses
  output zero for the root-sized fallback. Both drivers now follow monitor
  topology; original diagnostics and actual server listings remain.
- The mixed optional desktop fixture generated a ModulePath line beyond the
  existing Read stream's 1024-byte buffer in the longer private build path.
  A private directory of aliases preserves the selected module binaries and
  logging wrappers with a short setup line. Original failed suite/logs remain.
  Configuration-reader line/continuation review stays with DATA-01.

Affected tests run without rebuilding. Strict derivations preserve the base
full-suite records, original driver bytes and executable identities, then replace
only the revised drivers' entries. The two intermediate native Pager passes
remain usable because their exact driver bytes and production binaries match the
final source; no redundant repeat is made solely for the unrelated desktop
fixture change. The no-SVG full profile starts directly on final source.

## Required combined gate

The exact plan and runners are retained in
`builddir/codex-2026-10-11/core-performance-qualification/`:

- Eight compiler profiles across Linux/FreeBSD/OpenBSD AMD64/ARM64:
  1816 registered test entries and 120 installed native code identities.
- Four default BSD Xorg/Xenocara servers: 300 driver invocations, including
  staged core AutoDetect/explicit-policy resize bursts, scheduler ordering and
  independent input, and three-workload responsiveness profiling.
- Two Linux ARM64 optional-feature profiles: 232/212 entries.
- Scoped local Linux ARM64 ASan/UBSan: 41 policy checks with leak checking and
  77 actual-process checks with leaks disabled, including new resize coverage.
- Actual rendering of all 26 man pages and their optional derived HTML, with
  the new bounded-resize guidance in both outputs.

The strict validator retains the predecessor's detailed outcome requirements,
adds resize/scheduler/profile assertions, checks every frozen source hash/mode,
and checks exact executed runners and staged binary identities. Focused passes
and a partially collected summary are not acceptance of the whole matrix.

## Accepted results

All seven final archives pass the strict validator. Each archive retains exact
hashes/modes for all 1409 final source files, copied executed runners, original
full-suite records, affected-driver derivations and installed-code identities.
The accepted source is `01c2`: production/build/manual inputs remain `005f`;
only the three fixture repairs described above differ. This is compositional
qualification, not a claim that every full suite ran again after fixture edits.

| Platform | Standard compiler profiles | Accepted entries | Installed code | Default-X drivers |
| --- | --- | --- | --- | --- |
| Linux ARM64 | GCC and Clang | 233 + 233 | 30 | Nested X in suites |
| Linux AMD64 | GCC and Clang | 233 + 233 | 30 | Nested X in suites |
| FreeBSD ARM64 | Native Clang | 221 | 15 | 75 |
| FreeBSD AMD64 | Native Clang | 221 | 15 | 75 |
| OpenBSD ARM64 | Native Clang | 221 | 15 | 75 |
| OpenBSD AMD64 | Native Clang | 221 | 15 | 75 |
| Total | 8 | 1816 | 120 | 300 |

Both Linux ARM64 optional profiles pass: 232 mixed Xcursor/XRender entries and
212 SVG-disabled entries. The mixed profile retains its original desktop setup
failure and the exact affected replacements; the SVG-disabled full suite ran on
final source. All 118 scoped ASan/UBSan entries pass, with leak checking enabled
for 41 policy checks and disabled for 77 actual-process checks. All 26 man pages
and 27 derived HTML outputs render; bounded-resize guidance appears in both.

Resize cases preserve final 340×180 geometry and 350×190 constrained geometry
for AutoDetect, UseGravity and IgnoreGravity. Scheduler controls retain 256-job
FIFO order, service independent module input before backlog completion and
verify nested/cancelled timer behavior. Default-server profiling retains all
three selected workloads, 16 direct and scheduled samples per workload and
exact executable/source identities. Timing ratios are not acceptance criteria.

OpenBSD AMD64 keeps the established verification-profile TCG harness allowance
(60-second outer, 15-second internal); all 220 ordinary entries retain their
standard deadlines. No deadline was extended to conceal a production failure.
The inherited twelve-story `a653` and fourteen-story `3074` batches are covered
by this combined source; separate predecessor matrices are superseded.

Machine-readable totals and archive hashes are retained in
`builddir/codex-2026-10-11/core-performance-qualification/qualification-summary.json`.
Original fixture/environment failures remain alongside their exact recoveries.
No configuration-file or format migration is required.

## Environment and retained preparation attempts

The Linux ARM64 container remained available. All five project QEMU guests were
stopped. Their previous BSD runs used transient snapshots; restarting from saved
base disks required restoration of previously qualified private libpng files.
The retained archives and twelve regular-file hashes per BSD prefix were checked.
Linux AMD64 uses exact retained package hashes/metadata and a private extracted
PNG/XPM prefix; no system package installation or upgrade was performed.
OpenBSD AMD64 retains its saved system libpng1.6.59 profile.

Preparation retained an unavailable-guest inventory, an inspection that assumed
FreeBSD had `/home`, a Linux private extraction failure caused by missing parent
directories, and collection argument-size/standard-input corrections. These are
harness/environment failures, not changed production behavior. Original logs
remain; source and test deadlines were not changed for these corrections.

## Remaining release scope

Native Linux release sanitizer CI is configured but remains unrun; this checkout
has no Git remote. Physical GPU/input, mixed-monitor hotplug, external compositor
and actual GTK/Qt feedback remain separate release checks. This bounded milestone
does not close all SCRIPT/DATA/INPUT audits, the broader optimization backlog,
modern parser/runtime adoption or queued desktop-service work.

See [current checkpoint status](../PROJECT-TRACKER.md#current-checkpoint-status)
and [support requirements](../SUPPORT.md).
