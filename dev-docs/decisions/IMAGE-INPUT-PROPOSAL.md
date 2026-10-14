# Image input policy — approved

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Owner: IMAGE-01 / DATA-01. Status: the user approved this contract on
2026-10-05 (“i like your proposal”). Production adoption is qualified in the
[image checkpoint](../verification/VERIFICATION-2026-10-05-IMAGE-INPUT.md). Existing
PNG recovery, patched dependencies and header cleanup are separately accepted.

## Observed behavior

A task-owned empty FIFO named `waiting.png` was supplied to a colorset command
in the patched staged production WM on Linux ARM64. The initial command barrier
passed. The following barrier remained unanswered during a controlled two-second
observation; the live core's kernel wait state was `wait_for_partner`. The image
loader opens the supplied path with blocking stdio. The bounded harness then
closed its own processes; no existing desktop or external machine was involved.
This demonstrates an image-source availability problem independent of malformed
image bytes or library advisories. BSD behavior has not been separately measured
for this finding; shared source inspection identifies the same acquisition path.

Evidence: `builddir/codex-2026-10-05/image-input-review/`, with the controller,
logs, accepted observation and the exact qualified staged-core identity. This
is diagnostic evidence, not a claimed fix or an adversarial exploit demonstration.

## Recommended input contract

Use regular-file image sources with a default 128 MiB source-byte cap. A symlink
to a regular file remains usable: validate the opened descriptor, not the link
itself. Refuse FIFO/device/socket/directory inputs before handing them to a
third-party parser. Provide a build-time size override, with an explicit bounded
range documented at implementation review. Preserve ordinary themes, PNG/XPM/SVG
and XBM images, compressed XPM support and SVG relative resources where already
supported. Do not silently remove optional formats or replace their libraries.

The policy deliberately changes handling of special files and oversized image
sources. The present 32768 dimension / 64-million-pixel checks remain independent;
a file-byte budget can refuse an otherwise accepted image with large pixel data
or ancillary metadata. The user approved that compatibility tradeoff.

Implementation must retain the opened input identity through parsing, or hand a
bounded immutable snapshot to APIs that only accept buffers. A pathname-only
preflight followed by an unchecked reopen is insufficient. Count actual reads
and handle growth, short reads, arithmetic, close errors and failure outputs.
Compressed formats and nested SVG resources need their explicit owning limits;
a compressed-file size alone does not bound decompression or secondary loads.
Keep cache paths/generations and existing diagnostics/format ordering consistent.

This contract reduces uncontrolled acquisition and allocation. It does not make
all regular-file I/O or decoder CPU work nonblocking: slow storage, compression
and SVG complexity still require measured budgets and possibly a bounded
isolated helper. That remains a separate architectural step; do not add a broad
worker/decoder rewrite as part of this policy approval.

## Acceptance after approval

Implement one reviewable acquisition change at a time, starting with the proven
blocking source. Pure policy requires native tests for type/size boundaries,
opened-descriptor identity, growth/read failures and ownership. Actual image
loads need real-X refusal/recovery and valid color/alpha/palette/scale/cache
coverage, compressed/relative-resource compatibility, six-target source/runtime
qualification and installed BSD default servers. Do not reclassify the earlier
PNG package gates as coverage for this future implementation.

## Decision authority

[AGENTS.md](../../AGENTS.md) says: “A specific compatibility break requires an
explicit user decision.” The new file-type and byte limits intentionally refuse
some existing input paths/large files. That contract is now approved. Broad decoder isolation and new compressed-input
deadlines remain separate decisions; they are not implied by this approval.
