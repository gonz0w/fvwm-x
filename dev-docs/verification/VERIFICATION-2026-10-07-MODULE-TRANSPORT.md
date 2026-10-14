# Module iteration and packet construction checkpoint

Status: the frozen combined local six-target/compiler/install/default-X checkpoint
is qualified; focused normal/scoped sanitizer and native BSD ARM64 checks pass.
This source is separate from locally qualified lifecycle ownership. No FVWM
configuration migration is needed.

The related batch combines [iterator lifetime](VERIFICATION-2026-10-07-MODULE-ITERATION.md),
[bounded word/named packets](VERIFICATION-2026-10-07-MODULE-PACKETS.md),
[pure structured field packing](VERIFICATION-2026-10-07-MODULE-FIELDS.md) and
[configure consumer identity](VERIFICATION-2026-10-07-MODULE-CONFIGURE.md).
It preserves surviving producer delivery across nested retirement, uses fixed
operation-local construction storage and publishes complete bounded configure
records with the correct managed-window identifier. Existing copied asynchronous
output ownership and synchronous completion remain intact. No library is added.

Final affected Linux normal/scoped profiles each pass twelve pure and twenty-two
actual-core tests. Both native BSD ARM64 overlays pass runtime/field units and
the expanded packet fixture; preceding iterator overlays cover both retirement
cases. Prior-source comparisons preserve ordinary name/configure bytes except
the reviewed incorrect identifier, and demonstrate removal of temporary named
construction allocations. Focused source/evidence remain separately archived.

Frozen source: 1346 files, fifteen owned additions/changes from the qualified
1339-file lifecycle checkpoint. Archive SHA-256:
`39d743c50a420a493686332bf0cf78cb47d16c4757cdc17425b929182bd88db8`.
Source/combined evidence: `builddir/codex-2026-10-07/module-transport-production`.
Seven complete archives validate exact source hashes/modes and compressed EOF,
all registered test identities and staged bytes after install RPATH adjustment.
All eight compiler profiles pass: 1312 standard entries (170 per Linux profile,
158 per BSD profile), 120 staged identities and 124 default BSD X driver
invocations (31 per server). Optional software/SVG-disabled profiles pass 169/149
entries. Scoped sanitizers pass 48 checks: 17 pure with leak detection enabled and
31 actual-core controls with the documented leak-disabled scope. No combined
source repair or affected rerun was needed. OpenBSD AMD64 retains the established
finite TCG-only verification-profile allowance; ordinary deadlines are unchanged.
A local sanitizer runner quote typo was caught and corrected during Python
preflight before dispatch; the original script is retained. No production edit
was required for that correction.

Native Linux release sanitizer CI and physical/toolkit/compositor checks remain
unrun and are not established by local QEMU/emulated results.
