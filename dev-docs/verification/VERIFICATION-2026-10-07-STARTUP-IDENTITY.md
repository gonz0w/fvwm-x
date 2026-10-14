# OUTPUT-03 / STACK-01: finite startup and indexed parent identity

Status: combined frozen checkpoint locally qualified on 2026-10-07.
Native Linux release CI and physical desktop coverage remain separate.

The user approved startup defaults on 2026-10-07. ModuleSynchronous captures the
current ModuleTimeout (30 seconds initially) or a positive explicit Timeout,
uses one monotonic deadline through input/output/activity and nested commands,
and retains a live producer asynchronously when the barrier ends. Escape and
Expect-prefix behavior remain; ordinary X events stay queued. Deadline checks
cannot preempt an executing command. A complete frame read at the deadline stays
in the existing transport buffer for later asynchronous admission, including a
final continuation=0 command. The frame pins producer lifetime across retirement
and nested cleanup; terminal readiness errors stop retrying. Expect/null-action
cleanup now frees its owned string.

STACK-01 reuses existing client X contexts with XID and managed-membership checks,
plus the compatible construction/XID-reuse list fallback. Its separate
[measurement report](VERIFICATION-2026-10-07-STACK-IDENTITY.md) records the lookup
reduction and the unchanged reverse-chain closure cost. No closure rewrite is
included in this checkpoint.

## Frozen source and focused checks

Final archive: `builddir/codex-2026-10-07/startup-identity/source-startup-identity.tgz`.
1279 files; SHA-256
`47a6ee359e996d1850a62143755014394f1e88edce6a1a2299db929f735d265d`.
It derives from the accepted 1274-file OUTPUT-02/DATA-01 snapshot and changes only
thirteen owned paths. Other chats' current desktop/theme/tooling edits are
excluded. Root documentation is the maintained project record, not a claim that
the entire shared working directory matches this source archive.

Eight focused Linux entries pass: startup, packet deadline, asynchronous output,
client identity, actual stack/lifetime, event policy, module runtime and child-list
ownership. Startup covers 29 groups: omitted/captured/explicit limits, finite
readiness, activity, partial/interrupted/complete input handoff, output budgets,
errors, nested waits, retirement/final-command pins, clocks, Escape, deferred X
and real asynchronous continuation after both omitted and explicit timeout.
The finite real producer originally held the barrier for 2.030 seconds despite
ModuleTimeout=1; it now returns near one second and all eight commands plus its
later startup response are observed through normal asynchronous transport.

Both native BSD ARM64 groups pass eight affected entries each. Fourteen scoped
Clang ASan/UBSan checks pass: five policy tests with leak detection, nine actual
core tests without leak detection because fixtures exit before normal teardown.
These local checks are separate from native Linux release CI.

## Retained development failures

Linux fixture-only build corrections fixed missing/context header order; an
initial queued-command assertion incorrectly assumed link wrappers intercept
calls within their defining compilation unit. Its corrected check observes the
actual ModuleTimeout effect and queue/ref release. Those earlier diagnostics are
retained separately from final passes.

The initial frozen attempt used `/bin/true`, absent on both BSD guests. The final
snapshot changes only that fixture path to `/usr/bin/true`; all affected BSD and
scoped sanitizer checks were rerun. OpenBSD ARM64 also exhausted its small root
partition while retaining failure output. Six owned historical verification
directories moved to `/home/fvwm-owned-checkpoint-evidence`, keeping old paths as
symlinks. All 679 relocated file hashes/modes match before/after; no evidence was
deleted. Initial frozen source, both initial BSD archives and initial sanitizer
archive are retained and readable to EOF under `initial-frozen-attempt/`.

OpenBSD AMD64 also filled its root partition. Its default Xorg crashed during
VESA initialization before FVWM launched. After preserving that server log and
moving six owned historical evidence directories to the larger /home partition,
all 680 relocated file hashes/modes matched, and the affected default-server
rerun passed. This establishes the successful rerun; it does not prove the disk
condition caused the Xorg crash. No WM production change was made for it.

## Combined qualification

All eight standard compiler profiles pass 1040 registered entries: Linux GCC and
Clang on AMD64 and ARM64 (136 each), FreeBSD/OpenBSD native compilers on both
architectures (124 each). All 120 staged binary identities match tested code after
verified install RPATH adjustment. Four default BSD X servers pass nine drivers
each (36 invocations), including 29 startup and five identity groups per server.
OpenBSD ARM64 wsfb's zero-monitor single-root fallback is included.

The XRender-disabled optional profile passes 135 entries; SVG-disabled passes
115. Fourteen scoped sanitizer checks pass, with five pure checks leak-enabled
and nine core fixtures leak-disabled because they exit before normal teardown.
Seven collected evidence archives validate all 1279 source hashes/modes and
complete payloads to EOF. Results and per-archive hashes are in
`builddir/codex-2026-10-07/startup-identity/qualification-summary.json`.

Native Linux release CI, physical hardware, other toolkit clients and external
compositors remain separately unrun. Later JSON/vendor changes are outside this
accepted source snapshot and require their own checkpoint.
