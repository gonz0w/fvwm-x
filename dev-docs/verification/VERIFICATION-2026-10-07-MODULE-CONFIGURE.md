# Configure packet consumer layout and identity

Status: final focused Linux normal/scoped sanitizer and native FreeBSD/OpenBSD
ARM64 controls pass. Combined qualification remains separate. No configuration
names, syntax or arguments change.

Exact consumer-layout review found that CONFIGARGS supplied the address of its
local pointer-to-window variable when packing the opaque window identity. The
field therefore contained a temporary local address rather than the managed
window record described by ConfigWinPacket. The sender now supplies the address
of the actual window pointer value. It preserves the existing native packet
layout and all ordinary geometry, hints, colors, flags and allowed-action fields.
This metadata correction does not establish a desktop compromise or an exposed
remote interface.

The permanent packet fixture now creates a real managed X window through
AddWindow, then compares direct and broadcast configure payloads byte-for-byte
with the shipped consumer structure. A second sample covers signed coordinate
widening, native integer endpoints, short fields and raw flags/actions. The
window identity, declared byte size and final padding are checked explicitly;
construction still performs no temporary allocation. Existing packet lengths,
names, lifetime, actual transport/sync/backpressure controls remain included.

A bounded prior-source comparison compiles the exact 1343-file builder and the
new fixture with one explicit baseline expectation: the identity differs from
the managed record. All other consumer fields and padding match the same
ordinary and boundary samples. Both normal and scoped sanitizer comparisons
pass. This demonstrates the metadata difference without claiming a latency or
physical-desktop result.

Final normal and scoped Clang ASan/UBSan profiles each pass twelve pure and
twenty-two actual-core tests. Pure units enable leak detection; startup/core
controls retain the existing leak-disabled scope. Native BSD ARM64 overlays pass
the field/runtime units and expanded packet fixture. Four complete focused and
prior-source archives validate payload hashes, modes and compressed EOF.

Frozen source: 1346 files, two owned changes from the separate
[field-builder source](VERIFICATION-2026-10-07-MODULE-FIELDS.md).
Archive SHA-256: `39d743c50a420a493686332bf0cf78cb47d16c4757cdc17425b929182bd88db8`.
Evidence: `builddir/codex-2026-10-07/module-configure-production`.
Full six-target/compiler/install/default-X/optional qualification and native
Linux release CI remain separate; physical/toolkit/compositor evidence is unrun.

Combined qualification update: this story is locally qualified in the frozen
1346-file [transport checkpoint](VERIFICATION-2026-10-07-MODULE-TRANSPORT.md):
1312 standard entries, 120 staged identities, 124 default BSD X drivers, optional
169/149 and 48 scoped sanitizer checks. Seven complete archives validate source
hashes/modes and EOF; native release CI and physical checks remain separate.
