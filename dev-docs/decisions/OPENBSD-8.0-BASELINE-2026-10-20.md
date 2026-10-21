# OpenBSD release baseline — 2026-10-20, clarified 2026-10-21

Accepted by direct user instruction on 2026-10-20: qualify OpenBSD 8.0 and
exclude 7.9. Clarified by the user on 2026-10-21: follow future OpenBSD releases;
8.0 is the current qualified baseline, not a permanent product restriction.
Keep AMD64 and ARM64, default Xenocara, Linux and FreeBSD. No release
publication is authorized.

Maintain one current OpenBSD release baseline rather than backporting to 7.9.
When advancing it, refresh fixed image/bootstrap/package inputs, exact guest
identity checks and their fixtures together. Qualify both architectures with
the owning combined checkpoint before updating support claims. Keep dated
locks and results as historical evidence; a version-policy change alone does
not qualify new artifacts or development snapshots.

The [official announcement](https://www.openbsd.org/80.html) dates 8.0 to
October 20, 2026. Use fixed, verified 8.0 image/bootstrap inputs, release packages
and applicable signed errata. Development snapshots and 7.9 are not substitute
release evidence. Retain original 7.9 evidence/failures without relabeling them.

CI-01 / COMPAT-01 / IMAGE-01 / LIB-07 own a fresh two-architecture checkpoint.
First run the previously missing CI asset-worker, SVG isolation and legacy
property checks; diagnose actual failures before changing code or fixture
policy. Then qualify builds, full install identities, staged geometry and
authenticated default-X consumers. Reopen deferred VESA cause work only as
needed for a reproduced 8.0 acceptance failure. Preserve functional deadlines,
assertions and dependency checks. Native-CPU and physical hardware/toolkit/
compositor qualification and release acceptance remain separate.

The tracker records final source/profile/patch scope. Use isolated 8.0 guests;
retain old sources/builds/disks and canonical evidence, and retire completed
duplicate captures after canonical validation.
