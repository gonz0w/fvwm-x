# Accumulated collection platform and theme checkpoint

Status: **passed**, covering merged PR167–169 before the next implementation batch.
[Source-bound receipt](VERIFICATION-2026-10-23-COLLECTION-CYCLE.json).

The affected checkpoint composes **49 selected native entries on six targets /
seven profiles**: 35 fresh Linux AMD64 GCC/Clang, FreeBSD AMD64 and OpenBSD 8.0
ARM64/AMD64 entries, and 14 exact retained Linux/FreeBSD ARM64 entries from PR169.
The latter's runtime, policy fixture, manual and build inputs are identical;
only developer documentation and the extended theme fixture differ. Five Go
packages/36 top-level cases pass per target; the retained Linux race run gives
252 total top-case executions. Three leak-enabled scoped sanitizer policy
entries and the sanitized native consumer retain their exact matching inputs.

The six native browser drivers pass. Three additional Linux runs load **Slate
dark, Dune light and classic/minimal larger-font legacy colors** on the same
qualified binaries. Slate/Dune use the real shipped config/desktop Read paths
and configured FvwmDesktop palette owner, not a recolored isolated fixture.
Classic uses explicit Form Fore/Back/ItemFore/ItemBack, 9x15 font and changed
borders; Slate/Dune use xft Sans size 10. Inputs, generated configurations,
font requests, scale 1 and 1440x1000x24 server geometry are preserved. Each run
checks list/grid, empty/4096 entries, escaped names, focus/selection, mixed input,
keyboard, range limits, pointer/scroll and view switching. Eleven screenshots
per theme are captured; grid, selected/focused list and 4096-entry grid views
are inspected for clipping, fit and contrast. This is representative smoke
coverage, not the broader theme/scale/accessibility acceptance matrix.

All four fresh archives validate complete gzip EOF/CRC, exact capture
inventories and every payload hash/mode. Retained source/binary evidence is
bound to the PR169 report. OpenBSD's restored compiled cache also carries an
older browser-cycle capture: both manifests validate, but only the new
collection-cycle entries count as fresh. No product or fixture failure occurs
in this cycle. A verifier initially expected one capture manifest; it was
corrected to verify both and select the new result namespace, without rerunning
passing guest work.

The independent periodic comparison is recorded in
[PR169 performance/size observations](PERFORMANCE-2026-10-23-PR169.md).
PR169 becomes the integrated and performance reference (0/3 and 0/5 merged).
Full install/default-X/full compiler-feature and owner/release acceptance remain
at their boundary: no installation/discovery/server contract changes here.
Native-CPU sanitizers, hardware, toolkit/compositor and assistive checks remain
separate. The retained sanitized Form UI excludes the already recorded
fontconfig exit allocations; pure policy leak detection remains enabled.

Private canonical evidence and screenshots are preserved under
`builddir/codex-2026-10-23/collection-refresh/`. Completed guest capture/source
archive duplicates may retire only after exact host verification. Sources,
builds, qualified caches/SDKs, binaries and VM disks remain preserved.
