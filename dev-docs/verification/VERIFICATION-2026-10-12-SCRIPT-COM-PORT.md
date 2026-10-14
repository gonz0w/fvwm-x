# Installed Script communication example — 2026-10-12

Owners: UTIL-01 / RUNTIME-GO-01, with installed-interpreter retirement retained
under COMPAT-02. Status: affected local six-target checkpoint qualified;
strict archives pass.

Final frozen source:
`6fcc241762260f97c4c195080ffd5db8a836f4b1dbef9850030456bf7d8e2625`.
The baseline is the qualified converter snapshot `b6ca9aca`. Evidence and
drivers live under `builddir/codex-2026-10-12/script-com-port/`. This checkpoint
owns the helper, paired template, install/test registrations, affected harness
extensions and manual/navigation updates. It does not qualify another chat's
newer desktop code or every dirty file in the shared checkout.

## Change and configuration contract

The [Go helper](../../bin/FvwmPrompt/cmd/fvwmscriptcom/main.go) uses the standard
library only and shares the existing optional Go 1.21 floor. The installed
[native Script example](../../modules/FvwmScript/Scripts/FvwmScript-ComExample.in)
selects it when Go is enabled. Its startup, count, unknown-request, length-prefixed
Parse and exit interactions remain available through native Script commands.
The Go startup title identifies the new helper. Each multiline record retains
an independent random number in the historical range 0–19.

The Perl filename remains installed and byte-identical for copied configurations
that explicitly invoke Perl. Disabled-Go builds configure the same example to
use that file through Perl. This does not retire the Perl framework or require
configuration changes. The `script-com-example` install tag owns the paired
consumer, retained Perl file and optional Go executable. Meson uses build-time
Python `shlex` to quote the selected command; no new dependency is introduced.

The helper pins one user-directory inode before serving and creates private
FIFO endpoints without adopting or unlinking existing entries. It checks opened
FIFO type, user ownership and device/inode identity; cleanup preserves replacement
entries. Connected command reads and complete reply delivery have five-second
defaults, with `--timeout-ms` accepting 1–3600000 ms. Idle waiting ends when the
peer exits. Commands are bounded to 32000 bytes and refuse NUL. Startup/read/write
retries remain nonblocking and cancellation-aware. Expired communication resumes
at the next request. Exit, peer departure and SIGINT/SIGTERM/SIGHUP clean up without
terminating the borrowed Script PID. Filesystem metadata can still wait on storage;
the FIFO deadline is not a filesystem deadline.

## Qualification scope and cadence

The [transport fixture](../../tests/test_script_com_example.py) uses only owned,
disposable peers. Its legacy controls exercise ordinary messages; malformed-file,
deadline and termination controls target the Go helper. The retained Perl END
handler is never tested against the runner or an arbitrary process.

Seven native Go units cover PID policy, protocol framing, endpoint replacement,
symlink preservation, cancellation and both reply-open and blocked-write deadlines.
The CLI fixture contains two seven-request sequences, nine CLI controls and sixteen
file/lifetime controls: 39 controls per invocation. The normal and installed
executables run it independently on each target; Linux also executes the actual
race-instrumented helper.

The [native-X fixture](../../tests/live_x/script_com_example_live.py) relocates only
the installed command argument for DESTDIR and adds diagnostic WriteToFile/GetTitle
observations. It clicks five real buttons, checks ordered count replies, unknown
reply, three parsed widget titles and endpoint cleanup, and keeps the WM alive.
The configured example retains its commands and fonts. Nested X covers Go and
Perl profiles; the installed Go example additionally runs on the default BSD
Xorg/Xenocara servers with private Xauthority and cookie-less client refusal.

No C/header source changes belong to this port. Core, MFL and Script binaries are
reused only after exact C/header equality and native binary identity checks against
the accepted metadata/native-module checkpoint. The full C/optional-feature matrix
is not repeated for this isolated helper/consumer batch.

The accepted checkpoint contains 30 selected registered entries: six fresh final
transport entries and 24 retained, unchanged policy/X/harness entries. All six
Linux/FreeBSD/OpenBSD AMD64/ARM64 targets pass 468 normal/staged transport controls
and 78 actual Linux race controls (546 total), twelve tagged profile stages,
42 ordinary and fourteen retained Linux race Go package results, 36 native-gate
and 48 default-X harness unit controls. Eighteen native-X invocations exercise
90 real button requests, including Go/Perl nested-X profiles and four authenticated
default BSD servers. Source/runner/log composition, native ELF architectures,
unchanged tested/installed executable identities, legacy bytes and consumers all
validate. The 1448-file archive changes only its fourteen declared owning paths;
shared test registration includes only the new owning block.

## Retained failures and corrections

The first two early snapshots are retained: `2db3` failed Meson setup because the
new quoting used unsupported double-quoted Meson strings; `dcc6` built and passed
transport controls but its new X fixture incorrectly used a braced WriteToFile
filename. Standard `shlex` quoting and a Script filename variable corrected them.
The isolated Linux X retry then passed before the combined matrix started.

Production `56cd` passes all three ARM64 targets and Linux AMD64. FreeBSD AMD64
showed that ordinary malformed-input transfer tests were incorrectly using the
intentional 150-ms communication override; OpenBSD AMD64 showed the same mistake
in the directory-pinning control. Snapshot `6609` separates ordinary five-second
cases from intentional one-second timeout controls. Its remaining failures expose
an exists/stat race in the fixture's FIFO recreation probe. Snapshot `491f` uses one
stat with expected ENOENT handling. Its Linux AMD64 check then identifies an
overly strict zero-status expectation when deliberate endpoint replacement races
SIGTERM. Final `6fcc` accepts cancellation or explicit replacement refusal, requires
the replacement diagnostic for status 1, and still checks the unchanged file and
living peer. Production Go, native Script, Meson consumers and documentation bytes
are unchanged across these fixture corrections.

The final retry preserves per-target pre-correction source/log/binary archives,
reruns the affected registered/installed/race transport fixtures, and retains the
four unaffected registered policy/X/harness entries per target. Existing tagged
stages are reapplied; missing fallback/default-X gates on interrupted targets run
before closure. Exact source differences and reused tested executable bytes are
validated explicitly, rather than relabeling an earlier failure as a pass.

## Documentation and remaining gates

The [Script manual](../../doc/FvwmScript.adoc), [install guide](../../INSTALL.md)
and [architecture map](../ARCHITECTURE.md) describe the paired selection and helper
limits. Linux Asciidoctor man/offline-HTML generation and Linux/FreeBSD narrow
mandoc rendering pass for the exact unchanged manual bytes. Mandoc reports inherited
mso/LINKSTYLE, date/paragraph and long-input-line diagnostics, including two new long
generated lines; this is not a clean lint result. The website/manual-language
redesign remains deferred.

Actual provider-native Linux sanitizer CI, physical desktop checks, publishing,
full XDG provider/frontend migration and Perl framework retirement remain separate
owners. This local QEMU/Podman checkpoint is not evidence of those release gates.
