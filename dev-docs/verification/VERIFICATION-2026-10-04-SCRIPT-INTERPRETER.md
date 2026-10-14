# FvwmScript command, startup and output maintenance — 2026-10-04

Status: implemented and verified on the local Linux/BSD matrix. This extends
the approved [file/message pass](VERIFICATION-2026-10-04-SCRIPT-FILE.md).
SCRIPT-01 remains open; these repairs do not certify the whole interpreter.

## Behavior and implementation

Do now evaluates each argument once and assembles a bounded command using
owned storage. This preserves the effective legacy 997-byte limit and avoids
running a function with side effects twice. It sends through the existing
module command API rather than duplicating protocol writes.

LaunchScript replaces fixed filename/command buffers with bounded dynamic
assembly. It preserves the first whitespace-delimited filename and raw
argument-tail syntax. Empty or oversized commands return the existing -1
result without reserving a child slot. Child count is capped at 98 and the
selection suffix search is finite. Names already reserved by the parent are
excluded even while a newly launched child has not claimed its selection.

Communication registration now precedes Init after window/widget creation.
Children launched by Init retain their identifiers and can exchange messages.
Short child identifiers are refused before suffix arithmetic. Startup argument
storage retains the module name plus 19 user arguments; extras are ignored,
and out-of-range getters return empty text.

GetOutput reads at most the existing 32,000-byte prefix into a 32,001-byte
allocation and always terminates it. Read errors and embedded NUL bytes do
not publish a new cache entry. The cache owns the complete command string;
commands are capped at 64 KiB. Dynamic line/word extraction removes the fixed
255-byte word buffer and validates numeric positions. Existing zero line/word
arguments continue to select the first line/word; word -1 selects the whole
line. Missing positions return empty text. The one-second cache behavior is
retained, with backward clock movement causing a refresh.

The command shell contract remains intentional trusted-script behavior.
At this snapshot GetOutput still used synchronous subprocess reads and
close/wait without a deadline. The subsequent
[bounds/deadline pass](VERIFICATION-2026-10-04-SCRIPT-BOUNDS.md) adds the approved
default and override. The results below refer to the earlier memory/ownership
snapshot. Work remains inside the isolated module; no blocking operation
was added to the authoritative WM event loop and no runtime library was added.

## Verification

Later fresh-snapshot checks found inherited Form objects and a stale-output
assertion in the Form fixture. The complete-rebuild and fresh-output checks in
[the numeric follow-up](VERIFICATION-2026-10-04-SCRIPT-VALUES.md) supersede those
specific Form assertions; the historical counts below describe their saved runs.


| Target | Integrated full suite | Final focused follow-up | Actual widgets |
| --- | --- | --- | --- |
| Linux ARM64 GCC | 58/58 | 5/5 | 18/18 under Xvfb |
| Linux ARM64 Clang/lld | 58/58 | 5/5 | 18/18 under Xvfb |
| Linux AMD64 GCC | 58/58 | 5/5 | 18/18 under Xvfb |
| FreeBSD ARM64 | 47/47 | 4/4 | 18/18 under Xvfb and default Xorg |
| OpenBSD ARM64 | 47/47 | 4/4 | 18/18 under Xvfb and default Xenocara/wsfb |
| FreeBSD AMD64 | 47/47 | 4/4 | 18/18 under Xvfb and default Xorg |
| OpenBSD AMD64 | 47/47 | 4/4 | 18/18 under Xvfb and default Xenocara |

All integrated gates include staged installation. Linux uses the established
Podman harness; BSD uses disposable QEMU snapshot guests on this machine.
AMD64 emulation results verify behavior, not native hardware latency.

Native policy tests cover exact limits, empty/rejected assembly, long filenames,
word/line boundaries, explicit unterminated 32,000-byte input, binary input,
numeric extremes and owned output. The actual-module fixture launches a child
from Init, passes 25 arguments and checks the retained nineteenth argument.
A Do containing LaunchScript must return child identifier 3, proving one
evaluation; its filename exceeds the old 100-byte storage. A long command
produces a single 32,000-byte word, which is extracted with both `0 0` and
whole-line selection and written back for content verification. Existing
clipboard, file/message and recovery checks remain in the 18-check fixture.

Evidence is saved in `builddir/codex-2026-10-04/script-output/`:

- `matrix-summary.json` records parsed passing rows and archive hashes.
- Seven `*-integrated-evidence.tgz` archives retain full gate logs and widget
  results; seven `*-final-followup-evidence.tgz` retain final focused results.
- `integrated-source-final.tgz` and its fingerprint maps preserve full-gate
  sources. `source-final.tgz` and `verified-script-fingerprints.json` preserve
  the final formatted sources. Independently read fingerprints for 61 files
  match each corresponding source snapshot and all seven guest/compiler legs.
- `documentation-only-followup.json` records the subsequent manual clarification
  of file permissions, final-component symlinks and non-atomic writes. This is
  a documentation-only difference from the tested snapshot.
- `whitespace-only-source-format.json` records the final four-file wrapping
  change. Whitespace-stripped content matches; the final focused tests ran
  after that change. Full-suite results refer to the integrated snapshot.

Snapshots preserve the prior verified build graph and existing shared module
keyboard-focus, parse-error and fire-and-forget changes. Unrelated concurrent
desktop work is not claimed as verified by these snapshots. Initial fixture
and build failures remain in the earlier evidence directories; passing results
above come from the final parsed logs.

## Remaining work

The subsequent [bounds/deadline pass](VERIFICATION-2026-10-04-SCRIPT-BOUNDS.md)
implements widget lookup/mutation checks, parser array/stack guards, native
module protocol close-on-exec and the approved GetOutput deadline. Arithmetic
and string conversions, recursion/loop budgets and broader descriptor audits
remain. The historical matrix above belongs to this earlier batch.
WriteToFile's approved owned/single-link/no-final-symlink rule remains in
effect, with 0600 new files. Its in-place updates remain non-atomic and can
leave partial content on storage failure.

Native Linux ASan/UBSan/fuzz CI is configured but has not run. The checkout has
no remote. Local normal-build tests do not replace that gate, and no sanitizer
or whole-project security result is claimed.
