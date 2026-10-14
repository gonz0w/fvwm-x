# GitHub hosting and local verification — 2026-10-13

Status: approved directly by the maintainer's request.

Create `gonz0w/fvwm-x` for code, PR review and roadmap issues. The repository
starts private; public release/publication remains a separate decision. Follow
the DNS/Nomvium example's manual review, signed commits, DCO, milestone/Project
organization and source-bound evidence, without adopting its product scope.

Do not use GitHub Actions. Review, tests, lint, validation and qualifying platform
runs take place locally. Disable Actions at repository level and archive inherited
workflow/bot/funding metadata. Do not add hosted/self-hosted runners, fabricated
success checks or an automatic issue/status synchronizer.

This supersedes requirements for remote CI execution or workflow adoption as
acceptance criteria. It does not waive native Linux sanitizer, relevant X11,
BSD/default-server, hardware or release qualification: record those local results
and remaining gaps honestly. Preserve original dated reports and failures.

The project tracker remains authoritative for execution order and acceptance.
GitHub issues/Project mirror stable tracker IDs, stage and priority. Keep one
status/priority label and matching milestone/Project fields manually. A planning
issue or merged component does not complete a broader milestone/release gate.

Keep README brief and update its status with milestone changes. Use coherent
`codex/` PR batches, leave PRs open for maintainer review, and preserve unrelated
working-tree changes. Repository setup permits initial code publication to the
private repository; it does not authorize release tags, deployment, credential
changes or notifications to upstream.
