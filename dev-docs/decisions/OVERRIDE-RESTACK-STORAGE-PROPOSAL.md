# STACK-01: skip unmanaged-window correction on storage refusal

Status: proceeding under the user-reiterated configuration-only compatibility
scope; no configuration migration or separate approval is required. Private
normal/scoped sanitizer controls pass; production is implemented with focused normal/scoped sanitizer and native BSD
ARM64 checks passed; combined qualification remains pending. This contract is separate from approved Layer storage refusal and
parent-walk fallback.

## Concrete owner and failure

`raise_over_unmanaged()` is the optional correction used by
`BugOpts RaiseOverUnmanaged on`: after the ordinary managed-window Raise, it
places the target and higher managed windows above the highest eligible
unmanaged override-redirect window. It allocates a temporary array before issuing
this correction's configure/restack requests. Its current allocator exits the
WM if a tiny allocation is refused.

A finite actual-core control uses one managed record with a real mapped X frame
and one real override window. The frame already leads the managed ring, so the
control isolates only the unmanaged correction. Forced refusal of an eight-byte
array on the local 64-bit build exits the baseline with status 1. Ordinary
allocation raises the frame above the override as expected. This is a resource-
failure observation, not a demonstrated desktop compromise.

The private candidate checks multiplication, uses fallible allocation, logs the
failure and returns before any correction request. It retains the completed
managed Raise; it skips only this extra unmanaged-window correction. In the
one-managed-frame control, authoritative record/ring state and real-server
order remain unchanged. The two-managed-frame variant confirms the ordinary
Raise already moved the target above its managed peer: both the new ring order
and real-server managed order are retained while the override remains above
the target. A subsequent ordinary Raise completes the correction.
Normal allocation, highest override selection, visible icons and managed order
use the existing owners. No new dependency, cache, registry, timing rule or
configuration option is introduced.

## Implementation contract

If this correction cannot allocate its array, skip the correction, keep the WM
running and allow a later Raise to retry. Previously completed managed Raise
work is retained; this is not a rollback of the whole Raise command. This differs
from Layer's approved refusal before any layer/ring mutation. The previous fatal allocator is the recorded baseline for this owner.

The prior compatibility approval hold is withdrawn. This failure-handling
change does not alter configuration syntax, names or arguments, remove a
configured feature or require users to rewrite their configuration. It proceeds
under the [configuration-only scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
Correctness, ownership, diagnostics and qualification remain required.

## Review evidence and remaining adoption

Exact baseline, candidate, patch, finite startup control, compiled binaries and
normal/scoped sanitizer logs are retained under
`builddir/codex-2026-10-07/raise-unmanaged-storage-decision`. Both profiles confirm
baseline exit, candidate continued operation, preserved managed/server order,
retained completed managed work in the two-frame variant, and successful retry. The candidate also passes the existing real-client
highest-override/InputOnly, popup-retirement and focus integration checks.
No forced failure is applied to arbitrary desktop processes or large memory
allocations. Production remains the approved Layer snapshot. Production adoption includes
explicit array ownership, pure arithmetic tests, a permanent failure regression,
native BSD checks and combined qualification before qualification status. Native
Linux release CI and physical coverage are separate.

[Production evidence](../verification/VERIFICATION-2026-10-07-OVERRIDE-STORAGE.md)
records the shared constructor, explicit owner, permanent refusal/retry controls
and separate source/qualification scope.
