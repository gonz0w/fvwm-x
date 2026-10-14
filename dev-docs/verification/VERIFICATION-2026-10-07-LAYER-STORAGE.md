# STACK-01: Layer restack storage ownership and refusal

Status: user-approved contract locally qualified from the frozen 1328-file
snapshot across all six targets and required local profiles.
The separately frozen initialized-list checkpoint remains independent.

## Behavior and ownership

`fvwm/stack_window_buffer.c/h` owns checked operation-local window-array storage.
It counts the authoritative stack ring using the shared frame/icon policy and
prepares capacity before Layer scratch planning, link mutation or publication.
Failure leaves an empty owner and Layer returns through existing diagnostic
logging with all layers, ring links, server order and temporary state unchanged.
Same-layer commands keep their allocation-free fast path. The restack consumer
borrows the buffer synchronously and submits only its populated prefix; Layer
finishes the owner once, including the empty-selection path. No managed-record
references, cache, registry, callback, thread or new library are retained.

The approved contract keeps the WM running and permits a later successful Layer
command. Counting adds one linear full-ring pass and can reserve more slots than
the selected subtree needs. Other commands' restack-allocation failure behavior
is outside this approval. See the [decision](../decisions/LAYER-RESTACK-STORAGE-PROPOSAL.md).

## Focused evidence

Seven policy and twelve actual-core tests pass in the isolated Linux development
build. The same nineteen tests pass scoped Clang ASan/UBSan: pure policy has leak
checks enabled; actual-core lifetime/publication tests use the established leak-
disabled scope. Existing real-client Layer, parent destruction/redirection,
maximum-layer, transient closure, query and RaiseOverUnmanaged tests are included.

The buffer unit covers refusal, untouched records, empty-ring/size arithmetic,
borrowed populated-prefix use, idempotent cleanup and retry without large
allocations. The new actual-core driver has twelve raised/lowered cases with
zero/one/two icon windows, ordinary allocation and forced small allocation
failure. Failure submits no restack/decor/EWMH/focus changes, preserves all four
managed records byte-for-byte and exact server/ring order, and permits retry.
Synthetic real X frames isolate storage policy; existing real-client tests cover
publication and retirement. Both native BSD ARM64 environments pass the unit
and all twelve actual-core cases without modifying the frozen acceptance tree.

Exact production snapshot: 1328 files in
`builddir/codex-2026-10-07/layer-storage-production/source-layer-storage.tgz`,
nine owned paths changed from the initialized-list snapshot. Other chats' shared
manual/build edits remain outside this archive. Private baseline/candidate
controls remain separate historical evidence under `layer-storage-decision`.
Native Linux release CI and physical hardware coverage remain unrun.

The focused evidence archives also validate every payload hash/mode and exact
normal/scoped/native results; the pure/helper compilation logs add no warnings.
Native OpenBSD mandoc verifies the new Layer paragraph at 60 columns and retains
the previous 101 normalized diagnostics, including two inherited macro errors.


## OpenBSD AMD64 affected reruns

The initial ordinary gate passes 147/148 entries; menu-image-boundaries fails
while waiting five seconds for its release grab to be restored. Original suite
logs, JSON and menu artifacts are preserved. Three affected-only reruns pass
with exactly unchanged source and original outer/internal deadlines. No runtime
or fixture repair is made, and no deadline is enlarged for this UI test. The
qualified ordinary log retains all 147 initial passes and uses the final passing
record only for that test; validation checks all three raw reruns and exact
replacement. Cause of the isolated observation remains unestablished; retain it
for interaction/harness follow-up instead of attributing it to VM load by guess.
The separately documented verification-profile TCG outer allowance remains
60 seconds with its internal 15-second subprocess deadlines unchanged.


## Linux AMD64 Clang harness affected reruns

The initial full profile passes 160/161 entries; the harness self-test misses
its two-second display-fd readiness deadline for its fake X process. All WM
integration cases pass in that initial run. Preserve the original suite/logs,
then rerun only this self-test three times with unchanged source and deadlines.
All three pass; installed identities also pass. Final archive validation checks
the raw failure, affected passes and exact replacement. This is separate from the OpenBSD menu-release
observation, and neither is assigned a VM-load cause without evidence.


## Combined qualification

Eight standard compiler profiles are locally qualified: Linux ARM64/AMD64
GCC/Clang each has 161 accepted entries; FreeBSD/OpenBSD ARM64/AMD64 each has
149. The total is 1240 qualified entries, incorporating the exact affected
replacements described above rather than claiming every initial run passed.
All 120 installed code identities match the tested binaries after expected
RPATH adjustment. Four default BSD X servers each pass 24 driver invocations,
including all twelve new ordinary/refusal/retry controls. Optional mixed
cursor/no-XRender and no-SVG profiles pass 160/140 entries. Scoped Clang
ASan/UBSan passes 39 checks: 15 pure leak-enabled and 24 actual-core leak-disabled.

Seven final evidence archives validate full compressed-stream EOF/CRC, every
payload, exact 1328 source hashes/modes, registered identities and installed
bytes against SHA-256
`92256ab21ce8b7ed7ff85dbea99ac7b95b9742d317f3a2ef3992bae8e6ca19e2`.
The archive checker also verifies the original failed logs, three unchanged
reruns for each affected check and exact replacement records. No production,
UI-test or harness-source timeout change is made. Original failures remain
follow-up observations; this local qualification does not establish their cause.
Native Linux release CI and physical hardware coverage remain separate and
unrun. The private RaiseOverUnmanaged storage candidate is outside this snapshot
and proceeds separately under the clarified configuration-only scope; it is not
covered by this frozen Layer qualification.
