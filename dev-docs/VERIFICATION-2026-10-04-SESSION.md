# Legacy property and session verification

This follow-up advances DATA-01, FD-01 and CLEAN-01 in
[the tracker](PROJECT-TRACKER.md). It preserves FVWM configuration and theme
syntax. Interactive Move/Resize starvation remains the next implementation task.

## Changes

- Shared ICCCM readers cap complete format-8 text, classes, command arguments
  and session strings at 64 KiB. Names retain their server encoding and existing
  locale conversion. Class hints require an instance separator and accept an
  omitted final class terminator. Command lists retain interior/final empty
  arguments and strip exactly one property terminator, matching
  [Xlib's getters](https://github.com/mirror/libX11/blob/master/src/GetHints.c).
- Protocol and colormap-window lists require complete typed format-32 replies
  of at most 16 KiB. Standard ATOM protocols and the historical Corel
  WM_PROTOCOLS type both deliver WM_DELETE_WINDOW. The old fallback's leaked
  allocation and duplicate parsing are removed.
- Startup WM_STATE is checked before indexing: valid one/two/three-field records
  remain accepted. Root restart markers require exactly one typed format-32
  value and their replies are freed. Malformed markers cannot select restart
  startup behavior.
- Session ID/role acquisition shares capped STRING readers and releases rejected
  replies. Client leaders require one WINDOW/32 value. Duplicate leader parsing
  and the module proxy's leaked client ID are removed.
- ICE listeners, accepted connections, authentication streams and the core
  session connection receive checked close-on-exec before use. An accepted
  connection is rejected outside its libICE watch callback if protection fails.
  Temporary-file failures close raw descriptors and restore the original umask.
- Module session authentication invokes iceauth with literal arguments instead
  of a shell command containing a pathname. Save directories containing spaces,
  quotes and shell metacharacters work; exit and file-write failures are checked.
- Two disconnect lifetime hazards are fixed. Processing a connection may free
  its watch/list node, so iteration retains the next node first. Closed handles
  are removed from the pending registration queue before later status checks.
  Processing no longer dereferences the watch record after libICE callbacks.

## Results

| Target | Full suite | Final affected checks | Staged install |
| --- | --- | --- | --- |
| Linux ARM64 GCC | 41/41 | 4/4 | Passed |
| Linux ARM64 Clang/lld | 41/41 | 4/4 | Passed |
| Linux AMD64 GCC | 41/41 | 4/4 | Passed |
| FreeBSD 15.1 ARM64 | 32/32 | 4/4 | Passed |
| FreeBSD 15.1 AMD64 | 32/32 | 4/4 | Passed |
| OpenBSD 7.9 ARM64 | 32/32 | 4/4 | Passed |
| OpenBSD 7.9 AMD64 | 32/32 | 4/4 | Passed |

The final affected checks are legacy properties, hardening, compatibility and
session integration. All four also pass on each default BSD X server: FreeBSD
scfb on both architectures, OpenBSD AMD64 Xenocara and OpenBSD ARM64 wsfb with
the root-sized fallback. FreeBSD AMD64 uses UEFI with an explicit disk boot
priority; BIOS boots support Xvfb but provide no scfb screen. The working boot
command is recorded in [BSD.md](BSD.md).

Full suites preceded final native-policy/startup regression additions and an
include-only source correction from concurrent work. Native policy and startup
checks were followed up on each target; the four affected live-X checks, staged
installs and default-X checks were repeated with the exact final hardening C sources.
Linux ARM64 GCC also builds with session management disabled and passes its
24-test native suite. Python syntax and git diff whitespace checks pass.

The new session test uses a real authenticated XSMP connection. It disconnects
three clients before registration, verifies core registration and management,
checks that modules inherit no session descriptor, and shuts down cleanly.
A wrapper around the real iceauth checks literal paths and no descriptor leaks
through exec; both authentication files are removed. The legacy test exercises
pre-existing clients during startup, malformed and valid records, actual Close
client messages, name rejection/recovery and ICCCM selection replies.

Restoring the pre-fix ICE watch/pending lifetimes makes the new early-disconnect
regression fail. An earlier registered-client shutdown also crashed before the
iteration fix. A temporary plain stack trace identifies the early-disconnect
failure in IceCloseConnection after a dead pending handle is revisited. These
are negative controls, not successful test runs. GDB could not fetch ARM SVE
register state in this VM; the plain stack trace required no sanitizer.

## Scope and evidence

Verification used a case-sensitive Linux workspace and native BSD QEMU guests.
Concurrent desktop/configuration changes and their new test registrations are
preserved in the shared checkout but excluded from this hardening snapshot.
These counts do not claim a full gate for that separate desktop work.

Native Linux sanitizer CI now enables live-X tests, including the new session
regression. It remains configured but unrun; no Git remote is configured. Local
ASan was not used. Setup-failure fault injection, module-side generic property
getters, image acquisition/decoder stress, tree traversal limits and the broader
helper descriptor audit remain open. These bounded Xlib reads are synchronous;
this batch does not eliminate blocking X round trips or modal input loops.

Evidence is retained under builddir/codex-2026-10-04/legacy-session/ in the
arm64-evidence.tar.gz, amd64-evidence.tar.gz and source-snapshot.tar.gz archives.
Each BSD directory under builddir/qemu-bsd/ has a
legacy-session-evidence-2026-10-04.tar.gz archive. They include full gate logs,
final follow-ups, default-server observations, JSON results and independently
computed production fingerprints. All eleven changed production files match
the frozen verification snapshot byte-for-byte across the six platform workspaces.
The shared checkout subsequently received a separate default ImagePath change
from images to wallpapers as part of the concurrent desktop work. That change
is outside this batch's verification scope; the hardening changes are preserved.
