# Colorset image dimensions and effect controls — 2026-10-06

Tracker: IMAGE-01 / INPUT-01. Status: implemented, focused checks passed;
combined qualification and production asynchronous activation pending.

A valid 4096×2 deferred background reproduced a zero-width module packet. The
same 12-bit fields also truncated shape dimensions. The four image/shape extent
fields now retain all 16 X11 bits; the 20-field text packet and Perl metadata
order are unchanged. Invalid extents above 65535 are refused before allocation
or mutation. Zero remains available for absent resources. The independent probe
now sees 4096×2 both in packets and actual background/shape geometry at 24/8-bit.
The next [checked packet checkpoint](VERIFICATION-2026-10-06-COLORSET-PACKET.md)
closes numeric-overflow loopholes in this initial scanf-based range check.

## Exact source and focused gates

Frozen source: `builddir/codex-2026-10-06/svg-colorset-geometry/source-colorset-geometry-final.tgz`.
SHA-256: `21488944193276e09bb6b4f99293f8e9c7d641640e4fb7c9b89b471dcd0919c0`.
It extends the Average checkpoint with Colorset.h/Colorset.c, the colorset API
unit, deterministic worker fixture and actual-core colorset observer. Metadata
verifies all 1243 file hashes/modes and seven built binary identities per profile.
These are local build identities, not staged installs or release qualification.

| Profile | Focused result |
| --- | --- |
| Ubuntu ARM64, Clang 18.1.3, PNG 1.6.59, XRender | 4 Meson entries plus direct 8-bit colorset run pass: 5/5 |
| Same source, scoped local ASan/UBSan | 2 native policy entries with leaks enabled; 2 actual-core entries plus direct 8-bit run with core leaks disabled pass: 5/5 |
| GCC 13.3.0, Xcursor enabled, WM XRender disabled | Same 4 entries plus direct 8-bit software-rendering control pass: 5/5 |

The four entries are colorset-api, picture-average, image-colorsets-xvfb and
image-owners-xvfb. Finite-memory workers remain unsanitized with instrumented
parents. Evidence archives beside the source are `evidence-normal.tgz`,
`evidence-sanitizer.tgz` and `evidence-software.tgz`. Per-profile metadata, build
logs, final test logs and independent X artifacts are included. Optional GCC's
first compile lookup preceded target registration; explicit incremental Meson
reconfiguration registered the new Average target before final acceptance.

The failing 4096-pixel control is retained as `source-reproducer.tgz`
(SHA-256 `065d60389a349287b085dbcfb273e3e0553589e4c3035e6919090035496ae308`)
and `evidence-reproducer.tgz`. Final normal/sanitizer/software logs use the
`fvwm-colorset-geometry` prefix, including separate direct 8-bit logs.

## Existing tint and alpha observations

An independent XGetImage/XQueryColor probe observes native red PNG colorsets
with Tint green 50, and Tint green 50 plus ImageAlpha 50 over blue. These are
controls for existing rendering, not a newly implemented compositor:

| Path/depth | Tint RGB16 | Tint plus alpha RGB16 |
| --- | --- | --- |
| XRender / 24-bit | (32896,32639,0) | (16448,16191,32896) |
| XRender / 8-bit | (43690,21845,0) | (21845,0,43690) |
| Software / 24-bit | (32639,32639,0) | (16191,16191,32896) |
| Software / 8-bit | (21845,21845,0) | (21845,21845,43690) |

A scalar source-RGB preview matches the software 24-bit case but differs from
XRender rounding and is not a final 8-bit palette result. It must not be claimed
as a verified replacement. Existing software blending uses queried 16-bit server
colors after source palette reduction and later allocates output colors without
image dithering. Preserve those contracts when moving work out of the core.

Whole-image software readback/blending and derived-resource acknowledgement
remain open. Production asynchronous loading stays off; full Linux/BSD,
installed/default-server, mixed-monitor and native release sanitizer gates remain
pending for the combined checkpoint.
