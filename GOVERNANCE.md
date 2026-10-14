# Governance

Cam (@gonz0w) maintains FVWM-X in `gonz0w/fvwm-x`. Keep the repository private
until deliberate publication. Repository setup does not authorize a release,
service deployment, credential changes or upstream notifications.

The project tracker owns scope, priorities and execution order. Git owns design,
decisions and exact verification evidence. Issues, milestones and the Project
mirror tracker IDs and organize live review; reconcile them manually, without
an automatic tracker or GitHub Actions.

Require PRs for subsequent main changes, signed contributor commits, author DCO
trailers, resolved conversations and source-bound local evidence. During solo
maintenance there are zero mandatory independent approvals: an author cannot
approve their own PR. Add an independent-review requirement when a second
reviewer joins. No required remote status checks or bypass actors are configured.
Only squash merging is enabled; agents leave PRs open. Preserve DCO trailers and
sign the canonical merge commit. Release tags are immutable and need separate
release acceptance and authorization.

Remote settings and their observed enforcement are recorded in the repository
setup report. Versioned policy files do not apply themselves. Neither local
hooks nor an issue label proves checks passed. Keep failures, skips, native
limitations and configuration compatibility explicit.
