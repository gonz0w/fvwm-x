# Desktop-provider observation — IPC-02

Selected after merged PR158, base
`61b6d70ccd876c1d9dc3443f0ee8c972b5378f0b`. Three related features supply
concrete consumer primitives: provider capability/configuration-state queries,
opt-in identity/generation invalidations and conditional settings snapshots.
The [tracker](../PROJECT-TRACKER.md#ipc-02-structured-queries-and-operation-results)
remains authoritative. This is the existing preference owner, not a new provider
loader or an implementation of SESSION's queued service lifecycle.

Reuse the strict settings transport and fixed FvwmDesktop endpoint. The provider
query can report starting before END_CONFIG_INFO, then ready; settings operations
retain their existing readiness fence. Expose the supported preference operations
and known load error separately from future application completion. Optional
exact identity targets one helper; discovery is first responder, not inventory.

One opt-in watch/filter per client receives only owner/identity/generation after
configuration readiness or preference publication. No values, action text,
provider cache, retained history, process-death inference or replay. New helper
identity is visible to unfiltered discovery watches. Reconnect, malformed notice
or a raced query requires an explicit fresh snapshot. Watch/unwatch is local,
works in read-only mode and at pending capacity, and uses existing bounded
client queues. Unqueueable readers disconnect independently. Notices are hints
inside existing same-user/X trust, not an authentication or applied-action proof.

Conditional settings queries require an exact identity and valid generation.
A matching preference generation returns unchanged without registry values;
a mismatch returns the requested complete scope. Validate scope even when
unchanged. This token does not cover rendering, liveness, clock-driven theme
application or un-reloaded external files. Actions retain execution rechecks.

No configuration migration, core/native packet ABI, dispatch, OS API, installed
path or visible UI changes. Add no dependencies or background service. Focused
Linux policy/actual-X regression and scoped native module sanitizers qualify this
ordinary PR; six-target/alternate-profile coverage stays due at shared closure.
PR157 remains integrated reference, PR158 is 1/3 merged, this batch becomes 2/3.
Full acceptance remains at milestone/release boundaries. Performance reference
PR154 is 4/5 merged, 5/5 on this merge; compare using existing binaries next cycle.
