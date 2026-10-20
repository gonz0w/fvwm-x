# Style flag predicates checkpoint — 2026-10-19

Owner: STYLE-01 #38 / CLEAN-01 #34. Base: merged PR125. Implementation and
nine focused Linux entries pass; combined qualification is pending.

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
original signed-char byte expressions: every pair of byte values under eight
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

## Combined checkpoint

Pending: freeze one signed/DCO runtime candidate, run the required six-target,
ten-profile matrix once, retain focused entries only after exact non-Markdown
source and eleven binary matches, and validate source/capture/installed/runner
identities. Fresh tests request prerequisite builds; installation uses
`--no-rebuild` after successful build/tests. Publication will not repeat the matrix.

The three known required OpenBSD AMD64 omissions (CI worker, SVG/MFL startup,
legacy property startup) and its full-install/two staged-geometry gap retain
their existing owner/source evidence without retries. Native-CPU sanitizer,
hardware, toolkit/compositor and release acceptance remain separate. VESA #93
is deferred. Actions remain disabled. The review QEMU/VNC session remains closed.
