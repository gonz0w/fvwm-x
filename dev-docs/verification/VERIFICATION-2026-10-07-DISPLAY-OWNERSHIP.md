# Owned OpenBSD nested-display allocation

Status: focused overlapping-display checks pass on all six local targets, three
runs each. Both OpenBSD architectures also pass the deterministic owned-slot
race. Combined qualification belongs to the next module-client checkpoint.
This changes verification infrastructure, not installed WM configuration.

On the owned OpenBSD ARM64 guest, two Xvfb servers using -displayfd both report
:0. Both initially accept connections; closing the first makes the second
unreachable. Sharing TMPDIR does not prevent this. Explicit displays :1200/:1201
retain independent connectivity after teardown. The source, result files, binary
identity and platform are retained. Xenocara's [option parser](https://raw.githubusercontent.com/openbsd/xenocara/master/xserver/os/utils.c)
disables its usual server lock for -displayfd; the local control establishes the
observed Unix-display collision. Earlier unrelated display refusals retain their
own evidence; no historical cause is inferred automatically.

OpenBSD Xvfb now uses a finite range of explicit display candidates and the
server's existing atomic lock. It skips occupied lock/socket paths, confirms the
lock belongs to its child and waits for an actual RandR reply within the original
startup deadline. A race closes only the refused child and retries another slot;
its diagnostics remain preserved. TCP stays disabled. Other platforms retain
server allocation through -displayfd; no new dependency or global harness lock
is introduced.

The permanent check starts three overlapping servers across two runtime owners,
then closes one owner and verifies the other display remains usable. On OpenBSD,
a controlled owned rival wins a slot after the initial availability check. The
refused child exits, diagnostics are retained, retry chooses a different slot,
and the rival remains reachable after retry teardown. No unowned server is
started, closed or removed. Seven complete archives validate hashes/modes/EOF:
22 focused files per Linux/FreeBSD target, 40 per OpenBSD target and seven
original OpenBSD control files. Evidence:
`builddir/codex-2026-10-07/display-ownership-production`.
The final harness/unit source is included in the frozen module-client1351 batch.
Native release CI/physical evidence remains separate.
