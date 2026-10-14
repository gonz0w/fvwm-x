# Unsigned monitor identity in configure records

Status: final focused Linux normal/scoped sanitizer and native FreeBSD/OpenBSD
ARM64 checks pass. This source is outside the frozen transport checkpoint and
will join typed configure/picture cleanup in one combined batch. No FVWM
configuration names, syntax or arguments change.

Configure monitor identity now copies the native unsigned RROutput value directly
into the existing unsigned native-word consumer field. The prior builder widened
it through signed int, giving different upper bits for identifiers with bit 31
set on the supported 64-bit targets. Other configure fields, window identity,
flags, layout, padding and packet ordering remain unchanged.

The permanent actual-core packet fixture uses bounded synthetic monitor metadata
at 0x7fffffff, 0x80000000 and 0xffffffff, checking exact direct/broadcast consumer
bytes. These samples issue no requests using synthetic resources and do not claim
a physical server assigned those identifiers. Ordinary managed-window and
signed geometry/short/raw fields remain covered. A prior-source comparison
confirms four signed-widened results (two upper identifiers through two senders)
in both normal and scoped sanitizer profiles; all other fields match.

Final Linux normal and scoped Clang ASan/UBSan profiles each pass eleven pure
and twenty-two actual-core tests. Pure units enable leak checking; startup/core
controls retain the established leak-disabled scope. Native BSD ARM64 overlays
pass the runtime unit and expanded consumer fixture. Four complete archives
validate payload hashes, modes and compressed EOF. No production/fixture repair
or deadline extension was needed in these final gates.

Frozen source: 1343 files, two owned changes from the separate typed source.
Archive SHA-256:
`367d38d0cd636c054792a3092b6149455da6331f67dffecb5fdf90049caa0fee`.
Evidence: `builddir/codex-2026-10-07/module-monitor-production`.
Combined platform/install/default-X/optional qualification remains pending.
Native Linux release CI and physical/toolkit/compositor evidence remain unrun.

Combined local qualification now passes under the exact schema1345 checkpoint:
1312 standard entries/eight compiler profiles, 120 staged identities, 128 default
BSD X driver invocations, optional 169/149 and 48 scoped checks. Seven archives
validate. The [schema report](VERIFICATION-2026-10-07-MODULE-SCHEMA.md) retains
the initial OpenBSD ARM64 display refusal and three unchanged affected passes;
this does not claim every initial run passed. Native release CI and physical
evidence remain separate.
