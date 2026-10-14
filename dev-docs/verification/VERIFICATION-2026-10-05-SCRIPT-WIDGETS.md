# FvwmScript widget arithmetic and input — 2026-10-05

The user approved clamping progress-indicator and scrollbar-thumb rendering
while retaining stored values. This follows the
[value storage pass](VERIFICATION-2026-10-05-SCRIPT-STORAGE.md).
All seven final archives pass the verification matrix below.

## Implemented behavior

Horizontal/vertical dipsticks and scrollbars share pure checked arithmetic.
Rendering clamps a copy of Value to the range. ChangeValue still stores its
requested integer, and GetValue returns that stored value. Endpoint commands
retain their existing value-adjustment behavior and now redraw even when the
current value stays inside the new range. Equal/inverted runtime endpoints
produce empty indicators or stationary thumbs, and mouse selection on unusable
ranges/tracks returns without entering a drag loop.

Representable startup corrections remain +50 for inverted dipsticks and +10
for equal/inverted scrollbars. An overflowing upper endpoint saturates at
INT_MAX; a collapsed result is rendered safely. Proportions use checked int64
intermediates over the complete int32 range. Tiny fills omit nonpositive X
rectangle dimensions rather than converting negative dimensions to unsigned.
MiniScroll performs its existing wrap before converting the next value to int.

Scrollbar pointer conversion retains its two divisions and their original
integer rounding; it is not replaced by a single ratio. Return selects once
and returns. Mouse handlers check queued releases before waiting and use poll
instead of a fixed select(32). These are isolated module handlers; the WM event
loop and authoritative X state remain unchanged. The fixture inherits occupied
descriptor slots through 80 and verifies Return and press/release completion.
The rest of the module's select-based event loop still needs its broader FD
and lifecycle review; this is not a claim of support for unlimited descriptors.

ChangeValue/MinValue/MaxValue/Position/Size reuse prefix-compatible checked
integer conversion. No digits still means zero. Overflow refuses that mutation,
leaves its previous state intact and permits later instructions. Diagnostics
bound the quoted value to 80 characters. This does not impose a new geometry
policy or certify all widget geometry/font arithmetic.

The review also found an independent literal encoding defect: negative numbers
could overlap private variable/function tags, while large positive numbers
could overflow during decoding. Private numeric tags now occupy a separate
positive range, with checked decode and no additional constant-table entries.
All int32 literals, including INT_MIN/INT_MAX, retain their decimal value in
expressions and comparisons. Outside-int32 numeric tokens refuse parsing.
The supported AMD64/ARM64 targets use the required 64-bit long model. Module
wire framing, FVWM configuration syntax and argument counts are unchanged.

No runtime dependency is added. Committed parser/lexer outputs are regenerated
from source with Bison 2.3/Flex 2.6.4; build-time generators remain unnecessary.
The manual documents the approved drawing and numeric behavior.

## Verification

Native policies cover complete signed ranges, exact endpoints, empty/inverted
tracks, overflowing startup expansion, wrapping, private tag separation and
invalid tags. Safe ordinary pointer expressions are compared against the
previous formula across track sizes/positions to preserve rounding.

Eleven real-module outcomes cover explicit startup endpoint/getter results,
occupied inherited descriptor slots, actual clamped pixels and unchanged stored
values, tiny fills, equal/inverted runtime ranges, full signed proportions,
mutation overflow refusal and prefix conversion, signed literals/comparisons,
ordinary/extreme MiniScroll wrap, extreme scrollbar keys, one-shot Return with
legacy rounding, and queued-release mouse completion. Existing six storage,
seven execution and 24 selection/widget outcomes remain covered.

| Target | Full suite | Final widget outcomes | Default-server repeat | Fresh install |
| --- | --- | --- | --- | --- |
| Linux ARM64 GCC | 69/69 | 11/11 under Xvfb | — | passed |
| Linux ARM64 Clang/lld | 69/69 | 11/11 under Xvfb | — | passed |
| Linux AMD64 GCC | 69/69 | 11/11 under Xvfb | — | passed |
| FreeBSD ARM64 | 58/58 | 11/11 under Xvfb | 11/11 on default Xorg | passed |
| OpenBSD ARM64 | 58/58 | 11/11 under Xvfb | 11/11 on default Xenocara/wsfb | passed |
| FreeBSD AMD64 | 58/58 | 11/11 under Xvfb | 11/11 on default Xorg | passed |
| OpenBSD AMD64 | 58/58 | 11/11 under Xvfb | 11/11 on default Xenocara | passed |

All seven archives match 101 source fingerprints and 1146 permission entries.
The summary checks full-suite results/return codes, eleven final widget outcomes,
prior storage/execution/widget outcomes, fixture-only provenance and fresh
installs. Existing interaction, configuration compatibility, module transport,
selection, session, image and event-loop gates pass. Produced Linux C ELF
hardening checks pass; optional Go is disabled.

Native Linux ASan/UBSan CI remains unrun. Ordinary sanitizer environment
variables in Meson logs do not imply instrumented builds.

## Provenance and fixture corrections

Evidence is in `builddir/codex-2026-10-05/script-widgets/`. Runners freeze the
preceding source baseline plus this owned follow-up, preserve modes, reconfigure
and refresh all overlaid source timestamps before complete builds. Concurrent
desktop/theme work is outside this frozen verification scope.

The first new test registration used an undefined xvfb variable; it now follows
the existing live-x-tests option/platform gate. The initial live fixture passed
braced strings directly to numeric commands, which require variables there.
A later key completion assertion incorrectly required the output file to end
with a record, overlooking WriteToFile's section footer. It now waits for a
fresh matching record. Initial logs and source snapshots are retained.

After the full-gate freeze, only the live fixture was strengthened with explicit
startup getter values and tiny-fill pixels. Production sources and binaries
were unchanged. Final per-target follow-ups repeat the fixture under Xvfb and
every default BSD server, verify unchanged code/binary hashes, and retain the
initial full-suite widget artifacts beside the final ones. The full-suite logs
remain intact. This fixture-only follow-up is recorded independently rather
than claiming it was present in the earlier full-suite invocation.

The initial previous-code control incorrectly expected a process exit on
ARM64's zero divisor. The saved ARM64 output instead reaches the following
instruction; no ARM64 crash is claimed. Its previous widget binary does fail
the endpoint-pixel comparison for below-range values. The AMD64 previous-widget control exits at the equal-range redraw before the
next instruction, while the fixed implementation completes and accepts later
events. The saved control records retain previous/current binary hashes.
Controlled production sources are restored and compared before final evidence
collection; the comparison does not modify the verified implementation.

An initial collection attempted to read OpenBSD AMD64's installation record
before fresh staging finished and correctly failed on the missing file. All
installs then completed; recollection and the final matrix validation pass.
This collection correction does not change test results.

Fresh installs use empty staging directories and retain whole-file hashes and
runtime paths. Executable text/read-only constants match the tested binaries,
accounting for Meson's removal of build RPATHs on BSD. Installed cores run
--version and install the session entry. Only this batch's snapshots on ports
2291–2294 are used and released after collection.

## Remaining work

SCRIPT-01 remains open for other widgets' indices/text/geometry and font-layout
arithmetic, large colorset allocations and legacy FIFO processing. MiniScroll's
repeat delays still busy-wait, and modal widget interactions still defer normal
module event servicing. Input-target and subprocess lifetimes, broader descriptor
handling and native sanitizer/fuzzing gates retain their separate tracker items.

The [sleeping-delay follow-up](VERIFICATION-2026-10-05-SCRIPT-DELAYS.md)
resolves the busy waits recorded here while preserving callback order. Broader
modal servicing and pending-repeat cancellation remain separate work.
