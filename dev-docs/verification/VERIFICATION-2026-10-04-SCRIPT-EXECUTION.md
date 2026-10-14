# FvwmScript execution limits — 2026-10-04

The user approved 10000 work steps per handler after discussing the earlier
unmeasured 100000-step proposal. A separate nesting default is 64. This follows
the [numeric/string pass](VERIFICATION-2026-10-04-SCRIPT-VALUES.md).

## Implemented behavior

ExecutionLimit defaults to 10000; configuration accepts 1–2147483647 steps.
RecursionLimit defaults to 64; configuration accepts 1–256 active executions,
including the top-level handler. Invalid settings retain their prior values.
Both are selected before launching the module and documented in FvwmScript's
manual.

Each instruction consumes a step. Each For iteration and While condition
attempt consumes another, including empty loop bodies and the final false
While condition. Nested blocks and synchronous SendSignal handlers share the
outer handler's allowance. A failed nested entry does not acquire a depth slot.
Failure stays latched during unwind, so a subsequent nested call cannot refresh
that handler's budget.

Exhaustion stops further instructions in that handler, unwinds existing C
frames through their normal cleanup, and reports one diagnostic at the outer
boundary. Diagnostics use the module logger when available and stderr otherwise.
Earlier side effects and published values remain. Later Init/widget/periodic/
quit-handler entries from outside an execution start a fresh allowance. No
longjmp, threads, runtime dependencies or changes to the WM event loop are added.
The policy state is compact, allocation-free and local to the module.

Work counts are not elapsed-time bounds. GetOutput retains its independent
five-second default/override; other module operations and command execution can
still take time. The thresholds are configurable compatibility choices, not a
measured universal maximum for FVWM themes. Value allocation limits, widget
arithmetic and remaining FIFO processing remain open.

## Verification

Native policy coverage includes a shared allowance across sibling/nested calls,
exact completion, first excess, sticky failure during unwind, depth 64, maximum
step configuration and recovery on the next outer entry. A preliminary macOS C
unit run also passed; the matrix below is the supported-platform gate.

The real-module fixture verifies seven outcomes: default 10000 steps and
periodic recovery; empty For and While termination followed by another event;
64-deep synchronous signals sharing the handler state; valid overrides with
invalid settings retaining them; exact five-step completion; and one diagnostic
per stopped handler. It checks that later instructions do not execute and that
previously completed values remain. A 20000-step override completes 6000
iterations; a configured four-level signal chain stops at four.

| Target | Full suite | Execution checks | Existing widget checks | Fresh staged install |
| --- | --- | --- | --- | --- |
| Linux ARM64 GCC | 65/65 | 7/7 under Xvfb | 24/24 | passed |
| Linux ARM64 Clang/lld | 65/65 | 7/7 under Xvfb | 24/24 | passed |
| Linux AMD64 GCC | 65/65 | 7/7 under Xvfb | 24/24 | passed |
| FreeBSD ARM64 | 54/54 | 7/7 under Xvfb and default Xorg | 24/24 on both | passed |
| OpenBSD ARM64 | 54/54 | 7/7 under Xvfb and default Xenocara/wsfb | 24/24 on both | passed |
| FreeBSD AMD64 | 54/54 | 7/7 under Xvfb and default Xorg | 24/24 on both | passed |
| OpenBSD AMD64 | 54/54 | 7/7 under Xvfb and default Xenocara | 24/24 on both | passed |

Full gates include existing interaction, compatibility, module transport,
selection, session, image and event-loop coverage. Linux's produced C ELF
hardening checks pass; optional Go is disabled. Native Linux ASan/UBSan CI is
unrun. Meson's sanitizer environment variables in ordinary logs do not imply
instrumentation.

## Evidence and fixture corrections

Evidence is in `builddir/codex-2026-10-04/script-execution/`. The final matrix
summary validates every logged result and return code, seven execution outcomes,
24 widget outcomes, source fingerprints, source permission entries and fresh
install records. All seven archives match 86 source fingerprints and 1138 mode
entries. Runners explicitly reconfigure and refresh every trusted overlay source
before compiling. Verification uses the preceding frozen source baseline plus
this owned follow-up; it is not a claim about unrelated concurrent theme edits.

The initial new fixture used incorrect widget-case syntax and was rejected by
the parser. Its failed focused log and source snapshot are retained. Correcting
it to `Case message of` with numbered labels passed both Linux ARM64 compilers
before the final full gates. No runtime policy change was needed for that failure.

Fresh installs are in newly created empty staging directories. The first
comparison assumed identical whole-file hashes on BSD and failed. Meson removes
build runtime-library paths during installation there; final records retain
both path lists and whole-file hashes and verify matching executable text and
read-only constants. Installed cores also run --version and install the X session
entry. The comparison correction does not change runtime source or test results.

Only this batch's QEMU snapshots on ports 2291–2294 are used. They are shut down
and released after collection; other project guests remain untouched.

The next policy decision is documented in
[SCRIPT-VALUE-LIMITS-PROPOSAL.md](../decisions/SCRIPT-VALUE-LIMITS-PROPOSAL.md). No proposed
value-storage limit has been implemented or accepted in this batch.

The subsequent user-approved storage limits are implemented and verified in the
[2026-10-05 storage follow-up](VERIFICATION-2026-10-05-SCRIPT-STORAGE.md).
That report supersedes this batch's pending allocation-limit status.
