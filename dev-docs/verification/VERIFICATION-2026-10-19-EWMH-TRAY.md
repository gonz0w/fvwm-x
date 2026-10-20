# Existing legacy tray membership — 2026-10-19

STATE-01 #37 / CLEAN-01 #34. Base: merged PR #123,
`afc41adf51aadad7835381f1a93372e6ed1c7e5f`. One coherent three-operation runtime checkpoint in the existing
KDE legacy tray path. New TRAY-01 desktop features retain their later-stage
ownership; no configuration, native packet or command API changes.

## Result and implementation

Three existing legacy-tray insertion/retirement/publication improvements pass
one combined checkpoint: 2029/2029 standard results, fresh or retained after
source/binary verification, and 447/447 optional entries across six targets/ten
profiles. Four focused standard entries are retained; 2025 standard entries run
fresh. Selected 64-window add/remove snapshots each reduce heap requests from
one to zero. Source review shows two insertion/retirement searches become one,
and two publication walks become one. Four scoped sanitizers, 9 fresh full
stages and four authenticated BSD displays pass. Three required OpenBSD AMD64
omissions and its broader install/staged-geometry gap remain without retries.

| Operation | Predecessor | Candidate | Evidence |
| --- | --- | --- | --- |
| Add a distinct member | Membership search, then append search | Reuse one matching/end link | Code review; ordered membership/property reference |
| Retire a member | Event lookup, then deletion lookup | Unlink through the found link | Code review; head/middle/tail and X event-mask/flush/order checks |
| Publish members | Count walk, exact heap allocation, fill walk | One bounded append walk, 64 IDs inline | Selected add/remove at 64 IDs: one snapshot heap request each becomes zero |

`find_kst_item_link` returns a matching link or the empty insertion link. Add
still reads and validates the marker before looking up membership; duplicates
still stop without allocating a node or publishing. Retirement keeps the same
`XSelectInput(NoEventMask)` then `XFlush`, unlinks/frees the found record once,
and publishes afterward. Unmap/Reparent selections/flushes and ignored/missing
events retain their behavior. The borrowed link never survives event/command
dispatch. Project X error handling only records flags/logs here; its fatal I/O
path exits. Neither reenters tray mutation between finding and using the link.
No tail pointer, persistent index or new global state is introduced.

Publication reuses the existing `ewmh_client_list` operation-owned buffer and
its checked SIZE_MAX/Xlib signed-count growth. It owns window IDs, not window
records. Empty lists still replace the property; insertion order remains.
Refused growth diagnoses, releases the partial snapshot and retains the old
property. The membership mutation still commits; a later actual membership
mutation repairs the property. This internal failure behavior replaces the
old fatal snapshot allocation without changing configuration contracts.

The helper has 64 inline IDs (512 bytes on the tested 64-bit targets), plus
its pointer/count/capacity. Spilled doubling can allocate more than the previous
exact-count buffer: 65 IDs use a 128-slot heap buffer as well as the inline
owner. Only selected snapshot heap requests are instrumented. Search/walk
reductions are established by source structure; they are not separately
instrumented node-visit or wall-clock measurements. There is no universal
latency or total-memory improvement claim.

## Independent probe and development evidence

The existing actual-core EWMH fixture links a separate tray probe and two
X call wrappers. It compares actual ordered server properties against its
own window-ID array, checks membership and inspects actual selected masks.
The wrappers assert selection/flush occur while the retiring member still
exists and that publication follows retirement. Shared assertion-enabled
core objects and all existing EWMH/atom/publication probes remain.

Each normal/missing-atom mode has 141 predecessor cases and 145 candidate
cases: absent/mistyped/empty markers, a valid zero ID, duplicates, 64/65-ID
inline/spill boundaries, insertion order, head/middle/tail and missing removal,
Unmap/Reparent/ignored events, empty replacement and two refused publication/
repair cases. All created windows and tray nodes are retired. Existing pure
snapshot tests retain arithmetic, refused-growth and ownership coverage.
The refusal hook exercises the publication owner's cleanup/recovery; the
unchanged pure helper tests exercise its allocation failure implementation.

The first baseline probe incorrectly expected an empty marker to be accepted.
It failed its assertion in 0.17 seconds. The existing fixed-record policy
requires exactly four bytes; the corrected probe tests rejection and a valid
zero-valued record. The failed source/logs remain. Corrected baseline runs the
unchanged production file and passes one native entry/two modes. Candidate
passes four focused entries with WM/MFL builds. C probe, Meson and wrapper
sources are identical between the successful baseline and candidate. The
candidate Python driver tightens the retained baseline's observed 141 cases
from an earlier >=140 assertion; it changes no fixture or deadline. Both
baseline modes' preserved records satisfy the tighter assertion without a
repeat baseline execution.

## Combined checkpoint and retained focused work

- Six targets/ten profiles reuse owner builds: Linux ARM64 GCC/Clang/software/
  minimal, Linux AMD64 GCC/Clang and default FreeBSD/OpenBSD on both architectures.
  At most two heavy guests and two Ninja jobs each run. FreeBSD ARM runs early
  for the existing X call/lifetime path. Dependency detection is refreshed.
- Standard scope is 2029/2029, composed of
  2025 fresh and four retained focused entries. Optional scope
  is 447/447 fresh. Including three required
  omissions, the standard gate has 2032 registered entries.
- Linux ARM GCC retains exactly four successful focused results only after all
  six actual binaries and all frozen non-Markdown inputs match. Its raw fresh
  log and retained rows/bindings remain separate; the combined receipt validates
  their unique-name composition and exact retained state evidence. Meson's
  prerequisite build remains for fresh tests. No ordinary focused suite repeats
  just for publication or to fill the combined count.
- 15 EWMH invocations / 30 modes verify
  4350 tray cases and 60 refused-publication repairs,
  alongside the existing action, strut, atom, client-list and desktop-name checks.
  Core fixtures preserve 75 assertion-enabled compilations and Linux's two
  fault-wrapper compilation contracts. No registered test is added/removed.
- Four scoped guest ASan/UBSan entries pass: EWMH state, pure state policy,
  style profiles and window updates. Process-lifetime leak detection remains
  disabled for WM fixtures and enabled for pure policy. This is guest evidence,
  not native-CPU Linux sanitizer acceptance.
- 9 fresh full stages validate 135 code and
  88 catalog identities after normal RPATH adjustment.
  Prior comparisons use PR123's source-bound scoped stage identities, without
  promoting its scoped OpenBSD AMD64 installation into full qualification.
- Four authenticated native BSD displays pass 38 fixtures,
  refuse unauthenticated clients and exit cleanly. 18
  staged geometry invocations verify 126 cases; built OpenBSD
  AMD64 probes remain separate from its unperformed full stage/two geometry probes.
  10 real-client invocations retain existing wildcard,
  shared-condition, simplification and candidate checks.
- All 2037 frozen source hashes/modes pass native postchecks.
  Raw archive sizes/hashes, every capture member, executed runner versions,
  compile/test options, binary and installed identities are validated. Compiler
  diagnostics remain in the receipt/logs. Local source/ownership review has no
  findings. Portable `make verify` runs once: 166 passes / 11 native-Meson skips.
  Final documentation upkeep uses foundation/link/whitespace/source checks only.

Only completed task-owned duplicate input/evidence archives were evacuated
from the ARM guest, after their exact size/SHA256 matched canonical host copies.
Source/build trees, VM disks, logs and other owners' work remain. The 1 GiB owner
and 256 MiB temporary-space floors are retained. Large qualification input
archives go to each owner's filesystem to preserve temporary-space headroom.

## Source, evidence and remaining limits

Signed author-DCO runtime: `71e42b627dec50737dd46cebbbf07202ecc71155`;
tree `68833afb79f7ce75ad50d4b9e4f57d1c84687120`. Frozen archive SHA256:
`7ccdf4d8a691cc458747b3884267eb012405ba57e959fcfa19be22f38fb72cca` (2037 regular inputs).
Final upkeep changes only Markdown and the machine receipt.

[Machine receipt](VERIFICATION-2026-10-19-EWMH-TRAY-COMBINED.json) contains profile results, tray work,
source/runner/archive/capture identities, retained/fresh composition, installed
identities, diagnostics and limits. Full raw manifests, source/compile attempts,
initial failed probe, corrected baseline, focused checks and native archives
remain under `builddir/codex-2026-10-19/ewmh-tray-work` in the owning workspace.

Three required OpenBSD AMD64 omissions retain exact predecessor scope without
retries: CI-01's worker timeout, IMAGE-01's SVG/MFL startup refusal and PR120's
legacy-property first-connection refusal. The latter's one previously completed
unchanged control remains with its original source/deadline and unproved cause;
it is not rerun here. Full fresh installation and two staged geometry probes
remain unperformed on that target. Earlier raw timing failures/controls remain
in their original reports. [Prior full runtime scope](VERIFICATION-2026-10-19-EWMH-PUBLICATION.md).

Linux execution uses macOS-hosted environments; BSD native kernels run under
QEMU. Native-CPU sanitizer, physical hardware, actual toolkits/compositors and
release acceptance remain separate. Actions stays disabled; VESA #93 remains
deferred. Broader STATE-01, CLEAN-01, STYLE-01 and MAINT-02 scope stays open.
