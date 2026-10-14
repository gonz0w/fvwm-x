# Asynchronous image preparation — 2026-10-06

Owner: IMAGE-01 / INPUT-01. Status: preparation verified; core adoption remains
incomplete. The optional loader is not installed by core startup. Production SVG
calls still wait synchronously. This report does not claim improved running-WM
responsiveness, full-suite acceptance, staged install parity or default BSD X
qualification for asynchronous behavior.

## Frozen scope and evidence

The candidate overlays 32 owned sources onto the accepted font-shadow source,
with scoped library/test build-graph additions. All six guest source trees match
1222 file hashes and modes; the 32 root-owned sources also match. This preserves
concurrent desktop/tooling work outside the candidate. The source archive SHA256
is `c3954a7eebe7185b73064af0745b42fbe388010d9f62bf9cdd67b92f4243fb41`.

Evidence is under `builddir/codex-2026-10-06/svg-async/`: `source-current.tgz`,
`source-fingerprints.json`, `source-modes.json`, `owned-files.json`,
`preparation-results.json`, `sanitizer-results.json`, the six preparation evidence
archives and the separate sanitizer evidence archive. The private runners and
integration notes remain there. Preparation results explicitly mark full suites,
staged installs and actual WM responsiveness as unverified.

## Implemented foundation

Shared literal start/step/cancel process transport replaces duplicated child and
pipe management. Service performs at most one 64 KiB read/write, exposes ordinary
readiness/deadlines, and observes only its owned child. SIGCHLD is not blocked for
an asynchronous worker's lifetime. Shutdown transfers final reap to the installed
FVWM handler without waiting or keeping a cancellation target after reap.

Owner-specific views retain immutable previous backing. Equivalent cache
requests coalesce; queued entries retain keys and opened-identity expectations,
while only the active request acquires full source/output buffers. Nested service
pumps transport only. Outer conversion uses small ordinary XImages, with tiles of
at most 16384 pixels, or 256 on <=8-bit visuals. Commit scans retain and rotate at
most 32 views. Deferred owners do not force a polling loop.

An unmapped private InputOnly window provides property acknowledgements for
bounded X request ranges. Publication waits for server processing, including
resource creation and cleanup. Owned errors retain old/absent images; unrelated
errors continue to the ordinary handler. Synthetic and wrong-serial events cannot
acknowledge a result. No production XSync or XGetGeometry wait is added.

The allocation-free 16-byte source probe shares checked regular-file opening with
full snapshots. Definite PNG and readily identified ordinary XPM/XBM retain
immediate loading. Ambiguous inputs use the original active snapshot and preserve
format preference. Compressed XPM expansion uses the finite asynchronous
collector; refusal still permits SVG with its original bytes and SVG limits.
Non-SVG fallback cannot start another decompression wait. The extra timing change
is proposed in [the format decision](../decisions/IMAGE-ASYNC-FORMAT-PROPOSAL.md).

## Checks completed

All core/modules compile in seven compiler legs. Each leg passes eight focused
entries: image-input, image-decompress, image-process, image-process-async,
image-svg-protocol, image-svg-client, picture-argb-xvfb and picture-async-xvfb.
That is 56 focused executions, with nested X profiles included within their test
entries; it is not 56 full suites or 56 individual X scenarios.

| Environment | Compiler legs | Focused passes | XPM profile |
| --- | --- | --- | --- |
| Linux ARM64 container | GCC, Clang | 16 | Enabled |
| Linux AMD64 QEMU | GCC | 8 | Enabled |
| FreeBSD ARM64 QEMU | Native Clang | 8 | Disabled; refusal checked |
| OpenBSD ARM64 QEMU | Native Clang | 8 | Enabled |
| FreeBSD AMD64 QEMU | Native Clang | 8 | Disabled; refusal checked |
| OpenBSD AMD64 QEMU | Native Clang | 8 | Enabled |

PNG, SVG, Xcursor and XRender are enabled on these profiles. ARGB comparison uses
24-bit, 8-bit and strict 8-bit nested X; ownership/scheduler fixtures use 24-bit
and 8-bit nested X. They cover legacy pixel/alpha/mask comparison, bounded work,
first/previous views, coalescing, cancellation, source replacement, generation
discard, independent property events, server pixmap refusal, shutdown and format
fallback. PNG-named SVG remains immediate. XPM-enabled profiles check compressed
precedence and original-source SVG fallback after expansion refusal.

Seven local Linux ARM64 Clang ASan/UBSan preparation entries pass with leak
detection enabled, against the same frozen hashes/modes. They include the above
focused tests except the real resource-limit self-exec image-process test, which
passes natively; ASan shadow address space does not fit that fixture's finite
limit. The synthetic SVG output fixture is unsanitized so the sanitized parent
can test its ownership within the worker limit. These are scoped local guest
checks, not native Linux release sanitizer CI or a full-project sanitizer pass.

## Retained findings, failures and limits

The accepted renderer's <=8-bit no-limit path allocated through the limited
allocator but recorded no-limit release ownership. A private baseline control
produces X_FreeColors BadAccess; the corrected candidate control passes. Effective
strict-palette ownership is normalized, and duplicate releases are bounded. This
restores existing flags rather than adding a theme limit. Broad staged acceptance
of this shared library correction is still required.

Preparation exposed an incorrect worker fixture argc and an invalid XBM fixture
line layout; both fixtures were corrected without changing their production
contracts. Linux fortified prefix reads enter __read_chk, so fault injection now
covers that entry without disabling fortification. Missing XPM on FreeBSD now
checks refusal instead of incorrectly expecting an optional format to succeed.
Overlapping private AMD64 runs collided during configuration; exclusive per-guest
locks now protect source sync/build/test, and affected gates were rerun. Failed X
fixture logs are retained in the evidence archives.

The earlier 32768-wide legacy comparison receives an X server allocation refusal;
parity uses width 32767 and a separate asynchronous 32768 refusal fixture. Decoder
limits remain unchanged. The synchronous worker, its actual resource-limit and
relative-resource qualification remain the previous accepted SVG checkpoint.

Next required work: the ambiguous-image timing decision; core readiness/error/
event/shutdown adoption; colorset, decoration, scale-variant, menu, icon, cursor
and XorPixmap lifetimes; absent-first drawing guards; actual WM input/commands/
timers through slow loads; stale-owner/scale, mapped-menu and drag snapshots;
full seven-leg suites, exact staged installs, default BSD X and optional profiles.
Primary storage latency, broader non-SVG decode work, native release CI and
physical hardware remain open.
