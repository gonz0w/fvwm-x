# Property, descriptor and compatibility verification

This batch advances DATA-01 and FD-01 and establishes COMPAT-01's representative
baseline in [the tracker](PROJECT-TRACKER.md). It does not complete the whole
security audit or remove the modal Move/Resize loops.

## Changes verified

- EWMH reads request a bounded number of wire bytes, check declared type and
  format, reject incomplete replies, and use checked sizes before copying.
  Format-32 Xlib longs are packed into uint32_t values; failure outputs reset.
  Text is capped at 64 KiB, ordinary lists at 16 KiB, icons at 64 MiB + 64 KiB.
  Empty text/lists remain valid. Fixed records require complete values.
- Short struts can no longer reach a four-value consumer. _NET_WM_PID now uses
  CARDINAL/32, as specified by the
  [EWMH window-properties contract](https://specifications.freedesktop.org/wm/latest/ar01s05.html).
  The bounded request length and LP64 conversion follow the
  [Xlib property API](https://xorg.freedesktop.org/archive/current/doc/libX11/libX11/libX11.html).
- Motif and Open Look properties are validated before native-long indexing.
  Invalid Motif replies are freed. Existing four-field Motif records remain
  supported alongside five-field records; Open Look attributes accept three
  or five fields. Decoration-add/delete lists retain the existing 32-item cap.
- ICCCM MULTIPLE requests require complete atom pairs, declared type/format
  and at most 128 pairs. Invalid requests are refused. Valid requests process
  pairs in order, mark failed conversions, and acknowledge the result property.
  This follows the [ICCCM selection contract](https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.pdf).
- Both core module pipes are marked close-on-exec before fork. Children clear
  that flag only for their protocol endpoints; failed exec uses _exit.
  Read/PipeRead streams stop before executing commands if flag setup fails.
  The shared logging descriptor remains an intentional module contract.
- MFL lockfiles reject symlinks, hard links, foreign ownership, nonregular
  objects and group/other permissions. Nonblocking open prevents FIFO stalls.
  Retaining the private lockfile avoids splitting the lock across two inodes
  during shutdown/restart. Direct signals now exit libevent instead of merely
  setting an unused termination flag. Socket cleanup precedes process exit.

## Results

| Target | Completed full suite | Final affected tests | Staged install |
| --- | --- | --- | --- |
| Linux ARM64 GCC | 40/40 | 4/4 | Passed |
| Linux ARM64 Clang/lld | 40/40 | 4/4 | Passed |
| Linux AMD64 GCC | 40/40 | 4/4 | Passed |
| FreeBSD 15.1 ARM64 | 31/31 | 4/4 | Passed |
| FreeBSD 15.1 AMD64 | 31/31 | 4/4 | Passed |
| OpenBSD 7.9 ARM64 | 31/31 | 4/4 | Passed |
| OpenBSD 7.9 AMD64 | 31/31 | 4/4 | Passed |

Two new native tests exercise property policy and descriptor/exec/lock ownership.
Three new live-X tests exercise property handling, lifecycle and compatibility.
The final MFL lock/signal corrections followed the full suites; all four affected
live-X tests were rerun on every target. A subsequent FreeBSD ps-format correction
was confined to the probe and verified there; the portable probe also passes
Linux and all default BSD servers. Staged installs were refreshed afterward.

All three new live-X checks pass on the four default BSD X servers: FreeBSD
scfb, OpenBSD AMD64's default Xenocara server, and OpenBSD ARM64 wsfb with the
root-sized fallback. Archives retain server and core logs and JSON observations.

The compatibility fixture checks decoration recreation, vector buttons,
colorsets, styles, themed menu popup/dismissal, server-generated mouse and key
bindings, complex-function resize/move geometry, quoted nested Read, PipeRead,
reload ordering and inherited stream descriptors. A 200x120 client has a
206x150 themed frame; the function produces a 280x160 client and a 286x190 frame
at 120,90, retained across reload.

The lifecycle check rejects a symlink lock without modifying its target,
starts successfully afterward, checks Module/ModuleListenOnly descriptors,
survives failed exec, rejects a duplicate MFL, retains the lock inode through
KillModule restart, and exits/restarts after direct SIGTERM.

Two negative controls fail pre-fix AMD64 binaries: malformed Motif properties
produce incorrect geometry, and direct SIGTERM leaves an MFL connection open.
These failures are retained as regression evidence, not passing gates.

## Responsiveness and remaining work

The new interaction diagnostic uses real server-generated pointer input. A
100 ms scheduled Echo and an independent module Echo both remain stalled during
approximately 400 ms opaque Move and Resize holds. Each resumes after release.
The enclosing function's continuation also waits for release, which is the
compatibility behavior the refactor must retain. --require-responsive fails
deliberately on this baseline; it is not registered as a passing Meson test.

Native Linux ARM64 GCC, private Xvfb, 64 immediate and 64 scheduled samples per
case, 10,000 requested load updates/second, after BSD guests were shut down:

| Load | Median command reply | Median 10 ms scheduled reply | WM CPU |
| --- | --- | --- | --- |
| Idle | 0.316 ms | 12.391 ms | 2.47% |
| Title changes | 0.252 ms | 11.539 ms | 3.91% |
| Resize requests | 2.895 ms | 14.491 ms | 31.39% |

An earlier sample in this turn measured 11.932/19.032 ms for resize load.
Both samples are retained. Local variation does not establish a causal speedup
or regression against the earlier batch; no release threshold is inferred.

Inspection of the Linux ARM64 GCC ELF confirms PIE, a non-executable GNU_STACK,
GNU_RELRO with BIND_NOW, and __stack_chk_fail references. These observations do
not substitute for native sanitizer or fuzz coverage.

Remaining DATA-01 boundaries include legacy name/class/protocol/session reads
and scalar-property validation. ICE/session descriptor audit remains open.
Property reads are still synchronous Xlib calls; the large icon cap preserves
existing supported icon sizes and can still incur substantial allocation and
copying. Image isolation and modal-loop removal remain tracked work.

## Retained evidence

- Linux: `builddir/codex-2026-10-04/hardening/{arm64,amd64}-evidence.tar.gz`.
- BSD: `builddir/qemu-bsd/{freebsd,openbsd,freebsd-amd64,openbsd-amd64}/hardening-evidence-2026-10-04.tar.gz`.
- Archives retain full-suite and follow-up logs, installs, default-server
  checks, independently computed production-source fingerprints, and Linux
  negative controls, ELF inspection and native interaction/load samples.

All four QEMU guests were shut down cleanly after archive retrieval. Linux ASan
CI remains configured but unrun, and this checkout has no Git remote. Arbitrary
third-party themes, physical GPU/input hardware and compositors were not covered.
