# STACK-01: initialized window lists and ring membership

Status: locally qualified from the frozen 1323-file snapshot. This follows the separately frozen
[parent-continuation checkpoint](VERIFICATION-2026-10-07-PARENT-CONTINUATION.md).

## Three related stories

1. Actual-core Layer on an iconified parent counted that parent's visible icons
   again for each collected transient. Three moved records with one/two parent
   icons supplied six/nine XRestackWindows slots, but populated only four/five.
   A deterministic zero-filled allocation control suppresses invalid baseline
   requests; it measures the core's arguments, not ordinary desktop compromise.
   Collection now counts the selected record, and restack submits the populated
   prefix rather than an inferred allocation count. Numeric fixed controls pass
   exact layer membership, authoritative links and real-server order.
2. New-window resynchronization counted managed records then traversed the stack
   ring. A synthetic detached managed record made it pass the root sentinel's
   None frame as a fourth slot for a three-member ring. Counting/filling now both
   traverse the authoritative ring. The private corrected control submits three
   real frame IDs and preserves server/ring order. No managed registry is added;
   detached record cleanup remains with its existing lifetime owner.
3. `fvwm/stack_window_list.h` shares pure frame/icon count and append policy across
   local restack, full resynchronization and raise-over-unmanaged. Selection and
   packing keep the same icon visibility and frame/title/pixmap order. Count
   additions check Xlib's int range before arithmetic; append validates capacity
   and count before any write. Array allocation uses the existing checked
   element allocator with no zero-fill pass. Unrecoverable allocation policy
   stays fatal; no new graceful-failure target or command-timing rule is added.

## Focused coverage

The pure test independently enumerates all 16 icon/suppression/title/pixmap
combinations, capacity/start sentinel cases, exact output order and unchanged
buffer/count on refusal, plus INT_MAX/SIZE_MAX boundaries without large
allocations. The real-frame driver has 36 cases: raised/lowered Layer with
zero/one/two visible or suppressed parent icons, and new-window parent follow-up
with matched/detached managed membership, both directions and icon variants.
Every XRestackWindows ID must be known and non-retired; exact submitted counts,
layer membership, all ring links and real-server order are checked. Focus,
decoration/EWMH publication wrappers leave those policies to existing real-client
regressions.

Ten focused Linux and ten scoped Clang ASan/UBSan tests pass (two pure with leaks
enabled; eight actual-core with leaks disabled). Both native FreeBSD/OpenBSD
ARM64 groups pass the pure policy and all 36 X cases without replacing their
frozen parent qualification source. Existing live tests cover layer propagation,
parent teardown, RaiseOverUnmanaged, cycle policy, maximum Layer and transient
closure. Initial archive preparation used the wrong base filename; no source
was synced, and the attempted unknown-test run is retained. Corrected sync/build
and focused results follow. No production repair was needed after those checks.

Exact source is the 1323-file `source-stack-window-list.tgz` in
`builddir/codex-2026-10-07/stack-window-list`, seven owned path changes from the
parent checkpoint. The approved fallback paragraph is reflowed to avoid one
new mandoc long-input-line style diagnostic; prose/behavior remain unchanged.
Other chats' manual/build changes stay in the shared working tree and outside
this archive. Native Linux release CI and physical hardware coverage remain separately
unrun.


## Combined qualification

All eight compiler profiles pass: GCC/Clang on Linux ARM64 and AMD64 (159 tests
per profile), plus FreeBSD/OpenBSD ARM64 and AMD64 (147 each), totaling 1224
standard entries. All 120 staged code identities match the tested bytes after
expected install RPATH adjustment. Four default BSD X servers pass all 23
registered driver invocations each, including all 36 initialized-list cases.
Mixed cursor/no-XRender and no-SVG profiles pass 158/138 entries; 37 scoped
sanitizer checks pass (14 pure leak-enabled, 23 actual-core leak-disabled).

All seven final archives validate complete payloads, exact source hashes/modes,
registered tests and installed identities against snapshot SHA-256
`8667a76a2d3a64996423ba18deca2596e8084e0e653ec4a582dd32bd747e994b`.
OpenBSD AMD64 uses the previously documented finite outer harness accommodation:
146 ordinary tests keep their original timeouts; only verification-profile uses
60 seconds, retaining its internal 15-second subprocess deadlines. Raw split
logs and exact identities are validated. No production timeout changes.

Native OpenBSD mandoc confirms the approved parent fallback paragraph renders
correctly at 60 columns. Reflow removes the added long-input style diagnostic;
all 101 normalized pre-parent diagnostics are unchanged, including two inherited
Asciidoctor macro compatibility errors. Source, rendered output, lint logs,
manual review and qualification summary are retained under the checkpoint dir.
The subsequent approved Layer storage contract is outside this snapshot.
