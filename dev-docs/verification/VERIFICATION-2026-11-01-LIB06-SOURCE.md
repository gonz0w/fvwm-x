# LIB-06 comparison source checkpoint

Owner: Cam; #48. The [finite comparison](../decisions/STATUS-SERIALIZER-COMPARISON-2026-11-01.md)
now has pinned inputs, a non-installed native prototype, independent output and
failure controls, and a six-target decision framework. Production keeps patched
cJSON. The [source-bound receipt](VERIFICATION-2026-11-01-LIB06-SOURCE.json)
records the actual host build/observation, all failure ordinals, repeated native
samples and hashes of retained complete evidence. #48 remains open.

## Observations

The assembled development observation completes in **1.10 seconds** on the
macOS ARM64 host, with no VM or broad suite. It checks 33 backend/mode/case
observations: all eleven baseline outputs complete; each candidate string mode
completes nine and explicitly refuses the legacy-value and legacy-key cases.
These four refusals are candidate differences, not parity passes. Numeric values
match the independent oracle, including exact `UINT64_MAX`, but lexical output
differs. Valid ordered object structure, strict JSON/UTF-8 and transport-cap
models complete. Duplicate replacement keys remain ordered pairs, not lost in
a dictionary. The deliberate input-mutation controls distinguish copied strings
from caller-retained borrowed strings.

All **71 observed small-record allocation ordinals** are injected separately;
each checks accounting and either complete independent output or explicit
refusal. Trials do not stop at the first recovered success. All 66 small-budget
observations refuse cleanly. Tracked live storage finishes at zero. This is
requested-allocation accounting, not a sanitizer or production allocator proof.

Three alternating native-allocator repetitions use twenty iterations for each
ordinary/large/stress case and string mode. Native allocation counters are null.
The receipt retains all 27 native samples, raw whole-process RSS and process
launch-to-exit time. These short host intervals include clock quantization;
they establish the comparison mechanism, not an R1 speed result or adoption.

| Synthetic case | cJSON requested peak bytes | yyjson copied | yyjson borrowed |
| --- | ---: | ---: | ---: |
| Ordinary | 631 | 952 | 696 |
| 64 KiB string | 262,220 | 459,277 | 393,724 |
| 256 nested records | 296,376 | 361,704 | 230,888 |

Peaks include the tracked copy-realloc overlap and exclude headers, allocator
metadata/rounding and separately reported input workspace. Borrowing retains
caller lifetime obligations. Fewer allocations do not automatically mean less
memory or simpler integration. The actual native timing uses default allocators,
since custom cJSON hooks remove its realloc fast path.

## Build, failures and limits

Source review clears ownership/unwind, oracle, arithmetic and measurement
boundaries. Six strict C11 `-O2 -g -Wall -Wextra -Werror` object/link commands
pass with Apple Clang 21.0.0. The isolated configuration enables no product
features; both variants use identical flags and the exact upstream candidate.
The first POSIX-only host compile failed before any object because Darwin hides
`ru_maxrss` behind its full feature view. The selected SDK header confirms that
gate. A new plan adds `_DARWIN_C_SOURCE` to both variants; warnings and source
remain unchanged. Original compile failure evidence is retained.

Host executable sizes are 79,800 bytes for the baseline and 272,856 for the
candidate, including debug information and the probe. These are not installed
product deltas. No candidate is installed or linked into the WM/MFL; current
MFL still parses JSON and retains cJSON. No claim of parser retirement or a
zero-cost extra dependency follows.

The host is outside the supported Linux/FreeBSD/OpenBSD matrix. All six target
comparisons, actual production-builder/independent-client MFL parity, complete
integration/dependency/license costs and adopt/defer/reject remain pending.
No supported-profile, full WM, install, default-X, theme or native-sanitizer
qualification ran. No new UI/screenshots. Core performance cadence stays at
zero after PR330's due comparison; this non-installed prototype does not reset
or increment the production counter.
