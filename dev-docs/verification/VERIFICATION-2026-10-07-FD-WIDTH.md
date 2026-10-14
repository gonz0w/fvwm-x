# Finite descriptor width from one system query

Status: focused Linux normal/scoped sanitizer and native FreeBSD/OpenBSD ARM64
checks pass. This source joins argument growth/shared-library cleanup in the next
combined batch, separate from frozen schema qualification. No configuration
command, format, limit or dependency changes.

GetFdWidth now reads the system limit once into long, returns a positive result
within FD_SETSIZE and uses FD_SETSIZE when the query is unavailable/nonpositive.
The existing upper clamp remains. Initialization uses the same helper, so an
unavailable result cannot become the core's negative readiness/admission width.

A bounded prior-source normal/leak-enabled sanitizer unit confirms -1 and zero
previously propagated unchanged. The old min macro queries twice for results
below FD_SETSIZE; ordinary high results are queried once. No invalid width was
passed to select and no large descriptor set was opened by these controls.

The permanent native unit checks unavailable/zero, one, both adjacent bounds,
INT_MAX and (where sysconf is used) LONG_MAX. Each helper/initializer operation
performs one query and returns the expected finite width. A real unmodified host
query also stays in range. Final Linux normal/scoped profiles each pass twelve
pure and twenty-four actual-core checks, retaining constructor and icon behavior.
Pure units enable leak checking; actual core controls retain the documented
leak-disabled scope. Both native BSD ARM64 overlays pass the deterministic/native
unit using isolated objects. Four full archives validate payload hashes/modes and
compressed EOF: twelve Linux focused, eleven per BSD overlay and fourteen
prior-source files.

A generated runner quote typo was caught by explicit Python preflight before
staging or dispatch. The original script remains; no production repair or timeout
extension was needed in final focused checks.

Frozen source: 1346 files, three owned additions/changes from argument source1345.
Archive SHA-256:
`45a0ac82b7eee49194e0edf90827d569e0c508bde975f8045fecee775f8629ca`.
Evidence: `builddir/codex-2026-10-07/fd-width-production`.
This covers descriptor-width queries, not all high-descriptor ownership or
readiness paths. Combined platform/install/default-X qualification, native release
CI and physical/toolkit/compositor checks remain separate.

Combined local qualification now passes under descriptor1349: 1336 standard
entries/eight profiles, 120 staged identities, 132 default BSD X drivers,
optional 172/152 and 55 scoped checks (22 pure/33 core). Seven archives validate.
The [descriptor report](VERIFICATION-2026-10-07-MODULE-DESCRIPTORS.md) retains
the original OpenBSD display refusal and Linux AMD64 Xorg teardown failure with
three unchanged affected passes each. Native release CI/physical evidence remains
separate; this does not claim every initial run passed.
