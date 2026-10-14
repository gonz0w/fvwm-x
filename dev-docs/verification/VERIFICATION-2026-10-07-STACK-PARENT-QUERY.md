# STACK-01 / XIO-01: finite parent traversal and highest override queries

Status: locally qualified from the exact 1301-file snapshot. Eight compiler
profiles pass 1144 standard test entries, 120 installed code identities and
72 default BSD X driver invocations. Optional profiles pass 148/128 entries;
27 scoped emulated sanitizer checks pass. Native release CI remains separate.

## Scope and source

The frozen four-story checkpoint is
`builddir/codex-2026-10-07/stack-parent-query/source-stack-parent-query.tgz`:
1301 files, SHA-256
`ce73e8f7735b0c489b950a173f7198b81f27f6a65c29fd2c07041101f962bea1`.
It extends the accepted 1291-file atom/expose snapshot through 15 owned paths,
with ten new files. Narrow function/include/header/Meson/manual edits preserve
other chats' work. The old snapshot remains historical evidence, not coverage
for this code.

The user approved the invoked-window fallback separately for Layer and for
Raise/Lower/RestackTransients. The shared constant-space Brent detector remains
operation-local, retains one ordinary parent lookup per step, adds no depth cap
and leaves client hints intact. Redirection unwinds before any restack, clears
only temporary branch bonuses and applies the existing direct-target operation.
Ordinary inclusion, rank, layer, focus and packet owners remain in place.

Hint validation now also stops on an existing cycle that does not contain the
validated window. It retains existing root-parent rejection behavior and does
not edit other members. Ordinary public hints already reject cycles; private
cyclic fixtures do not demonstrate a normal application reaching internal cyclic
state or a compromised desktop.

`is_above_unmanaged` retains its root snapshot and current server attributes,
including for managed frames. It scans from the top, skips already-excluded
NoFocus/hidden-frame windows and stops at the highest eligible override sibling.
[Xlib documents the root child order as bottom to top](https://www.x.org/releases/current/doc/libX11/libX11/libX11.html#Obtaining_Window_Information).
No XSync barrier, authoritative-state owner, mandatory library or registry is added.

## Focused evidence

- Pure policy: 10000 distinct acyclic nodes, 992 cycle/prefix combinations and
  checked counter saturation; identical header/unit bytes passed on all six
  Linux/FreeBSD/OpenBSD AMD64/ARM64 targets before activation.
- Actual Layer/hint control: two/three-node and prefixed cycles; original target,
  unchanged hints, 128-node ordinary ancestor with one lookup per parent; missing,
  root, self, style stop and retired membership; new hint-cycle rejection retains
  the normal acyclic lookup count.
- Real Layer: 16 client parent chain, ordinary ancestor selection, existing public
  cycle rejection with client property intact, focus, unrelated identity, parent
  destruction, missing/root/self hints and subsequent commands. Existing layer/
  per-window EWMH/highlight/stack/focus driver also passes.
- Actual Raise/Lower/RestackTransients: two/three-node and prefixed cycles across
  24 command/inclusion combinations match direct-target execution. Existing
  transient inclusion is both enabled and disabled; authoritative circular-list
  order matches the actual server frame order. Hints remain intact, and an
  acyclic follow-up retains ordinary parent redirection.
- Query control: the unchanged 108-child snapshot reads all 108 attributes. The
  new highest-match case reads 3, retaining InputOnly/failure/internal exclusions.
  Lower eligible and no-match cases read 105 and 106. Missing target, failed tree
  and off-desk paths retain zero reads. This is a call-count result, not an input
  latency benchmark or a bounded total time guarantee.
- Real override windows: both popup orders with an InputOnly sibling, focus,
  popup teardown and following Lower/Raise pass. The remaining foreign attributes
  are read normally; no assumption of immutable managed-frame attributes is used.

Eight focused Linux registered checks pass for the final four-story source.
The first three stories also passed six private checks each on FreeBSD/OpenBSD
ARM64 and scoped ASan/UBSan: one leak-enabled pure unit and five actual-core/
real-window checks with leak detection disabled for fixture/process scope.
Their separate 1299-file artifacts remain scoped to those earlier three stories.
The final combined sanitizer gate passes 27 checks: nine pure leak-enabled and
18 actual-core leak-disabled. Local emulated results do not satisfy native Linux
release sanitizer CI.

Retained initial failures: omitted existing identity executable; forward client
ID initialization in the finite validation control; private candidate signature
editing and missing legacy RestackWindow declaration; real BSD Focus sent before
clients were viewable. Explicit X map readiness corrects the last fixture race.
No production change was needed for that BSD readiness failure. The existing
RestackWindow definition now has its matching header declaration.

## Combined qualification

Linux GCC/Clang on AMD64/ARM64 each pass 149 entries. FreeBSD/OpenBSD on
AMD64/ARM64 each pass 137. All registered tests are accounted for; installed
executables/modules match tested bytes after verified RPATH transformations.
Four BSD default servers each pass 18 drivers, including OpenBSD ARM64 wsfb.
XRender-disabled and SVG-disabled profiles pass 148 and 128 entries. The
27 scoped sanitizer checks comprise nine leak-enabled pure units and eighteen
actual-core checks with leak checking disabled.

Seven collected archives match all 1301 source hashes/modes. Every archive
member was read through EOF; registered test identities, installed bytes,
default-server evidence and sanitizer scope were validated by
`builddir/codex-2026-10-07/stack-parent-query/validate.py`.
`qualification-summary.json` records the archive hashes and exact counts.

The initial OpenBSD AMD64 suite passed 136 entries and failed the existing
interaction-cancellation fixture with ConnectionRefusedError after its socket
pathname appeared but before listening readiness. Initial test logs/artifacts
remain under `verification-evidence/native-initial-endpoint-readiness` in its
archive. One unchanged affected fixture rerun passed; 136 original passing
entries plus that rerun form the explicitly validated 137-entry combined log.
No source change or full-suite repetition was needed. The initial host standard
runner exit remains recorded; recovery and install logs are separate.

Native Linux release CI, physical hardware, independent toolkit/compositor
interoperability remain unrun. Reverse-chain closure, stable rank sorting,
rank arithmetic and deep acyclic recursion remain separate measured STACK-01
work. This detector removes cycles; it does not make all stacking work constant
or establish a wall-clock event-loop budget.
