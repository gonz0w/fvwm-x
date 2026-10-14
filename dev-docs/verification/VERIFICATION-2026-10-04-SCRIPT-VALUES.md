# FvwmScript numeric and string maintenance — 2026-10-04

Implemented follow-up to the [bounds/deadline pass](VERIFICATION-2026-10-04-SCRIPT-BOUNDS.md).
The interpreter audit remains open; execution/allocation budgets and other
widget/protocol arithmetic are not covered by these repairs.

## Behavior

Add, Mult, Div and RemainderOfDiv share checked signed arithmetic. Overflow,
zero divisors and the minimum integer divided/remaindered by -1 return owned
empty text with a diagnostic. Decimal conversions preserve legacy numerical
prefixes, whitespace, signs, and no-numerical-prefix-as-zero behavior. Ordering
comparisons reject overflowing operands; equality and inequality compare text
without the unused numeric conversions.

HexToNum preserves an initial #, numeric prefixes and the existing signed
interpretation of 32-bit patterns, including FFFFFFFF as -1. Conversion is now
explicit rather than an out-of-range narrowing cast. Oversized patterns are
refused. NumToHex allocates padding plus the terminator before writing; natural
uppercase output and zero/negative-width behavior remain. Widths up to 65536
are supported; larger widths are refused. The prior padding path wrote beyond
its allocation for ordinary requests wider than the natural number.

StrCopy retains one-based inclusive positions, clamping below one, and empty
results for missing/reversed ranges, without narrowing the source length.
Parse reads only complete four-decimal-digit length headers, bounds each body
against remaining input and validates the complete sequence before returning
a selected record. Missing/zero/negative indices return empty text, zero-length
records are accepted, and malformed streams are refused. The former header
buffer lacked a terminator and the loop read beyond short/truncated input.

For no longer decodes the body pointer as a third argument into a two-element
array. It evaluates exactly two numeric limits and uses a wider counter so the
final maximum-int iteration cannot wrap. It publishes newly owned numeric text
each iteration because a body can replace the loop variable's allocation.
Inclusive ascending ranges and no iterations for descending ranges are
preserved. Refused bounds leave the loop variable alone. This does not impose
an execution budget on large ranges or While/recursive signal handling.

Set and Key concatenation now use the existing checked byte append helper with
tracked length. This removes pointer-sized allocation multipliers and repeated
scans of already assembled text, without adding a new ordinary-string size
limit. Set publishes only complete assembly. General memory budgets remain
open. A While comparison selector is checked before indexing its function table.

## Coverage

The native script-value unit covers exact integer limits and first overflows;
legacy prefixes/no-digit input; signed division/remainder; zero divisors;
hexadecimal pattern boundaries and padding to the exact limit; inclusive
substring boundaries; truncated, non-decimal and excessive record lengths;
zero-length records; malformed tails after valid selections; and long sequences
of empty records.

The real-module fixture checks 31 numeric/string outputs and three comparison
results, including refusal followed by continued operation. Actual For bodies
exercise the final two INT_MAX and first two INT_MIN values, nested arithmetic
bounds, replacement of the loop variable with empty text, descending ranges,
and rejected bounds preserving its previous value. Existing clipboard, file,
message, subprocess and recovery checks remain, totaling 24 widget results.

## Final verification

| Target | Full suite | Actual widgets | Staged install |
| --- | --- | --- | --- |
| Linux ARM64 GCC | 63/63 | 24/24 under Xvfb | passed |
| Linux ARM64 Clang/lld | 63/63 | 24/24 under Xvfb | passed |
| Linux AMD64 GCC | 63/63 | 24/24 under Xvfb | passed |
| FreeBSD ARM64 | 52/52 | 24/24 under Xvfb and default Xorg | passed |
| OpenBSD ARM64 | 52/52 | 24/24 under Xvfb and default Xenocara/wsfb | passed |
| FreeBSD AMD64 | 52/52 | 24/24 under Xvfb and default Xorg | passed |
| OpenBSD AMD64 | 52/52 | 24/24 under Xvfb and default Xenocara | passed |

Full suites include existing interaction, compatibility, resource-cache,
hardening, session and event-loop coverage. Linux checks produced C executable
PIE/RELRO/immediate-binding/non-executable-stack flags; Go is disabled and
excluded. BSD repeated full suites after the executable-mode correction below.
Source file contents are identical across that metadata-only follow-up.

## Verification harness corrections

Initial Linux focused gates passed, while FreeBSD exposed its strtol/strtoll
no-conversion error behavior. The helper now explicitly returns the legacy zero
result for no conversion before treating errors as overflow. OpenBSD's native
checks already passed. Initial logs and source snapshots are retained.

The strengthened Form fixture exposed two verification problems. A core echo
reports launch of its shell command, not completion of its file write. The
fixture now removes the prior private output file, waits for the current exact
content, and retains partial results/output on failure. With that wait in
place, fresh BSD snapshot guests showed malformed clipboard bytes reaching
Form. `stale-form-object.json` confirms the checked source contains the guard
while the inherited object and executable lack its diagnostic string; their
object timestamp is newer than the frozen overlay's timestamp zero.

All final runners explicitly reconfigure Meson and refresh every file in the
trusted source overlay before compiling, ensuring inherited objects rebuild
against that snapshot. Snapshot packaging now preserves source permissions;
its prior blanket 0644 mode made the generated Python desktop helper
non-executable in fresh BSD guests. The failing desktop gate was repeated
after restoring its executable mode, with identical source contents.
`desktop-source-mode.json` and `mode-only-followup.json` retain that diagnosis
and the exact metadata-only changes. This also corrects the link failure where the
new source list had not triggered build-graph regeneration. Earlier matching
source fingerprints alone did not prove inherited objects were current, and
older Form checks could read the preceding result. The final matrix in this
report supersedes those Form assertions. Historical test counts describe their
saved runs, not additional certification of that component.

## Evidence and remaining work

Evidence is under `builddir/codex-2026-10-04/script-value/`. Seven final
`*-evidence.tgz` archives retain parsed full-suite logs, widget results and
independently matched fingerprints/modes. `matrix-summary.json` records passing
rows and archive hashes. Initial/portability/gate-phase archives and logs retain
the failures above; `*-mode-gate.log` records the final BSD reruns. The final source overlay
and 82-file fingerprint map, with 1134 verified source permission entries,
separate this work from unrelated concurrent
desktop edits. The permission follow-up changes archive metadata only; source
contents are identical before and after it. Linux uses Podman and BSD uses disposable QEMU snapshots with
Xvfb and default Xorg/Xenocara. AMD64 emulation establishes functional behavior,
not native hardware latency.

Remaining work includes execution/recursion and allocation budgets, widget
value/geometry arithmetic, remaining module FIFO processing, helper/Perl
ownership and constructor failures, module-termination child lifetimes,
window/resource targets, and tree/image decoder stress. WriteToFile remains an
in-place operation; storage failures can leave partial content.

Native Linux ASan/UBSan/fuzz CI is configured but unrun, with no Git remote in
this checkout. These are normal-build tests, not sanitizer results or a complete
interpreter/project security certification.

The later [execution-limit follow-up](VERIFICATION-2026-10-04-SCRIPT-EXECUTION.md)
implements the separately approved 10000-step/64-level defaults and verifies
fresh full gates. The limitations above describe the numeric/string batch;
value-allocation and remaining widget/FIFO work are still open.
