# Final public-release gate

Recorded 2026-10-05 at the user's request. RELEASE-01 is the last execution
stage in [PROJECT-TRACKER.md](PROJECT-TRACKER.md#execution-order). Activate it
after the planned coding work has reached its required verified checkpoints
and the user decides to prepare a release. It does not reorder ongoing coding,
hardening, maintenance adoption or their existing acceptance gates.

This document records launch requirements and ideas, not a passing checklist
or authorization to publish. Status: queued, pending the release decision.

## Entry and ownership

- Establish the release scope and candidate commit/source fingerprint. Confirm
  all required coding work, fixes and qualification for that scope are complete.
  Do not infer completion from archived checkboxes or configured CI workflows.
- Reuse the owning backlog items and evidence below. This gate assembles their
  results and closes public-launch gaps; it does not create duplicate runtime,
  packaging, documentation or test implementations.
- Resolve repository identity, destinations, maintainer/contact ownership,
  release version, credentials and publication actions at release planning.
  Recording this gate does not enable inherited publishing/notification jobs.
- If review finds a new defect or requires new code, return it to the owning
  coding item, verify the resulting candidate and repeat affected release checks.
  A deadline or attractive README is not evidence that a failed gate passed.

## Required public-release review

| Area | Required outcome | Existing owner / gap |
| --- | --- | --- |
| Project identity and attribution | Clear FVWM-X scope and relationship to FVWM heritage, preserved credits/licenses, correct repository/version/session identity and links. Deliberately retain or replace inherited badges, funding, support, release destinations and upstream notification behavior. | CI-01 / REPRO-01 plus the launch identity sweep |
| Public README and demonstration | Real light/dark desktop screenshots, a short demonstration, supported platforms, installation, customization, project differentiators and an honest ready/experimental summary. Link user manuals and contributor entry points. | DOCS-01 plus public presentation |
| Contributions and community | CONTRIBUTING entry point, conduct expectations, actual maintainer responsibilities/contact paths, support channels and useful issue/PR templates. Explain review/test expectations and offer well-scoped starter issues without promising unsupported response times. | DEVELOPERS/WORKFLOW plus public community files |
| Security reporting | Public SECURITY policy, supported release/update policy, owned private reporting channel and disclosure process. Configure GitHub private reporting when the repository/maintainer are ready. Describe actual X11/security limits; do not claim a sandbox or protection that has not been verified. | Existing security backlog / SECURITY-STATUS plus public reporting policy |
| Release artifacts and provenance | Tested source archive tied to the accepted commit, expected generated manuals/install contents, checksums, dependency/license inventory and release notes. Review signing/provenance with established ownership. Verify a clean archive can build/test/install with the intended offline/package profiles. | CI-01 / REPRO-01 / MAINT-02 / LIB / ARCHIVE-01 |
| Install, trial and recovery | Verified recipes for supported Linux/FreeBSD/OpenBSD profiles, a safe isolated trial, clear dependencies/capabilities, reversible setup and upgrade/uninstall guidance. Verify actual installed desktop/session/manual behavior and preserve personal configuration. | COMPAT-01 / VERIFY-01 / SUPPORT / MAINT-02 plus newcomer walkthrough |
| Manuals and lightweight HTML | Useful man coverage for installed commands/modules/options, correct defaults/examples, SEE ALSO links, portable roff rendering/lint and staged package inclusion. Keep HTML optional, static and derived from the same reviewed source. No separate website framework is required for release. | DOCS-01 / DOCUMENTATION / existing manifest and renderer |
| Contributor's first hour | A tested clone → build → test → first bounded change walkthrough and a small native-module/configuration example. Explain architecture/ownership and relevant tests without requiring the whole historical archive. | MAINT-02 / DOCS-01 / ARCHITECTURE / DEVELOPERS |
| Regression protection and support claims | Actual required native tests/sanitizers, relevant real-X and six-target gates, compatibility and dependency qualification. Review focused formatting/static analysis, doc links, man checks and safe example checks; distinguish remaining physical GPU/input/toolkit/compositor qualification from emulated evidence. | CI-01 / VERIFY-01 / COMPAT-01 / LIB / PERF-01 / SUPPORT |
| Standards and accessible defaults | Assemble the pinned requirement/profile coverage register, independent protocol/capability evidence, default theme contrast/keyboard/layout/native rendering review and actual public HTML accessibility results. Close adopted required gaps before the release claim; state optional/delegated/unsupported limits. No blanket desktop/WCAG badge from token or source checks. | STANDARDS-01 / THEME-01 / A11Y-01 / DOCS-01 and implementation owners; [standards](STANDARDS.md), [theme policy](THEME-STANDARDS.md) |
| Repository and release presentation | Clean source/package boundaries, no incidental core dumps/build/VM/cache data in release artifacts, accurate badges and links, concise release notes and public roadmap derived from the authoritative tracker. Preserve upstream Git history/credits and useful archived decisions rather than deleting provenance for appearance. | ARCHIVE-01 / REPRO-01 / DOCS-01 |

## Additional ideas to disposition at release planning

Review each idea for the first release or explicitly record its later scope.
These are retained notes, not a demand to add runtime features after coding
has finished or to defer existing required verification.

- **Teach X11 through FVWM-X:** short code walkthroughs for managed windows,
  module packets, event order, style policy and mixed-DPI transitions.
- **Publish measured improvements:** workloads, hardware, source revisions and
  before/after latency, memory, queue-work and X-request results. Reuse PERF-01
  and related evidence; avoid unsupported performance claims.
- **Safe diagnostic export:** consider a separate opt-in, read-only helper for
  version/capability/platform details with redaction and explicit user control.
  Do not collect full environment/configuration/window titles or upload data
  automatically. New helper code needs its own scoped implementation/tests.
- **Approachable customization:** a curated set of commented recipes for
  bindings, bar buttons, launcher, palettes and desktop services, verified against
  the final default configuration and capability behavior.
- **Visible ongoing quality checks:** selective static analysis and changed-file
  formatting/doc/man/example checks, avoiding a flood of unrelated legacy churn.
  New tooling follows CI-01/MAINT-02 parity and security gates.
- **Short public roadmap:** summarize release goals from PROJECT-TRACKER without
  maintaining a second queue or exposing users to every historical status entry.

Website visual redesign, extra development plugins and broad runtime redesigns
are not implicit release prerequisites. The initial release should present one
polished default desktop, dependable supported installation paths, strong
manuals, real passing qualification and clear contribution instructions.

## Exit evidence and publication

Record a dated release review under `verification/` with the candidate revision,
artifact hashes, required gate results, supported package/platform profiles,
manual/install checks, public links, responsible owners and explicit disposition
of the additional ideas. Resolve failed required checks before marking RELEASE-01
complete. Existing hardware/sanitizer/compatibility requirements remain in force.

Publish only after the user has reviewed the concrete release and authorized
the publication actions/destinations. No GitHub repository, tag, release, package,
website, credentials, private-reporting setting or external notification is
created or changed merely by adding this gate.

References retained from the launch review:
[FVWM project](https://www.fvwm.org/),
[GitHub community guidance](https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions/about-community-profiles-for-public-repositories),
[private vulnerability reporting](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository).
