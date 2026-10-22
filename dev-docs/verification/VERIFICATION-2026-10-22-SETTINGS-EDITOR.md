# Grouped desktop settings editor — CFG-01

The next consumer checkpoint after merged PR151 adds a native grouped editor.
The functional checkpoint is locally qualified with the existing native Form
exit-cleanup diagnostic described below; this is not release acceptance.
[Machine-readable receipt](VERIFICATION-2026-10-22-SETTINGS-EDITOR.json).

Three related behaviors ship together:

- Edit the seven registered saved preferences in one native Form. Apply (F3)
  validates the complete selected scope, performs one private atomic save and
  then submits the resulting theme. Registry-defined choices, time validation
  and explicit module-override precedence remain authoritative.
- Defaults (F2) fills only the selected scope's unsaved draft. Cancel, Escape,
  closing the window and the five-minute deadline leave preferences unchanged.
  Failed validation/persistence leaves the current draft available for repair.
- One opaque per-instance callback token, preference generation and monotonic
  lease bound the consumer. Replacement, successful commands/reload and helper
  restart retire older Forms. External saved-file changes still fail the
  existing revision check rather than being overwritten by a stale draft.

Generation identifies helper preferences, not arbitrary core Style/Colorset
changes. Live desktop preview remains false. Enabling rollback before those
shared-core changes can be observed would risk overwriting newer state; keep
that remaining CFG-01 contract coordinated with IPC-02. This batch introduces
no setting domains, toolkit, module ABI, dependency or core-state owner.

Native Form teardown needs both process name and alias. The editor uses
`KillModule FvwmForm SlateSettingsForm`; repeated notices now use the same
correct process/alias selection. An alias-only KillModule did not select Form.

## Source and combined checkpoint

Frozen runtime/policy candidate: `5f9a76f00bf1b7234778969a886ac85d72a81601`;
2155 inputs; source archive SHA-256
`8bf5c5a8d5898ae7d622cd330a68ba3f23e4cd8e5897b7969f6d2b3a85c27394`.
The final test observer is `d2de34234e6c0cc101d440664868e0f86498cacb`, archive
`8634a9d5448c9238bd7221433f880aa44ec7847232959f4938ba447175a3065e`.
Only `tests/live_x/desktop_live.py` changes between these snapshots. Runtime,
policy, build and dependency inputs remain identical. Final publication updates
documentation/status/receipts only.

One combined checkpoint runs the two affected registered tests on six targets /
ten profiles: Linux AMD64/ARM64 GCC/Clang, Linux ARM64 software/minimal, FreeBSD
AMD64/ARM64 and OpenBSD 8.0 AMD64/ARM64. All 20 affected entries pass. The
unchanged native foundation retains 2931 PR151 entries: composed totals remain
2420 standard / 531 optional. These are composed results, not a new full-suite
run. A focused Linux observer correction adds two passing registered entries;
only the failed OpenBSD observer profile is repeated.

All 150 native ELF targets and 77 catalogs retain exact built/installed PR151
identities. The ten helper stages verify all 927 installed file identities;
only the ten generated FvwmDesktop files change. Eight stages compose a fresh
helper with verified unchanged prior installed files, using additional hard
links without editing old inodes. Both disposable OpenBSD stages are fresh
complete Meson installs. Prior stages/builds/disks remain preserved. This avoids
unnecessary copies on the nearly full Linux ARM64 guest.

Ten staged settings-X sessions and four authenticated default BSD-X sessions
pass; unauthenticated clients are refused. Disposable OpenBSD jobs exit QEMU
with status zero and remove their private job keys. Twenty-eight policy cases
cover grouped publication, invalid/failed writes, scoped unsaved defaults,
expiry, stale/replaced/restarted callbacks, external saved-file changes and
personal overrides, plus the existing storage/type checks. Native-X cases
exercise actual choice input/repaint, F3 Apply, F2 Defaults, Escape Cancel,
external refusal, accepted-change/reload retirement and predecessor recovery.

Actual-X captures now include the grouped editor, changed unsaved draft,
post-Apply desktop/menu and scoped Defaults. Five selected PNGs are extracted
from the validated Linux candidate archive; their payload hashes and canonical
paths are in the receipt. They are running-application captures, not mockups.
Screenshots remain alongside canonical evidence and are shown in the handoff.

## Corrections, diagnostics and remaining scope

The first Linux AMD64 run exceeds the existing 120-second registered deadline
in the old palette/barrier scenario, before the editor. Its original coordinator
and timeout diagnostic archives remain failed. One unchanged isolated control,
after the other emulated AMD64 jobs exit, passes both compiler profiles and
both staged sessions with unchanged test deadlines. This is a composed recovery;
it is not a relabeled passing initial run or a proved scheduling root cause.

OpenBSD AMD64 exposes a test-observer race: XFetchName reads a title after the
listed window disappears, producing BadWindow on GetProperty. The corrected
observer accepts only the exact borrowed window's QueryTree/GetProperty/
GetWindowAttributes disappearance; unexpected X errors still fail the check.
The corrected Linux follow-up and repeated OpenBSD profile pass. Original
failure, corrected source bindings and all captures remain preserved.

Development diagnostics retain Return advancing a time field, asynchronous
choice input/repaint and native Form replacement assumptions. The completed UI
uses explicit F3/F2 shortcuts, acknowledges choice repaint and observes
predecessor destruction rather than assuming a replacement gets a new XID.
The initial README word-limit failure and private manual coordinator/diagnostic
normalization corrections are also retained. No runtime changes follow freeze.

The leak-enabled guest ASan/UBSan desktop exercise completes its functional
scenarios, but its sanitizer gate is **not clean**. Seven exiting native Forms
report 3080 bytes / 67 allocations each in fontconfig/Xft font acquisition and
exit cleanup. A standalone existing native Form, without FvwmDesktop or any
new editor callbacks, reproduces the same diagnostic family (3048 bytes / 66
allocations). There is no ASan memory-access or UBSan diagnostic in this scoped
capture. No detector is disabled and no suppression is added. Keep this native
Form process-exit cleanup with CLEAN-01 in Later, per the finite feature cutoff;
this report does not establish clean leak/release acceptance. Process exit
reclaims these process-local allocations; this observation does not establish
any wider runtime-growth or font-ownership audit.

Portable verification passes 177 of 191 entries with 14 environment skips;
repository checks pass. The updated manual renders with Linux Asciidoctor and
host mandoc. Lint remains exit 3 with exactly PR151's diagnostic multiset:
one insecure mso, one unknown LINKSTYLE and seven section-spacing warnings.
No additional formatter diagnostic is introduced. DOCS-01 retains that scope.

Native-CPU release sanitizers, physical hardware, toolkit/compositor behavior
and release acceptance remain separate. Successful owner-controlled capture
and source-archive duplicates are retired only after complete canonical hash
verification; failed diagnostics remain preserved. Performance reference PR149
is at 2/5 after merged PR151, so no comparison or tuning is due this batch.
