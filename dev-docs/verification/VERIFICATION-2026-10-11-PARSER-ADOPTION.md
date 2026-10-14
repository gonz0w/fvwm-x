# FvwmScript parser generation and runtime adoption — 2026-10-11

Tracker owners: GEN-01 / MAINT-02. Status: combined production checkpoint locally qualified;
modern generated runtime and portable developer wrapper adopted.
Configuration names, grammar, arguments and theme files require no migration.

## Coherent maintenance batch

The modern Bison 3.8.2/Flex 2.6.4 candidate already matched the thirteen shipped
scripts in the [ordinary generation checkpoint](VERIFICATION-2026-10-08-PARSER-GENERATION.md).
This batch completes its configuration/diagnostic/runtime checks, makes the
scanner include `config.h` before system headers, records exact generation
inputs/outputs and replaces the Bash developer helper with a portable wrapper
for the existing atomic fresh-output generator. Normal builds still use committed
C files; Bison and Flex are developer generation tools.

The grammar and type header match locally accepted source `01c2`; scanner token
rules are unchanged. A Flex `%top` block moves only the configuration include.
Two fresh generations are byte-identical. Two further provenance-enabled
generations preserve every native-tested runtime/input byte. The manifest adds
input/output hashes while retaining the recorded tool-version fields.

The wrapper now requires an explicit fresh `--output` directory. It preserves
argument boundaries and the generator's exit status. This developer-tool change
does not change FVWM configuration files. Generator version mismatch, misplaced
headers and incomplete output refuse publication. Four focused tooling checks
and all 28 maintenance-tool checks pass locally.

## Focused native evidence

Eight compiler profiles cover Linux GCC/Clang, FreeBSD and OpenBSD on AMD64 and
ARM64: 152 existing policy cases, 160 paired inputs/320 parse invocations and 96
actual-module test invocations. Each profile compares all thirteen shipped
structures plus six ordinary misconfiguration diagnostics and one comment/UTF-8
case. All result structures, exits and normalized diagnostic locations/messages
match the accepted parser. Twelve existing module fixtures cover selections,
transport/session/packets/partial input, execution/storage, widgets/text, colorsets,
FIFO and geometry.

These are isolated object-reuse candidate checks against accepted core `01c2`.
Accepted source, libraries, objects and binaries remain unchanged. Six complete
candidate archives validate matching parser/scanner input/output hashes, parsed
structures, module identities and preserved-object evidence. Scope and exact
commands are retained under `builddir/codex-2026-10-11/parser-adoption/`.

## Retained harness attempts and timing observation

The initial checker selected only absolute executable paths; its eight completed
module cases remain. Relative Meson paths add four missing transport cases.
An intermediate recovery checked the wrong artifact filename and is also kept.
Affected recoveries verify identical candidate module hashes before reusing any
completed module result; candidate-only diagnostics/policy checks are repeated.

Linux AMD64 Clang's first colorset invocation exceeded its unchanged 180-second
outer deadline. The stopped driver left its owned runtime processes; their logs
and complete initial source/artifacts were archived before targeted cleanup.
The unchanged baseline control and affected candidate retry both pass. The cause
remains unestablished; this is not claimed fixed by parser generation. The
checker now requests graceful teardown on timeout before its bounded kill
fallback. A teardown allowance does not extend a successful test's deadline.
The timeout and exact affected recovery remain part of the evidence.

## Supplementary developer-tool scope

The runtime Meson suite does not register the separate maintenance-tool unit
class. A supplementary run attempts all 28 checks on six targets. Linux ARM64
passes the entire class. The other five targets pass 26 checks and report two
inventory/archive errors because `git` is absent from those guests. Those
original failures, complete logs and source identities are retained; no complete
28-check pass is claimed for them. The four affected parser-generation/version/
header/wrapper checks pass on every target: 24 focused passes. Completed subset
results are validated without rerunning tests or changing source/deadlines.

## Combined acceptance and release scope

Production source `2514` is frozen from accepted `01c2` with exactly eight owning
source/tool changes: 1409 files, SHA256
`251417ca4f8679e1fd397ddc18f7ed3cf01e5adebd0bd9d050366514ed81e47a`.
All eight clean standard compiler profiles pass: Linux AMD64/ARM64 GCC and
Clang each pass 233 registered tests; four native BSD profiles each pass 221.
Total: 1816 entries and 120 staged executable identities, with expected install
RPATH changes verified against tested code. Linked PNG profiles preserve the
previously accepted patched/backport dependency choices.

All four default BSD servers pass twelve affected installed-module fixtures
apiece: 48 invocations. OpenBSD ARM64's wsfb root-monitor fallback remains usable;
servers exposing RandR monitors preserve their normal monitor behavior. Unchanged
core-only default-server checks retain their earlier evidence. The optional
Xcursor-without-XRender suite passes 232 tests; SVG-disabled passes 212.

The scoped ARM64 ASan/UBSan run passes fifteen registered checks: two policies
with leak detection, plus the nineteen-case parser and twelve actual-module
fixtures with leak detection disabled. That is scoped lifetime/arithmetic
coverage, not a full leak or native-release sanitizer qualification. OpenBSD AMD64
retains ordinary test deadlines and the already established verification-harness
TCG allowance (60 seconds outer, 15 seconds internal). No failure deadline was
widened. The earlier candidate colorset timeout remains unexplained; it did not
recur in the fresh full Clang production suite.

Strict validation drains seven complete production archives and verifies all
1409 source hashes/modes, executed runner identities, registered tests, staged
code and affected default-X artifacts. The developer-tool subset is retained in
six separate archives, with wider missing-Git failures explicitly preserved.
Evidence is under `builddir/codex-2026-10-11/parser-production-qualification/`.

All 32 documentation inputs under `doc/` match accepted `01c2`; its 26-man/27-HTML
rendering evidence is retained, not rerun or relabeled as a new pass. Final guide
status/link updates follow the frozen production source and are documentation-only.
Fresh private trees preserve accepted core qualification and other chats' newer
source/build/evidence. Qualification covers this frozen owning batch, not every
newer shared-checkout edit.

Native Linux release sanitizer CI and physical desktop/toolkit checks remain
separate release requirements. No parser-generator removal from user systems,
whole-project audit completion or public release is claimed.
