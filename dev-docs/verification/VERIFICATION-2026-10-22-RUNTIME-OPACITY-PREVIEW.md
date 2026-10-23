# Runtime menu-opacity preview and recovery

Owners: IPC-02 / CFG-01. **The accumulated affected cycle passes 140 selected
registered entries across six targets/nine profiles.** This qualifies the
PR161 colorset and PR162 appearance-policy work together with the runtime
preview described here. Broader owner/release acceptance remains open.
[Machine receipt](VERIFICATION-2026-10-22-RUNTIME-OPACITY-PREVIEW.json),
[contract](../decisions/RUNTIME-OPACITY-PREVIEW-2026-10-22.md).

## Delivered behavior

1. One fixed core lease snapshots the exact prior menu-opacity scalar and
   applies a candidate atomically under the shared appearance generation.
   Confirm keeps its runtime value; revert restores it. No borrowed menu/image
   pointer survives a command. The monotonic 1–30 second deadline, disconnect
   abandon and native module retirement recover without consumer polling.
2. Typed MFL start/query/confirm/revert share bounded directed replies. Native
   producer/incarnation/connection-view/lease identities isolate old and foreign
   controls. Read-only refuses mutations; wait cancellation cannot retract a
   submitted action. Conflicting appearance activity refuses restoration and
   never recreates a deleted target. Ordinary and interaction service paths
   preserve producer order and synchronous-reply barriers.
3. Native Desktop launcher-opacity choices and a Keep/Revert Form retain one
   bounded reply wait and one queued control, with Revert precedence. One stale
   start can re-inspect and retry conditionally; further refusal stays explicit.
   Actual-X keyboard controls, helper death and exact saved-file preservation
   pass. The shortened dialog lines, controls and translucent launcher were
   captured and visually inspected.

**Keep is runtime-only.** No saved preference is written. Theme/configuration
updates or restart can replace it. General saved-preference confirmation and
owned image/resource snapshots remain future owner work. Appearance freshness
is conservative; unrelated tracked activity can refuse recovery. Already
presented menu pixels can require reopening. A deadline cannot preempt one
blocking command or a stalled X server. A dialog orphaned by helper death can
be closed manually; its callbacks cannot control a replacement helper.

## Combined checkpoint

Base: merged PR162 `fb510466bb0a034c03014424fc16384b83392777`.
Previous integrated reference: PR160
`b9dc633123d7dd7478fd49f5019e500bc9185274`. The frozen archive contains 2,209
source files; 22 affected C/helper/fixture inputs remain exact after
freeze. Archive/manifests/product identities are in the receipt. All commands
run locally, with owned incremental builds and verified current OpenBSD caches.

| Platform / profile | Selected passes |
| --- | ---: |
| Linux ARM64 GCC, Clang, minimal | 48 |
| Linux AMD64 GCC, Clang | 32 |
| FreeBSD ARM64 / AMD64 | 30 |
| OpenBSD 8.0 ARM64 / AMD64 | 30 |
| Total: six targets / nine profiles | 140 |

Every profile runs preview policy, core query JSON, MFL request/stream/event
policy, JSON serialization, desktop policy (65 cases), module queue order,
and actual-X preview, core-state, request, settings, operations, stream and
baseline desktop consumers. Linux adds interaction servicing. The preview
fixture verifies exact baseline capture, keep/revert, global busy admission,
read-only, connection isolation, stale tokens/old controls, autonomous expiry,
expiry during a live menu grab, disconnect and native producer death, external
changes and target deletion. Existing appearance/CopyMenuStyle/colorset checks
run against the accumulated candidate. Count each registration once; archived
copies of the same testlog are not additional passes.

Seven scoped guest Clang ASan/UBSan exercises pass with leak detection enabled:
three pure policy registrations and four actual-X preview/request/settings/
operation fixtures using **instrumented current core and MFL**. No sanitizer
diagnostic is observed. FvwmForm is not instrumented here; its existing
fontconfig exit-cleanup gap stays Later. This is not native-CPU sanitizer or
full-module/release qualification.

Final `make verify` passes **177 cases / 14 explicit native-tool skips**.
The new internal command's generated Perl catalog row has deterministic parity,
syntax and direct consumer checks. Native packet constants/types are unchanged.
The affected manuals render as roff and are inspected through mandoc. Existing
formatter diagnostics remain: MFL status 4, Desktop/core status 3; paragraph
spacing notices increase with new sections, without a new diagnostic class.
Compiler warnings remain source/profile-scoped in the receipt.

## Failures, amendments and evidence

Original development logs remain under
`builddir/codex-2026-10-22/menu-opacity-preview/`: an initial missing integer
format include, unsupported underscore module alias, overly fast synthetic
mouse callback assumptions, same-producer popup/query ordering, a temporary
fixture literal error, and rapid reopen freshness refusal. The final fixture
uses actual native keyboard controls and a scheduled popup without bypassing
producer ordering; one bounded conditional retry handles stale start.

After freeze, portable checking catches README word count and the generated
Perl command-catalog parity omission. Those data/docs fixes pass the final
portable/Perl checks. Manual review also corrects heading/resource descriptions;
a private existing-output-path refusal uses a new output location. **No
C/helper/fixture correction or failed selected gate follows freeze.**
The receipt separates these amendments from the tested runtime identities;
it does not claim that changed documentation/catalog bytes were compiled in
all guests. No matrix is repeated for publication or documentation-only edits.

Canonical archives are drained through gzip EOF/CRC and every regular payload
and mode is verified; capture manifests are reconciled. Verified guest capture,
duplicate evidence and duplicate source archives are retired. Source trees,
builds, stages, performance references, VM disks and canonical host evidence
remain. The review images are `screenshots/opacity-preview-dialog.png` and
`screenshots/opacity-preview-launcher.png` in that private checkpoint directory,
with canonical Linux ARM64 GCC capture identities in the receipt.

The cycle is **2/3 merged (PR161/162), third candidate qualified**. On merge,
use its canonical commit as the integrated reference and reset to 0/3.
Performance reference PR159 is 3/5 merged, 4/5 on this merge; no comparison is
due and tuning remains deferred. Full compiler/optional-feature/install/
default-X, owner/milestone/release, native-CPU, hardware/toolkit/compositor and
general saved-preference-preview acceptance remain separate. Deferred gates
are not passes. Existing configuration syntax/arguments remain usable.
