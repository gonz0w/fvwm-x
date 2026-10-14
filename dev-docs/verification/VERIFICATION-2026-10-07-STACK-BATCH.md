# STACK-01: bounded restacks and suppressed-icon siblings

Status: locally qualified from the immutable 1332-file source. All combined
local Linux/BSD gates pass. No configuration compatibility decision is pending.

## Changes and ownership

This checkpoint combines the [unmanaged storage owner](VERIFICATION-2026-10-07-OVERRIDE-STORAGE.md)
with ordinary restack batching, full resynchronization batching and consistent
suppressed-icon sibling selection. Its twelve owned paths extend the accepted
1328-file Layer source; unrelated chats remain outside the overlay.

Ordinary Raise/Lower/RestackTransients and new-window full resynchronization now
use a 64-XID operation-local buffer. Each follow-on batch repeats the preceding
bottom XID as its first, unchanged anchor. The shared frame/icon append policy
packs complete records and sends only populated prefixes. There is no restack
heap allocation, whole-ring allocation scan, persistent cursor, callback or event
dispatch while borrowing the ring. Layer still lends its preflighted buffer and
retains its accepted failure contract. Existing initial sibling placement and
final EWMH/module publication surround the complete bounded submission.

The anchor rule follows [X.Org's XRestackWindows contract](https://www.x.org/releases/X11R7.5/doc/libX11/libX11.html):
the first window stays in place and subsequent windows stack below it. Carrying
that bottom window forward preserves the ordered adjacency across calls. This
bounds FVWM's temporary restack storage, not total operation time or Xlib's own
internal buffers. No asynchronous continuation, round trip, grab, library,
thread, configuration name or argument is added.

A new 22-record Lower control also reproduces an existing sibling defect in the
previous 1330-file implementation. A suppressed icon pixmap was selected as the
lower boundary even though it was excluded from the restack list. The affected
record/group then appeared above unrelated managed windows, disagreeing with the
ring. The shared pure bottom-XID policy now selects the last included XID: the
frame for suppressed icons. Visible title/pixmap ordering remains unchanged.
Independent old/new pre-fix controls fail at the same observed server XID; the
repaired real-X cases pass. This is a correctness finding, not a demonstrated
security compromise, and requires no configuration migration.

## Focused evidence

Seven pure and fourteen actual-core tests pass in both the normal Linux build
and scoped Clang ASan/UBSan profile. Pure leaks are enabled and core leaks are
disabled, as in the prior scoped profile. The packing unit covers 352 combinations
of uniform/mixed icon flags and empty/small/64-XID-boundary/multiple-batch rings,
as well as selection, bottom-sibling and representable-count/capacity refusal.
It compares concatenated batches and every repeated anchor with the complete
expected stream. Existing Layer, parent, transient, icon, query and real-client
unmanaged/focus checks remain included.

The new actual-core driver passes twenty cases: 3/22/65/130 managed records,
Raise/Lower/Restack and new-window raise/lower follow-up. Real X frames and mixed
visible/suppressed icons verify bounded Xlib batches, no core fxrealloc calls,
exact ring membership/links and final server order. Both native FreeBSD/OpenBSD
ARM64 checks pass the packing unit and all twenty cases in isolated overlays.

An initial three-record control incorrectly expected a restack even when its
only transient was an excluded icon; the fixture was corrected to include an
eligible transient. That failure, the subsequent baseline sibling reproduction,
old/new comparisons and compiled/native evidence are retained separately. The
combined source contains the corrected fixture and sibling policy.

Exact source and evidence are in
`builddir/codex-2026-10-07/stack-batch-production`. The combined checkpoint passes 1256 standard test entries across eight compiler
profiles: Linux GCC/Clang AMD64/ARM64 (163 each), and FreeBSD/OpenBSD
AMD64/ARM64 (151 each). It also passes 120 installed code identities, 104
default BSD X driver invocations (26 per target), optional profiles with 162/142
entries, and 41 scoped sanitizer checks (15 pure with leaks enabled, 26
actual-core with leaks disabled). No production change or affected rerun was
needed in these combined gates.

Seven complete evidence archives validate all 1332 source hashes/modes, test
registrations, installed bytes after the recorded RPATH transformation, gzip
CRC/EOF and every tar payload through EOF. Source archive SHA-256:
`0e6a3ced9778044936f4ae568d5a57f090bac2731882434b86e8e4c0e40a8be0`.
The retained OpenBSD AMD64 verification-profile split uses its previously
recorded emulation harness timeouts; ordinary test timeouts remain unchanged.

Linux Asciidoctor generates the changed manual; native OpenBSD ARM64 mandoc
renders it and reports the same 101 baseline diagnostics as the preceding
Layer source, including two inherited formatter errors. The initial missing
OpenBSD Asciidoctor attempt is retained; no dependency was installed.

Group-free closure, dead-source cleanup and subsequent callback ownership work
are outside this qualified source. Native Linux release CI, physical hardware,
toolkit and compositor checks remain separately unrun.
