# Core colorset observations — IPC-02 / CFG-01

Three related capabilities pass focused merge checks: bounded colorset inventory,
exact existing sparse-ID lookup, and summary/freshness inspection. The core
returns published pixel/opacity/background facts and separate pending/refused
image preparation over existing directed module transport. No record allocation,
X round trip, second state authority or new visible UI is introduced.
[Exact receipt](VERIFICATION-2026-10-22-IPC-COLORSET-STATE.json).
Base: merged PR160, `b9dc633123d7dd7478fd49f5019e500bc9185274`.

The colorset owner conservatively invalidates generations on allocation, parse
admission, asynchronous settlement and publication. Binding and colorset owners
share only the existing incarnation/token formatter; counters remain separate.
Pages carry generation-checked ordinal cursors. Exact lookup accepts the full
nonnegative signed-int ID range, returns no default fallback, and refuses missing
IDs without creating them. Summary omits entries. All forms retain bounded input,
partial-assembly, deadline, cancellation and output-queue contracts. Pixel strings
are colormap-local integers, not RGB or resource handles. See the
[decision](../decisions/IPC-COLORSET-STATE-2026-10-22.md) and
[manual](../../doc/FvwmMFL.adoc).

## Focused checks and composition

One reused Linux ARM64 GCC profile passes seven selected registered entries:
`core-query-json`, `mfl-request`, `mfl-core-state-xvfb`, `mfl-request-xvfb`,
`mfl-settings-xvfb`, `mfl-operations-xvfb` and `mfl-stream-xvfb`.
The frozen selected test command takes 25.776 seconds. No VM boot, install,
BSD/default-server run, broad rebuild or full matrix is repeated.

Native policy covers selector ambiguity, canonical ID/overflow refusal, page
bounds, exact pixel encoding, stale generations, unavailable tokens, independent
revision formatting and allocation-failure cleanup through every JSON allocation
in a representative colorset result. Actual-X uses the current core and native
MFL with a read-only client: it walks complete pages, looks up ID 2147483647,
checks a missing read leaves count/generation unchanged, rejects stale pages/
targets/summaries, and verifies pixel values independently with XQueryColor.
A failed asynchronous image replacement retains published pixels and reports
refusal; a later Plain command recovers. Existing topology/binding/workspace,
operation, provider/settings, wait/cancel/fragment and stream consumers pass.

Three scoped Clang ASan/UBSan/LSan exercises pass with leak detection enabled and
no suppressions or diagnostics: core JSON policy, native request policy, and
instrumented MFL in the expanded core-state actual-X fixture. The X core remains
uninstrumented GCC. This is emulated guest coverage, not native-CPU or whole-core
sanitizer acceptance. Existing Form exit cleanup remains CLEAN-01 / Later.

After the first frozen gate, two unit-test readability/line-width edits receive
only fresh GCC and sanitizer policy checks. Exact product binaries and the
unchanged actual-X fixture retain all five integration entries and the scoped
MFL X exercise. No product/fixture behavior correction or failed selected gate
occurs. The composed result remains seven focused entries and three scoped
sanitizer exercises; repeat policy commands are not counted as extra coverage.
The final candidate's 2197 source hashes/modes are guarded before and after the
follow-up. Twelve changed runtime/test inputs and seven native binary identities
bind the receipt. Subsequent edits affect documentation/status/receipts only.

The first repository check catches README's 250-word limit; trimming resolves it.
Final make check and diff/line-width checks pass. Portable maintenance verification
retains the completed PR160 cycle because its inputs do not change. Local source
review finds no unresolved selected-scope finding; this is not independent
maintainer approval. The existing binding-debug potentially-uninitialized warning
reappears during the development rebuild. No clean-warning claim is made.

The affected manual renders to roff/text. Mandoc remains status 4 with inherited
mso/LINKSTYLE/spacing diagnostics and one added subsection spacing diagnostic
(SS spacing count 10 to 11). No clean manual-lint/install claim. Rendered examples
and pixel/paging semantics are inspected. No new visual UI requires screenshots.

Canonical archives contain 45 initial and 15 follow-up files. Gzip EOF/CRC and
every payload hash/mode are verified before removing only their completed guest
captures and duplicate source/evidence archives. Host canonicals, guest source,
incremental builds, stages, performance references and VM disks remain intact.

## Remaining boundaries

PR160 is the new integrated reference at **0/3 merged**. This feature checkpoint
becomes **1/3 on merge**. Affected six-target/alternate-profile qualification
remains due at shared cycle closure; full compiler/feature/install/default-X and
owner/milestone/release acceptance stay at their required boundaries. No new OS
API, packet ABI, install/discovery or server-input contract requires an early
platform gate; the clock/PID token code already ran in the binding owner.
Native-CPU/hardware/toolkit/compositor checks remain separate. Deferred gates are
not passes.

Colorset tokens cover colorsets only. They do not observe arbitrary Style/decor
changes or make a later mutation atomic. CFG-01 preview/revert stays unavailable
pending conflict-safe application and recovery. Broader IPC provider/domain,
SESSION/file/clipboard adoption and owner acceptance remain open. Performance
reference PR159 is **1/5 merged**, **2/5 on this merge**; no comparison is due.
