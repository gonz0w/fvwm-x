## Problem and resulting behavior

Describe the concrete problem, final behavior and bounded scope.

## Roadmap and compatibility

Refs gonz0w/fvwm-x#N for broader/partial work. Use Closes only when every
criterion of that issue is satisfied on merge. Link tracker IDs and decisions.
State whether supported configuration names, arguments or formats change.
Name the R1/R2/R3 target, Major/Minor work breadth and exact acceptance criteria
completed. New feature scope needs an explicit later target or approved R1 scope
change; a broad later-release audit is not an automatic prerequisite.

## Local validation

Record head/tree/base, actual commands/results and appropriate native/X11,
race/sanitizer/install receipts. Preserve failures, skipped checks and gaps.
For a focused feature PR, name the integration cycle and deferred platform/
install/default-X checks. Deferred checks are not passes; full matrix results
are not required for every ordinary PR. See dev-docs/WORKFLOW.md.
`make verify` covers portable foundation checks only. Actions is disabled.

## Documentation and review

- [ ] Scope, tracker/issue/milestone/Project state and evidence agree.
- [ ] Contributor commits are signed and contain author DCO trailers.
- [ ] Required affected checks ran locally against the identified candidate.
- [ ] Manuals and the brief README status are updated when applicable.
- [ ] Fixture cleanup is verified; no private artifacts are included.

Leave this PR open for maintainer review; a component merge is not release acceptance.
