# FvwmScript file and message maintenance — 2026-10-04

Status: finalized; focused verification passed on all targets. The user
approved the stricter output-file rule on 2026-10-04, choosing secure defaults.

WriteToFile previously concatenated evaluated arguments into a fixed 256-byte
allocation, indexed before the start of empty text, and treated an exact
32,000-byte read as a terminated string. It also used unchecked file-open
results and unchecked replacement arithmetic. These are source-confirmed
memory and error-handling defects, distinct from the prior live clipboard
failure control.

The replacement evaluates each argument once, assembles at most 64 KiB, and
reads a complete file with a 1 MiB ceiling. A small module helper builds the
entire tagged replacement before writing. The same process appends within its
section; a new process replaces that section and retains surrounding content.
Empty text adds a newline. Malformed or duplicated matching sections, invalid
PID fields, embedded NUL bytes and reserved marker lines are refused. The
original contents remain unchanged on validation failure.

The approved filesystem rule refuses symlink output files, multiple hard links, foreign
owners and non-regular files. The descriptor is close-on-exec and remains open
through validation and writing, retaining inode identity. A nonblocking advisory
write lock protects cooperating writers. Existing file permissions are retained;
new files are private. Relative paths continue to use HOME. Scripts previously
writing through links must use an appropriate ordinary output file. The user
approved this compatibility change; finalization changed documentation/status
only, retaining the exact runtime code verified by the focused gates.

Updates retain the existing in-place write contract. Checked writes and
truncation report I/O failures; this is not a crash-atomic save, and a write or
storage failure after writing starts can leave partial content. Advisory locks
do not prevent writers that ignore them. Regular-file I/O occurs in the isolated
module process, never in the WM event loop.

GetScriptArgument and script-message destinations use checked integer parsing
before indexing. Invalid arguments return the existing empty/No message result
or skip sending. Outgoing messages must fit the selection-reader ceiling,
including their terminator. Failed sends release both owned strings.

The script selection sender uses explicit valid queue indices and replies with
the requested target even when no message exists. It handles omitted property
atoms using the ICCCM target fallback, and frees the destination string when
removing a queued message. Empty/refused requests no longer wait for the
receiver's timeout because of an incorrect target field.

## Checks

Native tests cover new sections, same-process append, replacement by a new
process, surrounding sections, empty text, malformed/duplicate sections, PID
range, embedded NUL bytes, exact 32,000-byte nonterminated input, the accepted
64 KiB text boundary, oversized text/input/output and unchanged content after
rejection. File tests exercise symlink, hard-link and FIFO refusals.

The real-module fixture writes a 4,096-byte evaluated value, replaces an old
section, appends empty text, retains a malformed file unchanged, survives an
unavailable output directory and invalid script argument/message indices, then
exchanges a parent/child message. It also verifies empty-queue refusal replies
and the previously covered clipboard/form behavior.

Linux ARM64 GCC, ARM64 Clang/lld and AMD64 GCC each pass all three
focused tests. All four BSD targets pass both focused tests plus the actual
module fixture on their default X servers. Every fixture passes 16 widget
checks. Fifty-five independent source fingerprints match the frozen source
and all six workspaces. Final archives retain passing Meson logs and widget
results; initial failures remain in separate host logs.

The final focused results and source/archive fingerprints are recorded in
`builddir/codex-2026-10-04/module-input/matrix-summary.json`. The prior reader
batch retains separate full gates: 55 tests on each Linux compiler/platform leg,
44 on each BSD target. Those counts are not later full-suite runs of this helper.
Linux focused checks include native script-file policy, real modules under Xvfb
and ELF hardening. BSD focused checks include native policy, real modules under
Xvfb, and the same module fixture on each default X server.

The integration fixture initially used brace strings where WriteToFile's
filename grammar requires a string token or variable. Correcting the fixture
restored valid FVWM syntax without changing the parser. The isolated snapshot
also initially omitted an existing widget declaration header; including that
header fixed the modern compiler failures. Initial failure logs are retained.
Existing parallel keyboard-focus changes remain preserved.

## Remaining work

LaunchScript, GetOutput and Do were subsequently repaired and verified in the
[interpreter pass](VERIFICATION-2026-10-04-SCRIPT-INTERPRETER.md), which includes
new integrated full-suite gates. Widget indices, parser arrays/stacks,
expression/loop bounds and synchronous subprocess deadlines remain open.
Native Linux sanitizer/fuzz CI has not run. This report records specific
repairs; it does not certify the whole interpreter or WM.
