# Configuration converter Go port — 2026-10-12

Owner: UTIL-01 / RUNTIME-GO-01. Status: final `b6ca` affected local six-target
checkpoint qualified. This report does not qualify the changing shared checkout.

## Scope

Final frozen source `b6ca9aca4e98362f899b2a37e2ce26805a5c659e911e1a03e6ae69803521508d`
extends the qualified directory checkpoint `a7e65f38` through thirteen owning
paths. Every C/header and other source path is unchanged. The archive, scope,
dispatchers and logs are under `builddir/codex-2026-10-12/converter-port/`.
The frozen archive contains 1444 source files. Exact baseline composition,
per-file hashes/modes, complete gzip trailers and executed runner bytes validate.
Initial source63c9/evidence/runner bytes are retained
under `initial-63c9/`; the final source changes only the job counter and its unit.

The optional standard-library Go executable keeps the existing command name,
help/follow-read options, default output names, literal Read search and ordinary
heuristic conversion bytes. The disabled-Go profile retains the original Perl
implementation. No interpreter is removed, no dependency is added, and the
FVWM runtime configuration parser is unchanged.

Conversion policy uses explicit job/function state and precompiled expressions.
It avoids dynamic function references and per-line retained slice nodes. Input
bytes, continued-line data and Read references are finite: 128 MiB per source,
1 MiB per logical line and 4096 references per job. A malformed terminal
continuation returns failure. Source descriptors are checked as regular files;
opening a FIFO does not wait for a writer. Source symlinks to regular files work.

Publication writes one complete mode-0600 temporary file in an anchored parent
directory and links it to an absent destination. Existing files, links,
directories and special paths are not replaced. The parent cannot be group/world
writable without the sticky bit. Source bytes stay unchanged. Followed outputs
complete one at a time; a later failure does not roll back earlier outputs.

Meson owns the `config-converter` tag in both profiles. The Go gate builds/tests
and stages the converter; its contract test checks those invocations. The new
CLI fixture can run the candidate against the immutable legacy utility baseline
but cannot record a replacement baseline.

## Verification

Focused macOS checks pass six initial Go package results, nine final converter
units, 69 CLI/conversion/file cases, 15 recorded observations and six gate contract
cases. The dependency inventory contains only standard-library packages and
the converter itself. These local checks are not the Linux/BSD checkpoint.

All six guests pass against final b6ca: Linux, FreeBSD and OpenBSD,
each on ARM64 and AMD64. Linux ARM64 uses Go 1.22.2; the others use Go 1.27.0.
These are existing local Podman/QEMU environments, not provider-native CI.

| Check | Validated result |
| --- | --- |
| Registered affected tests | 24 entries: four per target, all OK |
| Ordinary CLI/conversion/file cases | 828: 69 each on built and staged Go executables across six targets |
| Linux actual race-enabled converter cases | 138: 69 each on ARM64 and AMD64 |
| Recorded utility observations | 270: 15 on built Go, installed Go and installed Perl per target; 72 converter-specific observations |
| Go unit package results | 36 ordinary and 12 Linux race results; nine converter units run on all six targets and both Linux race profiles |
| Native gate contract controls | 36: six per target |
| Tagged stages | 12: Go-enabled and disabled-Go per target |
| Executable identities | Six native ELF64 Go binaries equal the staged bytes; six staged Perl fallbacks equal configured bytes, with executable modes |
| Linux vet | Two full-package runs, both pass |

The 69-case fixture includes 54 comparisons with the Perl oracle and 15 direct
file/limit controls. Coverage includes all conversion families, lower/uppercase
conditional quirks, function merging/order, continued lines, CRLF, invalid UTF-8
bytes, long physical lines, paths with spaces, literal Read search/order,
source aliases, ordinary/default outputs, all existing-destination types,
terminal continuation failure, input/reference limits, parent aliases and
competing complete-output publication. No source configuration command runs.

The immutable legacy fixture stays unchanged. Candidate record mode is refused.
Both profiles stage only the converter tag in this checkpoint; no whole-package
or manual-default migration is inferred. Installed candidates rerun the full
CLI/file suite. Linux race binaries run that suite as actual processes.
Evidence validates every frozen source hash/mode, command/log hash, registration,
result, installed/tested identity and ELF architecture. No C sanitizer or full
X/server rerun is claimed for this utility-only batch.

Linux Asciidoctor generation and Linux/FreeBSD narrow mandoc renders pass.
Lint returns 3 for inherited renderer macros/date/paragraph warnings and long
lines, including lines from the new text. This is not a clean mandoc lint result.
The man and offline static HTML share the exact qualified AsciiDoc source.
Their initial render evidence is reused after the final code-only correction:
the manual source and renderer bytes are identical.
Generated output hashes and executed review bytes validate. Manual contents
cover limits, private publication, examples, environment and exit behavior.
Owning source/docs links are checked by `docs_check.py`; its exact count is
stored in `documentation-result.json`.

The separate [consumer classification](RECONCILIATION-2026-10-12-UTILITY-CONSUMERS.md)
records installed Script communication, development-only cursor generation,
optional status recipes, the full XDG provider/frontend and retained wrappers.
Classification does not qualify those remaining runtime ports or retire Perl.

## Retained corrections and limits

The first focused candidate changed uppercase conditional matching and added
extra newlines to continued input. The legacy embedded `qr` expression is
case-sensitive, and `redo` executes `chomp` again. The candidate now matches
these rules; lower/uppercase conditions and multiple continuations are covered.

Final review found that the reference total counted only resolved files across
source boundaries. Missing Read references could reset that part of the total.
An explicit job counter now includes every literal reference before lookup.
The ninth unit exhausts the budget using missing references, then confirms a
second source fails without output or counter mutation. The initial full
checkpoint remains archived; the correction passes affected native Go/CLI,
installation and Linux race checks with the already populated owned caches.
Manual text and all C/header bytes are unchanged by this final correction.

Historical conversion weaknesses remain, including conditional/style/quoted
text heuristics and function/comment reordering. The manual tells users to
review output before use. The port is not a parser rewrite, a file-system
sandbox, or a guarantee of storage latency. A byte limit cannot preempt a
blocking regular-file access. No general speed, memory or desktop responsiveness
improvement is claimed.

The tool does not execute source commands. This utility-only batch changes no
X-visible core behavior, so previous exact unchanged C qualification is retained
instead of repeating the complete core/X matrix. Provider-native CI, physical
hardware, release packaging, manual-install defaults, full XDG/runtime ports and
Perl retirement retain their own checkpoints.
