# Appearance configuration and copied menu opacity — IPC-02 / CFG-01

Four related improvements pass focused merge checks: shared appearance policy
freshness, bounded menu-style pages, bounded decoration-definition pages, and a
reproduced CopyMenuStyle opacity repair. A source style at 73% copied as 100%;
new and existing destinations now preserve source opacity. The shipped launcher
uses this path. Existing configuration syntax stays usable.
[Exact source/command receipt](VERIFICATION-2026-10-22-IPC-APPEARANCE-STATE.json).
Base: merged PR161, `9f95beda2217a4d69b76d8965b6a86a334127713`.

The core observes existing definitions without an X round trip or second cache.
A conservative process-local revision covers window-style definition/removal,
decoration initialization/removal and title/button/border/ButtonState changes,
menu-style parse/copy/removal, DefaultFont and existing colorset activity.
Recursive functions/continuations share those owners. Read-only queries do not
advance it. Binding and colorset tokens keep independent resource scope.

Menu/decor pages borrow records during one synchronous serialization; limits,
ordinal traversal, names/templates and native reply budgets remain finite.
Entries expose configured policy, literal names/templates and configured sizing,
not rendered geometry, image resources or effective per-window state. Unrelated
covered activity can conservatively invalidate a page. A matching query is not
conditional application: preview/revert stays unavailable pending owned snapshots,
atomic compare/apply and recovery. See the
[contract](../decisions/IPC-APPEARANCE-STATE-2026-10-22.md) and
[primary manual](../../doc/FvwmMFL.adoc).

## Focused validation

One reused Linux ARM64 GCC profile passes eight registered entries:
`core-query-json`, `mfl-request`, `mfl-core-state-xvfb`, `mfl-request-xvfb`,
`mfl-settings-xvfb`, `mfl-operations-xvfb`, `mfl-stream-xvfb`, and
`desktop-baseline-xvfb`. The frozen selected test command takes 30.122 seconds;
all frozen compile/test/sanitizer commands total 31.353 seconds. No VM boot,
install, BSD/default-server session or full matrix is repeated.

Native policy covers strict scalar/list selectors, generation guards, bounds,
configured fields, literal strings, null names, bounded continuation even on a
synthetic cyclic list, and all allocation failures through representative
appearance/menu/decor JSON construction. Actual-X inspects registered definitions,
walks complete pages, preserves escaped literal names, checks configured opacity,
colorsets, delays and title geometry, and refuses stale tokens after normal
Style/WindowStyle/FocusStyle/removal, title/button/border/ButtonState, DefaultFont,
MenuStyle/copy/removal and decor deletion. Nested functions are tracked. Colorset
observations remain independent, while their changes also invalidate appearance.
Existing topology/binding/workspace, request/settings/provider, operation and
stream controls pass.

The first incremental build identifies a missing explicit execcontext.h dependency
before legacy screen/menu declarations; it is corrected before freeze. The next
actual-X assertion exposes copied opacity. An unchanged-product control records
source 73 and destination 100 with matching copied policy fields. The two-line
scalar copy/comment repair then passes, including replacement of an existing
destination explicitly changed to 31. Original development failures and exact
pre-fix records/inputs remain preserved. No runtime/fixture correction follows
the final freeze and no selected frozen gate fails.

Three scoped Clang ASan/UBSan/LSan exercises pass with leak detection enabled:
core JSON policy, native request policy, and instrumented MFL in the expanded
actual-X appearance fixture. No suppressions or diagnostics. The X core remains
uninstrumented GCC; this is emulated guest coverage, not native-CPU or whole-core
sanitizer acceptance. Existing Form exit cleanup stays CLEAN-01 / Later.

The existing native desktop consumer passes settings/editor/checker, menu launch,
application selection, palette/appearance and capture checks. Sixteen screenshots
include eight launcher palette/mode captures and settings/report/baseline views.
Slate dark/light
launcher captures are inspected: text, menu placement and visible layout remain
usable with the corrected copied policy. These are actual-X review artifacts,
not physical/compositor or numeric-opacity proof; the independent source/copy
records establish the scalar correction.

All 2200 frozen source hashes/modes match before and after the checkpoint.
Fourteen changed runtime/test inputs and eight core/helper/module/policy binary
identities bind the receipt. Subsequent edits affect docs/status/receipts only.
The canonical archive has 76 files; gzip EOF/CRC and every payload hash/mode are
verified. Its original verifier initially treated the preserved historical failing
control as a current pass requirement. Classification now requires exactly that
one known historical failure while all current gate entries must pass; no runtime
rerun or evidence replacement follows. Only the canonical-verified completed
guest capture and duplicate evidence/source archives are retired. Sources,
reused builds, stages, references, VM disks and host canonicals remain.

MFL, core and split-menu manuals render to roff/text. MFL mandoc remains status 4
with inherited macros/spacing diagnostics and one added SS spacing warning
(11 to 12). Core/split-menu remain status 3 with unchanged diagnostics. The shared
reproducible renderer emits the same missing-date diagnostic on both sides.
The split-menu opacity wording is present; the core wrapper's filtered roff stays
exact. No clean manual-lint/install claim. Existing binding-debug and border
fallthrough warnings recur during development; final no-op build does not erase
those observations. Final repository/diff/changed-line checks pass. Local source
review finds no unresolved selected finding; it is not independent maintainer
approval. Portable harness verification retains PR160 because its inputs do not
change.

## Next boundary

PR160 remains the integrated reference. PR161 is **1/3 merged**; this checkpoint
becomes **2/3 on merge**. Affected six-target/alternate-profile qualification is
due at shared closure. Full compiler/feature/install/default-X and owner/milestone/
release acceptance remain separate. No new X/OS API, native packet ABI or installed
path changes; the scalar copy reuses existing rendering. Native-CPU/hardware/
toolkit/compositor checks remain separate, and deferred gates are not passes.

CFG-01 owned snapshots, atomic conditional application and safe recovery remain
open. Broader IPC/domain and SESSION/file/clipboard acceptance stay with their
owners. Performance reference PR159 is **2/5 merged**, **3/5 after this merge**;
no comparison or tuning is due.
