# Visual startup ownership — 2026-10-20

DATA-01 #6 / IMAGE-01 #8 / CLEAN-01 #34. Base: merged PR #131,
`7f6f8e04e`. Three related startup ownership improvements; combined qualification
is pending. ROOT's separately owned runtime/index work is preserved.

## Operations and behavior

1. Module initialization admits a complete nonzero 32-bit hexadecimal
   visual/colormap pair before using either value. Canonical plain hexadecimal
   producer output remains supported, with optional 0x/0X input. Incomplete,
   empty, zero, signed, whitespace, trailing or overlong values use screen
   defaults with a diagnostic, without a visual query.
2. XGetVisualInfo selects the display's default screen and requested ID. Null,
   empty or unusable replies retain complete defaults. The reply array is
   released once, while the copied Visual pointer remains Display-owned.
3. White/black initialization commits both pixels only on complete success.
   White failure allocates no black; black failure releases the acquired white
   reference. Failure retains previously published pixels. Native module
   constructors then select and save complete screen defaults before color
   setup. The core caller stops startup explicitly on refusal instead of using
   undefined color outputs. Default/root initialization uses server pixels.

Well-formed colormap IDs are not proof that a live colormap exists or matches
its visual. Existing asynchronous X error policy remains authoritative; no
probe/barrier/global error handler is added. Successful startup still retains
its two allocated color references as before. This batch does not redesign
repeated successful initialization or color-table lifetimes. Temporary visual
switch semantics, global state owners, configuration options, native module
packets, Perl metadata, dependencies and main-loop ordering remain unchanged.

## Baseline and focused evidence

An isolated baseline probe compiles the unchanged merged PictureBase.c and
matching header with the real native flags, separate object/dependency outputs
and current library dependencies. Four separate real-X processes reproduce:
missing map and unknown visual both terminate with SIGSEGV; a valid visual
acquires one reply and releases zero; color refusal leaves a private visual and
refused output pixels. The latter two fail independent assertions. These are
ordinary startup defects; no remote exploitability claim is made.

Candidate native pure coverage checks 32 admission/reply/color observations,
including both failure stages and unchanged existing pixels. Real X covers
successful private-map window creation, white/black refusal fallbacks and an
unknown visual. Six focused Linux GCC entries pass: picture-visual,
picture-visual-xvfb, resource-cache-xvfb, script-geometry-xvfb,
iconman-transport-xvfb and buttons-transport-xvfb. Six exact binary identities
and source bytes/modes bind retained results for the combined checkpoint.

Preparation failures are retained: the initial Linux storage preflight refused
below its unchanged floor; the first focused compile succeeded but test
selection named an unregistered font test and executed no tests. The corrected
focused selection passes. The first isolated baseline compile used the changed
header and failed; its corrected matching-header control reproduces all four
baseline failures. No production fix or deadline change was inferred from
these tooling mistakes.

## Periodic performance

The first paired [reference](PERFORMANCE-2026-10-20.md) and
[receipt](PERFORMANCE-2026-10-20.json) use already built PR131/current GCC
binaries. Idle command medians are 0.406/0.411 ms; mixed loaded observations
remain explicit. Peak RSS is 7,200/7,192 KiB; GNU size text grows by 8,824 bytes,
with data/bss unchanged. There is no steady-state speedup or shrink claim.
The opt-in tool adds no benchmark registration or per-batch timing gate.
The tracker records the next regular comparison after five merged checkpoints.

## Combined checkpoint

Pending: one six-target/ten-profile combined checkpoint, relevant scoped guest
sanitizers, fresh install/default-BSD-X and portable/local review checks. Existing
OpenBSD AMD64 CI asset/SVG/legacy-property omissions and full-install/two staged
geometry gaps stay explicit without retries. Native-CPU Linux sanitizers,
physical hardware, toolkit/compositor and release qualification remain separate.

Canonical evidence is retained under
`builddir/codex-2026-10-20/visual-startup-ownership/`. Storage retirement is limited
to completed duplicate captures after full host gzip/CRC and native payload
hash/size agreement; canonical host archives, source/build/stage/VM trees and
another owner's work are preserved. The absent PR129 duplicate fails its guard
without deletion. PR131 and PR118 completed duplicates supply the required
space; the unchanged 1-GiB source/build and 256-MiB archive floors remain.
