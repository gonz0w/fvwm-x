# Agent Temper source record

This offline bundle supplies the portable development method for FVWM-X.
[Local project policy](../dev-docs/PROJECT.md) records its composition and action authority.
No optional profiles are selected. The two retained profiles only satisfy
upstream relative links; their presence does not activate them.

- Source: [gonz0w/agent-temper](https://github.com/gonz0w/agent-temper).
- Revision: `588698ff019fbde5bfc01f426c02af66c4987b6d`.
- Tree: `cd59f42e27e5f982f24cbdbb59a8b9b04d1c11b1`.
- Adoption work: [issue #192](https://github.com/gonz0w/fvwm-x/issues/192).
- Rights: complete upstream [BSD-3-Clause license](LICENSE); original project
  licensing and third-party notices remain in [the local license](../COPYING)
  and [notices](../THIRD_PARTY_NOTICES.md). No project relicensing is implied.

## Retained inventory

These eight files are byte-identical to the immutable upstream commit:

- [LICENSE](LICENSE)
- [METHOD.md](METHOD.md)
- [Workflow](docs/reference/workflow.md)
- [Verification](docs/reference/verification.md)
- [Documentation](docs/reference/documentation.md)
- [Extensions](docs/reference/extensions.md)
- [GitHub profile — inactive](profiles/github.md)
- [Strict test-first profile — inactive](profiles/strict-test-first.md)

[SHA256SUMS](SHA256SUMS) covers those eight files and this local source record,
with paths relative to `.agent-temper/`. Only the manifest itself is excluded.
The exact bundle is these ten files. Signed consumer history binds the manifest;
checksums alone do not authenticate an upstream source.

## Verification and updates

Verify exact inventory and manifest coverage, every digest and all eight upstream
bytes against the recorded commit in a trusted retained Agent Temper checkout.
Validate affected local links and heading fragments, license attribution and the
combined policy with separate reader review. Existing repository checks remain
required; they do not by themselves establish upstream equivalence or policy
meaning. No moving branch is fetched during verification.

An update needs a bounded item in the existing work authority, semantic review,
new immutable source identity, complete retained content, regenerated checksums
and applicable checks/review. Do not edit imported prose in place. If a required
source cannot be resolved or policies conflict, report the gap and stop affected
work. Resolve changed local decisions with the owner before applying them.

Keep product/platform rules local. Record reuse findings in the adoption item;
[extension guidance](docs/reference/extensions.md) distinguishes a core correction
from an optional profile or local rule. No upstream change silently advances this
pin, and adoption grants no execution, merge, release or deployment authority.
