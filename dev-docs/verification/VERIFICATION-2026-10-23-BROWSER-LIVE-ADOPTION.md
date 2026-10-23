# Browser live-update adoption

Status: **focused Linux / early FreeBSD qualification passes; ready for review**.
Owners: FILE-VIEW-01 / FILE-ACTION-01; file/clipboard metadata collection support.
Base: merged PR172 `a5c27ff1ee140c99d91abbb1edb04bf281a6d0e4`.
[Contract](../decisions/BROWSER-LIVE-ADOPTION-2026-10-23.md),
[source-bound receipt](VERIFICATION-2026-10-23-BROWSER-LIVE-ADOPTION.json).

## Shared cycle closed first

The accumulated PR170–172 checkpoint closes before production adoption edits:
**49 native entries across six targets/seven profiles, 357 composed Go cases,
three retained scoped sanitizer checks and nine unique native-X drivers**,
including the three representative themes. Linux/OpenBSD AMD64 reproduce a
unit observer race: a report file can be read before its JSON write completes.
A test-only atomic producer fixes it; only affected Go/unperformed browser work
runs again, retaining passed native/compiler work. Both failures and complete
archives remain preserved. See the
[cycle report](VERIFICATION-2026-10-23-LIVE-COLLECTION-CYCLE.md).

## Delivered batch

- Same-folder refresh/filter/sort, selection controls, observed properties and
  worker status update the existing Form/native collection. Current selection,
  focus/range/viewport identity and unsubmitted input survive. The helper reads
  native state through the owned earlier projection before commit and confirms
  the resulting generation/mapping afterward. Hidden selections return through
  explicit keep/on/off choices. Unchanged entries need no new native generation.
- Fixed clipped Label/SetLabel areas update status/count/filter/sort/properties
  without extra X children or layout replacement. Long labels clip; shorter
  replacements clear their old rectangle. Initial native readiness and optional
  status/selection replies provide an explicit handshake.
- Direct module updates avoid retaining runtime snapshots as configuration.
  Each live batch of at most 32 commands awaits native consumption; one reply
  slot, shared acknowledgement deadline and existing write bounds remain finite.
  Frame/projection/invocation guards refuse stale or replayed actions while a
  window stays open. Existing config syntax and plain status replies still work.

The UI/configuration/bridge code now has separate view/native files, while
worker/actions remain in the existing owner. No module packet ABI, mandatory
package, default theme, storage or configuration migration changes. Paths stay
literal bytes and side effects remain isolated/revalidated. Labels/general Echo
carry ordinary metadata only; protected clipboard content/private delivery is
separate. Navigation/layout changes and refused-state recovery can rebuild a
view. External providers, watchers, mutation and full owner acceptance stay open.

## Focused verification and review

Final native checks pass **seven registered entries on Linux ARM64 and seven on
FreeBSD ARM64**, plus **three Linux ASan/UBSan policy/parser entries with leak
detection**: 17 composed entries. Five Go packages / **53 top-level cases** pass
on both targets and in the separate Linux race run: **159 final top-case
executions**. Vet/build and five composed native-X drivers pass: Slate dark,
Dune light, larger-font classic, instrumented Form and FreeBSD native-X.

Each driver captures **24 screenshots**, including live sort, 4096-entry reorder,
properties/refresh, clipped and cleared labels. Exact window/child identities,
retained path draft bytes, hidden selection, native generation/ticket replay,
FIFO cancellation, literal non-UTF-8 action paths and bounded target batches are
verified. All three final theme refresh views are inspected for fit/contrast;
the long/short label views are inspected too. The richer fixed properties area
fits all profiles. Theme/config/font/scale/server inputs are preserved.

Development checks pass, and the initial combined checkpoint retains exact
native inputs rather than repeating native regressions. Final review catches a
lifetime regression before PR publication: a persistent window would keep its
original five-minute timeout. Live updates now renew the native orphan timeout;
already dispatched timeouts cannot be resurrected. A short real-X test proves
renewal beyond the original deadline, then expiry after renewal stops, including
pending staging cleanup. All affected native/Go/theme/sanitizer checks pass on
this final source. Earlier successful captures remain preserved separately.

Final frozen inputs: **2278 files**, archive
`2b03132f319228c278e826dffd7334c87ad4fd85d19f960159e2233b28993eca`.
Six feature captures verify compressed EOF/CRC, exact inventories and every
hash/mode: development Linux/FreeBSD **48/43 files**, initial combined **159/45**,
final renewal review **161/47**. All 19 modified runtime/manual/fixture inputs
match the published source. No product/fixture correction follows this freeze.
The authoritative optional-Go Meson profile registers new files and builds the
exact tested helper using its existing qualified cache. Both updated manuals
build and render with host mandoc. Repository/link/whitespace checks pass;
portable maintenance qualification is retained from PR170, not claimed fresh.

Canonical evidence/screenshots:
`/Users/cam/DEV/fvwm3/builddir/codex-2026-10-23/browser-live-adoption`.
Verified guest capture/archive duplicates retire: 374 Linux ARM64, 141 FreeBSD
ARM64, 65 Linux AMD64 and 48 FreeBSD AMD64 files. Sources, builds, SDK/caches,
helper references and VM disks remain preserved. Owned QEMU jobs are shut down;
OpenBSD ephemeral job keys are deleted by the runner.

## Remaining gates

PR172 is the integrated reference; this starts a new cycle **0/3, 1/3 on merge**.
Performance keeps PR169: **3/5, 4/5 on merge**. No comparison is due and no speedup
is claimed. Full install/default-X/compiler-feature, owner/milestone/release and
native-CPU/hardware/toolkit/compositor/assistive qualification remains separate.
All local Linux/BSD execution is emulated. Instrumented UI leak detection keeps
the inherited font-cache exit exclusion; policy/parser leak checks are enabled.
Acknowledgement waits share a ten-second deadline; transport writes retain
their own finite bounds. A time budget does not preempt a blocking call.
