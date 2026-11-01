# Core manager selection implementation — 2026-11-01

The source checkpoint implements the remaining selected core-manager ownership
and conversion-completion contracts under #22. It is separately source-reviewed
and compiles in the reused Linux ARM64 GCC debugoptimized profile. **No new unit,
native-X, sanitizer, theme or install runtime suite ran in this batch.** Those
checks remain queued at the next named shared checkpoint under the owner's faster
cadence. Whole #22 and #67 remain open.

The [decision](../decisions/MANAGER-SELECTION-2026-11-01.md) specifies finite
behavior and limits. The [machine receipt](VERIFICATION-2026-11-01-MANAGER-SELECTION.json)
binds reviewed inputs, immutable source guards, actual commands/artifacts and
manual renders. The [frozen role record](SELECTION-2026-11-01-ROLES.json) maps
adopted core, Script, Form and file-only roles, with the conditional clipboard
manager handoff and explicit remaining acceptance.

## Change and verification boundary

Previously the clear handler discarded event identity and called Done. It now
requires the active manager connection, selection and owner window, and a genuine
server event. Real ownership loss retires the epoch before normal cleanup; the
request timestamp's half-wrap rule is deliberately not applied to real clears.
No extra ownership query or configuration change is introduced.

Previously conversion success meant that XChangeProperty had been submitted.
The new manager-only continuation records exact request serials and settles
errors before success. Direct conversion uses one terminal marker. MULTIPLE
settles data writes, marks failed destinations None, then separately confirms
its control rewrite. A private per-screen InputOnly window supplies the marker;
one active operation, sixteen headers, 128 pairs and one admission-time second
bound work. Timeout and Close preserve serial ownership for late errors.

Modal masks need explicit handling: some skip markers, while opaque resize can
extract them beyond a queued clear. The bounded raw prefix scan preserves input
order and cached event state, and leaves lifecycle dispatch with the ordinary
loop. A separately reviewed correction ensures saved markers also resume in
ordinary dispatch after an interaction ends. Partial scans conservatively defer
publication; deadline refusal cannot preempt a blocking Xlib call.

These are source observations and reviewed corrections, not claims that the old
runtime failures were reproduced. Deterministic runtime controls are authored
alongside the change and intentionally deferred under the selected cadence.
The linked production probe models X transport/error ordering; injected BadAlloc
is not actual server memory-exhaustion evidence. Actual native-X peers cover
rejected property atoms, recovery, MULTIPLE siblings/control publication and two
screen managers with synthetic-clear survival, genuine loss and successor
preservation. The existing manager cases remain registered.

## Actual checks

| Check | Result |
| --- | --- |
| Source review | Clear at freeze04 after the saved-marker ordinary-loop liveness correction; the narrow manual dependency addition is clear at freeze05. Agent backend metadata unavailable. |
| Repository/static | Repository foundation, whitespace and affected Python AST checks pass. |
| Initial dry plan | Prepared successfully but omitted manual regeneration after the included source changed. This exposed a missing dependency, not a passing manual update. No compilation ran from that plan. |
| Affected build | Reviewed second plan executes 13 commands: six core objects, three linked-probe objects, two links and two generated manuals. All exit zero. Existing build options remain unchanged; source pre/post guards match. |
| Manuals | Exact generated fvwm3/fvwm3all pages contain the new contract and render with host mandoc exit zero. Only these two aggregate pages get the missing include dependency; unrelated page/default behavior stays unchanged. Optional HTML dependency declarations are source-reviewed, not rendered here. |
| Runtime | Not run; compiled probes and registered native-X cases are pending. No new screenshots are needed for these protocol/ownership changes, which add no UI. |

The profile is the existing owned Linux ARM64 Podman/AppleHV VM, GCC 13.3.0,
debugoptimized without sanitizers/LTO. Native instruction-set execution, emulation,
physical hardware and six-platform qualification remain distinct. No new build
profile, clean rebuild or package/install sweep was performed.

## Evidence reconciliation and continuation

The integrated PR326 baseline's icccm2.c and header match the PR148 upkeep file
hashes. Its events.c does **not** match that historical manifest. New candidates
change all three. Thus earlier named-profile startup/target results remain
historical; their full pass counts do not qualify this new core. The role record
keeps the exact bindings rather than treating unchanged function bodies as an
identical current binary.

Script's prior normal transfer/consumer results remain scoped to their own
source. Its full owner sanitizer exit still fails with 320 bytes / three
allocations through Fontconfig initialization; attribution/clean-exit acceptance
is pending. Q67 D1/D2 remain unrun after an independent OpenBSD source-upload
network failure, with 53/55 service groups unchanged. Neither gap is called
passed, expanded into a broad retry or absorbed into a new clipboard service.

The next shared runtime checkpoint must execute the linked manager probe and
independent manager ownership/target cases, retain relevant prior malformed and
handoff controls, and add warranted scoped ownership sanitizers once across the
assembled work. Full platform/install/default-server gates stay at milestone or
release. No performance comparison is due after only one subsequent source batch.

The owner stops the reused Linux container and machine after compilation and
exports; both stop commands exit zero. Current source/build/cache and the last
performance reference remain available for the next checkpoint. No old runtime
captures, new guest disk, disk expansion or extra matrix are created here.
