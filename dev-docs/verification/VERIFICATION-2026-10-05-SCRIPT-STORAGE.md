# FvwmScript value storage limits — 2026-10-05

The user approved 1 MiB per interpreted string and 32 MiB of stored text,
with configurable overrides. This follows the
[execution/nesting pass](VERIFICATION-2026-10-04-SCRIPT-EXECUTION.md).
All seven final verification archives pass the matrix checks below.

## Implemented behavior

ValueLimit defaults to 1048576 bytes, excluding the terminating byte.
MemoryLimit defaults to 33554432 bytes and counts stored variable/constant
names and values, including their terminators. Both configuration settings
accept 1–2147483647 bytes before module launch; invalid values retain their
previous settings. The manual documents their scope and failure behavior.

A small shared accounting helper checks lengths, terminating-byte arithmetic
and replacement credits. Runtime assembly checks before growing or publishing
values. Refusal preserves the previous value and accounting, latches the
current handler's failure, unwinds through normal cleanup and emits one outer
handler diagnostic. Instructions after refusal do not execute. Earlier effects
remain. Later events receive a fresh execution allowance and can recover by
replacing stored values with smaller strings; stored usage persists across
handlers.

The parser checks value tokens before allocating their owned copies and
reserves stored names and values before publishing table entries. Refusal
occurs before building and mapping script UI. Runtime checks cover variable
copies, numeric formatting, string assembly, translations, selected output and
received messages. GetOutput checks the selected result length before allocating
its returned copy. Existing smaller command/output/file limits remain in force.

The WM event loop, thread model and external-command permissions do not change.
No runtime library or generator build requirement is added. Parser sources and
committed generated outputs were updated using host Bison 2.3 and Flex 2.6.4.

MemoryLimit is a table-text cap, not a whole-process memory cap. Parser input
buffers, UI/font/image allocations, output caches and temporary copies have
separate behavior. ValueLimit bounds individual interpreted strings; it does
not bound the sum of all temporary copies. Widget arithmetic, very large
colorset requests and remaining legacy FIFO processing still need review.

## Verification

Native policy tests cover exact and first-excess lengths/totals, terminating
bytes, overflow, invalid replacement credit, shrinking and unchanged accounting
after repeated refusals. Parser tests exercise value-token and stored-table
refusal. Output tests cover exact-size extraction and first excess.

Six real-module outcomes verify the 1 MiB default boundary, unchanged values
and next-event recovery; configured total storage, replacement credits and
repeated refusal; hex/output refusal and invalid settings; larger overrides;
parser refusal; and single diagnostics. Existing seven execution outcomes and
24 widget outcomes are retained.

| Target | Full suite | Storage checks | Prior execution/widget checks | Fresh install |
| --- | --- | --- | --- | --- |
| Linux ARM64 GCC | 67/67 | 6/6 under Xvfb | 7/7 and 24/24 | passed |
| Linux ARM64 Clang/lld | 67/67 | 6/6 under Xvfb | 7/7 and 24/24 | passed |
| Linux AMD64 GCC | 67/67 | 6/6 under Xvfb | 7/7 and 24/24 | passed |
| FreeBSD ARM64 | 56/56 | 6/6 under Xvfb and default Xorg | 7/7 and 24/24 on both | passed |
| OpenBSD ARM64 | 56/56 | 6/6 under Xvfb and default Xenocara/wsfb | 7/7 and 24/24 on both | passed |
| FreeBSD AMD64 | 56/56 | 6/6 under Xvfb and default Xorg | 7/7 and 24/24 on both | passed |
| OpenBSD AMD64 | 56/56 | 6/6 under Xvfb and default Xenocara | 7/7 and 24/24 on both | passed |

Full suites include existing interaction, configuration compatibility, module
transport, selections, sessions, image and event-loop checks. Linux produced C
ELF hardening checks pass. Optional Go is disabled in this matrix.

Native Linux ASan/UBSan CI remains unrun. Ordinary Meson logs containing
sanitizer environment variables do not mean the build was instrumented.

## Evidence and corrections

Evidence is in `builddir/codex-2026-10-05/script-storage/`. Final runners
reconfigure and refresh every trusted overlay source before rebuilding.
All seven archives match 92 source fingerprints and 1142 permission entries.
The matrix summary checks every logged result/return code, new and prior live
outcomes, source hashes/modes and fresh installation records.
The frozen baseline isolates this follow-up from concurrent desktop/theme edits.

The initial lexer regeneration exposed obsolete safestrdup calls in scanner.l
that had previously been fixed only in committed scanner.c. The initial link
failure and source archive are retained. The source lexer now uses the checked
owned-token helper, and regenerated output builds without that obsolete symbol.
Focused Linux GCC/Clang checks passed before the final full rebuilds.

The stopped Linux containers needed attached starts to remain available during
verification. This was an environment lifecycle issue, not a runtime test
failure; no compromise or external interference is inferred.

Fresh staged installs use empty directories. Records retain whole-file hashes
and build/installed runtime-library paths, and verify matching executable text
and read-only constants. This accommodates Meson's removal of build RPATHs
on BSD. Installed cores run --version and provide the X session entry.

Only this batch's QEMU snapshots on ports 2291–2294 are used. Their shutdown and
release records are saved after evidence collection; other guests are untouched.

The subsequent [widget arithmetic/input follow-up](VERIFICATION-2026-10-05-SCRIPT-WIDGETS.md)
implements the approved rendering policy and supersedes the pending
progress/scrollbar range-arithmetic status above. Other widget work remains open.
