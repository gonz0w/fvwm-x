# Module startup and descriptor ownership checkpoint

Status: five related stories pass their affected Linux normal/scoped sanitizer
and native BSD ARM64 checks. The combined source and runners are locally qualified;
the preceding frozen schema checkpoint remains locally qualified.
Existing FVWM configuration files remain usable.

The batch combines [argument growth and shared-library cleanup](VERIFICATION-2026-10-07-MODULE-ARGUMENTS.md),
[finite descriptor width](VERIFICATION-2026-10-07-FD-WIDTH.md) and
[ICE/session admission and watched records](VERIFICATION-2026-10-07-FD-SELECT.md),
and [repeated ICE callback installation](VERIFICATION-2026-10-07-ICE-RETRY.md).
It removes per-token vector growth and implementation inclusions, checks system
query results once and validates select-based ownership before readiness use.
Borrowed ICE watch records retain their existing close-callback ownership.

Frozen source: 1349 files, fifteen owned paths from schema1345, including three
new units and one policy header. Archive SHA-256:
`b08cb75a9c1dc007ed86b1aca1508a4c02f03643a27510b59f43eef1e4e16e87`.
Evidence: `builddir/codex-2026-10-07/module-descriptor-production`.
Python preflight checks every runner. Source sync requires the qualified schema
baseline and preserves its evidence. Strict validation confirms 1336 standard entries/eight profiles, 120 staged identities, 132
default BSD X driver invocations (including real session/watch checks), optional
172/152 and 55 scoped checks (22 pure/33 actual core). The validator retains no predecessor recovery assumptions; the newly recorded
OpenBSD ARM64 failure has its own original log and exact affected-only recovery.

The established OpenBSD AMD64 verification-profile-only TCG allowance stays
finite; ordinary deadlines remain unchanged. Native Linux release CI, forced
high live admission, wider readiness and physical/toolkit/compositor checks
remain separate.

OpenBSD ARM64 initially passes 160/161 checks; the cancellation fixture cannot
open a nested display before exercising cancellation. Three affected-only
unchanged-source checks pass at original deadlines, retaining the other 160 rows;
install/default-X checks pass. A separate owned two-Xvfb control reproduces both
servers reporting :0 and loss of second-server connectivity when the first closes.
Explicit server-locked displays pass the same control. This is a concrete harness
collision; exact server identities from the initial failure were not retained, so
the relationship to that particular refusal remains an inference. The harness
repair has separate source/evidence and is outside this frozen checkpoint.

All seven final archives validate hashes/modes, registered tests, installed bytes
and compressed EOF. Linux AMD64 Clang initially retains 172/173 passes and one
Xorg teardown deadline after the first geometry interaction succeeds. Three
affected-only unchanged-source checks pass at original deadlines with serialized
execution, retaining the other 172 rows. Clang install checks then pass. Cause
remains unestablished; no source repair or deadline extension is claimed. Both
initial failure archives and exact recovery logs remain retained. Qualification
is not a first-pass claim. Native release CI/physical evidence remains separate.
