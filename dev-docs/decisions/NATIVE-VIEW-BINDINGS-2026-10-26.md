# BIND-01: committed native view bindings

Owner: Cam. Two adopters: resident desktop menus and the optional file browser.
The [finite #213 release contract](RELEASE-CONTRACTS-2026-10-26.json) owns closure;
[qualification](../verification/VERIFICATION-2026-10-26-NATIVE-BINDINGS.md) records
observed results and limits.

## Reuse existing authority

| Domain | Existing owner and invalidation | New projection |
| --- | --- | --- |
| Settings | FvwmDesktop registry, committed preference generation, effective theme/time/wallpaper policy; existing MFL provider query/watch | Settings/appearance menu labels and choices |
| Applications/status | Existing bounded Desktop worker admissions/results and operations resource; explicit discover and shared status sampler | Literal application rows, metrics and Greyed unavailable worker actions |
| Files | Browser worker serial, provider incarnation/model generation, existing MFL files query/control/watch and native Form acknowledgements | Collection, progress and readiness labels |

The binding package owns identity metadata, never settings, application records,
files, transport, persistence or commands. Existing CFG/IPC authorities continue
owning query/watch/admission/results. This does not complete broad #60/#70 owners.
No SQLite store or new service is required.

## Fixed dependency and work contract

`internal/viewbinding` is a pure, single-owner Go package. A binding has one to
eight fixed source IDs (1–64 ASCII domain bytes), each with opaque producer and
revision identities (1–128 printable nonspace ASCII bytes). Equality alone has
meaning. At most 2,560 copied identity bytes, one admitted ticket and one pending
refresh are retained. Fixed producer-source to view edges exclude computed
view-to-view expressions and cycles; this is not a general expression graph.

`Observe` admits a changed committed identity; `Invalidate` records a refresh
hint without inventing data. Repeated changes coalesce behind the active ticket.
`Begin` requires available sources and a pending refresh. `Complete` retires only
its exact active ticket; it publishes only a successful current epoch. Stale,
duplicate and foreign-lifetime results cannot retire or publish a successor.
Epoch exhaustion closes before wrap. A failure without another event waits for
explicit retry, never spinning. `Lost` invalidates availability; observing even
the same identity after loss creates a fresh epoch. `Close` is terminal. Construct
a new owner on reconnect; never copy a binding or replay uncertain effects.

Read-only saturating counters describe work admission/publication/rejection.
They create no event source. There are no binding timers, goroutines, I/O or
callbacks. Owners serialize calls and retain their original worker lifetimes.

## Desktop adopter

Exactly three domains own settings, applications and system menus. Existing
four retained jobs/two active workers bound execution; each domain coalesces one
latest intent. The request revision is committed admission identity, not a fake
worker-data generation. Only validated matching completion commits worker data.
Loss retains last-good rows, disables affected native actions and refuses launch;
explicit discovery, existing status refresh or reload obtains a fresh snapshot.
Independent non-worker actions retain their existing policy.

Desired menu deltas suppress equal projections. They are not acknowledgements
from the core: a mapped native menu refuses mutation. Every initial definition
installs a private instance/domain/name/projection-hash popdown callback. After
unmapping, a stale hash replays only that latest owned menu; an equal hash does
nothing. Removed owned pages can be destroyed. Names come from the fixed registry
or bounded page validators; application/search projections retain at most 54
names. Mapped copies can retry on their own popdown, with no timer or C API.

Generated application actions carry controller and committed full-data identity.
An old mapped row cannot use its index against a reordered application snapshot.
Equal full application values keep the token; a changed executable/argv/cwd or
terminal policy rotates it. Public `launch INDEX` configuration remains supported.
Private generated callbacks do not change the FVWM configuration contract.

## Browser adopter

Two owners track work intent and native model projection. Existing work serial,
provider incarnation, callback generation, selected-item identities and native
update ACK remain required. No policy ticket replaces these defenses. Repeated
refreshes retain one active worker and one pending latest intent. Cancel keeps
ownership until actual completion. Producer/native-transport loss closes the
lifetime; a fresh process registers and snapshots independently.

Progress reports unknown totals during enumeration and a known committed loaded
count afterward. Typed availability belongs to the observed provider/model
generation. Native text explains readiness; browser buttons retain their current
visual presentation and every effect still revalidates identity/type immediately
before execution. Desktop menus supply the independent actual Greyed-action
adopter. Browser views preserve their existing same-folder focus/selection/scroll
and draft contract. A disabled directory watcher has no 100 ms ticker; enabled
watch/reconciliation retains its existing cadence.

## Qualification boundary

Pressure/loss/reload/destroy and three actual native configurations are checked
at one combined checkpoint. Counter observations prove binding activity and drain;
process CPU/RSS only support those observations. Existing Desktop 250 ms polling
and five-second status sampling remain. Neither kernel wakeup counts nor whole
session resource budgets are inferred. Reuse unchanged C/module/platform receipts;
run changed Go packages and binaries on the six default targets. Feature/compiler
variants have no distinct new Go policy path. Physical, assistive and full release
qualification retain their existing owners.
