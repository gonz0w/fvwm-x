# Accumulated file-support integration — 2026-10-23

Status: **affected six-target cycle qualified**. PR164, PR165 and PR166 are
merged. New integrated reference: PR166,
`b95de9e521b5f290bebef04a93e3bb8a4c5acd3e`.
[Exact receipt](VERIFICATION-2026-10-23-FILE-SUPPORT-CYCLE.json).

The accumulated preview/settings C changes and optional Go file/collection
consumer pass on Linux, FreeBSD 15.1 and OpenBSD 8.0, AMD64/ARM64. Seven affected
C/desktop profiles account for 63 registered passes: 54 fresh, nine retained
Linux ARM64 passes with exact unchanged runtime/policy/fixture input hashes.
Linux AMD64 uses GCC and Clang; the remaining targets use their existing default
profile. Other compiler/optional profiles have no distinct changed contract in
this cycle and retain their release acceptance gates.

Five Go packages/26 top tests pass on each of six targets: 30 package outcomes,
156 top-test executions of the same case inventory. Five native browser-X runs
are fresh; the exact matching Linux ARM64 PR166 run is retained. Go vet passes
on the five fresh targets. The patched Go 1.27.2 executable hashes match the
pinned SDK receipts; historical SDK receipts identify compiler bytes, not an
old OpenBSD runtime claim. Both new OpenBSD jobs assert current 8.0 identities,
exit QEMU cleanly and remove their temporary job keys.

The Linux ARM64 registered command-client/browser entries, scoped race and four
PR164 guest sanitizer exercises retain their exact source-bound evidence. No
new native-CPU sanitizer execution is claimed. New Go metadata policy does not
require repeating unchanged C sanitizer exercises. Complete archive-member hash,
mode and gzip-CRC checks pass. Owned PR163 compiled caches preserve source,
compiler and configuration identities; no old test outcome is counted as fresh.
No selected gate fails. Actual FreeBSD AMD64 and OpenBSD ARM64 properties
screenshots are also visually inspected during composition.

Private canonical artifacts are under
`builddir/codex-2026-10-23/file-browser-handoff/cycle/` in the workspace root.
Sources are frozen at the merged reference before new implementation begins.
This closes the three-merge cycle and resets it to 0/3. Full compiler/features,
install/default-X, owner/milestone/release and native-CPU/hardware/toolkit/
compositor/assistive coverage remain explicit. No full release qualification.
The independent performance reference remains PR164, 2/5 merged; no comparison
is due. Full browser scrolling/grid/provider/handoff and protected clipboard
ownership/transfer/retention/storage remain open.
