# Module startup, script bounds and command deadlines — 2026-10-04

Subsequent fresh-snapshot verification found stale inherited Form objects and
an output-completion weakness in the earlier Form fixture. Those specific
assertions are superseded by the complete-rebuild and fresh-output checks in
[the numeric/string follow-up](VERIFICATION-2026-10-04-SCRIPT-VALUES.md).
The historical counts below describe their saved runs.

This follow-up implements checked native-module startup, FvwmScript widget and
parser bounds, and the user-approved five-second GetOutput deadline. FD-01 and
SCRIPT-01 remain open for the specific remaining work below.

## Native-module startup

ParseModuleArgs accepts decimal protocol descriptors only, checks their range
and validity, rejects duplicate endpoints, and applies checked close-on-exec
before publishing parsed arguments. It preserves caller ownership on failure.
The host's intentional ModuleListenOnly incoming descriptor `-1` remains
supported. Alias, user arguments and hexadecimal window-context behavior are
preserved; the window-context strings were not newly validated in this pass.

FvwmButtons, FvwmBacker, FvwmEvent, FvwmIconMan and FvwmPager now use the shared
startup checks while retaining their custom argument handling. Other native
modules already used the common parser. The intentional shared logging
connection is retained; this pass does not certify every inherited descriptor
or the Perl module framework.

Native tests cover malformed/closed/duplicate endpoints, listen-only startup,
four injected flag-operation failures, preserved errors and borrowed ownership,
usable parent pipes, and fork/exec closure. Descriptor 1025 is checked when the
platform permits allocating it. The actual FvwmScript subprocess fixture also
checks that protocol FIFO endpoints are absent after exec. The saved previous
build fails the new descriptor-closure assertion; it is a regression control,
not a claim that the user's machine was compromised.

## Widget and parser bounds

Eight widget getters and sixteen mutators use one checked lookup. Invalid,
missing or inconsistent IDs return empty getter text or leave the widget alone,
with a diagnostic. ID 1000 remains usable. ChangeColorset retains root ID 0,
rejects negative/overflowing colorset numbers, and keeps normal colorset
behavior. Large valid colorset allocations still need a resource budget.
ChangeWindowTitleFromArg validates the existing 0–19 argument range and returns
an empty title for an invalid index.

The parser now checks its existing six argument layers, twenty encoded argument
entries per layer, ten block-stack slots, 5120 constant/value entries and 1000
objects before indexing the corresponding arrays. Flattened function arguments
are freed after copying. ChangeWindowTitleFromArg now pushes its argument layer
in both grammar productions; the omission previously permitted stack underflow.
The committed parser was regenerated with Bison 2.3, matching its existing
version. No token/header contract or normal build prerequisite was added.

Native tests parse production grammar in independent children and check exact
boundaries and the first refused value, including both title-command forms.
Actual-window tests exercise all guarded operations with missing, negative,
overflowing and malformed IDs, verify unchanged ordinary widgets, and exercise
valid edge ID 1000.

## GetOutput deadline

The user chose five seconds on 2026-10-04. `*FvwmScript: CommandTimeout` accepts
1–3600000 milliseconds before module startup; invalid values retain the
previous setting. GetOutput uses one monotonic deadline for collecting output
and waiting for shell completion. The existing 32,000-byte output prefix,
extraction, cache ownership and ordinary nonzero-exit stdout behavior remain.
Timeouts and read errors return empty text without publishing a cache entry.

The helper creates close-on-exec pipes, uses nonblocking reads and poll, and
owns the shell child and its process group. On failure it discards output,
closes its pipe, sends termination, permits a 100-millisecond cleanup interval,
then kills and reaps the direct child. Keeping that child unreaped through group
cleanup prevents process-group ID reuse. Competing/automatic SIGCHLD reapers
are refused. Inherited module termination handlers are reset before restoring
the child signal mask and executing the shell.

This remains synchronous in the isolated module; no blocking work was added
to the authoritative WM event loop and no runtime library was added. Shell
commands are trusted configuration, not sandboxed programs. The process-group
cleanup is not confinement of descendants that deliberately detach. Kernel
scheduling and process termination can add latency beyond the configured
collection deadline; the setting is not a hard real-time guarantee.

Native tests cover ordinary, empty, binary and exact-prefix output; stalls
before and after stdout closure; a background pipe writer after shell exit;
termination-resistant commands; descendant cleanup; repeated EINTR; invalid
settings; refused automatic reaping; and successful commands after timeouts.
The real-window fixture verifies the five-second default and a 500-millisecond
configured override, followed by successful output collection. Native tests
retain 50-millisecond cases.

## Final combined verification

| Target | Full suite | Actual widgets | Staged install |
| --- | --- | --- | --- |
| Linux ARM64 GCC | 62/62 | 22/22 under Xvfb | passed |
| Linux ARM64 Clang/lld | 62/62 | 22/22 under Xvfb | passed |
| Linux AMD64 GCC | 62/62 | 22/22 under Xvfb | passed |
| FreeBSD ARM64 | 51/51 | 22/22 under Xvfb and default Xorg | passed |
| OpenBSD ARM64 | 51/51 | 22/22 under Xvfb and default Xenocara/wsfb | passed |
| FreeBSD AMD64 | 51/51 | 22/22 under Xvfb and default Xorg | passed |
| OpenBSD AMD64 | 51/51 | 22/22 under Xvfb and default Xenocara | passed |

These final gates cover the combined, formatted sources and adjusted external
fixture. Existing compatibility, hardening, session, resource-cache, interaction
and event-loop tests remain in the full suites. Linux also verifies produced
C executable hardening flags; optional Go builds are disabled and excluded.

## Evidence and provenance

Evidence is under `builddir/codex-2026-10-04/` in three distinct phases:

- `module-args/`: focused Linux 8/8 and BSD 7/7 on all corresponding targets,
  with 19 actual-widget checks and 69 matching source fingerprints.
- `script-index/`: focused Linux 5/5 and BSD 4/4, with 20 actual-widget checks
  and 75 matching fingerprints.
- `script-process/`: `source-final.tgz`, `source-fingerprints.json` and
  `verified-script-fingerprints.json` preserve the final snapshot. Seven
  `*-evidence.tgz` archives contain full gate logs, widget results and
  independently checked 79-file source fingerprints. `matrix-summary.json`
  records parsed successful test rows, widget results and archive hashes.
- `script-process/focused-*` sources and `*-focused-evidence.tgz` preserve the
  earlier deadline fixture. That focused matrix passed Linux 7/7 and BSD 6/6;
  the initial OpenBSD AMD64 default-server fixture failed as described below.
  The final combined matrix above supersedes it without overwriting the failure.

These are frozen overlays of the prior verified baseline. They preserve the
shared keyboard-focus, parse-error and message-mode changes without claiming
verification of later unrelated desktop edits. Linux runs use Podman; the four
BSD targets use disposable QEMU snapshots and their default Xorg/Xenocara
servers in addition to Xvfb. AMD64 emulation results establish behavior, not
native hardware latency.

Initial failures are retained: the startup fixture revealed the intentional
listen-only `-1` contract; an overlapping initial AMD64 build needed forced
recompilation of stale objects; parser fixtures needed an actual string operand
and a line-start command after Then. The first OpenBSD AMD64 default-server
command fixture used a 50-millisecond command limit and failed its recovery
assertion after the five-second harness wait. Its artifacts do not establish
whether command startup or recovery caused the failure. The final external fixture uses 500
milliseconds and a longer harness startup wait; production limits and native
50-millisecond tests are unchanged.

## Remaining work

Arithmetic/string conversion bounds, loop/recursion execution budgets, shared
colorset allocation budgets, Perl/helper descriptor ownership, and injected
failures in actual helper/session/subprocess constructors remain. Child
lifetimes when the module itself terminates also need broader coverage. Broader
window/resource target lifetimes, tree walks and image-decoder stress/isolation
also remain in the tracker. WriteToFile retains its approved strict checks and
in-place updates; storage failures can leave partial content.

Native Linux ASan/UBSan/fuzz CI is configured but unrun; this checkout has no
remote. Normal-build local tests do not replace that gate. No whole-interpreter
or whole-project security certification is claimed.
