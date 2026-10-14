# Checked colorset packet numbers — 2026-10-06

Tracker: IMAGE-01 / INPUT-01. Status: implemented, focused checks passed;
combined qualification pending.

The dimension follow-up reproduced scanf conversion overflow: an unsigned width
above its destination type wrapped to zero and bypassed the X11 extent guard.
LoadColorset now checks all 20 hexadecimal fields using strtoul before allocation
or mutation. It rejects overflow, negative unsigned fields, incomplete packets
and token suffixes. The first nine pixel/pixmap fields keep native unsigned-long
width; remaining attributes must fit unsigned int, and image/shape extents must
fit 16 bits. DumpColorset order, valid packet ABI and Perl metadata are unchanged.
Other small attribute bitfields retain their previous narrowing behavior; this
is not a claim that every enum or percentage domain has been hardened.

The tests demonstrate existing-record refusal without mutation and new-record
refusal without allocation. Large valid dimensions still round-trip, colorset
limits/fallback still pass, and actual-core background/shape ownership passes.

## Exact source and focused gates

Frozen source: `builddir/codex-2026-10-06/svg-colorset-packet/source-colorset-packet-final.tgz`.
SHA-256: `3050b2bc50ba455b094c637c1a211dd29dfc95421b8258c77ac563d9252e680c`.
Only libs/Colorset.c and tests/test_colorset_api.c extend the qualified focused
[dimension checkpoint](VERIFICATION-2026-10-06-COLORSET-GEOMETRY.md). Metadata
verifies all 1243 file hashes/modes and six built binary identities per profile.

| Profile | Focused result |
| --- | --- |
| Ubuntu ARM64, Clang 18.1.3, PNG 1.6.59, XRender | colorset-api, image-colorsets-xvfb and image-owners-xvfb pass: 3/3 |
| Same source, scoped local ASan/UBSan | Native policy with leaks enabled and two actual-core entries with core leaks disabled pass: 3/3 |
| GCC 13.3.0, Xcursor enabled, WM XRender disabled | Same three entries pass: 3/3 |

Evidence beside the source: `evidence-normal.tgz`, `evidence-sanitizer.tgz`,
`evidence-software.tgz`. It includes exact metadata/build logs, packet policy
results and actual-core X artifacts; acceptance logs use `fvwm-colorset-packet`.
Source line width/trailing whitespace checks pass for additions. The failure is
preserved in `source-reproducer.tgz` (SHA-256
`6f1681d93869bd0ff4d94db4a1561fff14f680df0224dd407030a9dc3b7d1b44`)
and `evidence-reproducer.tgz`.

This changes malformed input refusal, not a valid theme command. It adds no
runtime library, worker or blocking work. Production asynchronous activation,
full six-target/install/default-X checks and native Linux sanitizer release CI
remain pending for the combined checkpoint. Earlier 24/8-bit dimension/effect
observations are tied to their preceding frozen source, not relabeled as this
checkpoint's results.
