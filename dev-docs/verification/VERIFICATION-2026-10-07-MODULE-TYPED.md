# Typed configure and shared picture construction

Status: final focused Linux normal/scoped sanitizer and native FreeBSD/OpenBSD
ARM64 controls pass. This source remains separate from the frozen transport
checkpoint; combined qualification is pending. No FVWM configuration migration
or new library is needed.

Configure construction now fills the existing ConfigWinPacket consumer structure
directly, initializes all native structure/final-word padding and copies it into
aligned operation-local packet storage. A compile-time capacity assertion includes
the existing header. The private variadic structured senders, CONFIGARGS macro,
unused dummy and general field packer are removed. Fixed typed assignments keep
native scalar widening, window identity, geometry, hints, colors and raw flags.
Allowed actions fill the initialized local snapshot instead of returning shared
static storage. References in the managed-window schema name the current builder.

Both picture senders now share one bounded metadata constructor. Present and
absent pictures retain the same six fields, ordinary/truncated names, termination,
padding and existing sender filtering. The existing sender receives a header
declaration; the fixture's private declaration is removed. Picture fields are
captured before callbacks without adding allocation or persistent state.

The sender is 122 lines shorter than the frozen transport version. Three retired
packer/header/unit files contain 202 additional lines. Those general-length tests
belonged to the retired algorithm; the active fixed schema is compile-time bounded
and keeps the independent actual consumer-layout and native compiler controls.
The permanent fixture now also checks nonempty picture metadata at seven name
lengths through both senders, with exact bytes and zero temporary allocations.
These are code/operation-count results, not physical responsiveness measurements.

Final Linux normal and scoped Clang ASan/UBSan profiles each pass eleven pure and
twenty-two actual-core tests. Pure units enable leak checking; startup/core
controls retain the established leak-disabled scope. Native BSD ARM64 overlays
pass the runtime unit and expanded consumer fixture. The preceding corrected
configure/picture builder passes the same ordinary/boundary consumer checks in
normal/scoped comparisons. Four complete archives validate all payload hashes,
modes and compressed EOF. A private early native runner selected the old output
fixture at link time; the runner, errors and corrected consumer passes are retained.
No production edit was required for that runner correction.

Frozen source: 1343 files, nine owned changes/deletions from the 1346-file frozen
transport source. Archive SHA-256:
`5bdae137471016c784cacd5b42937a013adab16dbd2360604108f26d8b190557`.
Evidence: `builddir/codex-2026-10-07/module-typed-production`; initial private review
is in `builddir/codex-2026-10-07/configure-typed-review`.
The unsigned monitor-ID follow-up has a separate source. Native Linux release
CI and physical/toolkit/compositor checks remain unrun.

Combined local qualification now passes under the exact schema1345 checkpoint:
1312 standard entries/eight compiler profiles, 120 staged identities, 128 default
BSD X driver invocations, optional 169/149 and 48 scoped checks. Seven archives
validate. The [schema report](VERIFICATION-2026-10-07-MODULE-SCHEMA.md) retains
the initial OpenBSD ARM64 display refusal and three unchanged affected passes;
this does not claim every initial run passed. Native release CI and physical
evidence remain separate.
