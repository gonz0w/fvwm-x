# Verification and execution contracts

These local clauses were extracted from [PROJECT.md](../PROJECT.md) without changing their
requirements. Source identity is the consumer revision containing this extraction.
Command paths and literal configuration paths remain relative to the repository root.
Selected applicability is recorded only in root `policy.toml`; linked contracts retain
their existing scope. The owner changes required policy through the existing process.

## Verification and review

Documentation-only changes use `make check`, working/staged whitespace checks and affected source/link review. Preserve the current larger-batch and risk-based cadence; no WM build, VM or broad matrix solely for this adoption.

Follow existing behavior-first/regression and PRD/EDD/Test Design obligations
where they apply. Prose changes use source, link, policy and reader checks; do not
invent red/green runtime history. Preserve final required checks and conservative
fallbacks. Record passed, failed, skipped, unavailable and not-applicable results
separately. A stored recipe or component merge is not native, browser, hardware,
performance, operational or release acceptance.

On 2026-10-24 the owner approved lasting bounded read-only agent reviews of
accepted work. The lead may assign a finite source/policy/evidence inspection to
a separate agent and record the reviewer, exact candidate, scope, findings and
resolution. This replaces any previous blanket no-agent rule for that review
scope only. The October 24 routing decision below additionally permits bounded
implementation delegation. Candidate/native execution, other-resource control
and merging retain their existing authority gates. Self-review remains self-review;
an agent review is not an independent human approval or a GitHub approving review.
