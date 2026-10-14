# Typed module schemas and startup ownership checkpoint

Status: the frozen combined local six-target/compiler/install/default-X gate is
qualified. All four related stories pass their affected normal/scoped sanitizer
and native BSD ARM64 checks. Existing FVWM configuration files remain usable.

The batch combines [typed configure/shared picture construction](VERIFICATION-2026-10-07-MODULE-TYPED.md),
[unsigned monitor identity](VERIFICATION-2026-10-07-MODULE-MONITOR-ID.md) and
[module startup pipe ownership](VERIFICATION-2026-10-07-MODULE-CONSTRUCTOR.md).
It replaces the interim general field packer with the existing consumer structure,
shares picture metadata construction, preserves native unsigned monitor IDs and
closes constructor-owned pipes on fork refusal. No library or configuration
option is added. Earlier iterator and packet boundary repairs remain included.

Frozen source: 1345 files, twelve owned paths from qualified transport1346,
including three deleted packer/header/unit files. Archive SHA-256:
`bc63f5c5fa6cbea4b8acbf34d779ab65c447a010557e9141691db046925b9820`.
Evidence: `builddir/codex-2026-10-07/module-schema-production`.
Scripts pass explicit Python preflight and preserve predecessor evidence before
source sync. Strict validation confirms 1312
standard entries/eight profiles, 120 staged identities, 128 default BSD X driver
invocations, optional 169/149 and 48 scoped checks (16 pure/32 actual core). Seven complete
archives pass source hashes/modes, test identities, installed bytes and compressed
EOF validation.

Subsequent argument-growth/shared-library cleanup is separately frozen and cannot
change this source. Native release CI and physical/toolkit/compositor evidence
remain separate and unrun.

OpenBSD ARM64 initial standard results retain 157/158 passes and one image-control
XOpenDisplay refusal before the image operation. The server log provides no
cause. Three affected-only unchanged-source reruns pass with original outer and
fifteen-second inner deadlines; the other 157 raw rows are retained. Subsequent
install/default-X checks pass. The validator checks the exact replacement row,
original diagnostics and each rerun; this is not a first-pass claim or a
production/fixture repair. The display-connection cause remains unestablished.
