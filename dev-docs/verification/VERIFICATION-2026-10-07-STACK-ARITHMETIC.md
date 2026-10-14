# STACK-01 / DATA-01: transient rank and Layer arithmetic

Status: the four-story arithmetic checkpoint is locally qualified from its
exact 1306-file source. Eight standard compiler profiles pass 1168 entries;
120 installed code identities match; all four BSD default servers pass 80 driver
invocations. Optional profiles pass 151/131 entries and scoped sanitizers pass
30 checks (ten pure leak-enabled, twenty core leak-disabled). Native Linux
release CI and physical desktop checks remain separately unrun.

## Controls and changes

The unchanged qualified parent/query core overflows signed rank arithmetic in
a finite 10,739-record synthetic managed chain with the historical 200000 branch
bonus. The control uses real Xlib contexts and actual marking code, not thousands
of real desktop clients. UBSan stops at depth 10,738. Two separate real-window
controls reproduce `layer + 1` overflow for `Layer 0 2147483647` and relative
addition overflow after `Layer 0 2147483646`, `Layer 2`. They use owned Xvfb,
MFL and ordinary clients. Undefined arithmetic is confirmed; these controls do
not establish a compromised desktop.

Internal scratch rank storage and the straight-subtree minimum become int64_t.
Historical branch weights, comparisons and sorting remain unchanged. The
permanent actual-core control checks every rank in a 12,000-record chain for
both positive branch bonuses and negative lower penalties. This repairs the
reproduced 32-bit boundary. It does not close all rank arithmetic: scratch values
can be retained across marking consumers; accumulation/reset contracts, stable
sorting and traversal remain separate work.

Valid maximum-layer insertion uses direct less-than/equality comparisons instead
of incrementing the layer. Moving down still inserts below equal-layer siblings;
moving up still inserts above them. The real-X driver checks INT_MAX, equal-layer
order, representable relative moves, default/absolute commands, existing negative
clamping and following commands. Relative addition was unchanged in that earlier two-story source; the approved
production refusal is included in the final checkpoint below.

## Earlier focused two-story scope (historical)

The two-story source is
`builddir/codex-2026-10-07/stack-rank/source-rank-layer.tgz`, 1304 files,
SHA-256 `4a36fca8398e304931c9b157bedfc02072e91ea3a06067e02f3e447ada34dd1e`.
It derives from the qualified 1301-file source with narrow owning changes to
fvwm.h, stack.c and test registrations, plus three test files. Other chats'
changes are preserved. The qualified parent/query source remains immutable and
does not cover these repairs.

Six checks per profile: stack-rank, stack-redirection, stack-layer,
raise-unmanaged, parent-layer and layer-boundary. GCC normal and Clang
ASan/UBSan builds compile the changed core and actual-core fixtures. Scoped
sanitizer leak checking is disabled for these process/fixture checks. The earlier
rank-only five checks per profile have separate retained logs. Native release
CI and BSD/matrix acceptance remain unrun for this next batch.

The unactivated private relative candidate computes a wider sum, logs/refuses
results above INT_MAX before mutation, and preserves the previous layer/order.
Its real-window scoped ASan/UBSan driver passes small/large overflow refusal,
valid maximum/equal-layer placement, ordinary offsets/default/negative clamping
and subsequent commands. Its source is separate from production.

The complete archive is `builddir/codex-2026-10-07/stack-rank/focused-evidence.tgz`,
SHA-256 `76abe7a68027a2e146de524336776b09a91850d1732ef76220fdeb66465f8bd5`.
All 1304 source fingerprints, compiled binary identities, private candidate
source identities and archive payloads through EOF match;
`focused-verification-summary.json` records that validation.

Initial failures retained: ambiguous Meson executable target; a new sanitizer
profile missing the established compiler-rt runtime option; reconfigure syntax/
option persistence; and archive timestamps retaining an old stack object during
an incremental build. Explicit target selection, Meson configure and touching
the changed owned input corrected those build/evidence issues. The stale-object
maximum-layer failure is retained separately. The final build logs show changed
stack objects rebuilt before both six-check profiles; no design change was
required for those initial build failures.

## Approved activation and repeated-rank follow-up

The reviewed relative Layer candidate is now production code. It calculates
in int64_t, returns before stack mutation when the result exceeds INT_MAX, and
logs the refusal. Permanent real-window checks retain the previous layer and
sibling order for small/large overflows; representable offsets, default, absolute
INT_MAX and negative-to-zero behavior remain supported.

A separate finite actual-core control on the earlier wide-rank source uses
24 synthetic records and at most 128 marking calls. UBSan reports signed 64-bit
overflow on the 52nd call (iteration 51). No normal icon/window lifecycle sequence
is claimed by that synthetic control. Source inspection identifies retained
scratch values in marking consumers, so widening alone did not close arithmetic.

The shared checked rank helper now preserves every representable mathematical
`rank + parent + 1`, including endpoint cancellation, and stops outside values
at INT64_MIN/MAX. These are internal temporary keys, not script/config values or
new client-count limits. Existing stable sorting preserves order for equal keys.
Pure coverage includes ten boundary/weight values in a 100-pair independent wider
oracle plus explicit endpoints, cancellation and historical branch weights.
The actual-core regression finishes 128 repeated marks with unchanged membership
and endpoint ranks. Branch-weight/deep/ring/real-window regressions still pass.

The four-story frozen archive is
`builddir/codex-2026-10-07/stack-arithmetic/source-stack-arithmetic.tgz`,
1306 files, SHA-256
`a8169fb4775802fd898b6a0d773cd800a59e22ccebc41c9a5ba97736d4be98b9`.
It changes nine owned paths from the accepted 1301-file checkpoint. Earlier
1304-file evidence above remains historical and does not cover this helper or
approved production activation. Seven focused normal checks pass; one
leak-enabled pure and six leak-disabled actual-core/real-X sanitizer checks pass.
The combined scoped sanitizer gate now passes ten leak-enabled pure and twenty
leak-disabled core checks. All seven combined archives now validate exact 1306 source hashes/modes, registered
test identities, installed bytes after verified RPATH transformation and every
retained payload through EOF. `qualification-summary.json` records archive hashes
and profile metadata. No arithmetic production change or affected rerun was
needed during this checkpoint. Subsequent sorting/traversal changes are outside
this qualified source.
