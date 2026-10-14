# Combined native module consumer checkpoint

Status: locally qualified from the exact 1361-file source below. All six
Linux/BSD targets and eight compiler profiles pass; seven final archives pass
strict validation. Event-only and Auto-only full matrices were never dispatched.

The checkpoint combines [Event/configuration cleanup](VERIFICATION-2026-10-07-MODULE-EVENTS.md)
and [Auto readiness](VERIFICATION-2026-10-08-AUTO-READINESS.md): common packet and
termination handling, checked action arguments/tails, short-line progression,
obsolete quit cleanup, single-descriptor poll, checked delay conversion and short
window-packet admission. Existing supported configuration files remain usable;
no new mandatory library is added. Authoritative core X state is unchanged.

Frozen source: 1361 files, seven owned paths from corrected action1358; three
new drivers. The source archive is the final Auto source byte-for-byte under the
combined name. SHA-256:
`4ae424a94004145801b7464f4eb272fd2411a3e86d0e326e074d50f6a0e89c94`.
Evidence: `builddir/codex-2026-10-08/module-consumer-production`.
Earlier focused failures, source snapshots and actual descriptor-limit gaps
remain in their owning component reports/evidence.

Final normal/scoped profiles each pass four pure and five actual process/module
tests. Linux/FreeBSD actual Auto controls exercise descriptor 4096; OpenBSD's
inherited hard-limit exclusion is reported rather than changed. Event's final
wire control covers an upper-range window ID; native BSD ARM64 Event/Ident and
Auto controls pass. Component archives validate complete payload hashes/modes
and compressed EOF. Native release CI remains separate.

The combined matrix passes 1416 standard entries across eight compiler profiles,
120 staged code identities and 152 BSD installed/default-X driver invocations
(38 per target). Optional profiles pass 182/162 entries. Scoped sanitizers pass
65 distinct checks: 26 pure with leaks enabled and 39 process/core with leaks
disabled. All seven archives validate source hashes/modes, evidence payloads,
compressed EOF and installed identities. See qualification-summary.json in the
evidence directory. No full-matrix failure, affected rerun or source change was
required; earlier component failures remain retained.

Sync verified the corrected predecessor and preserved its accepted evidence.
Blocking packet transport, whole resource teardown and native release CI,
physical, toolkit and compositor verification remain open. Ident's separate
readiness path joins the next [popup batch](VERIFICATION-2026-10-08-POPUP-READINESS.md).
