# Coordinated XDG Go integration

Status: implementation contract under the approved RUNTIME-GO-01 / UTIL-01
migration. Qualification is pending in PROJECT-TRACKER; this is not release
approval or Perl-framework retirement.

Preserve existing FVWM commands, supported utility options, theme menu names,
Form alias and preference record format. The provider and settings frontend
move together. The Go-disabled build retains both predecessor producers. The
old FvwmPerl entry point delegates to the native Form in Go-enabled builds.
No new mandatory GUI toolkit, image toolkit, desktop service or Go dependency
is added. Go remains an optional build tool with the existing project floor.

Pure policy owns menu expansion, allocation, visibility/layout, encoded output
and icon rules. One isolated worker acquires bounded regular files and optional
KDE inventory, then publishes only completed native menu output. Five seconds
is the default acquisition deadline, with finite CLI/environment overrides.
The supervisor contains the worker process group; it does not wait indefinitely
for a kernel filesystem call. The Go memory setting is a soft GC target, not a
hard virtual-memory limit. Application lifetime is outside the acquisition cap.

Desktop metadata becomes typed literal argv. Fixed lookup actions use encoded
private catalog references, never interpolated metadata command text. Existing
records are retained in a bounded catalog; a nonblocking owned record lock
serializes writers. Saturation refuses regeneration rather than evicting actions
that existing or exported menus still use. An explicitly configured terminal
command retains its existing shell syntax; application arguments are separate.

Native module command/packet limits remain unchanged. Additive Form
CommandAppend/InputAppend directives assemble bounded templates and initial
values from short configuration records. Base64 substitutions give ordinary
full-name fields priority. A button invocation serial plus a fresh form-session
prefix owns each completed result. The existing local-command path passes only
encoded field data to the isolated utility; a short core PipeRead retrieves the
completed result. The module keeps its existing synchronous local-command
semantics, now using one bounded utility job instead of many shell writes.

Saved preference values are read and written as data. The original prefix,
field names and unknown records remain. Owned readable preferences can be
read through an owned symlink; publication refuses links and foreign/writable
files/directories, and writes complete mode-0600 replacements. The core never
executes saved field values as configuration. Result records are consumed once.

The existing DATA-01 Read dependency changes to an owned bounded logical-line
reader: complete lines up to 1 MiB, continuation retained, embedded NUL/I/O/
excessive-line refusal before partial dispatch, existing command count and
close-on-exec barrier retained. This extends supported command lengths and
fixes accidental fragments; it does not change configuration syntax or require
migration. It does not close the wider reader/stream audit.

The combined checkpoint must cover native Linux/FreeBSD/OpenBSD AMD64/ARM64,
ordinary and minimal feature installs, both Linux race runs, native Form/menu
controls, relevant C regressions/scoped sanitizers and rendered manuals. Native
release CI and physical desktop qualification remain separate. Production
retirement waits for its existing owner and acceptance contract.
