# Review and action authority

These local clauses were extracted from [PROJECT.md](../PROJECT.md) without changing their
requirements. Source identity is the consumer revision containing this extraction.
Command paths and literal configuration paths remain relative to the repository root.
Selected applicability is recorded only in root `policy.toml`; linked contracts retain
their existing scope. The owner changes required policy through the existing process.

## Decisions and action authority

Cam (@gonz0w) remains the owner. Agents may prepare accepted changes, choose
routine implementation/documentation details within scope, run authorized checks
and prepare signed, actual-author-DCO review candidates. Existing commit and PR rules remain. Use only the existing authorized signing identity;
do not create keys, trust files or alter security configuration to force a pass.

Maintainer direction, 2026-10-31: continue through R1 without waiting for the
owner to merge each PR. The lead has standing authorization to merge coherent R1
checkpoints after their applicable checks and separate review pass, and to
continue with the next accepted batch until blocked. This replaces the per-PR
merge wait for R1. Record the exact reviewed head, canonical signature/tree and
author DCO before each merge; preserve failed or pending broader acceptance.
Stop dependent work for unresolved findings, missing required execution or an
owner decision, and continue other accepted R1 work where possible.

Outside that R1 direction, PRs retain explicit maintainer merge authorization.
Method adoption and review permission supply no merge authority themselves.
Release, publication, deployment, credentials, operational/native effects and
changes outside the owning work item retain their separate authority. The
adopted method grants none of these.

If a required decision is missing or applicable rules conflict, record the
question in the owning work item, ask the owner and stop only dependent work.
Continue independent authorized work. Save the old decision, replacement, reason,
scope and authority here or in its canonical local decision record before
proceeding. Apply recorded answers in later sessions without asking again.
Compatible rules apply together; file order, a newer upstream timestamp or an
agent preference does not override them. No Temper core deviation is declared.
