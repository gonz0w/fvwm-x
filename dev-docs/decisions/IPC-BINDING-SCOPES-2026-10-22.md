# Binding inspection completion — IPC-02

Selected after merged PR157, integrated reference
`3f4f7052151f110ba6f398a3451e727af4ecf8c4`. Three related features finish the
binding inspector's scope: regular/fallback menu records, normalized placement
finish controls, and the context/modifier policy needed to interpret records.
The [tracker](../PROJECT-TRACKER.md#ipc-02-structured-queries-and-operation-results)
remains authoritative. General providers/actions, operation progress and
shared-core generations/CFG-01 preview retain their owners.

Extend `query resource: bindings` with explicit scope, preserving the omitted
scope's global behavior. Menu/fallback lists use existing bounded paging and
literal record serialization. Borrow their lists only during synchronous core
query evaluation; retain no pointers or second authoritative cache. Report
fallbacks separately because overrides do not delete defaults or establish an
independent event winner. Native menu storage retains keycodes, not key names.

Placement has no binding list: its parser maintains two finish masks. Report
those exact effective controls and existing unsupported key/modifier capability;
do not reconstruct command history or change input handling. Policy reports
compile-time context/X modifier names and the current ignored-modifier mask.
All binding scopes share the core incarnation/revision token. A real change to
IgnoreModifiers invalidates it; repeated identical policy does not. Scalar
placement/policy accept an optional generation but reject pagination fields.

Reuse directed MX_REPLY, strict schema, read-only admission, five-second waits,
eight outstanding slots and bounded assembly. No packet ABI, command ordering,
configuration syntax, dispatch, X acquisition, install or visible UI changes.
No compatibility decision or screenshots are needed. Document the public
contract in [FvwmMFL](../../doc/FvwmMFL.adoc).

Use one reused Linux GCC profile for affected request/query policy, actual-X
core binding observation and existing transport/provider regression checks.
Exercise allocation-failure cleanup and scoped sanitizers. No new OS API or
server-dependent input behavior requires early BSD/default-X qualification;
affected six-target/alternate-profile coverage is deferred to the new shared
cycle. Full install/default-X/compiler-feature and release acceptance remain
at their required boundaries. Performance PR154 is 3/5 merged, 4/5 after this
batch; no comparison is due.
