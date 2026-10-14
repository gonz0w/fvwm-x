# Root XOR-tile owner preparation — 2026-10-06

Tracker: IMAGE-01 / INPUT-01. Status: implemented; focused checks pass;
combined qualification and production asynchronous activation remain pending.

XorPixmap now holds a pending request separately from its immutable published
tile. It borrows the acknowledged root-visual picture directly for XorGC;
the extra XCreatePixmap/XCopyArea allocation/copy is removed. The picture retains
its color ownership until replacement/reset. Pending or refused replacement
retains the previous tile. XorValue, empty XorPixmap and superseding requests
cancel interest before releasing the old owner.

Frozen source: `builddir/codex-2026-10-06/svg-root-owner/source-root-owner-final.tgz`.
SHA-256: `2a933175d61a4b59b1d8216380f31e5f10e9c5bf64b88df73816d2844cf015d2`.
All 1239 source contents/modes match both tested trees. This extends the file-icon
checkpoint with move_resize.c, the direct root-owner probe and scoped Meson
registration. Concurrent desktop/tooling changes remain excluded.

Normal and sanitizer archives are `evidence-normal.tgz` and
`evidence-sanitizer.tgz` in the same directory. Metadata records exact options
and build identities; the tested probe uses unchanged production objects with a
substitute process entry. These are not installed-artifact identities.

| Profile | Entry | Result |
| --- | --- | --- |
| Ubuntu Linux ARM64, Clang 18.1.3, private PNG 1.6.59 | root-image-owner-xvfb, 24/8-bit | 1/1 pass |
| Same source, scoped ASan/UBSan with leak detection | same probe, 24/8-bit | 1/1 pass |

The probe observes the GC's actual tile, its pixel color, old pixmap identity
while pending/refused, absent first load, cancellation through reset and a new
successful owner. No unexpected X errors remain. It uses the matching
unsanitized finite-memory synthetic worker; the parent and production-object
probe are instrumented. No BSD, production activation, interactive XOR drawing,
non-default WM visual or native release sanitizer CI result is claimed here.
Those remain combined qualification gates.
