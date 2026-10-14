# Bounded image reader — preparatory checkpoint

Owner: IMAGE-01 / DATA-01. The user approved regular-file image sources, a
128 MiB default source cap and a build-time override on 2026-10-05. This
checkpoint implements and verifies the shared acquisition primitive. Production
image decoders are **not connected to it yet**; the earlier FIFO availability
finding remains open. No installed WM behavior or format support changed.

## Source and behavior

`libs/image_input.c` / `image_input.h` provide an operation-owned immutable byte
snapshot. Open uses nonblocking, no-controlling-terminal and close-on-exec flags;
checked descriptor flags and fstat precede reads. The descriptor must be a regular
file. Symlinks to regular files and shared/hard-linked theme files remain valid.
The reader never reopens the pathname after validation.

The caller supplies the finite byte limit. The approved default is defined as
128 MiB; production configuration/override plumbing accompanies decoder adoption,
not this unconnected helper. Declared size is an early refusal check. Actual
reads enforce the cap independently, including growth after fstat. Exactly at
the cap, a one-byte probe distinguishes EOF from overflow without growing the
allocation. Buffer growth and the terminating byte use checked arithmetic;
reads use at most 64 KiB. Embedded NUL bytes are preserved in the counted data.

Interrupted reads resume; short reads continue; other read, flag, stat,
allocation and close errors fail with empty outputs. Failure preserves its errno
through cleanup. Closing is not retried because EINTR ownership differs by
system. The snapshot becomes visible only after a successful close. Destruction
is idempotent. This does not bound regular-storage latency, decoder allocation
or CPU, compression expansion, SVG secondary resources or whole-process memory.

## Actual verification

Evidence: `builddir/codex-2026-10-05/image-input-reader/`.

- Seven root Meson `image-input` executions pass on Linux ARM64 GCC/Clang,
  Linux AMD64 GCC, FreeBSD ARM64/AMD64 and OpenBSD ARM64/AMD64.
- The same seven compiler legs pass the standalone development fixture with
  C11, warnings enabled and warnings treated as errors.
- Local Linux ARM64 Clang ASan/UBSan with leak detection passes this reader
  fixture. Native Linux **release CI remains unrun**. Linux AMD64 is emulated;
  BSD AMD64 uses QEMU TCG and BSD ARM64 uses QEMU/HVF.
- All six evidence archives match all 1178 frozen source fingerprints. The
  accepted PNG-cleanup snapshot was the base; overlays are limited to the reader,
  its unit fixture and a scoped Meson test registration. Other-thread changes
  were excluded. Every compiler leg records its test-binary digest and command.

The fixture covers empty/short/exact-limit/over-limit data, the default 128 MiB
oversize refusal before allocation, multi-chunk growth, binary data, regular
symlinks/hard links, empty FIFOs and symlinks to them, directories, devices,
sockets and missing files. It replaces the pathname after opening and verifies
that the old inode's contents are read. It also injects post-stat growth/shrink,
EINTR, short reads, open/flag/stat/read/close errors and initial/growing allocation
failures. Every opened descriptor closes once; failed snapshots release their
owned allocation. An alarm bounds the FIFO regression.

Initial BSD runs failed a Linux-specific expected socket-open errno. Captured
BSD diagnostics show EOPNOTSUPP; Linux uses ENXIO. The fixture now expects the
platform's error while requiring refusal and cleanup. Production code was not
changed for that discrepancy. Earlier failure logs are retained. The first
runner hid captured assertion stderr; the corrected runner retains it.

This policy-only Meson profile explicitly disables optional PNG; no third-party
decoder is invoked. The fresh BSD snapshots use their original base images, not
the previous turn's discarded signed-update/private-PNG profiles. These passes
are not new package, installed/default-X, theme or full-suite qualification.

## Remaining adoption gate

Connect PNG, SVG, XPM, XBM and cursor handoffs without unchecked pathname
reopening, preserve decoder/fallback order, SVG options/base paths, compressed
XPM and cursor hotspots/animation, and add the approved finite build-time
source override. Then run actual decoder/refusal/recovery, scale/cache and
installed default-X gates on restored patched six-target profiles.

Compressed XPM currently delegates expansion to external utilities via libXpm.
A compressed-file cap does not bound expanded bytes or completion time. The
[compressed input decision](../decisions/IMAGE-COMPRESSED-INPUT-PROPOSAL.md)
records the remaining compatibility choice. Avoid a partial production guard
that permits an unchecked fallback reopen and labels the original finding fixed.

Checkpoint cleanup: all four task-owned QEMU snapshots were shut down, remaining
QEMU processes were released only by exact saved argv identity, and exit was
verified. Original guests, concurrent desktop builds and the Linux containers
were preserved. `guest-release.json` records the cleanup.

Subsequent checkpoint: production decoder adoption, expanded-XPM limits and
real-X/six-target qualification are now accepted in the
[integrated image report](VERIFICATION-2026-10-05-IMAGE-INPUT.md). The preparatory
status and smaller source count above describe this earlier checkpoint only.
