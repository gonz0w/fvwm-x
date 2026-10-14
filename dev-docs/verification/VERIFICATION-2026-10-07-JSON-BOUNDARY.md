# LIB-01: reviewed serializer and UTF-8 output boundary

Status: combined frozen checkpoint locally qualified on 2026-10-07.
Native Linux release CI and physical desktop checks remain separate.

The user approved per-byte U+FFFD replacement at JSON output on 2026-10-07.
String values and object keys preserve valid UTF-8; invalid bytes each print as
`\ufffd`. Original FVWM text, command expansion and native module bytes retain
their existing owners. Conversion is linear at printing, with checked expansion,
quote/terminator and print-buffer offset arithmetic under the existing INT_MAX
size ceiling. No new mandatory dependency, thread or input-admission policy is
introduced. Raw JSON remains reserved for existing trusted numeric fields.

## Vendor provenance and reachable scope

The previous cJSON.c exactly matches upstream 1.7.17. The import is pinned to
[upstream 1.7.19](https://github.com/DaveGamble/cJSON/tree/v1.7.19), preserving its
MIT notice, public struct/API and vendor formatting. Original source hashes:

| Source | SHA-256 |
| --- | --- |
| 1.7.19 cJSON.c | `298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d` |
| 1.7.19 cJSON.h | `25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee` |

The [upstream change list](https://github.com/DaveGamble/cJSON/blob/master/CHANGELOG.md)
is context, not a claim that every listed defect is reachable here. Five
production owners create/serialize/free JSON: builtins, status_json,
monitor_scale_config, monitor_topology and FvwmMFL. No production parse,
duplicate or JSON-Patch call was found in the recorded search. cJSONUtils is not
shipped. Unit tests parse only JSON they generated.

Downstream patches correct two demonstrated numeric edge cases: a finite DBL_MAX
can otherwise print a decimal that parses as infinity; creating or setting NaN
otherwise casts NaN to an integer. Finite values use the existing 17-digit
fallback when needed; NaN keeps its stored double and existing printed `null`,
with a defined zero integer cache. These cases are unit correctness findings,
not demonstrated compromise paths in WM callers.

The separate UTF-8 patch uses the small project-owned json_print_policy header
for valid sequence lengths and checked sums. Rejects overlong, surrogate,
out-of-range and truncated sequences at printing; consumes one invalid byte
per replacement. String storage is not rewritten. Header hashes, the exact
upstream/downstream diff and production-call-site search are retained under
`builddir/codex-2026-10-07/cjson-review/`.

An [upstream maintenance notice](https://github.com/DaveGamble/cJSON/issues/1080)
says prompt fixes cannot be assumed. The project owns review and regression
coverage for its downstream patches; version 1.7.19 alone is not a security
verdict. [CERT Polska's printer advisory](https://cert.pl/en/posts/2026/07/CVE-2026-16554/)
concerns 32-bit escaped-size arithmetic on very large strings. Both supported
architectures here are 64-bit. This patch checks printer size/offset additions;
there is no claimed 32-bit qualification or reproduction of that advisory in a
WM workload. A broader LIB-06 alternative evaluation remains separate.

## Focused evidence and retained failures

The pre-import comparison passes ordinary byte-for-byte serialization on Linux
GCC/Clang/leak-enabled ASan/UBSan, and native Linux AMD64 plus all four BSD
platforms. The upstream candidate's finite-max failure and prior NaN UBSan
finding are retained separately from corrected passes. Review archives include
exact sources and complete readable payloads.

An actual baseline core/MFL Echo with one legacy byte produced a JSON record
that a strict UTF-8 reader rejected; a later ASCII Echo remained healthy. This
is an encoding defect observation, not a hostile-input or compromise claim.
The final driver stores/expands seven bounded legacy/valid text cases through
the command engine and checks strict JSON decoding and subsequent healthy Echo.

The final pure serializer test covers valid Unicode boundaries, overlong and
surrogate sequences, truncation with exact allocations, every non-NUL one-byte
input, key/value output, formatted/unformatted/buffered/preallocated printing,
original storage, finite/nonfinite numbers, control escaping, fixed-buffer bounds
and allocation-failure cleanup for both ordinary and legacy records. Size guards
exercise INT_MAX and SIZE_MAX without large allocations. Linux GCC, Clang and
leak-enabled ASan/UBSan pass; early native FreeBSD/OpenBSD ARM64 policy checks
pass. A signedness warning in the new fixture and incorrect environment-variable
syntax in its initial live driver were corrected; failed logs remain retained.
Affected topology/configuration and real MFL stream tests pass.

## Frozen combined checkpoint

Archive: `builddir/codex-2026-10-07/json-boundary/source-json-boundary.tgz`.
1282 files; SHA-256
`66b90a25a1940bf34571f3754ac46d937e3c730b9f9cf4ffbaa62b1c6b3580e6`.
Only eight owned paths change from the accepted startup/identity snapshot;
three files are new. Other chats' shared edits are excluded. All eight compiler profiles pass 1056 registered entries: Linux GCC/Clang on
both architectures (138 each), and four native BSD profiles (126 each). All 120
staged binary identities match tested code after verified install RPATH changes.
All four default BSD X servers pass ten drivers each (40 invocations), including
the seven-case strict JSON control. Optional XRender-disabled/SVG-disabled
profiles pass 137/117 entries. Sixteen scoped ASan/UBSan checks pass: six pure
checks leak-enabled, ten actual-core checks leak-disabled. Seven evidence
archives validate all 1282 source hashes/modes and complete payloads to EOF.
Per-profile results and archive hashes are in
`builddir/codex-2026-10-07/json-boundary/qualification-summary.json`.
The first validator invocation preceded completion of the last archive transfer;
the final validation includes all seven completed archives. No standard,
default-server, optional or scoped sanitizer gate failed in the frozen run. No whole-working-tree qualification claim.
Native Linux release CI, physical hardware, other toolkit clients and external
compositors remain separate coverage requirements.
