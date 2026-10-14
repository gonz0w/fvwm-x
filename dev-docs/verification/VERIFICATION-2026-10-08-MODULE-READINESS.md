# Combined module readiness checkpoint

Status: ten related stories are strictly locally qualified from one combined
matrix. All three components also pass final normal/scoped and native BSD ARM64
focused checks. No component-only full matrix was dispatched. The earlier
six-story preparation is retained under before-script-merge.

The batch combines [IconMan poll/tooltip cancellation](VERIFICATION-2026-10-08-ICONMAN-READINESS.md),
[Buttons/borrowed-session readiness](VERIFICATION-2026-10-08-BUTTONS-READINESS.md)
and [Script periodic/session readiness and Swallow ownership](VERIFICATION-2026-10-08-SCRIPT-READINESS.md).
It retains queued X priority, configured tooltip delays and borrowed FSM descriptor
ownership. Inactive sessions return immediately. Script periodic tasks use an
initialized monotonic clock; pending sessions are serviced without requiring a
periodic block. Unfilled Swallow widgets destroy their own placeholders without
closing Script's client connection. Existing configuration names and syntax remain
usable; no mandatory dependency or core X-state ownership change is introduced.

Frozen source: 1366 files, seventeen owned paths from qualified popup1363. The
archive is the final Script candidate byte-for-byte under the combined name.
SHA-256: 6439c478eb23193aef8b1a68b35d02082bd7218bfc6a7e8a4492242edad74312.
Evidence: builddir/codex-2026-10-08/module-readiness-production.
Seven final evidence archives pass strict hash/mode, registered-test, installed-byte
and complete payload/EOF validation. Accepted results: 1472 standard entries across
eight compiler profiles, 120 staged identities, 180 BSD installed/default-X driver
invocations (45 each), optional 189/169 and 76 scoped checks (27 pure with leaks
enabled; 49 process/core with leaks disabled). Both architectures pass on Linux,
FreeBSD and OpenBSD. No combined-gate failure, affected rerun or source change was
needed. Original component fixture/runner failures and source snapshots remain
retained in their reports. Sync preserved qualified popup evidence.

Ordinary deadlines and the existing finite OpenBSD AMD64 TCG harness
profile remain unchanged. Script session controls run the module on default BSD
X and a real XSMP client on a separate owned Xvfb display. Linux/FreeBSD input FD
4096 is checked where permitted; OpenBSD's inherited hard-limit gap remains
explicit. Native release CI and physical/toolkit/compositor checks remain separate.
