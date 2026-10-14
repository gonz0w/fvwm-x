# FvwmScript FIFO transport — 2026-10-05

Status: completed. The user approved a five-second overall deadline with a
configurable override. All seven final local build/test/install gates and all
four default BSD X-server gates pass.

## Implementation

SendMsgAndGet now delegates to a small transport helper using nonblocking
descriptors, monotonic elapsed time and poll-based progress. Modes 0 and 1
share one deadline across startup, command writing and reply collection.
`*FvwmScript: FIFOTimeout milliseconds` defaults to 5000 and accepts 1 through
3600000. Invalid settings retain the previous value. Mode 2 keeps one immediate
atomic write, refusing missing readers, unavailable capacity and messages
larger than the endpoint's PIPE_BUF allowance.

Protocol filenames and success/failure conventions are preserved: modes 0/2
return 1 on success; failures return 0; mode 1 returns the reply, including a
completed empty reply. Slow theme helpers need an explicit larger timeout.
Timeouts discard partial replies and close owned descriptors. Commands already
sent cannot be rolled back, and the transport does not kill an external helper.

The user directory is pinned by a close-on-exec descriptor. Communication IDs
must be nonempty, contain no slash and fit both protocol filenames on the
filesystem. Final endpoints must be named pipes owned by the current user;
symlinks and other file types are refused before I/O. The opened endpoint's
type, owner, device and inode are checked against the pathname observation.
Directory symlinks remain usable. The existing 32000-byte reply allowance now
has a separate terminator; embedded NUL bytes are refused. Descriptor zero is
valid, errors cannot reduce write progress, and all failure exits clear outputs.

Broken-peer writes return an error without installing a new signal handler.
The caller's signal mask, disposition and preexisting pending SIGPIPE are
retained. Where SIGPIPE was already pending, signal instances are left for the
caller; their coalescing/count is platform dependent. Termination is checked
through the existing Script flag, including during bounded startup retries.
This remains synchronous work in the isolated Script module. No new library,
thread or work in the authoritative WM event loop is introduced.

## Verification

| Target | Full tests | FIFO X outcomes | Default BSD X outcomes | Fresh install |
| --- | --- | --- | --- | --- |
| Linux ARM64 GCC | 75/75 | 10/10 | not applicable | passed |
| Linux ARM64 Clang/lld | 75/75 | 10/10 | not applicable | passed |
| Linux AMD64 GCC | 75/75 | 10/10 | not applicable | passed |
| FreeBSD ARM64 | 64/64 | 10/10 | 10/10, default Xorg | passed |
| OpenBSD ARM64 | 64/64 | 10/10 | 10/10, Xenocara/wsfb | passed |
| FreeBSD AMD64 | 64/64 | 10/10 | 10/10, default Xorg | passed |
| OpenBSD AMD64 | 64/64 | 10/10 | 10/10, default Xenocara | passed |

Native tests cover delayed startup/replies, empty and full-sized terminated
replies, descriptor zero and close-on-exec, missing readers, a reply pipe with
no writer, backpressure, interrupted deadlines, atomic-write refusal,
termination, broken peers, retained caller signal state, owned regular files,
symlinks, foreign-owned FIFOs and embedded NULs. The overall-deadline test also
checks that the transport leaves its fixture helper alive after timeout.

Actual Script checks cover the existing helper protocol, modes 0/1/2, empty
and complete 32000-byte replies, endpoint refusal without file writes, the
five-second default, a 150 ms override with invalid-setting retention, later
widget-event recovery and cancellation on termination. Observed timings include
fixture polling and ordinary module cleanup; they are not scheduling guarantees.
Seven colorset, sixteen widget, six storage, seven execution and 24
selection/Form outcomes also pass under nested X and every default BSD server.
All seven archives match the 160 fingerprints and 1159 permission entries.
Fresh installed core and Script code/constants match their tested binaries;
the installation records also match the full build hashes. These are local
Podman Linux and QEMU BSD checks on macOS, including emulated AMD64.

## Corrections and provenance

An initial native helper barrier was closed before the child acknowledged it;
the test now consumes that acknowledgement before closing its descriptor.
Another focused run exposed a stale module binary: archive extraction retained
old source timestamps, so Ninja did not rebuild the changed implementation.
The corrected synchronizer compares file contents and refreshes changed or
explicitly forced source timestamps. The resulting module rebuild and live
checks passed. These failures and their source snapshots are preserved.

The first complete FreeBSD runs stopped when the test restored its signal mask
after consuming only one pending SIGPIPE. A native probe confirmed two pending
instances on FreeBSD versus one coalesced instance on OpenBSD. The corrected
test first checks retention, then drains its own fixture-generated instances
before restoring its mask. Production transport code is unchanged by this
correction; the final fingerprint delta is solely tests/test_script_fifo.c.
This agrees with the documented platform-dependent pending-instance behavior
of [OpenBSD sigwait](https://man.openbsd.org/sigwait.3) and the process/thread
pending sets described by [FreeBSD sigwait](https://man.freebsd.org/cgi/man.cgi?query=sigwait&sektion=2).

Evidence is retained under builddir/codex-2026-10-05/script-fifo, including the
frozen source, 160 source fingerprints, 1159 permission entries, initial failed
runs, native probes and final test/install archives. The frozen source extends
the verified colorset baseline with the FIFO implementation and targets.
Concurrent desktop/launcher/session work is excluded from this verification;
the build-definition projection and workspace differences are recorded.
All four isolated snapshot guests were shut down and their exact saved QEMU
processes exited. Other chats' guests and the Linux verification containers
were preserved. Cleanup records are retained with the evidence.

## Remaining scope

Other widget text/list indices, geometry/font arithmetic, modal servicing,
resource lifetime and budgets remain open in SCRIPT-01. Endpoint validation
does not authenticate a same-user helper's message content or provide a script
sandbox. Native Linux ASan/UBSan CI is configured but unrun; local Podman/QEMU
checks do not replace that gate. No complete interpreter-safety claim is made.
