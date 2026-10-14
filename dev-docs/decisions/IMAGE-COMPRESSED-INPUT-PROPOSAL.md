# Compressed XPM input limits — approved

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Owner: IMAGE-01 / PROC-01. Regular-file inputs and the 128 MiB source cap are
approved. The user also selected 128 MiB expanded XPM and five seconds on
2026-10-05, with finite build-time overrides. The shared reader has passed its
[preparatory checkpoint](../verification/VERIFICATION-2026-10-05-IMAGE-INPUT-READER.md).
This decision covers the additional compatibility policy for compressed XPM,
not a new library or a broad asynchronous decoder rewrite.

## Approved limits

Use a **128 MiB expanded-XPM byte cap and a five-second overall
decompression deadline**, with finite build-time overrides. The deadline covers
input/output transport and direct-child completion, not just one poll. Refuse
oversized expanded data or timeout, discard partial data, clean up the owned
child/pipes and permit later image commands to recover. Approved finite override
ranges: 1–1024 MiB for source and expanded bytes; 1–60 seconds for decompression.

This can refuse valid compressed themes whose expansion exceeds 128 MiB or
whose decompression exceeds five seconds. The user explicitly approved these
separate limits. There is no remaining decision about their defaults; the
five-second Script GetOutput contract remains a separate setting.

## Implementation boundary

Preserve compressed XPM where the current platform/library supports it. Use the
existing system decompressor with a resolved executable and literal argv;
image names are not shell commands. Feed only checked input, count expanded
bytes, retain child identity until cleanup, close inherited descriptors and
check termination/status. Do not fall back to an unchecked original pathname.
The signed OpenBSD 7.9 libXpm source's `RdFToI.c` confirms the current `.Z`/`.gz`
pipe handoff and synchronous pipe reading. Other supported profile variants
need equivalent feature evidence before adoption.

PNG can consume a checked callback stream; SVG can consume buffered data with
its original base URI; XPM has a buffer API; Xcursor has callback read/seek APIs.
XBM needs a narrow buffer adapter with existing Xlib syntax/legacy bitmap parity.
Preserve current optional dependencies, decoder ordering and FVWM cache paths.
Those adapters belong to the approved acquisition change, with their native and
real-X compatibility tests; this proposal does not replace the format libraries.

Replacing existing unbounded decompressor waits with a deadline does not make
all image decoding nonblocking. Do not add extra unbounded waits or claim that
a five-second deadline meets desktop responsiveness by itself. Regular storage,
SVG secondary loads and decoder CPU/isolation retain their separate tracker
owners and architectural acceptance gates.

## Decision authority and acceptance

[AGENTS.md](../../AGENTS.md) says: “A specific compatibility break requires an
explicit user decision.” These expanded-size/time limits can reject valid legacy
assets, and the user has now approved the recommended defaults.

After selection: test valid compressed assets, exact/over expanded limits,
stalled/error/early-exit child cleanup, recovery, source replacement/growth,
format fallback, real color/alpha/scaling/cache behavior, SVG relative resources,
XBM legacy syntax and cursor metadata. Restore qualified patched profiles for
six-target decoder and installed BSD default-X gates. No production fix is
claimed by the preparatory reader tests.

## Accepted implementation checkpoint

The approved defaults, finite overrides, shared decoder handoffs and owned
subprocess cleanup are adopted and qualified in the
[image report](../verification/VERIFICATION-2026-10-05-IMAGE-INPUT.md).
SVG secondary-resource isolation/limits remain a separate pending decision.
