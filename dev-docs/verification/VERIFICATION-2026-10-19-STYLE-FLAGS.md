# Style flag predicates checkpoint — 2026-10-19

Owner: STYLE-01 #38 / CLEAN-01 #34. Base: merged PR125. The three-operation
combined checkpoint is complete; required broader gaps remain explicit below.

## Three related operations

- List/simplification subset checks use bounded 64-bit reads and a 32-bit
  short-record step, followed by bytes. No set bit may be absent from the
  containing span.
- Simplification intersection checks use the same bounded read shape and stop
  on the first nonzero intersection.
- Conditional-window masked equality compares XOR differences under the mask.
  The public `blockcmpmask` wrapper still returns Xlib True for equality.

The private inline policy uses memcpy for unaligned, alias-safe reads. Only
complete words are read; byte tails preserve the supplied bound. All wrappers
retain their zero/negative-length results. Inputs may overlap because no span
is written. Mutable blockor/blockand/blockunmask operations are unchanged.
No configuration syntax, record layout, packet format, persistent state,
allocation, command ordering or update/server-grab/X-error barrier changes.

## Focused evidence and measured limits

The separate `style-flags` unit compares 1,351,339 cases against independent
original plain-char byte expressions: every pair of byte values under eight
masks, isolated bits, lengths through 257, all eight alignments, overlapping
spans and mappings guarded on both sides. NULL spans with zero, -1 and INT_MIN
lengths are included. Actual layouts in the development guest are four bytes
for selector ID flags and 48 bytes each for style/window flags.

Nine entries pass in one incremental Linux ARM64 GCC profile: style-flags,
style-match, style-id-filter, style-profile, style-update-work,
style-command-owner, style-index-maintenance, style-index-retention and
window-update. The actual core adds 1,154 public-wrapper cases and 128 parsed
window conditions (Maximized, StickyAcrossPages, Iconic, Shaded; both polarities
across all sixteen combinations). Existing full-record/order/simplification
reference witnesses remain in place.

Before changing production callers, one same-binary probe measured the original
byte expressions against the proposed policies via volatile function-pointer
calls. Three paired samples per record role alternate invocation order; the
two 48-byte roles supply six observations per width. For full 48-byte scans and
late differences, wider predicates take roughly 19–21% of byte-reference time
in this guest. First-byte differences take roughly 1.26–1.75 times as long.
These are selected microbenchmarks, not a universal or whole-WM speed claim;
production callers can inline/specialize differently. The captured development
production object retains 36,972 allocated executable bytes before/after;
the generic masked wrapper grows from 20 to 48 disassembled instructions while
other specialized code changes offset that growth. The pre-change WM identity
matches PR125's completed capture. Probe/archive, compiler-command/object and
raw paired sample identities are retained under the owning local work tree.

## Combined checkpoint and source binding

Three read-only subset/intersection/masked-equality improvements pass one
combined checkpoint: 2037/2037 standard results (2028 fresh / nine retained by
exact non-Markdown source and eleven binary identities), plus 449/449 optional
entries across six targets/ten profiles. Eleven pure drivers verify 14,864,729
reference cases; fifteen actual-core drivers verify 17,310 wrapper cases and
1,920 parsed conditions. Four scoped guest sanitizers, nine fresh full stages
and four authenticated BSD displays pass. Three required OpenBSD AMD64
omissions and its full-install/two staged-geometry gap remain without retries.

| Target/profile | Executed standard or optional entries | Passed |
| --- | ---: | ---: |
| freebsd-amd64/default | 249 | 249 |
| freebsd-arm64/default | 249 | 249 |
| fvwm-x-linux-arm64-svg/clang | 261 | 261 |
| fvwm-x-linux-arm64-svg/gcc | 261 | 261 |
| fvwm-x-linux-arm64-svg/minimal | 209 | 209 |
| fvwm-x-linux-arm64-svg/software | 240 | 240 |
| linux-amd64/clang | 261 | 261 |
| linux-amd64/gcc | 261 | 261 |
| openbsd-amd64/default | 246 | 246 |
| openbsd-arm64/default | 249 | 249 |

The preceding ten profiles retain all 2,479 registrations in their original
order, with only the new `style-flags` entry added once per profile. The
2,489 registrations include three required OpenBSD AMD64 omissions;
all 2,486 executed standard/optional entries pass. Nine focused entries are
retained in Linux ARM64 GCC after every non-Markdown source/mode, eleven
binary identities and six fixture evidence sets match. Their rows remain
separate from fresh results; publication repeats no ordinary focused suite.

All fifteen actual-core style-profile drivers retain independent linear
lookup/order/simplification witnesses and add the wrapper/condition results.
Ten normal profiles plus one scoped sanitizer execute the new pure policy.
The initial pre-production measurement probe is separate from these counts.

The four scoped guest ASan/UBSan entries are pure style-flags (leak detection
enabled) and EWMH state, style-profile and window-update core fixtures (their
existing scoped leak policy). Native CPU sanitizer acceptance is separate.
Nine fresh full installs validate 135 code and 88 catalog identities after
normal rpath handling. Four private-authority BSD displays pass 38 fixture
invocations with clean server exits and unauthenticated refusals. Fresh stages
exercise 18 geometry invocations / 126 cases; real-client style consumers
remain covered.

Fresh tests retain Meson's prerequisite builds; installation uses
`--no-rebuild` after successful build/tests. Required unrelated omissions
still prevent the affected full install. Portable `make verify` runs once:
166 passes and eleven explicit native-Meson skips. All eleven compiler
logs are retained; none warn in changed C/header lines.

Runtime commit: bb68a2566e271f7a5e88aac670f1a6e061872973.
Runtime tree: 593da1c5365f7d4e4b21eca2f824f3ac7e3306d6.
Frozen archive: f6486a9d432c0f6293125b34e47d0c6b6d36adc9c844a003fceff4ef3454db22, 2046 regular inputs.
All input content and canonical tracked executable/non-executable modes
match the Git tree. Final deltas are documentation/receipts only; production
inputs stay exact. The final PR identifies its signed/DCO head and tree.
The [machine receipt](VERIFICATION-2026-10-19-STYLE-FLAGS-COMBINED.json) binds sources, captures, executed
runners, commands, registrations, binaries and installs.

## Required gaps and ownership

OpenBSD AMD64 executes 246 passing entries. Three known required omissions
remain: CI asset worker timeout, SVG/MFL startup refusal and PR120 legacy
property startup refusal. Original source/archive/control identifiers remain
in the receipt; no retries, deadline changes or causal claims are added.
Its full install and two staged-geometry invocations remain unqualified.
Default-X built-client checks pass there. The preceding
[matching report](VERIFICATION-2026-10-19-STYLE-MATCH-WORK.md) retains their
original scope; broader CI/IMAGE/STATE owners remain open.

One Linux development storage preflight found less than the 1 GiB owner
reserve. All raw members and the canonical PR125 archive were verified
before removing 357,018,972 duplicate guest bytes.
Canonical host evidence, source, builds, stages and VM disks remain intact.
The next six-target preflight passes; source uploads use owner storage to
preserve Linux AMD64's small /tmp margin.

Native CPU sanitizer, hardware/input, toolkit/compositor and release
acceptance remain separate. VESA #93 is deferred; Actions remain disabled.
The review QEMU/VNC remains closed. Broader STYLE/CLEAN scope, mutable flags,
update plans and grabs retain their own checkpoints. Publication repeats
no matrix.
