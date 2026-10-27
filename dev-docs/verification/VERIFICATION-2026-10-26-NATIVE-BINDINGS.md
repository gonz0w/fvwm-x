# Native view bindings: BIND-01 closeout

Owner: Cam. Base: merged PR268, `f336d638285d722eecda9faa8927ad0a9e44882b`.
Candidate: `codex/r1-native-bindings`. All five finite #213 criteria are qualified
for review; #213 closes on merge. CFG/IPC/file-manager/clipboard owners retain
their independent remaining work.

[Contract and authority inventory](../decisions/NATIVE-VIEW-BINDINGS-2026-10-26.md),
[acceptance receipt](VERIFICATION-2026-10-26-NATIVE-BINDINGS.json),
[actual screenshots](../../doc/development-preview.md#committed-native-bindings-and-recovery),
[periodic comparison](PERFORMANCE-2026-10-26-NATIVE-BINDINGS.md).

## Frozen acceptance map

The five criteria are the existing 2026-10-26 release contract; none was reduced
or split to obtain closure. This checkpoint batches reusable policy, desktop
menus, browser projections, loss/recovery and combined qualification.

| Criterion | Implementation and observed result |
| --- | --- |
| Inventory accepted authority and typed committed revisions | Existing Desktop settings generation/effective presentation, worker admissions/results, MFL provider/operations and browser model/native ACK remain authoritative. Pure `internal/viewbinding` holds only opaque producer/revision metadata; no store or transport. Design inventory specifies each source and projection. |
| Bounds, cycles, stale/lost/reconnect/cancel behavior | Fixed source-to-view edges exclude computed cycles; one active ticket/one pending refresh per owner, at most eight source identities/2,560 copied bytes. Unique lifetime tickets refuse stale/foreign/duplicate completion, close before epoch wrap, and wait after failure. Three menu domains and two browser owners reuse existing worker and native lifetimes. Deterministic 10,000-event pressure and source/lifetime/limit tests pass. |
| Labels, collection, progress and enabled actions | Actual desktop menu rows become Greyed after producer loss; generated launches bind full committed application identity. Browser native collection and labels distinguish unknown totals, known loaded counts and observed action readiness. Browser buttons retain their presentation, with backend identity/type checks and effects refused until fresh data. No incoming arbitrary command evaluation or per-control polling shell. |
| Two independent real consumers under pressure/loss/reload/destroy, bounded retained state and idle activity | Actual Desktop and browser run together in three configurations. Menu 17-intent burst admits two workers, publishes one latest result and rejects one stale result. Browser F5 burst drains its single active/pending slots. Loss retains last-good rows without ready actions; retry restores them. Reload and browser destroy/reconnect establish fresh lifetimes. Source bounds and native diagnostics are checked; three-second idle intervals show unchanged binding counters and absent browser-off ticker. RSS/CPU samples support observations, not leak or whole-process wakeup claims. |
| Source-bound policy/native/theme/support evidence | Native Linux affected build, full Go packages, race/vet, independent worker peer, full existing browser integration and three theme/configurations pass. All 21 final captures are independently inspected, including enlarged text. Six actual-OS default Go targets pass; unchanged C/native profiles retain PR268 qualification. Final two display-only literals are explicitly reconciled below. |

## Native results and limits

The final fixture runs Slate dark, Dune light and 18pt DejaVu Sans Mono on a
1440×1200 Xvfb display. Each passes actual native menu loss/recovery, mapped-old
row selection, latest menu convergence, controller reload, browser held work,
progress/readiness updates, failure recovery, F5 pressure, destruction and fresh
provider registration. The literal application label never executes its marker.
There are no observer X errors. New progress/readiness rows fit all three
configurations. Older fixed-width loading/properties hint text can still clip in
the enlarged configuration; broad text/layout polish remains separate.

Mapped interaction preserves producer command ordering: unsafe menu updates and
later diagnostics can defer until unmapping. The fixture observes the real
controller's emitted committed projection while the old native row stays mapped,
then selects that row, verifies stale callback refusal and observes convergence.
It does not claim a native mutation-refusal occurred in that deferred scenario.
Source/unit tests separately cover the bounded desired-hash popdown retry when
an update is refused, unchanged-hash suppression and removed owned menu pages.
Desired cache entries are never described as native acknowledgements.

Both consumers demonstrate zero binding-generated periodic wakeup sources and
zero unsolicited binding deliveries during the observed idle intervals. Event
pressure remains bounded and drains. Existing Desktop 250 ms polling and shared
five-second status sampling continue. Browser watch-off has no 100 ms ticker.
Kernel and whole-process scheduler wakeup counts were unavailable and are not
claimed; CPU/context switches are not substitutes. Short RSS observations and
fixed retained metadata establish neither process leak freedom nor #29 resource
budgets. No physical/assistive/compositor/whole-release qualification is implied.

The full existing browser fixture also passes literal bytes, configured actions,
selection/draft/window preservation, native watching/loss/rearm, typed work and
portable handoff, 4,096-entry atomic updates/backpressure and bounded callbacks.
After deliberate failed navigation or cancelled work, it now explicitly checks
unavailable effects and refreshes before attempting subsequent actions; the
original exact hidden-selection bitmap assertions remain. The initial failures
are retained. The independent desktop worker/CLI peer passes all seven cases.

## Source and test receipts

Linux archive `aeff7750507b84dc43f39385ce51ed3be5c1d6b2894bee645ff29bfce1e84544`
is 5,847,625 bytes. Host verification recomputes all 790 payload hashes/sizes/modes,
full gzip CRC, 754 final implementation/fixture inputs and all 21 final captures.
Final source has no extra Go inputs. Initial diagnostic overflow, fixture menu
surface/ordering corrections, regression precondition failures and passing runs
remain in the same archive. The diagnostic repair validates three domain records
against the real 1,000-byte native command cap before submitting any; maximum
counters and no-partial-snapshot tests pass.

Affected Linux Go policy passes `viewbinding`, `desktopcontroller`,
`fvwmfilebrowser` and `fvwmdesktop`. Race checks cover all three changed runtime
packages; repaired menu/policy race and final display-string browser policy checks
pass separately. Scoped vet passes. Both changed manuals render to man pages
without renderer diagnostics. The host sandbox's initial Unix socket test refusal
is not a pass; the full browser package, including that reuse test, passes on the
actual Linux targets. No new C code changes here, so no C rebuild, sanitizer or
full installation matrix is repeated. Exact unchanged core/MFL/Form/Script
binary hashes are checked before and after native work.

One full source freeze for the support gate is
`93d5a8787a9d3460b54c0ee58f3d4e467f00c5afd2d3392f4dc5ce210801f248`
(2,654 regular inputs). All compiled Go source bytes match the final tree except
exactly two reviewed browser display literals shortened after enlarged-text
review. The delta receipt proves those replacements and no other Go change;
final Linux browser build/policy and three-theme native results cover the exact
final text. Documentation, Meson registration and fixture-only changes are
recorded separately. Five default support gates are retained for unchanged
platform logic, without repeating the matrix for those strings.

## Combined affected support checkpoint

| Target / default profile | Changed Go packages and two binaries |
| --- | --- |
| Linux ARM64, retained GCC build | Pass; native/race/integration above |
| Linux AMD64 | Pass |
| FreeBSD 15.1 ARM64 | Pass |
| FreeBSD 15.1 AMD64 | Pass |
| OpenBSD 8.0 ARM64 | Pass |
| OpenBSD 8.0 AMD64 | Pass |

Each additional default target executes all four affected Go packages and builds
Desktop/browser on its actual OS/architecture with existing pinned Go SDK/cache.
OpenBSD uses hash-verified official 8.0 inputs and disposable guests. No C/X/full
install gate is repeated there. Linux compiler/reduced feature variants have no
separate changed Go policy path; unchanged native C/eight-profile qualification
remains source-bound in the merged [PR268 checkpoint](VERIFICATION-2026-10-26-CONFIGURATION-DIAGNOSTICS.md).
This does not upgrade focused passes to full release qualification.

Separate source/evidence/21-capture review found no unresolved blocker. Project
routing requested Sol/high implementer/reviewer and Astra/high scope analysis;
actual model routing is not independently attested. All checks are local, without
runners, Actions or fabricated statuses. Owned test guests stop after verified
retrieval; only byte-verified guest evidence duplicates are retired, retaining
builds, VM disks and actual performance-reference binaries.
