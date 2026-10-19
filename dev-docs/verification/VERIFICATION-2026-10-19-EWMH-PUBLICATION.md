# EWMH publication work — 2026-10-19

STATE-01 #37. Base: merged PR #121,
`4ececc82cd0348c0d1c7dc0ef8f05957bbc95964`. One three-story runtime checkpoint;
existing command/configuration and X publication/query barriers remain.

## Result and implementation

Three EWMH publication improvements pass one combined checkpoint: 2029/2029
executed standard and 447/447 optional entries across six targets/ten profiles.
Selected strut walks fall from two to one, permission queries from nine to six
and converted-name length scans from 189 to 63. Four scoped sanitizers, 9 fresh
stages and four authenticated BSD displays pass. Three required OpenBSD AMD64
omissions and its broader install/staged-geometry gap remain without retries.

| Improvement | Selected baseline | Candidate | Ownership and limit |
| --- | --- | --- | --- |
| Paired strut refresh | Two managed-window walks | One paired walk | Same synchronous handler; individual static/dynamic paths retained |
| Allowed actions | Nine policy queries | Six distinct queries | Six-key local bitset; fullscreen resize override separate; ordered atom vector retained |
| Desktop names | 189 converted-name length scans | 63 | Cache one length per owned conversion for this publication; no retained result |

`ewmh_WMStrut` updates both records before the existing paired work-area helper
when both changed. No callback or property publication separated the old walks.
Permanent static-strut retention on a zero property, dynamic clearing, initial
reset, missing properties, filtering and independent single-mode paths retain
their behavior. The shared helper retains independent validation of static and
dynamic rectangles. `EWMH_GetWorkAreaIntersection` still refreshes both owners,
including ignored-area queries, as required by the DRAW-01 contract.

`EWMH_SetAllowedActions` uses typed operation-local tested/allowed bits for the
six requests made by the fixed read-only table predicates. A fullscreen resize
with override enabled has its own key. The current predicates only read window
flags and hints; these requests invoke no X calls or callbacks. Results expire
at return, so the next publication observes changed hints/flags. Atom order,
publication points, constant predicates and external handlers are preserved;
this is not general property-write suppression or a cross-window cache.

`EWMH_SetDesktopNames` stores the converted byte length beside each owned
buffer, reusing it for copy and offset calculation. Null conversions still
publish empty entries. Negative desks, the contiguous prefix, gaps, empty/UTF8
bytes and long names retain their assembly/termination rules. Counts and array/
property arithmetic are checked before allocation/copy; an unrepresentable
property is diagnosed, temporary conversions are freed once, and the previous
property is retained. The existing desktop-count assignment order remains.
On the tested 64-bit ABI, a 64-entry temporary array grows from 512 to 1024
requested bytes: eight extra transient bytes per entry. No persistent cache or
universal total-memory/latency improvement is claimed.

## Independent probes and focused evidence

The existing actual-core EWMH fixture now links a separate publication probe;
its shared assertion-enabled core and original reference tests remain. Each
normal/missing-atom mode compares 80 permission combinations against the old
callback traversal and the server's ordered atom vector, including MWM override,
fixed-size/style flags and changes between publications. Seven strut cases use
an independent rectangle/filter reference and measure actual refresh entry
calls. Name assembly uses controlled conversion results to check UTF8 bytes,
empty/refused conversions, prefix/gap selection, long strings, one-time frees
and a refused mocked length without allocating a huge string. It tests the
assembly owner; the conversion implementation itself is unchanged. Iconv-disabled
profiles report that capability and run the other publication cases.

The same C probe and Meson registration measure the old three production files
and the candidate on one incremental Linux ARM GCC profile. Both normal/missing
modes report 9/2/189 versus 6/1/63; the final Python driver differs from focused
input only by an AST-equivalent line wrap. Focused candidate checks pass four
native entries with production WM/MFL builds. The first probe initializer was
corrected at compile time; the first candidate compile caught the missing
logging header. Both failed attempts and completed phases remain preserved.
Neither baseline nor successful focused suites repeat for publication.

## Combined checkpoint

- One frozen run covers six targets/ten profiles: Linux ARM64 GCC/Clang/software/
  minimal, Linux AMD64 GCC/Clang and default FreeBSD/OpenBSD on both architectures.
  Original executed scope is 2029/2029 standard and
  447/447 optional entries. Including the three
  required omissions, standard scope is 2029/2032.
- 15 EWMH driver invocations / 30 modes
  retain old atom/client/work-area checks and add 2400
  permission cases and 210 strut cases. Name work has
  28 supported modes out of
  30; unsupported modes explicitly report no conversion.
- Four scoped guest ASan/UBSan entries pass: EWMH state, pure state policy,
  style profile and window updates. Existing WM process-lifetime leak detection
  remains disabled; the pure policy scope enables it. Compiler-rt/options and
  exact commands are retained. This is not native-CPU Linux sanitizer evidence.
- 9 fresh stages validate 135 code and
  88 catalog identities after normal RPATH adjustment.
  Four authenticated default BSD displays pass 38 fixtures,
  refuse unauthenticated clients and exit cleanly. Built OpenBSD AMD64 probes
  remain distinct from its unperformed fresh full install/staged probes.
- 18 staged geometry invocations / 126
  cases and 10 real-client invocations retain
  170 wildcard, 120
  simplification, 30 candidate and
  90 shared-condition cases.
- All 2028 regular source hashes/modes and affected fixture,
  runner, archive, binary and installed identities are validated. Eleven native
  compile logs retain diagnostics separately. `make verify` runs once on macOS:
  166 passes / 11 native-Meson skips. Final review and foundation/link/whitespace/
  source checks cover only the documentation/receipt delta.

Owner builds are reused, with two heavy guests at most and two Ninja jobs each.
The Linux AMD64 temporary-space guard refused before source sync/compilation;
that failed launch/log/result was copied byte-for-byte, the local dispatcher
stopped while detached BSD jobs continued, and a resumed dispatcher reconnected
to the active jobs first. Only unfinished Linux work was requeued. Three old
source archives and eight completed ARM qualification archives were removed
from guests only after exact size/SHA256 matched preserved canonical host copies.
No source/build tree, VM disk or another owner's evidence was removed. The full
1 GiB owner / 256 MiB temporary-space floors remain; no completed suite repeats.

## Source and retained gaps

Signed author-DCO runtime: `33b286875f41649f7dffaaed5a43307654759258`;
tree `6e11667881cfad37cf7ae20149223a0b870b40d1`. Frozen archive:
`d37cb51d864774a99d0e539284471780c050ee07f10202a3646b7c31451a17e0` (2028 regular inputs).
Final upkeep is Markdown/receipt only.

[Machine receipt](VERIFICATION-2026-10-19-EWMH-PUBLICATION-COMBINED.json) records counts, affected input hashes,
manifest/commit binding, per-profile publication results, installed identities,
script/archive digests, diagnostics and remaining gaps. Full raw logs/manifests
and collector proofs remain in the owner workspace under
`builddir/codex-2026-10-19/ewmh-publication-work`, including the original failed
compile/capacity attempts and exact baseline comparison.

Three required OpenBSD AMD64 entries stay omitted without retries: CI-01's
original two-second worker timeout, IMAGE-01's SVG/MFL startup refusal, and
PR120's original legacy-property first-MFL-connection refusal. The last failure
has one already-completed unchanged control, with all five property checks
passing at the same deadline; the cause remains unproved. Its control is not
rerun or rebound to this source. Exact predecessor runtime/archive/capture
identities remain in the omission receipt. The broad OpenBSD AMD64 fresh install
and two staged-geometry probes remain unperformed. Earlier raw timing failures/
controls remain with their original reports.

Native-CPU sanitizer, physical hardware, actual toolkits/compositors and release
acceptance remain separate. Actions is disabled; VESA #93 stays deferred.
The broader STATE-01 burst/coalescing owner remains open. The user requested an
isolated interactive QEMU/VNC desktop after PR readiness; that subsequent manual
session will use this qualified build and is not additional release evidence.
