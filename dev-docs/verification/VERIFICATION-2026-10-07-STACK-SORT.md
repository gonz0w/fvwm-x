# STACK-01: stable transient ring sorting

Status: locally qualified in the exact 1314-file three-story traversal checkpoint.
The 1309-file focused evidence below remains historical. This change is outside
the independently qualified arithmetic checkpoint. See the
[combined report](VERIFICATION-2026-10-07-STACK-TRAVERSAL.md).

## Behavior and measured work

The temporary transient ring now uses an iterative stable merge sort. Descending
rank order, ties, circular sentinel ownership and reciprocal links are preserved.
It uses no allocation, recursion, callbacks or X requests. Already sorted rings
return without changing links after one linear comparison pass.

Private copies of the unchanged and candidate actual core use comparison counters
and startup fixtures. Those accessors exist only in private measurement sources.
Five rank patterns and fourteen sizes cover 70 rings, including empty/singleton,
partial runs, endpoint ties, duplicates and 2049 records. Exact order is checked
against an independently sorted rank/original-position oracle.

For 2048 records, ascending-rank comparisons fell from 4,190,209 to 11,265;
duplicate-rank comparisons from 1,909,843 to 19,850; alternating endpoints from
1,049,599 to 16,386. Already descending and all-equal cases retain 2047 comparisons.
These operation counts do not measure end-to-end desktop input latency. The first
candidate lacked the sorted fast path; its measurements are retained separately.

## Evidence scope

Frozen focused source: `builddir/codex-2026-10-07/stack-sort/source-stack-sort.tgz`,
1309 files; SHA-256 `fb110c0a140eb894ae1b996ae1961c13dbb0fc1b04ca5c22f1e9fd189597d58a`.
Seven normal checks pass: native ring policy, actual-core rank/redirection and
real-X raise-unmanaged, parent-layer, layer-boundary and stack-layer drivers.
One leak-enabled pure policy and six leak-disabled actual-core/real-X checks pass
under scoped Clang ASan/UBSan. Both private old/new 70-ring controls also pass
normal and scoped sanitizers. Native FreeBSD/OpenBSD ARM64 each pass the pure
70-ring unit against exact policy/unit source bytes; these are early policy
checks, not full native core/default-server acceptance.

`focused-evidence.tgz` SHA-256
`38b3a921f7179482f38c89158944cc9a23761cfa5d85cb3a5e378339ea6bd584`
retains exact 1309 source fingerprints, compiled identities, core/X logs and
private controls. Native policy archives and validation are in the same directory;
all retained archive file payloads were read through EOF. The scoped pure sanitizer
pass is recorded in `focused.log`; the later six-core runner replaced Meson's
current testlog. A private sanitizer runner quoting failure is retained; the
corrected runner passes without a production change.

The remaining reverse-chain closure and acyclic parent recursion are separate
stories. Full six-target/compiler/install/default-server/optional/scoped-sanitizer
qualification now passes in the coherent traversal batch. Native Linux release CI and
physical desktop checks remain separately unrun.
