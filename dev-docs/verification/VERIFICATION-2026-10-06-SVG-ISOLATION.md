# Isolated SVG decoding — 2026-10-06

Status: accepted bounded security checkpoint for IMAGE-01 / PROC-01 / LIB-04.
The user approved five seconds and 1024 MiB on 2026-10-05. Event-loop
responsiveness, wider decoder/storage isolation and native Linux release
sanitizer CI remain open. This follows the accepted
[primary-image/XPM checkpoint](VERIFICATION-2026-10-05-IMAGE-INPUT.md).

## Implementation and compatibility

The shared loader sends its already checked primary bytes, literal original
path/rendering options, scale and target geometry to a fresh private executable.
The worker owns librsvg/Cairo state and returns a small typed header plus ARGB
pixels. It performs no X server operations. The parent validates dimensions,
pixel counts and exact payload length before allocating/applying results.
Existing SVG/SVGZ, relative images, mirror/transpose/rotation/scale behavior,
color/alpha, image fallback and cache generations remain available.

The common checked process transport also serves compressed XPM. SVG has a
five-second monotonic deadline for startup, input/output, rendering, secondary
resources and direct-child completion. Partial/late results are discarded.
Shutdown cancellation terminates and reaps the owned child; unrelated BSD child
notifications and caller descriptors/signals are retained. A child alarm also
bounds the worker lifetime if its owner disappears. Arguments are literal;
there is no shell or PATH search. The versioned libexec worker path is compiled
in; FVWM_IMAGE_SVG_WORKER supplies a trusted absolute development/relocation
path. The installed parent manual documents this private helper.

The worker lowers both soft and hard memory bounds before exec: RLIMIT_AS on
Linux/FreeBSD, RLIMIT_DATA on OpenBSD. The default is 1024 MiB, retaining stricter
inherited bounds. Core dumps are disabled. Each setting is read back; startup
fails if finite containment cannot be verified. These policies are not identical
RSS limits or a whole-desktop memory cap. Finite build overrides are 1–60 seconds
and 128–4096 MiB. Existing 32768-per-axis/67,108,864-pixel color limits remain.

Checked floating/integer SVG option and dimension arithmetic removes
unrepresentable conversion/offset cases. Ordinary representable rendering and
rounding remain. A PNG signature cannot start SVG XML or gzip data: recognizing
that signature before worker startup avoids repeated useless process launches
when a malformed PNG falls through the existing decoder order. Other source
families/encodings retain the existing SVG attempt.

Qualification also found an inherited two-byte leak for the discarded `Style *`
identifier token. Its ownership is repaired without changing style behavior.

## Final qualification

All core C executables/modules compile and the final suites pass on seven legs:

| Target | Compiler | Full suite |
| --- | --- | --- |
| Linux ARM64 | GCC 13 | 88/88 |
| Linux ARM64 | Clang 18 | 88/88 |
| Linux AMD64, full-system QEMU | GCC 14 | 88/88 |
| FreeBSD ARM64 | native Clang 19 | 77/77 |
| FreeBSD AMD64 | native Clang 19 | 77/77 |
| OpenBSD ARM64 | native Clang 19 | 77/77 |
| OpenBSD AMD64 | native Clang 19 | 77/77 |

This totals **572 full-suite executions**. Go is disabled for this C/image scope.
Pure units cover streamed headers/payloads, early allocation refusal, exact
completion, literal forwarding, malformed-PNG no-spawn behavior, output/error
ownership, actual memory enforcement, inherited stricter limits, rejected limit
readback, core limits, the installed alarm and descriptor/child cleanup.
Direct renderer coverage includes valid transforms and numeric boundaries.

Exact staged installations pass **192 Linux image outcomes plus 15 SVG timeout/
recovery/cancellation outcomes**. Default BSD X passes **232 image, 20 SVG and
40 Script FIFO outcomes**. The installed default servers are FreeBSD scfb on
both architectures, OpenBSD ARM64 wsfb and OpenBSD AMD64 vesa, all depth 24.
The zero-monitor fallback remains exercised on wsfb. These are guest default
servers, not qualification of every physical graphics device.

The ARM64 no-Xcursor profile passes 12 focused entries, using actual one-second
and 128 MiB SVG overrides. Bitmap-only passes eight entries with SVG/PNG/XPM/
Xcursor/XRender disabled and existing one-MiB input/one-second decompression
limits. Local Clang ASan/UBSan passes nine scoped entries with leak detection and
halt-on-error. The production resource-limited worker and self-exec resource
fixture exclude ASan's huge shadow mapping; the parent/transport/protocol/client
and separate direct renderer are instrumented. This virtualized local profile
does not replace native Linux release sanitizer CI.

The final worker renders the manual's theme example in about 4 ms. A maximum
8192-by-8192 result (256 MiB ARGB) completes in about 0.25 seconds at 278492 KiB
peak worker RSS under the approved one-GiB/five-second policy. Measurements are
linked to its exact tested worker hash. This is a manual example and maximum
output check, not a survey or guarantee for third-party SVG themes.

All six archived targets match **1199 frozen file fingerprints and modes**.
All **15 installed C binaries per compiler leg**, including the private worker,
match the exact expected Meson RPATH transformation: **105 identities**. Patched
PNG profiles and the audited Debian PNG/XPM backport remain explicit; actual
loader/compiler/kernel metadata is archived. Pending Script geometry work and
concurrent desktop/tool changes are excluded from this frozen checkpoint.

## Environment and failed-attempt evidence

AMD64 Podman user emulation reports unlimited address space after a finite limit
is set. It cannot qualify this policy. The required production readback rejects
that condition. A new task-owned full-system Linux AMD64 QEMU guest with a real
Linux kernel verifies finite limits and failed over-limit anonymous allocation.
The official Debian cloud image was fetched over publisher HTTPS and checked
against SHA512SUMS; no detached signature is listed for that cloud image. Signed
apt packages and the previously authenticated private dependency profile supply
the build environment. Provenance, pinned SSH host identity and console are saved.

The shared ARM64 container had sleep as PID 1 and 2032 unreaped children at its
2048-process ceiling. It was left untouched. A private local profile container
uses podman-init; fresh full/optional/sanitizer/install gates pass without zombie
accumulation. This is fixture lifecycle evidence, not a production reaper fix.

Earlier failures are retained: test getrlimit64 injection initially missed the
Linux large-file alias; an interactive-placement fixture lacked deterministic
placement; a nonsquare renderer expectation ignored SVG letterboxing; new units
initially lacked OpenBSD's X11 header path; a follow-up Meson variable was wrong;
one installed Clang check started before staging finished. These fixtures/build
definitions were corrected. The leak-enabled shutdown run exposed the real
Style-token leak, which was repaired rather than suppressed.

OpenBSD AMD64's resource-cache test exceeded its 30-second fixture deadline
while launching SVG workers for repeated malformed PNG fallbacks. The measured
no-spawn PNG policy removes that work; the final full suite passes with the
same fixture deadline. Failed source/log archives remain separate from the final
accepted evidence. No failed or partial run is counted in final totals.

Actual generated roff and native BSD mandoc rendering contain both SVG build
options, defaults and the worker environment variable. Baseline/candidate have
the same two inherited Asciidoctor macro errors and 33 warnings; STYLE counts
are 25/26 and lint exits 3. This is a reviewed render, not clean manual lint.

## Evidence and remaining work

Artifacts: `builddir/codex-2026-10-05/svg-isolation/`: source-final.tgz,
source fingerprints/modes, owned-root-match.json, six final evidence archives,
four installed-default-X archives/server logs, accepted-results.json,
measurements-reviewed.json, manual-review.json, failed-attempt archives and
Linux guest/container provenance. validate.py independently checks final test
counts, source identity, staged bytes, actual library profiles and runtime
results. Original guests/shared containers are preserved.

The worker is not a filesystem sandbox and cannot promise immediate termination
of uninterruptible storage I/O. Relative-file permissions remain the user's.
**Parent SVG waiting is still synchronous.** Isolation and a five-second ceiling
do not meet the responsiveness goal; safe event-loop integration remains P1,
without recursive command/X lifecycle dispatch or inferred broad async changes.
Other decoder CPU/storage budgets, shared font arithmetic/lifetimes and native
Linux release sanitizer CI remain open. The separately
[approved Script geometry policy](../decisions/SCRIPT-GEOMETRY-PROPOSAL.md)
is the next bounded SCRIPT-01 batch.
